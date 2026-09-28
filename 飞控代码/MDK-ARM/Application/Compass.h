#ifndef __COMPASS_H
#define __COMPASS_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define QMC5883L_ADDR (0x0D << 1)
#define HMC5883L_ADDR (0x1E << 1)

#define HMC5883L_UT_PER_LSB (100.0f / 1090.0f)

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} CompassRawData;

typedef struct
{
    float x;
    float y;
    float z;
} CompassData;

typedef struct
{
    float offset_x;
    float offset_y;
    float offset_z;

    float scale_x;
    float scale_y;
    float scale_z;
} MagCalibrationData;
int Compass_Init(void);
HAL_StatusTypeDef Compass_Start_DMA_Reading(void);
bool Compass_ReadRaw(CompassRawData *raw_data);
bool Compass_DecodeRaw(CompassRawData *raw_data);
bool Compass_ApplyCalibration(const CompassRawData *raw_data, const MagCalibrationData *calibration,
                              CompassData *output_data);

#endif
