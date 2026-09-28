

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx_hal.h"

    void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

    void Error_Handler(void);

#define BUZZER_Pin GPIO_PIN_13
#define BUZZER_GPIO_Port GPIOC

#ifdef __cplusplus
}
#endif

#endif
