#include "App_freeRTOS.h"
#include "Fusion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "ICM42688P.h"
#include "ICM42688P_Reg.h"
#include "Kalman.h"
#include "BMP388.h"
#include "BMP388_Reg.h"
#include "Compass.h"
#include "App_flight.h"
#include "Buzzer.h"
#include "MagCalibration.h"
#include "AI_Analysis.h"
#define ACC_OFFSET_X 0.1f
#define ACC_OFFSET_Y -7.4f
#define ACC_OFFSET_Z -21.3f
#define ACC_SCALE_X 0.9929f
#define ACC_SCALE_Y 0.9993f
#define ACC_SCALE_Z 0.9992f

uint8_t spi_tx_buf[15];
uint8_t spi_rx_buf[15];
uint8_t BMP388_DMA_rxbuf[6];
float roll_trim = 0.0f;
float pitch_trim = 0.0f;
volatile uint8_t level_trim_ready = 0U;
float roll_deg = 0.0f;
float pitch_deg = 0.0f;
float yaw_deg = 0.0f;
float final_altitude = 0.0f;
float final_speed_z = 0.0f;
float roll_gyro_extern = 0.0f;
float pitch_gyro_extern = 0.0f;
float yaw_gyro_extern = 0.0f;
BMP388_Data bmp388_data;
FusionAhrs ahrs;
FusionBias offset;
CompassRawData raw_mag;
CompassData calibrated_mag;
CompassData mag_data;
kalman_t kalman;
extern I2C_HandleTypeDef hi2c1;
volatile uint8_t baro_sample_ready = 0U;
volatile uint8_t mag_sample_ready = 0U;
volatile uint8_t mag_valid = 0U;
volatile TickType_t baro_last_update_tick = 0U;
volatile uint32_t baro_update_sequence = 0U;
volatile uint8_t baro_valid = 0U;

Remote_Data remote_data = {.yaw = 500, .pit = 500, .rol = 500};
volatile uint16_t raw_motor_pwm[4] = {1000, 1000, 1000, 1000};
volatile Flight_State flight_state = IDLE;
volatile uint8_t reconnect;

extern uint8_t usb_rx_suffer[128];
extern volatile uint8_t usb_rx_flag;
volatile uint8_t is_armed = 0;

volatile uint8_t ai_link_active = 0U;
extern UART_HandleTypeDef huart3;
#define AI_UART_DMA_BUFFER_SIZE 64U
#define AI_UART_RING_BUFFER_SIZE 256U
static uint8_t ai_uart_dma_buffer[AI_UART_DMA_BUFFER_SIZE];
static uint8_t ai_uart_ring_buffer[AI_UART_RING_BUFFER_SIZE];
static volatile uint16_t ai_uart_ring_head = 0U;
static volatile uint16_t ai_uart_ring_tail = 0U;
static volatile uint8_t ai_uart_overflow = 0U;
static volatile uint8_t ai_uart_error = 0U;

void FlightControl_Task(void *args);
#define FLIGHT_TASK_STACK_SIZE 1024
#define FLIGHT_TASK_PRIORITY 4
TaskHandle_t FlightControlTaskHandle;

void AI_Comm_Task(void *args);
#define AICOMM_TASK_STACK_SIZE 512
#define AICOMM_TASK_PRIORITY 3
TaskHandle_t AI_CommTaskHandle;

static HAL_StatusTypeDef AI_UART_StartReceive(void)
{
    HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(&huart3, ai_uart_dma_buffer, AI_UART_DMA_BUFFER_SIZE);

    if (status == HAL_OK && huart3.hdmarx != NULL)
    {
        __HAL_DMA_DISABLE_IT(huart3.hdmarx, DMA_IT_HT);
    }
    return status;
}

static uint8_t AI_UART_PopByte(uint8_t *byte)
{
    uint8_t has_byte = 0U;

    taskENTER_CRITICAL();
    if (ai_uart_ring_tail != ai_uart_ring_head)
    {
        *byte = ai_uart_ring_buffer[ai_uart_ring_tail];
        ai_uart_ring_tail = (uint16_t)((ai_uart_ring_tail + 1U) % AI_UART_RING_BUFFER_SIZE);
        has_byte = 1U;
    }
    taskEXIT_CRITICAL();
    return has_byte;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (huart->Instance != USART3)
    {
        return;
    }

    if (size > AI_UART_DMA_BUFFER_SIZE)
    {
        size = AI_UART_DMA_BUFFER_SIZE;
    }
    for (uint16_t i = 0U; i < size; i++)
    {
        uint16_t next_head = (uint16_t)((ai_uart_ring_head + 1U) % AI_UART_RING_BUFFER_SIZE);
        if (next_head == ai_uart_ring_tail)
        {
            ai_uart_overflow = 1U;
            break;
        }
        ai_uart_ring_buffer[ai_uart_ring_head] = ai_uart_dma_buffer[i];
        ai_uart_ring_head = next_head;
    }

    if (AI_UART_StartReceive() != HAL_OK)
    {
        ai_uart_error = 1U;
    }
    if (AI_CommTaskHandle != NULL)
    {
        vTaskNotifyGiveFromISR(AI_CommTaskHandle, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (huart->Instance != USART3)
    {
        return;
    }

    ai_uart_error = 1U;
    if (AI_CommTaskHandle != NULL)
    {
        vTaskNotifyGiveFromISR(AI_CommTaskHandle, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

void BaroTask(void *args);
#define BARO_TASK_STACK_SIZE 512
#define BARO_TASK_PRIORITY 3
TaskHandle_t BaroTaskHandle;

void BuzzerTask(void *args);
#define BUZZER_TASK_STACK_SIZE 256
#define BUZZER_TASK_PRIORITY 1
#define BUZZER_QUEUE_LENGTH 8
TaskHandle_t BuzzerTaskHandle;
QueueHandle_t BuzzerTask_Queue_Handle;

#define LEVEL_TRIM_REQUIRED_TIME_S 3.0f
#define LEVEL_TRIM_BIAS_SETTLE_TIME_S 27.0f
#define LEVEL_TRIM_GYRO_LIMIT_DPS 0.5f
#define LEVEL_TRIM_ACC_MIN_G 0.95f
#define LEVEL_TRIM_ACC_MAX_G 1.05f

static void LevelTrim_Update(FusionVector corrected_gyro, FusionVector accelerometer, float dt, uint8_t ahrs_startup,
                             uint8_t bias_learning_started)
{
    static float roll_sum = 0.0f;
    static float pitch_sum = 0.0f;
    static float bias_settle_time = 0.0f;
    static float stable_time = 0.0f;
    static uint32_t sample_count = 0U;

    if (level_trim_ready)
    {
        return;
    }

    if (dt < 0.0005f) dt = 0.0005f;
    if (dt > 0.03f) dt = 0.03f;

    const float acc_norm =
        sqrtf(accelerometer.axis.x * accelerometer.axis.x + accelerometer.axis.y * accelerometer.axis.y +
              accelerometer.axis.z * accelerometer.axis.z);

    const uint8_t safe_to_settle = (ahrs_startup == 0U) && bias_learning_started && (remote_data.shutdown == 1U) &&
                                   (remote_data.thr < 50) &&
                                   (fabsf(corrected_gyro.axis.x) < LEVEL_TRIM_GYRO_LIMIT_DPS) &&
                                   (fabsf(corrected_gyro.axis.y) < LEVEL_TRIM_GYRO_LIMIT_DPS) &&
                                   (fabsf(corrected_gyro.axis.z) < LEVEL_TRIM_GYRO_LIMIT_DPS) &&
                                   (acc_norm >= LEVEL_TRIM_ACC_MIN_G) && (acc_norm <= LEVEL_TRIM_ACC_MAX_G);

    if (!safe_to_settle)
    {
        roll_sum = 0.0f;
        pitch_sum = 0.0f;
        bias_settle_time = 0.0f;
        stable_time = 0.0f;
        sample_count = 0U;
        return;
    }

    if (bias_settle_time < LEVEL_TRIM_BIAS_SETTLE_TIME_S)
    {
        bias_settle_time += dt;
        roll_sum = 0.0f;
        pitch_sum = 0.0f;
        stable_time = 0.0f;
        sample_count = 0U;
        return;
    }

    roll_sum += roll_deg;
    pitch_sum += pitch_deg;
    stable_time += dt;
    sample_count++;

    if (stable_time >= LEVEL_TRIM_REQUIRED_TIME_S && sample_count > 0U)
    {
        roll_trim = roll_sum / (float)sample_count;
        pitch_trim = pitch_sum / (float)sample_count;
        level_trim_ready = 1U;
        debug_printf("Level trim ready: roll=%.3f pitch=%.3f samples=%lu\r\n", roll_trim, pitch_trim,
                     (unsigned long)sample_count);
    }
}

void Parse_PC_Command(const char *cmd_str)
{
    if (strncmp(cmd_str, "STOP", 4) == 0)
    {
        flight_state = IDLE;
        remote_data.thr = 0;
        raw_motor_pwm[0] = 1000;
        raw_motor_pwm[1] = 1000;
        raw_motor_pwm[2] = 1000;
        raw_motor_pwm[3] = 1000;
        PID_Reset(&pitch_pid);
        PID_Reset(&roll_pid);
        PID_Reset(&yaw_pid);
        PID_Reset(&gyro_y_pid);
        PID_Reset(&gyro_x_pid);
        PID_Reset(&gyro_z_pid);
        is_armed = 0;
        debug_printf("!!! EMERGENCY STOP !!!\r\n");
        return;
    }
    if (strncmp(cmd_str, "MAGINFO", 7) == 0)
    {
        MagCalibration_PrintInfo();
        return;
    }
    if (strncmp(cmd_str, "MAGSTOP", 7) == 0)
    {
        MagCalibration_Abort();
        debug_printf("MAG calibration aborted\r\n");
        return;
    }
    if (strncmp(cmd_str, "MAGCAL", 6) == 0)
    {
        if (remote_data.shutdown != 1U || remote_data.thr >= 50)
        {
            debug_printf("MAGCAL rejected: lock CH6 and lower throttle\r\n");
            return;
        }

        if (flight_state != IDLE)
        {
            debug_printf("MAGCAL rejected: flight state is not IDLE\r\n");
            return;
        }

        if (MagCalibration_RequestStart(MAG_CAL_ENTRY_SERIAL))
        {
            debug_printf("MAG calibration requested by serial\r\n");
        }
        else
        {
            debug_printf("MAG calibration request rejected, state=%d\r\n", MagCalibration_GetState());
        }
        return;
    }
    if (strncmp(cmd_str, "ARM", 3) == 0)
    {
        if (remote_data.thr == 0)
        {
            is_armed = 1;
            debug_printf(">>> ARMED! CAUTION! <<<\r\n");
        }
        else
        {
            debug_printf("Arm Failed: Throttle not zero!\r\n");
        }
        return;
    }

    if (cmd_str[0] == 'T')
    {
        int thr = 0;
        if (sscanf(cmd_str, "T,%d", &thr) == 1)
        {
            if (thr > 700) thr = 700;
            if (thr < 0) thr = 0;
            remote_data.thr = thr;
            if (thr > 0 && is_armed)
            {
                flight_state = NORMAL;
            }
            else
            {
                flight_state = IDLE;
                if (thr > 0) debug_printf("WARNING: Not Armed!\r\n");
            }
            debug_printf("Set Throttle: %d\r\n", thr);
        }
    }

    else if (cmd_str[0] == 'A')
    {
        float r, p, y;
        if (sscanf(cmd_str, "A,%f,%f,%f", &r, &p, &y) == 3)
        {
            remote_data.rol = (int16_t)(r * 50.0f) + 500;
            remote_data.pit = (int16_t)(p * 50.0f) + 500;
            remote_data.yaw = (int16_t)(y * 50.0f) + 500;

            debug_printf("Set Angle: R=%.1f P=%.1f Y=%.1f\r\n", r, p, y);
        }
    }

    else if (cmd_str[0] == 'P')
    {
        int id, type;
        float kp, ki, kd;
        if (sscanf(cmd_str, "P,%d,%d,%f,%f,%f", &type, &id, &kp, &ki, &kd) == 5)
        {
            if (type == 0)
            {
                if (id == 1)
                {
                    gyro_x_pid.kp = kp;
                    gyro_x_pid.ki = ki;
                    gyro_x_pid.kd = kd;
                    PID_Reset(&gyro_x_pid);
                    debug_printf("GyroX PID updated: %.3f, %.3f, %.3f\r\n", kp, ki, kd);
                }
                else if (id == 2)
                {
                    roll_pid.kp = kp;
                    roll_pid.ki = ki;
                    roll_pid.kd = kd;
                    PID_Reset(&roll_pid);
                    debug_printf("AngleX PID updated: %.3f, %.3f, %.3f\r\n", kp, ki, kd);
                }
            }
            else if (type == 1)
            {
                if (id == 1)
                {
                    gyro_y_pid.kp = kp;
                    gyro_y_pid.ki = ki;
                    gyro_y_pid.kd = kd;
                    PID_Reset(&gyro_y_pid);
                    debug_printf("GyroY PID updated: %.3f, %.3f, %.3f\r\n", kp, ki, kd);
                }
                else if (id == 2)
                {
                    pitch_pid.kp = kp;
                    pitch_pid.ki = ki;
                    pitch_pid.kd = kd;
                    PID_Reset(&pitch_pid);
                    debug_printf("AngleY PID updated: %.3f, %.3f, %.3f\r\n", kp, ki, kd);
                }
            }
            else if (type == 2)
            {
                if (id == 1)
                {
                    gyro_z_pid.kp = kp;
                    gyro_z_pid.ki = ki;
                    gyro_z_pid.kd = kd;
                    PID_Reset(&gyro_z_pid);
                    debug_printf("GyroZ PID updated: %.3f, %.3f, %.3f\r\n", kp, ki, kd);
                }
                else if (id == 2)
                {
                    yaw_pid.kp = kp;
                    yaw_pid.ki = ki;
                    yaw_pid.kd = kd;
                    PID_Reset(&yaw_pid);
                    debug_printf("AngleZ PID updated: %.3f, %.3f, %.3f\r\n", kp, ki, kd);
                }
            }
        }
    }

    else if (strncmp(cmd_str, "CALIB", 5) == 0)
    {
        flight_state = CALIBRATION;
        remote_data.thr = 0;
        PID_Reset(&pitch_pid);
        PID_Reset(&roll_pid);
        PID_Reset(&yaw_pid);
        PID_Reset(&gyro_y_pid);
        PID_Reset(&gyro_x_pid);
        PID_Reset(&gyro_z_pid);
        debug_printf("=== ESC CALIBRATION MODE ===\r\n");
        debug_printf("Outputting 2000us, please power on ESCs now...\r\n");
        debug_printf("After beeps, send M,1000 to set min throttle.\r\n");
        return;
    }

    else if (cmd_str[0] == 'M')
    {
        int motor_id = 0;
        int pwm_val = 1000;
        if (sscanf(cmd_str, "M,%d,%d", &motor_id, &pwm_val) == 2)
        {
            if (pwm_val >= 1000 && pwm_val <= 2000)
            {
                flight_state = RAW_PWM;

                if (motor_id == 0)
                {
                    raw_motor_pwm[0] = pwm_val;
                    raw_motor_pwm[1] = pwm_val;
                    raw_motor_pwm[2] = pwm_val;
                    raw_motor_pwm[3] = pwm_val;
                    debug_printf("Test ALL Motors at %d us\r\n", pwm_val);
                }

                else
                {
                    raw_motor_pwm[0] = (motor_id == 1) ? pwm_val : 1000;
                    raw_motor_pwm[1] = (motor_id == 2) ? pwm_val : 1000;
                    raw_motor_pwm[2] = (motor_id == 3) ? pwm_val : 1000;
                    raw_motor_pwm[3] = (motor_id == 4) ? pwm_val : 1000;
                    debug_printf("Test Motor %d at %d us\r\n", motor_id, pwm_val);
                }

                debug_printf("Test Motor %d at %d us\r\n", motor_id, pwm_val);
            }
        }
    }
}
BaseType_t Buzzer_Post(BuzzerEvent event)
{
    if (BuzzerTask_Queue_Handle == NULL || event <= BUZZER_EVENT_NONE || event >= BUZZER_EVENT_COUNT)
    {
        return pdFALSE;
    }
    return xQueueSendToBack(BuzzerTask_Queue_Handle, &event, 0);
}
BaseType_t Buzzer_PostFromISR(BuzzerEvent event, BaseType_t *higher_priority_task_woken)
{
    if (BuzzerTask_Queue_Handle == NULL || event <= BUZZER_EVENT_NONE || event >= BUZZER_EVENT_COUNT)
    {
        return pdFALSE;
    }
    return xQueueSendToBackFromISR(BuzzerTask_Queue_Handle, &event, higher_priority_task_woken);
}
void App_FreeRTOS_Init(void)
{
    remote_data.thr = 0;
    remote_data.rol = 500;
    remote_data.pit = 500;
    remote_data.yaw = 500;
    FusionBiasInitialise(&offset);
    FusionBiasSettings bias_settings = {
        .sampleRate = 1000,
        .stationaryPeriod = 3.0f,
        .stationaryThreshold = 2.0f,
    };
    FusionBiasSetSettings(&offset, &bias_settings);

    FusionAhrsInitialise(&ahrs);
    FusionAhrsSettings ahrs_settings = {
        .convention = FusionConventionNwu,
        .gain = 0.5f,
        .gyroscopeRange = 2000.0f,
        .accelerationRejection = 10.0f,
        .magneticRejection = 10.0f,
        .recoveryTriggerPeriod = 5 * 1000,
    };
    FusionAhrsSetSettings(&ahrs, &ahrs_settings);

    spi_tx_buf[0] = ICM42688_SPI_READ_CMD(ICM42688_ACC_XH);
    for (int i = 1; i < 15; i++)
    {
        spi_tx_buf[i] = 0xFF;
    }
    Kalman_Init(&kalman);

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    BuzzerTask_Queue_Handle = xQueueCreate(BUZZER_QUEUE_LENGTH, sizeof(BuzzerEvent));
    if (BuzzerTask_Queue_Handle == NULL)
    {
        Error_Handler();
    }
    xTaskCreate(FlightControl_Task, "FlightCtrl", FLIGHT_TASK_STACK_SIZE, NULL, FLIGHT_TASK_PRIORITY,
                &FlightControlTaskHandle);
    xTaskCreate(BaroTask, "BaroTask", BARO_TASK_STACK_SIZE, NULL, BARO_TASK_PRIORITY, &BaroTaskHandle);
    xTaskCreate(AI_Comm_Task, "AI_Comm", AICOMM_TASK_STACK_SIZE, NULL, AICOMM_TASK_PRIORITY, &AI_CommTaskHandle);
    if (xTaskCreate(BuzzerTask, "SerialPrompt", BUZZER_TASK_STACK_SIZE, NULL, BUZZER_TASK_PRIORITY,
                    &BuzzerTaskHandle) != pdPASS)
    {
        Error_Handler();
    }
    vTaskStartScheduler();
}

void FlightControl_Task(void *args)
{
    debug_printf("=== Flight task started ===\r\n");
    uint16_t imu_reconnect_count = 0;

    debug_printf("Initializing ICM42688P...\r\n");
    if (ICM42688P_Init() != 0)
    {
        debug_printf("ICM42688P init failed! Halting task.\r\n");
    }
    debug_printf("ICM42688P init OK, entering main loop.\r\n");

    uint32_t ulNotifiedValue;
    uint32_t last_cyccnt = DWT->CYCCNT;
    float dt;
    float acc_sens = 16.0f / 32768.0f;
    float gyro_sens = 2000.0f / 32768.0f;
    while (1)
    {
        static uint8_t timeout_count = 0;
        ulNotifiedValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (ulNotifiedValue == 0)
        {
            if (timeout_count < 2)
            {
                timeout_count++;
            }
            else
            {
                flight_state = FAIL;
                reconnect = 0;
                imu_reconnect_count = 0;
                remote_data.thr = 0;

                PID_Reset(&gyro_x_pid);
                PID_Reset(&gyro_y_pid);
                PID_Reset(&gyro_z_pid);
                PID_Reset(&roll_pid);
                PID_Reset(&pitch_pid);
                PID_Reset(&yaw_pid);

                Int_motor_set_pwm(&lf_motor, STOP_CCR);
                Int_motor_set_pwm(&rf_motor, STOP_CCR);
                Int_motor_set_pwm(&lb_motor, STOP_CCR);
                Int_motor_set_pwm(&rb_motor, STOP_CCR);
            }
            last_cyccnt = DWT->CYCCNT;
            continue;
        }
        if (ulNotifiedValue > 0)
        {
            timeout_count = 0;
            if (flight_state == FAIL)
            {
                if (imu_reconnect_count < 20)
                {
                    imu_reconnect_count++;
                }

                reconnect = (imu_reconnect_count >= 20);
            }
            else
            {
                imu_reconnect_count = 0;
                reconnect = 0;
            }
            uint32_t now_cyccnt = DWT->CYCCNT;
            dt = (float)(now_cyccnt - last_cyccnt) / SystemCoreClock;
            last_cyccnt = now_cyccnt;

            float raw_ACC_X = (int16_t)(spi_rx_buf[1] << 8 | spi_rx_buf[2]);
            float raw_ACC_Y = (int16_t)(spi_rx_buf[3] << 8 | spi_rx_buf[4]);
            float raw_ACC_Z = (int16_t)(spi_rx_buf[5] << 8 | spi_rx_buf[6]);
            raw_ACC_X -= ACC_OFFSET_X;
            raw_ACC_Y -= ACC_OFFSET_Y;
            raw_ACC_Z -= ACC_OFFSET_Z;
            float raw_GYRO_X = (int16_t)(spi_rx_buf[9] << 8 | spi_rx_buf[10]);
            float raw_GYRO_Y = (int16_t)(spi_rx_buf[11] << 8 | spi_rx_buf[12]);
            float raw_GYRO_Z = (int16_t)(spi_rx_buf[13] << 8 | spi_rx_buf[14]);

            FusionVector acc_data = {
                .axis.x = raw_ACC_X * acc_sens / (float)ACC_SCALE_X,
                .axis.y = raw_ACC_Y * acc_sens / (float)ACC_SCALE_Y,
                .axis.z = raw_ACC_Z * acc_sens / (float)ACC_SCALE_Z,
            };
            FusionVector gyro_data = {
                .axis.x = raw_GYRO_X * gyro_sens,
                .axis.y = raw_GYRO_Y * gyro_sens,
                .axis.z = raw_GYRO_Z * gyro_sens,
            };
            CompassData mag_snapshot;
            uint8_t if_use_magdata;
            taskENTER_CRITICAL();
            mag_snapshot = mag_data;
            if_use_magdata = mag_valid;
            taskEXIT_CRITICAL();
            FusionVector compass_data = {
                .axis.x = mag_snapshot.y,
                .axis.y = mag_snapshot.x,
                .axis.z = -mag_snapshot.z,
            };
            gyro_data = FusionBiasUpdate(&offset, gyro_data);
            if (if_use_magdata && MagCalibration_HasValidData())
            {
                FusionAhrsUpdate(&ahrs, gyro_data, acc_data, compass_data, dt);
            }
            else
            {
                FusionAhrsUpdateNoMagnetometer(&ahrs, gyro_data, acc_data, dt);
            }
            FusionEuler euler_angle = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));
            static float last_roll_gyro = 0.0f;
            static float last_pitch_gyro = 0.0f;
            static float last_yaw_gyro = 0.0f;
            last_roll_gyro = (-gyro_data.axis.x) * 0.50f + last_roll_gyro * 0.50f;
            last_pitch_gyro = gyro_data.axis.y * 0.50f + last_pitch_gyro * 0.50f;
            last_yaw_gyro = (-gyro_data.axis.z) * 0.50f + last_yaw_gyro * 0.50f;
            roll_gyro_extern = last_roll_gyro;
            pitch_gyro_extern = last_pitch_gyro;
            yaw_gyro_extern = last_yaw_gyro;
            roll_deg = -euler_angle.angle.roll;
            pitch_deg = euler_angle.angle.pitch;
            yaw_deg = -euler_angle.angle.yaw;

            App_remote_data_process();

            const FusionAhrsFlags ahrs_flags = FusionAhrsGetFlags(&ahrs);
            const uint8_t bias_learning_started = (offset.timeout > 0U && offset.timer >= offset.timeout) ? 1U : 0U;

            LevelTrim_Update(gyro_data, acc_data, dt, ahrs_flags.startup ? 1U : 0U, bias_learning_started);

            App_flight_pid_process(dt);
            App_flight_fix_height_pid_process(dt);
            App_flight_control_motor(dt);
        }
    }
}
void BaroTask(void *args)
{
    uint8_t mag_fault_report = 0U;

    if (BMP388_Init() != 0)
    {
        debug_printf("BMP388 init failed! Halting task.\r\n");
        vTaskSuspend(NULL);
    }
    if (Compass_Init() != 0)
    {
        debug_printf("Compass init failed! Halting sensor task.\r\n");
        vTaskSuspend(NULL);
    }
    MagCalibration_Init();
    if (!MagCalibration_RequestStart(MAG_CAL_ENTRY_FIRST_BOOT))
    {
        debug_printf("MAG auto calibration start failed\r\n");
        vTaskSuspend(NULL);
    }
    debug_printf("BMP388 and Compass init OK, MAG calibration requested.\r\n");
    uint32_t last_cyccnt = DWT->CYCCNT;
    while (1)
    {
        uint32_t now_cyccnt = DWT->CYCCNT;
        float dt = (float)(now_cyccnt - last_cyccnt) / SystemCoreClock;
        last_cyccnt = now_cyccnt;

        if (BMP388_ReadData(BMP388_DMA_rxbuf) == 0)
        {
            BMP388_Process_Data(BMP388_DMA_rxbuf, &bmp388_data);
            FusionVector acc_data = FusionAhrsGetEarthAcceleration(&ahrs);
            float real_acc_z = acc_data.axis.z * 9.80665f;
            Kalman_GetAltitude(&kalman, bmp388_data.altitude, real_acc_z, dt);
            final_altitude = kalman.altitude;
            final_speed_z = kalman.speed;
        }

        if (Compass_ReadRaw(&raw_mag))
        {
            MagCalibration_Update(&raw_mag, xTaskGetTickCount());
            if (MagCalibration_HasValidData() &&
                Compass_ApplyCalibration(&raw_mag, MagCalibration_GetActiveData(), &calibrated_mag))
            {
                taskENTER_CRITICAL();
                mag_data = calibrated_mag;
                mag_valid = 1U;
                taskEXIT_CRITICAL();
            }
            else
            {
                mag_valid = 0U;
            }
            mag_fault_report = 0U;
        }
        else
        {
            mag_valid = 0U;
            if (!mag_fault_report)
            {
                mag_fault_report = 1U;
                Buzzer_Post(BUZZER_EVENT_MAG_DISCONNECTED);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void AI_Comm_Task(void *args)
{
    uint8_t received_byte;

    (void)args;
    AI_ResetParser();
    AI_InvalidateCommand();
    if (AI_UART_StartReceive() != HAL_OK)
    {
        ai_uart_error = 1U;
    }

    while (1)
    {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10U));

        if (ai_uart_error || ai_uart_overflow)
        {
            taskENTER_CRITICAL();
            ai_uart_ring_tail = ai_uart_ring_head;
            ai_uart_error = 0U;
            ai_uart_overflow = 0U;
            taskEXIT_CRITICAL();

            AI_ResetParser();
            AI_InvalidateCommand();
            ai_link_active = 0U;
            (void)HAL_UART_AbortReceive(&huart3);
            if (AI_UART_StartReceive() != HAL_OK)
            {
                ai_uart_error = 1U;
            }
        }

        while (AI_UART_PopByte(&received_byte))
        {
            (void)Analyse_data_from_ai(received_byte);
        }

        TickType_t now = xTaskGetTickCount();
        bool remote_locked = (remote_data.shutdown == 1U);
        bool throttle_low = (remote_data.thr < 50);
        bool remote_online = (HAL_GetTick() - ppm_last_sync_time) <= 80U;
        MagCalibration_Service(now, remote_online, remote_locked, throttle_low);
        static Flight_State previous_flight_state = IDLE;
        if (flight_state != previous_flight_state)
        {
            if (flight_state == FAIL)
            {
                Buzzer_Post(BUZZER_EVENT_IMU_FAIL);
            }
            else if (flight_state == NORMAL || flight_state == FIX_HEIGHT || flight_state == AI_TRACKING)
            {
                Buzzer_Post(BUZZER_EVENT_ARMED);
            }
            else if (flight_state == IDLE)
            {
                Buzzer_Post(BUZZER_EVENT_LOCKED);
            }
            previous_flight_state = flight_state;
        }
        if (usb_rx_flag)
        {
            Parse_PC_Command((const char *)usb_rx_suffer);
            usb_rx_flag = 0;
        }
    }
}

static const char *Buzzer_GetSerialMessage(BuzzerEvent event)
{
    switch (event)
    {
    case BUZZER_EVENT_BOOT_OK:
        return "[MAG] READY: calibration valid; keep level and still until LEVEL READY";
    case BUZZER_EVENT_ARMED:
        return "[FLIGHT] ARMED";
    case BUZZER_EVENT_LOCKED:
        return "[FLIGHT] LOCKED";
    case BUZZER_EVENT_MAG_NEED_CAL:
        return "[MAG] CAL REQUIRED: keep locked and throttle low; start in about 4 s";
    case BUZZER_EVENT_MAG_CAL_START:
        return "[MAG] CAL START: slowly rotate the whole aircraft through all 3 axes";
    case BUZZER_EVENT_MAG_CAL_PROGRESS:
        return "[MAG] CAL IN PROGRESS: keep rotating slowly";
    case BUZZER_EVENT_MAG_CAL_VALIDATE:
        return "[MAG] CAL VALIDATING: keep rotating for about 10 s";
    case BUZZER_EVENT_MAG_CAL_SUCCESS:
        return "[MAG] CAL SUCCESS: place aircraft level and keep completely still";
    case BUZZER_EVENT_MAG_CAL_FAILED:
        return "[MAG] CAL FAILED: remove magnetic interference and retry";
    case BUZZER_EVENT_MAG_CAL_ABORTED:
        return "[MAG] CAL ABORTED";
    case BUZZER_EVENT_MAG_DISCONNECTED:
        return "[MAG] READ FAILED: check HMC5883L power and I2C wiring";
    case BUZZER_EVENT_MAG_INTERFERENCE:
        return "[MAG] INTERFERENCE DETECTED";
    case BUZZER_EVENT_PPM_LOST:
        return "[RC] PPM LOST: flight output locked";
    case BUZZER_EVENT_IMU_FAIL:
        return "[IMU] FAILURE: flight output locked";
    case BUZZER_EVENT_EMERGENCY:
        return "[FLIGHT] EMERGENCY STOP";
    case BUZZER_EVENT_STOP:
        return "[NOTICE] pending alert stopped";
    default:
        return NULL;
    }
}

void BuzzerTask(void *args)
{
    BuzzerEvent received;
    (void)args;

    while (1)
    {
        if (xQueueReceive(BuzzerTask_Queue_Handle, &received, portMAX_DELAY) == pdPASS)
        {
            const char *message = Buzzer_GetSerialMessage(received);
            if (message != NULL)
            {
                vTaskDelay(pdMS_TO_TICKS(10U));
                usb_printf("%s\r\n", message);
                vTaskDelay(pdMS_TO_TICKS(20U));
            }
        }
    }
}
