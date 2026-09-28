#ifndef __APP_FLIGHT_H
#define __APP_FLIGHT_H
#include "stm32f4xx_hal.h"
#include "PID.h"
#include "App_freeRTOS.h"
#include "Motor.h"
#define PPM_MIN_PULSE 900
#define PPM_MAX_PULSE 2100
#define PPM_SYNC_PULSE 3000
#define PPM_CHANNELS 6
extern volatile uint16_t ppm_values[PPM_CHANNELS];
extern volatile uint16_t ppm_frame_buf[PPM_CHANNELS];
extern volatile uint8_t ppm_current_ch;
extern volatile uint8_t ppm_updated_flag;
extern volatile uint32_t ppm_last_sync_time;
extern PID_parameterStruct pitch_pid;
extern PID_parameterStruct gyro_y_pid;
extern PID_parameterStruct roll_pid;
extern PID_parameterStruct gyro_x_pid;
extern PID_parameterStruct yaw_pid;
extern PID_parameterStruct gyro_z_pid;
extern PID_parameterStruct height_pid;
extern PID_parameterStruct vertical_speed_pid;
extern Motor_Struct lf_motor;
extern Motor_Struct rf_motor;
extern Motor_Struct lb_motor;
extern Motor_Struct rb_motor;

void App_flight_init(void);
void App_flight_pid_process(float dt);
void App_flight_control_motor(float dt);
void App_flight_fix_height_pid_process(float dt);
void App_remote_data_process(void);
#endif
