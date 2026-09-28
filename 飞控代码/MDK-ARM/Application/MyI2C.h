#ifndef __MYI2C_H
#define __MYI2C_H
#include "stm32f4xx_hal.h"

void I2C_Init(void);
void I2C_SDA(uint8_t DataBit);
void I2C_SCL(uint8_t DataBit);
uint8_t READ_SDA(void);
void I2C_Start(void);
void I2C_Stop(void);
uint8_t I2C_Read_Ack(void);
void I2C_SendAck(uint8_t ack);
void I2C_Send_Byte(uint8_t txd);
uint8_t I2C_Read_Byte(uint8_t ack);
uint8_t Soft_I2C_Mem_Write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *pData, uint16_t Size);
uint8_t Soft_I2C_Mem_Read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *pData, uint16_t Size);

#endif
