#ifndef __MYSPI_H
#define __MYSPI_H
#include "stm32f4xx_hal.h"
extern SPI_HandleTypeDef hspi1;
void MySPI_W_CS(uint8_t Bitvalue);
void MySPI_Start(void);
void MySPI_Stop(void);
uint8_t MySPI_Swapdata(uint8_t SendData);
void MySPI_Init(void);

#endif
