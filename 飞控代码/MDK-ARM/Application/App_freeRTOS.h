#ifndef __APP_FREERTOS_H
#define __APP_FREERTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include "Com_debug.h"
#include "Compass.h"
#include "Buzzer.h"
typedef struct
{
    float roll_deg;
    float pitch_deg;
    float yaw_rate_dps;
    float throttle;

    TickType_t received_tick;
    uint8_t valid;
} AiControlCommand;
typedef enum
{
    CONTROL_MODE_NORMAL = 0,
    CONTROL_MODE_FIX_HEIGHT,
    CONTROL_MODE_AI
} Control_Mode;
typedef struct
{
    int16_t thr;
    int16_t yaw;
    int16_t pit;
    int16_t rol;
    uint8_t shutdown;
    Control_Mode control_mode;
    float height;
} Remote_Data;
typedef enum
{
    IDLE = 0,
    NORMAL,
    FIX_HEIGHT,
    AI_TRACKING,
    FAIL,
    CALIBRATION,
    FAILSAFE_LANDING,
    RAW_PWM,
} Flight_State;

extern TaskHandle_t FlightControlTaskHandle;
extern TaskHandle_t BaroTaskHandle;
extern uint8_t spi_tx_buf[15];
extern uint8_t spi_rx_buf[15];
extern uint8_t BMP388_DMA_rxbuf[6];

extern float roll_deg;
extern float pitch_deg;
extern float yaw_deg;
extern float roll_gyro_extern;
extern float pitch_gyro_extern;
extern float yaw_gyro_extern;
extern float final_altitude;
extern float final_speed_z;
extern float roll_trim;
extern float pitch_trim;
extern volatile uint8_t level_trim_ready;
extern volatile uint8_t baro_sample_ready;
extern volatile uint8_t mag_sample_ready;
extern volatile uint8_t mag_valid;
extern CompassData mag_data;
extern CompassRawData raw_mag;

extern Remote_Data remote_data;
extern volatile Flight_State flight_state;
extern volatile uint8_t reconnect;
extern volatile uint8_t ai_link_active;
extern volatile uint16_t raw_motor_pwm[4];
extern volatile TickType_t baro_last_update_tick;
extern volatile uint32_t baro_update_sequence;
extern volatile uint8_t baro_valid;
void App_FreeRTOS_Init(void);
#endif
