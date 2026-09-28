#include "stm32f4xx_hal.h"
#include "Motor.h"

void Int_motor_init(Motor_Struct *motor)
{
    __HAL_TIM_SET_COMPARE(motor->tim, motor->channel, STOP_CCR);
    motor->current_pwm = STOP_CCR;
    HAL_TIM_PWM_Start(motor->tim, motor->channel);
}

void Int_motor_set_pwm(Motor_Struct *motor, uint16_t pwm_us)
{
    if (pwm_us > MAX_VALID_CCR) pwm_us = MAX_VALID_CCR;
    if (pwm_us < MIN_VALID_CCR) pwm_us = MIN_VALID_CCR;

    motor->current_pwm = pwm_us;
    __HAL_TIM_SET_COMPARE(motor->tim, motor->channel, pwm_us);
}

void Int_motor_set_throttle(Motor_Struct *motor, uint16_t throttle_permil)
{
    if (throttle_permil > 1000) throttle_permil = 1000;
    uint16_t target_ccr = IDLE_CCR + (ALL_CCR - IDLE_CCR) * throttle_permil / 1000;
    Int_motor_set_pwm(motor, target_ccr);
}

void Int_motor_ESC_Calibration(void)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ALL_CCR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, ALL_CCR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, ALL_CCR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, ALL_CCR);

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
    HAL_Delay(1000);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, STOP_CCR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, STOP_CCR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, STOP_CCR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, STOP_CCR);
    HAL_Delay(4000);
}
