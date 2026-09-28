#include "stm32f4xx_hal.h"
#include "main.h"
#include "BMP388.h"
#include "BMP388_Reg.h"
#include "Com_debug.h"
#include "math.h"
#include "MyI2C.h"
#include "FreeRTOS.h"
#include "task.h"
typedef struct
{
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    int32_t t_fine;
} BMP280_Calib_t;
static float ground_altitude = 0.0f;
static uint8_t is_calibrated = 0;
static int calib_count = 0;
static float calib_sum = 0.0f;
static BMP280_Calib_t calib;
static int BMP388_LoadCalibData(void)
{
    uint8_t buf[24];

    if (Soft_I2C_Mem_Read((BMP280_I2C_ADDR_PRIM << 1), 0x88, buf, 24) != 0)
    {
        return -1;
    }

    calib.dig_T1 = (uint16_t)((buf[1] << 8) | buf[0]);
    calib.dig_T2 = (int16_t)((buf[3] << 8) | buf[2]);
    calib.dig_T3 = (int16_t)((buf[5] << 8) | buf[4]);

    calib.dig_P1 = (uint16_t)((buf[7] << 8) | buf[6]);
    calib.dig_P2 = (int16_t)((buf[9] << 8) | buf[8]);
    calib.dig_P3 = (int16_t)((buf[11] << 8) | buf[10]);
    calib.dig_P4 = (int16_t)((buf[13] << 8) | buf[12]);
    calib.dig_P5 = (int16_t)((buf[15] << 8) | buf[14]);
    calib.dig_P6 = (int16_t)((buf[17] << 8) | buf[16]);
    calib.dig_P7 = (int16_t)((buf[19] << 8) | buf[18]);
    calib.dig_P8 = (int16_t)((buf[21] << 8) | buf[20]);
    calib.dig_P9 = (int16_t)((buf[23] << 8) | buf[22]);
    return 0;
}
static int BMP388_WriteReg(uint8_t RegAddress, uint8_t Data)
{
    return Soft_I2C_Mem_Write((BMP388_I2C_ADDR_PRIM << 1), RegAddress, &Data, 1);
}
uint8_t BMP388_ReadReg(uint8_t RegAddress)
{
    uint8_t data = 0;
    Soft_I2C_Mem_Read((BMP388_I2C_ADDR_PRIM << 1), RegAddress, &data, 1);
    return data;
}
int BMP388_Init(void)
{
    uint8_t chip_id;

    I2C_Init();
    chip_id = BMP388_ReadReg(BMP388_REG_CHIP_ID);
    if (chip_id != BMP388_CHIP_ID_VAL)
    {
        debug_printf("BMP388 Init ERROR! ID=0x%02X\r\n", chip_id);
        return -1;
    }

    if (BMP388_WriteReg(BMP388_REG_CMD, BMP388_CMD_SOFTRESET) != 0)
    {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    if (BMP388_LoadCalibData() != 0)
    {
        debug_printf("BMP388 calibration read failed!\r\n");
        return -1;
    }

    if (BMP388_WriteReg(BMP280_REG_CTRL_MEAS, 0x57) != 0 || BMP388_WriteReg(BMP280_REG_CONFIG, 0x10) != 0)
    {
        debug_printf("BMP388 configuration failed!\r\n");
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    debug_printf("BMP388 Init OK!\r\n");
    return 0;
}
int BMP388_ReadData(uint8_t *buf) { return Soft_I2C_Mem_Read((BMP280_I2C_ADDR_PRIM << 1), 0xF7, buf, 6); }
void BMP388_Process_Data(uint8_t *buf, BMP388_Data *pData)
{
    int32_t adc_P = (int32_t)((((uint32_t)buf[0]) << 12) | (((uint32_t)buf[1]) << 4) | (((uint32_t)buf[2]) >> 4));
    int32_t adc_T = (int32_t)((((uint32_t)buf[3]) << 12) | (((uint32_t)buf[4]) << 4) | (((uint32_t)buf[5]) >> 4));

    double var1, var2;
    var1 = (((double)adc_T) / 16384.0 - ((double)calib.dig_T1) / 1024.0) * ((double)calib.dig_T2);
    var2 = ((((double)adc_T) / 131072.0 - ((double)calib.dig_T1) / 8192.0) *
            (((double)adc_T) / 131072.0 - ((double)calib.dig_T1) / 8192.0)) *
           ((double)calib.dig_T3);
    calib.t_fine = (int32_t)(var1 + var2);
    pData->temperature = (float)((var1 + var2) / 5120.0);

    double p;
    var1 = ((double)calib.t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double)calib.dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double)calib.dig_P5) * 2.0;
    var2 = (var2 / 4.0) + (((double)calib.dig_P4) * 65536.0);
    var1 = (((double)calib.dig_P3) * var1 * var1 / 524288.0 + ((double)calib.dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double)calib.dig_P1);

    if (var1 == 0.0)
    {
        pData->pressure = 0.0f;
    }
    else
    {
        p = 1048576.0 - (double)adc_P;
        p = (p - (var2 / 4096.0)) * 6250.0 / var1;
        var1 = ((double)calib.dig_P9) * p * p / 2147483648.0;
        var2 = p * ((double)calib.dig_P8) / 32768.0;
        p = p + (var1 + var2 + ((double)calib.dig_P7)) / 16.0;
        pData->pressure = (float)p;
    }
    float raw_altitude = 0.0f;

    if (pData->pressure > 10000.0f)
    {
        raw_altitude = 44330.0f * (1.0f - powf(pData->pressure / 100075.9f, 0.190295f));
    }

    if (!is_calibrated)
    {
        if (pData->pressure > 80000.0f && pData->pressure < 115000.0f)
        {
            calib_sum += raw_altitude;
            calib_count++;
            if (calib_count >= 50)
            {
                ground_altitude = calib_sum / 50.0f;
                is_calibrated = 1;
            }
        }
        pData->altitude = 0.0f;
    }
    else
    {
        pData->altitude = raw_altitude - ground_altitude;
    }
}
