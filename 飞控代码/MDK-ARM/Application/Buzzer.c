#include "Buzzer.h"
#include "main.h"
#include "stm32f4xx_hal.h"

#define BUZZER_ACTIVE_LEVEL GPIO_PIN_RESET
#define BUZZER_IDLE_LEVEL GPIO_PIN_SET

const Buzzer_Pattern buzzer_patterns[BUZZER_EVENT_COUNT] = {[BUZZER_EVENT_NONE] = {0, 0, 0, 0},

                                                            [BUZZER_EVENT_BOOT_OK] = {100, 100, 1, 2},

                                                            [BUZZER_EVENT_ARMED] = {100, 100, 2, 2},

                                                            [BUZZER_EVENT_LOCKED] = {120, 100, 1, 2},

                                                            [BUZZER_EVENT_MAG_NEED_CAL] = {100, 120, 3, 3},

                                                            [BUZZER_EVENT_MAG_CAL_START] = {700, 200, 1, 3},

                                                            [BUZZER_EVENT_MAG_CAL_PROGRESS] = {40, 0, 1, 1},

                                                            [BUZZER_EVENT_MAG_CAL_VALIDATE] = {200, 150, 2, 3},

                                                            [BUZZER_EVENT_MAG_CAL_SUCCESS] = {700, 250, 2, 4},

                                                            [BUZZER_EVENT_MAG_CAL_FAILED] = {80, 80, 5, 5},

                                                            [BUZZER_EVENT_MAG_CAL_ABORTED] = {300, 150, 2, 4},

                                                            [BUZZER_EVENT_MAG_DISCONNECTED] = {80, 80, 3, 5},

                                                            [BUZZER_EVENT_MAG_INTERFERENCE] = {200, 150, 2, 4},

                                                            [BUZZER_EVENT_PPM_LOST] = {350, 200, 3, 5},

                                                            [BUZZER_EVENT_IMU_FAIL] = {1000, 200, 1, 5},

                                                            [BUZZER_EVENT_EMERGENCY] = {70, 70, 10, 6},

                                                            [BUZZER_EVENT_STOP] = {0, 0, 0, 255}};
void Buzzer_On(void) { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, BUZZER_ACTIVE_LEVEL); }
void Buzzer_Off(void) { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, BUZZER_IDLE_LEVEL); }
void Buzzer_Init(void) { Buzzer_Off(); }
