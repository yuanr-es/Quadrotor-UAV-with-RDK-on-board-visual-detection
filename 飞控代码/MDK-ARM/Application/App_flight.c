#include "App_flight.h"
#include "Motor.h"
#include "Buzzer.h"
#include "MagCalibration.h"
#include "AI_Analysis.h"
#include <math.h>
#include "App_freeRTOS.h"
#define YAW_MAX_RATE_DPS 120.0f
#define YAW_HOLD_MAX_RATE_DPS 80.0f
#define YAW_STICK_DEADBAND 0.08f
#define ATTITUDE_I_ENABLE_THR 450
#define ATTITUDE_PID_OUTPUT_LIMIT 500.0f
#define ATTITUDE_I_OUTPUT_LIMIT 10.0f
#define CH5_NORMAL_MAX_US 1300U
#define CH5_AI_MIN_US 1700U
#define AI_ROLL_PITCH_SCALE 50.0f
#define AI_YAW_RATE_SCALE (500.0f / AI_YAW_RATE_LIMIT_DPS)
#define ASCENT_GUARD_ENABLE 0U
#define ASCENT_GUARD_ENTER_SPEED_MPS 2.5f
#define ASCENT_GUARD_EXIT_SPEED_MPS 2.0f
#define ASCENT_GUARD_GAIN_PWM_PER_MPS 50.0f
#define ASCENT_GUARD_MAX_REDUCTION_PWM 120.0f
#define ASCENT_GUARD_ATTACK_PWM_PER_SEC 250.0f
#define ASCENT_GUARD_RELEASE_PWM_PER_SEC 100.0f
#define ASCENT_GUARD_MAX_TILT_DEG 45.0f
#define ASCENT_GUARD_BARO_TIMEOUT_MS 120U

#if ASCENT_GUARD_ENABLE
static float ascent_guard_correction = 0.0f;
static uint8_t ascent_guard_latched = 0U;
#endif
static uint8_t motor_output_saturated = 0U;
static float LimitFloat(float input, float min_value, float max_value);

PID_parameterStruct pitch_pid = {.kp = 4.0, .ki = 0.00, .kd = 0.00};
PID_parameterStruct gyro_y_pid = {.kp = 2.5, .ki = 0.10, .kd = 0.0006};

PID_parameterStruct roll_pid = {.kp = 4.0, .ki = 0.00, .kd = 0.00};
PID_parameterStruct gyro_x_pid = {.kp = 2.5, .ki = 0.10, .kd = 0.0006};

PID_parameterStruct yaw_pid = {.kp = 1.80, .ki = 0.00, .kd = 0.00};
PID_parameterStruct gyro_z_pid = {.kp = 4.5, .ki = 0.00, .kd = 0.00};

PID_parameterStruct height_pid = {.kp = 0.00, .ki = 0.00, .kd = 0.00};
PID_parameterStruct vertical_speed_pid = {.kp = 0.00, .ki = 0.00, .kd = 0.00};

Motor_Struct lf_motor = {.tim = &htim2, .channel = TIM_CHANNEL_1, .current_pwm = 1000, .idle_pwm = 1080};
Motor_Struct rf_motor = {.tim = &htim2, .channel = TIM_CHANNEL_4, .current_pwm = 1000, .idle_pwm = 1080};
Motor_Struct lb_motor = {.tim = &htim2, .channel = TIM_CHANNEL_2, .current_pwm = 1000, .idle_pwm = 1080};
Motor_Struct rb_motor = {.tim = &htim2, .channel = TIM_CHANNEL_3, .current_pwm = 1000, .idle_pwm = 1080};
volatile uint16_t ppm_values[PPM_CHANNELS];
volatile uint16_t ppm_frame_buf[PPM_CHANNELS];
volatile uint8_t ppm_current_ch = 0;
volatile uint8_t ppm_updated_flag = 0;
volatile uint32_t ppm_last_sync_time = 0;
static float yaw_target = 0.0f;

static uint8_t yaw_target_valid = 0;
static int16_t height_throttle_correction;
#if ASCENT_GUARD_ENABLE
static float MoveToward(float current, float target, float max_step)
{
    if (target > (current + max_step))
    {
        return current + max_step;
    }
    if (target < (current - max_step))
    {
        return current - max_step;
    }
    return target;
}
#endif
static float AscentGuard_Update(float dt)
{
#if ASCENT_GUARD_ENABLE
    TickType_t last_update;
    TickType_t now;
    float vertical_speed;
    uint8_t valid;
    uint8_t protection_allowed;
    float target_correction = 0.0f;
    float slew_rate;
    taskENTER_CRITICAL();
    vertical_speed = final_speed_z;
    last_update = baro_last_update_tick;
    valid = baro_valid;
    taskEXIT_CRITICAL();
    now = xTaskGetTickCount();
    protection_allowed = valid && (now - last_update) < pdMS_TO_TICKS(ASCENT_GUARD_BARO_TIMEOUT_MS) &&
                         isfinite(vertical_speed) && (remote_data.shutdown == 0) && (remote_data.thr >= 50) &&
                         (flight_state == NORMAL || flight_state == FIX_HEIGHT || flight_state == AI_TRACKING) &&
                         (fabsf(roll_deg) < ASCENT_GUARD_MAX_TILT_DEG) &&
                         (fabsf(pitch_deg) < ASCENT_GUARD_MAX_TILT_DEG) && !MagCalibration_IsFlightInhibited();
    if (!protection_allowed)
    {
        ascent_guard_correction = 0.0f;
        ascent_guard_latched = 0;
        return 0.0f;
    }
    if (dt < 0.0005f)
    {
        dt = 0.0005f;
    }
    if (dt > 0.03f)
    {
        dt = 0.03f;
    }
    if (!ascent_guard_latched)
    {
        if (vertical_speed > ASCENT_GUARD_ENTER_SPEED_MPS || vertical_speed < -ASCENT_GUARD_ENTER_SPEED_MPS)
        {
            ascent_guard_latched = 1;
        }
    }
    else
    {
        if (vertical_speed < ASCENT_GUARD_EXIT_SPEED_MPS && vertical_speed > -ASCENT_GUARD_EXIT_SPEED_MPS)
        {
            ascent_guard_latched = 0;
        }
    }
    if (ascent_guard_latched)
    {
        if (vertical_speed > ASCENT_GUARD_EXIT_SPEED_MPS)
        {
            target_correction = -(vertical_speed - ASCENT_GUARD_EXIT_SPEED_MPS) * ASCENT_GUARD_GAIN_PWM_PER_MPS;
            target_correction =
                LimitFloat(target_correction, -ASCENT_GUARD_MAX_REDUCTION_PWM, ASCENT_GUARD_MAX_REDUCTION_PWM);
        }
        else
        {
            target_correction = -(ASCENT_GUARD_EXIT_SPEED_MPS + vertical_speed) * ASCENT_GUARD_GAIN_PWM_PER_MPS;
            target_correction =
                LimitFloat(target_correction, -ASCENT_GUARD_MAX_REDUCTION_PWM, ASCENT_GUARD_MAX_REDUCTION_PWM);
        }
    }
    slew_rate = (fabsf(target_correction) > fabsf(ascent_guard_correction)) ? ASCENT_GUARD_ATTACK_PWM_PER_SEC
                                                                            : ASCENT_GUARD_RELEASE_PWM_PER_SEC;
    ascent_guard_correction = MoveToward(ascent_guard_correction, target_correction, slew_rate * dt);

    return ascent_guard_correction;
#else
    (void)dt;
    return 0.0f;
#endif
}
static float LimitFloat(float input, float min_value, float max_value)
{
    if (input > max_value) return max_value;
    if (input < min_value) return min_value;
    return input;
}
static float Wrap180(float angle)
{
    while (angle > 180.0f)
        angle -= 360.0f;
    while (angle < -180.0f)
        angle += 360.0f;
    return angle;
}
static float GetYawRateCommend(void)
{
    float stick = (remote_data.yaw - 500.0f) / 500.0f;
    if (fabsf(stick) < YAW_STICK_DEADBAND) return 0.0f;
    if (stick > 0)
    {
        stick = (stick - YAW_STICK_DEADBAND) / (1.0f - YAW_STICK_DEADBAND);
    }
    else
    {
        stick = (stick + YAW_STICK_DEADBAND) / (1.0f - YAW_STICK_DEADBAND);
    }
    return stick * YAW_MAX_RATE_DPS;
}
void App_flight_init(void)
{
    Int_motor_init(&lf_motor);
    Int_motor_init(&rf_motor);
    Int_motor_init(&lb_motor);
    Int_motor_init(&rb_motor);
}
int16_t Com_limit(int16_t speed, int16_t max_speed, int16_t min_speed)
{
    if (speed > max_speed)
    {
        return max_speed;
    }
    else if (speed < min_speed)
    {
        return min_speed;
    }
    return speed;
}
void App_remote_data_process(void)
{
    uint8_t has_frame = 0;
    uint16_t ppm_snapshot[PPM_CHANNELS];
    AiControlCommand ai_command;
    static int16_t failsafe_thr = 0;
    static int16_t rc_throttle = 0;
    static uint8_t ppm_lost_report = 0;
    static uint8_t ai_fault_latched = 0U;
    if (flight_state != FAILSAFE_LANDING)
    {
        failsafe_thr = remote_data.thr;
    }
    if ((HAL_GetTick() - ppm_last_sync_time) > 80)
    {
        remote_data.rol = 500;
        remote_data.pit = 500;
        remote_data.yaw = 500;
        static uint8_t ct = 0;
        if (++ct >= 20)
        {
            ct = 0;
            if (failsafe_thr > EMERGENCY_LANDING_THR)
            {
                failsafe_thr--;
            }
            else if (failsafe_thr < EMERGENCY_LANDING_THR)
            {
                failsafe_thr++;
            }
        }
        remote_data.thr = 0;
        remote_data.control_mode = CONTROL_MODE_NORMAL;
        remote_data.shutdown = 1;
        ai_link_active = 0U;
        ai_fault_latched = 1U;

        flight_state = IDLE;

        if (!ppm_lost_report)
        {
            ppm_lost_report = 1;
            Buzzer_Post(BUZZER_EVENT_PPM_LOST);
        }
        return;
    }
    else
    {
        ppm_lost_report = 0;
    }
    taskENTER_CRITICAL();
    if (ppm_updated_flag == 1)
    {
        ppm_updated_flag = 0;
        for (int i = 0; i < PPM_CHANNELS; i++)
        {
            ppm_snapshot[i] = ppm_frame_buf[i];
        }
        has_frame = 1;
    }
    taskEXIT_CRITICAL();
    if (has_frame)
    {
        rc_throttle = Com_limit((int16_t)(ppm_snapshot[2] - 1000), 1000, 0);
        if (ppm_snapshot[4] < CH5_NORMAL_MAX_US)
        {
            remote_data.control_mode = CONTROL_MODE_NORMAL;
        }
        else if (ppm_snapshot[4] < CH5_AI_MIN_US)
        {
            remote_data.control_mode = CONTROL_MODE_FIX_HEIGHT;
        }
        else
        {
            remote_data.control_mode = CONTROL_MODE_AI;
        }
        remote_data.shutdown = (ppm_snapshot[5] > 1700);
    }

    remote_data.pit = 500;
    remote_data.rol = 500;
    remote_data.yaw = 500;
    remote_data.thr = rc_throttle;

    if (remote_data.control_mode != CONTROL_MODE_AI)
    {
        ai_fault_latched = 0U;
        ai_link_active = 0U;
    }
    else if (!ai_fault_latched && AI_GetLatestCommand(&ai_command, pdMS_TO_TICKS(AI_COMMAND_TIMEOUT_MS)))
    {
        remote_data.rol = Com_limit((int16_t)lroundf(500.0f + ai_command.roll_deg * AI_ROLL_PITCH_SCALE), 1000, 0);
        remote_data.pit = Com_limit((int16_t)lroundf(500.0f + ai_command.pitch_deg * AI_ROLL_PITCH_SCALE), 1000, 0);
        remote_data.yaw = Com_limit((int16_t)lroundf(500.0f + ai_command.yaw_rate_dps * AI_YAW_RATE_SCALE), 1000, 0);
        remote_data.thr = Com_limit((int16_t)lroundf(ai_command.throttle), (int16_t)AI_THROTTLE_LIMIT, 0);
        ai_link_active = 1U;
    }
    else
    {
        ai_link_active = 0U;
        ai_fault_latched = 1U;
    }
}
void App_flight_pid_process(float dt)
{
    float yaw_processed_ratecommend = GetYawRateCommend();
    bool flight_active = flight_state == NORMAL || flight_state == FIX_HEIGHT || flight_state == AI_TRACKING;
    uint8_t rate_integral_enabled =
        flight_active && remote_data.thr >= ATTITUDE_I_ENABLE_THR && !motor_output_saturated;
    bool mag_aviliable = mag_valid && MagCalibration_HasValidData();
    if (!flight_active || remote_data.thr < ATTITUDE_I_ENABLE_THR)
    {
        PID_ResetIntegral(&gyro_x_pid);
        PID_ResetIntegral(&gyro_y_pid);
        PID_ResetIntegral(&gyro_z_pid);
    }

    pitch_pid.desire = pitch_trim;
    if (flight_state == AI_TRACKING)
    {
        pitch_pid.desire += (remote_data.pit - 500.0f) / AI_ROLL_PITCH_SCALE;
    }
    pitch_pid.measure = pitch_deg;
    gyro_y_pid.measure = pitch_gyro_extern;
    PID_mono(&pitch_pid, dt);
    gyro_y_pid.desire = pitch_pid.output;
    PID_mono_anti_windup(&gyro_y_pid, dt, rate_integral_enabled, ATTITUDE_PID_OUTPUT_LIMIT, ATTITUDE_I_OUTPUT_LIMIT);

    roll_pid.desire = roll_trim;
    if (flight_state == AI_TRACKING)
    {
        roll_pid.desire += (remote_data.rol - 500.0f) / AI_ROLL_PITCH_SCALE;
    }
    roll_pid.measure = roll_deg;
    gyro_x_pid.measure = roll_gyro_extern;
    PID_mono(&roll_pid, dt);
    gyro_x_pid.desire = roll_pid.output;
    PID_mono_anti_windup(&gyro_x_pid, dt, rate_integral_enabled, ATTITUDE_PID_OUTPUT_LIMIT, ATTITUDE_I_OUTPUT_LIMIT);

    if (!flight_active)
    {
        yaw_target = yaw_deg;
        yaw_target_valid = 0;
        PID_Reset(&gyro_z_pid);
        PID_Reset(&yaw_pid);
    }
    else
    {
        if (!mag_aviliable || fabsf(yaw_processed_ratecommend) > 0.01f)
        {
            gyro_z_pid.desire = yaw_processed_ratecommend;
            gyro_z_pid.measure = yaw_gyro_extern;
            yaw_target = yaw_deg;
            yaw_target_valid = 1;
            PID_Reset(&yaw_pid);
            PID_mono_anti_windup(&gyro_z_pid, dt, rate_integral_enabled, ATTITUDE_PID_OUTPUT_LIMIT,
                                 ATTITUDE_I_OUTPUT_LIMIT);
        }
        else
        {
            if (!yaw_target_valid)
            {
                yaw_target = yaw_deg;
                PID_Reset(&yaw_pid);
                yaw_target_valid = 1;
            }
            else
            {
                float yaw_error = Wrap180(yaw_target - yaw_deg);
                yaw_pid.desire = yaw_target;
                yaw_pid.measure = yaw_target - yaw_error;
                PID_mono(&yaw_pid, dt);
                gyro_z_pid.desire = LimitFloat(yaw_pid.output, -YAW_HOLD_MAX_RATE_DPS, YAW_HOLD_MAX_RATE_DPS);
                gyro_z_pid.measure = yaw_gyro_extern;
                PID_mono_anti_windup(&gyro_z_pid, dt, rate_integral_enabled, ATTITUDE_PID_OUTPUT_LIMIT,
                                     ATTITUDE_I_OUTPUT_LIMIT);
            }
        }
    }
}
void App_flight_control_motor(float dt)
{
    float ascent_guard = AscentGuard_Update(dt);
    Flight_State requested_state = NORMAL;
    Flight_State previous_state = flight_state;
    motor_output_saturated = 0U;

    if (remote_data.control_mode == CONTROL_MODE_FIX_HEIGHT)
    {
        requested_state = FIX_HEIGHT;
    }
    else if (remote_data.control_mode == CONTROL_MODE_AI && ai_link_active)
    {
        requested_state = AI_TRACKING;
    }
    else if (remote_data.control_mode == CONTROL_MODE_AI && flight_state == IDLE)
    {
        requested_state = IDLE;
    }
    if (MagCalibration_IsFlightInhibited())
    {
        flight_state = IDLE;

        Int_motor_set_pwm(&lf_motor, STOP_CCR);
        Int_motor_set_pwm(&rf_motor, STOP_CCR);
        Int_motor_set_pwm(&lb_motor, STOP_CCR);
        Int_motor_set_pwm(&rb_motor, STOP_CCR);

        PID_Reset(&gyro_x_pid);
        PID_Reset(&gyro_y_pid);
        PID_Reset(&gyro_z_pid);
        PID_Reset(&roll_pid);
        PID_Reset(&pitch_pid);
        PID_Reset(&yaw_pid);
        return;
    }
    if (remote_data.shutdown == 1)
    {
        flight_state = IDLE;
    }
    else if (flight_state == FAIL && reconnect == 1)
    {
        flight_state = NORMAL;
        reconnect = 0;
    }

    else if (flight_state == IDLE)
    {
        if (remote_data.thr < 50)
        {
            flight_state = requested_state;
        }
    }
    else if (flight_state == NORMAL || flight_state == FIX_HEIGHT || flight_state == AI_TRACKING)
    {
        flight_state = requested_state;
    }
    if (flight_state == FIX_HEIGHT && previous_state != FIX_HEIGHT)
    {
        remote_data.height = final_altitude;
        PID_Reset(&height_pid);
        PID_Reset(&vertical_speed_pid);
        height_throttle_correction = 0;
    }
    if (!level_trim_ready && (flight_state == NORMAL || flight_state == FIX_HEIGHT || flight_state == AI_TRACKING ||
                              flight_state == FAILSAFE_LANDING))
    {
        flight_state = IDLE;
    }
    if (roll_deg > 85 || roll_deg < -85) flight_state = IDLE;
    if (pitch_deg > 85 || pitch_deg < -85) flight_state = IDLE;
    if (flight_state == IDLE || flight_state == FAIL)
    {
        Int_motor_set_pwm(&lf_motor, STOP_CCR);
        Int_motor_set_pwm(&rf_motor, STOP_CCR);
        Int_motor_set_pwm(&lb_motor, STOP_CCR);
        Int_motor_set_pwm(&rb_motor, STOP_CCR);

        PID_Reset(&gyro_x_pid);
        PID_Reset(&gyro_y_pid);
        PID_Reset(&gyro_z_pid);
        PID_Reset(&roll_pid);
        PID_Reset(&pitch_pid);
        PID_Reset(&yaw_pid);
        return;
    }
    if (flight_state == RAW_PWM)
    {
        Int_motor_set_pwm(&lf_motor, raw_motor_pwm[0]);
        Int_motor_set_pwm(&rf_motor, raw_motor_pwm[1]);
        Int_motor_set_pwm(&lb_motor, raw_motor_pwm[2]);
        Int_motor_set_pwm(&rb_motor, raw_motor_pwm[3]);
        return;
    }

    if (flight_state == CALIBRATION)
    {
        Int_motor_set_pwm(&lf_motor, ALL_CCR);
        Int_motor_set_pwm(&rf_motor, ALL_CCR);
        Int_motor_set_pwm(&lb_motor, ALL_CCR);
        Int_motor_set_pwm(&rb_motor, ALL_CCR);
        return;
    }
    switch (flight_state)
    {
    case FAILSAFE_LANDING:
    {
        int16_t collective_throttle = (int16_t)LimitFloat((float)(remote_data.thr + ascent_guard), 0.0f, 700.0f);

        int16_t out_lf = (int16_t)(collective_throttle + gyro_x_pid.output + gyro_y_pid.output +
                                   Com_limit(gyro_z_pid.output, 150, -150));
        int16_t out_rf = (int16_t)(collective_throttle - gyro_x_pid.output + gyro_y_pid.output -
                                   Com_limit(gyro_z_pid.output, 150, -150));
        int16_t out_lb = (int16_t)(collective_throttle + gyro_x_pid.output - gyro_y_pid.output -
                                   Com_limit(gyro_z_pid.output, 150, -150));
        int16_t out_rb = (int16_t)(collective_throttle - gyro_x_pid.output - gyro_y_pid.output +
                                   Com_limit(gyro_z_pid.output, 150, -150));

        motor_output_saturated = out_lf < 0 || out_lf > 700 || out_rf < 0 || out_rf > 700 || out_lb < 0 ||
                                 out_lb > 700 || out_rb < 0 || out_rb > 700;

        lf_motor.current_pwm = Com_limit(out_lf, 700, 0);
        rf_motor.current_pwm = Com_limit(out_rf, 700, 0);
        lb_motor.current_pwm = Com_limit(out_lb, 700, 0);
        rb_motor.current_pwm = Com_limit(out_rb, 700, 0);
        break;
    }
    case NORMAL:
    case AI_TRACKING:
    {
        int16_t collective_throttle = (int16_t)LimitFloat((float)(remote_data.thr + ascent_guard), 0.0f, 700.0f);

        float roll_out = gyro_x_pid.output;
        float pitch_out = gyro_y_pid.output;
        float yaw_out = Com_limit(gyro_z_pid.output, 150, -150);

        int16_t out_lf = (int16_t)(collective_throttle + roll_out + pitch_out + yaw_out);
        int16_t out_rf = (int16_t)(collective_throttle - roll_out + pitch_out - yaw_out);
        int16_t out_lb = (int16_t)(collective_throttle + roll_out - pitch_out - yaw_out);
        int16_t out_rb = (int16_t)(collective_throttle - roll_out - pitch_out + yaw_out);
        motor_output_saturated = out_lf < 0 || out_lf > 700 || out_rf < 0 || out_rf > 700 || out_lb < 0 ||
                                 out_lb > 700 || out_rb < 0 || out_rb > 700;
        lf_motor.current_pwm = Com_limit(out_lf, 700, 0);
        rf_motor.current_pwm = Com_limit(out_rf, 700, 0);
        lb_motor.current_pwm = Com_limit(out_lb, 700, 0);
        rb_motor.current_pwm = Com_limit(out_rb, 700, 0);
        break;
    }
    case FIX_HEIGHT:
    {
        int16_t collective_throttle =
            (int16_t)LimitFloat((float)(remote_data.thr + height_throttle_correction + ascent_guard), 0.0f, 700.0f);
        int16_t out_lf = (int16_t)(collective_throttle + gyro_x_pid.output + gyro_y_pid.output +
                                   Com_limit(gyro_z_pid.output, 150, -150));
        int16_t out_rf = (int16_t)(collective_throttle - gyro_x_pid.output + gyro_y_pid.output -
                                   Com_limit(gyro_z_pid.output, 150, -150));
        int16_t out_lb = (int16_t)(collective_throttle + gyro_x_pid.output - gyro_y_pid.output -
                                   Com_limit(gyro_z_pid.output, 150, -150));
        int16_t out_rb = (int16_t)(collective_throttle - gyro_x_pid.output - gyro_y_pid.output +
                                   Com_limit(gyro_z_pid.output, 150, -150));
        motor_output_saturated = out_lf < 0 || out_lf > 700 || out_rf < 0 || out_rf > 700 || out_lb < 0 ||
                                 out_lb > 700 || out_rb < 0 || out_rb > 700;
        lf_motor.current_pwm = Com_limit(out_lf, 700, 0);
        rf_motor.current_pwm = Com_limit(out_rf, 700, 0);
        lb_motor.current_pwm = Com_limit(out_lb, 700, 0);
        rb_motor.current_pwm = Com_limit(out_rb, 700, 0);
        break;
    }
    default:
        break;
    }
    lf_motor.current_pwm = Com_limit(lf_motor.current_pwm, 700, 0);
    rf_motor.current_pwm = Com_limit(rf_motor.current_pwm, 700, 0);
    lb_motor.current_pwm = Com_limit(lb_motor.current_pwm, 700, 0);
    rb_motor.current_pwm = Com_limit(rb_motor.current_pwm, 700, 0);

    if (remote_data.thr < 50)
    {
        Int_motor_set_pwm(&lf_motor, STOP_CCR);
        Int_motor_set_pwm(&rf_motor, STOP_CCR);
        Int_motor_set_pwm(&lb_motor, STOP_CCR);
        Int_motor_set_pwm(&rb_motor, STOP_CCR);

        PID_Reset(&gyro_x_pid);
        PID_Reset(&gyro_y_pid);
        PID_Reset(&gyro_z_pid);
        PID_Reset(&roll_pid);
        PID_Reset(&pitch_pid);
        PID_Reset(&yaw_pid);

        return;
    }
    Int_motor_set_pwm(&lf_motor, lf_motor.current_pwm + lf_motor.idle_pwm);
    Int_motor_set_pwm(&rf_motor, rf_motor.current_pwm + rf_motor.idle_pwm);
    Int_motor_set_pwm(&lb_motor, lb_motor.current_pwm + lb_motor.idle_pwm);
    Int_motor_set_pwm(&rb_motor, rb_motor.current_pwm + rb_motor.idle_pwm);
}
void App_flight_fix_height_pid_process(float dt)
{
    if (flight_state != FIX_HEIGHT)
    {
        height_throttle_correction = 0;
        PID_Reset(&height_pid);
        PID_Reset(&vertical_speed_pid);
        return;
    }
    height_pid.desire = remote_data.height;
    height_pid.measure = final_altitude;
    PID_mono(&height_pid, dt);
    vertical_speed_pid.desire = LimitFloat(height_pid.output, -1.0f, 1.0f);
    vertical_speed_pid.measure = final_speed_z;
    PID_mono(&vertical_speed_pid, dt);
    height_throttle_correction = (int16_t)LimitFloat(vertical_speed_pid.output, -150.0f, 150.0f);
}
