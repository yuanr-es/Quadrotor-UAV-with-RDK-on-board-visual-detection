#ifndef __ICM42688P_H
#define __ICM42688P_H

#include <stdint.h>

int ICM42688P_Init(void);
void ICM42688P_WriteReg(uint8_t RegAddress, uint8_t Data);
uint8_t ICM42688P_ReadReg(uint8_t RegAddress);
uint8_t ICM42688P_Multi_ReadReg(uint8_t RegAddress, uint8_t num, uint8_t *received);
uint8_t ICM42688P_GetID(void);
void ICM42688P_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ, int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ);

#endif
