#ifndef __BMP388_H
#define __BMP388_H
#include "stm32f4xx_hal.h"
typedef struct
{
    float temperature;
    float pressure;
    float altitude;
} BMP388_Data;
uint8_t BMP388_ReadReg(uint8_t RegAddress);
int BMP388_Init(void);
int BMP388_ReadData(uint8_t *buf);
void BMP388_Process_Data(uint8_t *buf, BMP388_Data *pData);

#endif
