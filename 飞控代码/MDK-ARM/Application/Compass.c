#include "Compass.h"
#include "main.h"
#include "Com_debug.h"
#include "App_freeRTOS.h"
extern I2C_HandleTypeDef hi2c1;
uint8_t compass_rx_buf[6];
static const MagCalibrationData identity_calibration = {.offset_x = 0.0f,
                                                        .offset_y = 0.0f,
                                                        .offset_z = 0.0f,

                                                        .scale_x = 1.0f,
                                                        .scale_y = 1.0f,
                                                        .scale_z = 1.0f};

static bool Compass_DecodeBuffer(const uint8_t buffer[6], CompassRawData *raw_data)
{
    if (buffer == NULL || raw_data == NULL)
    {
        return false;
    }

    raw_data->x = (int16_t)((uint16_t)buffer[0] << 8 | buffer[1]);
    raw_data->z = (int16_t)((uint16_t)buffer[2] << 8 | buffer[3]);
    raw_data->y = (int16_t)((uint16_t)buffer[4] << 8 | buffer[5]);

    if (raw_data->x == -4096 || raw_data->y == -4096 || raw_data->z == -4096)
    {
        return false;
    }
    return true;
}
int Compass_Init(void)
{
    debug_printf("--- Initializing HMC5883L ---\r\n");

    uint8_t ID = 0;
    if (HAL_I2C_Mem_Read(&hi2c1, HMC5883L_ADDR, 0x0A, 1, &ID, 1, 10) == HAL_OK)
    {
        debug_printf("HMC5883L Responded! (ID: 0x%02X)\r\n", ID);

        uint8_t config_a = 0x78;
        HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x00, 1, &config_a, 1, 10);

        uint8_t config_b = 0x20;
        HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x01, 1, &config_b, 1, 10);

        uint8_t mode = 0x00;
        HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x02, 1, &mode, 1, 10);

        debug_printf("HMC5883L Init Successful!\r\n");
        return 0;
    }

    debug_printf("HMC5883L Init Failed! Check Wiring.\r\n");
    return -1;
}

HAL_StatusTypeDef Compass_Start_DMA_Reading(void)
{
    return HAL_I2C_Mem_Read_DMA(&hi2c1, HMC5883L_ADDR, 0x03, 1, compass_rx_buf, 6);
}

bool Compass_ReadRaw(CompassRawData *raw_data)
{
    if (HAL_I2C_Mem_Read(&hi2c1, HMC5883L_ADDR, 0x03, 1, compass_rx_buf, sizeof(compass_rx_buf), 20) != HAL_OK)
    {
        return false;
    }
    return Compass_DecodeBuffer(compass_rx_buf, raw_data);
}

bool Compass_DecodeRaw(CompassRawData *raw_data) { return Compass_DecodeBuffer(compass_rx_buf, raw_data); }
bool Compass_ApplyCalibration(const CompassRawData *raw_data, const MagCalibrationData *calibration,
                              CompassData *output_data)
{
    if (raw_data == NULL || output_data == NULL)
    {
        return false;
    }
    if (calibration == NULL)
    {
        calibration = &identity_calibration;
    }
    float x_temp = (raw_data->x - calibration->offset_x) * calibration->scale_x;
    float y_temp = (raw_data->y - calibration->offset_y) * calibration->scale_y;
    float z_temp = (raw_data->z - calibration->offset_z) * calibration->scale_z;
    output_data->x = x_temp * HMC5883L_UT_PER_LSB;
    output_data->y = y_temp * HMC5883L_UT_PER_LSB;
    output_data->z = z_temp * HMC5883L_UT_PER_LSB;
    return true;
}
