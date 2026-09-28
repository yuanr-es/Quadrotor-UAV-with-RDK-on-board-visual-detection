#ifndef __MOTOR_H
#define __MOTOR_H
#include "main.h"
#include "Com_debug.h"
#define STOP_CCR 1000
#define HALF_CCR 1500
#define ALL_CCR 2000
#define MIN_VALID_CCR 1000
#define MAX_VALID_CCR 2000
#define IDLE_CCR 1080
#define BALANCE_CCR 1080
#define EMERGENCY_LANDING_THR 200
extern TIM_HandleTypeDef htim2;
typedef struct
{
    TIM_HandleTypeDef *tim;
    uint16_t channel;
    uint16_t current_pwm;
    uint16_t idle_pwm;
} Motor_Struct;

void Int_motor_init(Motor_Struct *motor);

void Int_motor_set_pwm(Motor_Struct *motor, uint16_t pwm_us);

void Int_motor_set_throttle(Motor_Struct *motor, uint16_t throttle_permil);

void Int_motor_ESC_Calibration(void);
#endif
