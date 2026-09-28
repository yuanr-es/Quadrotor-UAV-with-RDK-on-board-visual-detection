#ifndef __BUZZER_H
#define __BUZZER_H

#include "FreeRTOS.h"
#include "queue.h"
#include "stm32f4xx_hal.h"

typedef enum
{
    BUZZER_EVENT_NONE = 0,
    BUZZER_EVENT_BOOT_OK,
    BUZZER_EVENT_ARMED,
    BUZZER_EVENT_LOCKED,
    BUZZER_EVENT_MAG_NEED_CAL,
    BUZZER_EVENT_MAG_CAL_START,
    BUZZER_EVENT_MAG_CAL_PROGRESS,
    BUZZER_EVENT_MAG_CAL_VALIDATE,
    BUZZER_EVENT_MAG_CAL_SUCCESS,
    BUZZER_EVENT_MAG_CAL_FAILED,
    BUZZER_EVENT_MAG_CAL_ABORTED,
    BUZZER_EVENT_MAG_DISCONNECTED,
    BUZZER_EVENT_MAG_INTERFERENCE,
    BUZZER_EVENT_PPM_LOST,
    BUZZER_EVENT_IMU_FAIL,
    BUZZER_EVENT_EMERGENCY,
    BUZZER_EVENT_STOP,
    BUZZER_EVENT_COUNT
} BuzzerEvent;

typedef struct
{
    uint16_t on_time_ms;
    uint16_t off_time_ms;
    uint8_t repeat_count;
    uint8_t priority;
} Buzzer_Pattern;

extern const Buzzer_Pattern buzzer_patterns[BUZZER_EVENT_COUNT];

void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Init(void);
BaseType_t Buzzer_Post(BuzzerEvent event);
BaseType_t Buzzer_PostFromISR(BuzzerEvent event, BaseType_t *higher_priority_task_woken);

#endif
