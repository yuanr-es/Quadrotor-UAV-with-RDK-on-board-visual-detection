#ifndef __GPS_H
#define __GPS_H
#include "stm32f4xx_hal.h"

#define GPS_BUF_SIZE 256

void GPS_Init(void);
void GPS_Process_Data(void);

#endif
