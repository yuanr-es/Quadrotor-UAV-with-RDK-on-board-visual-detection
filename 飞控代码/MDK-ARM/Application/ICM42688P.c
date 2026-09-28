#include "stm32f4xx_hal.h"
#include "ICM42688P.h"
#include "ICM42688P_Reg.h"
#include "MySPI.h"
#include "main.h"
#include "Com_debug.h"

void ICM42688P_WriteReg(uint8_t RegAddress, uint8_t Data)
{
    MySPI_Start();
    MySPI_Swapdata(ICM42688_SPI_WRITE_CMD(RegAddress));
    MySPI_Swapdata(Data);
    MySPI_Stop();
}
uint8_t ICM42688P_ReadReg(uint8_t RegAddress)
{
    uint8_t received;
    MySPI_Start();
    MySPI_Swapdata(ICM42688_SPI_READ_CMD(RegAddress));
    received = MySPI_Swapdata(0xFF);
    MySPI_Stop();
    return received;
}
uint8_t ICM42688P_Multi_ReadReg(uint8_t RegAddress, uint8_t num, uint8_t *received)
{
    MySPI_Start();
    MySPI_Swapdata(ICM42688_SPI_READ_CMD(RegAddress));
    for (uint8_t i = 0; i < num; i++)
    {
        received[i] = MySPI_Swapdata(0xFF);
    }
    MySPI_Stop();
    return num;
}
int ICM42688P_Init(void)
{
    uint8_t whoami;

    debug_printf("ICM42688P: Starting MySPI_Init...\r\n");
    MySPI_Init();
    debug_printf("ICM42688P: MySPI_Init done.\r\n");
    vTaskDelay(pdMS_TO_TICKS(50));

    whoami = ICM42688P_ReadReg(ICM42688_WHO_AM_I);
    debug_printf("HXY-ICM42688P WHO_AM_I: 0x%02X (expected 0x%02X)\r\n", whoami, ICM42688_EXPECTED_WHO_AM_I);

    if (whoami != ICM42688_EXPECTED_WHO_AM_I)
    {
        debug_printf("ICM42688P ERROR: WHO_AM_I mismatch! Check SPI wiring.\r\n");
        return -1;
    }

    ICM42688P_WriteReg(ICM42688_SOFT_RST, 0x80);
    vTaskDelay(pdMS_TO_TICKS(20));
    ICM42688P_WriteReg(ICM42688_SOFT_RST, 0x01);
    ICM42688P_WriteReg(ICM42688_PWR_CTRL, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));

    ICM42688P_WriteReg(ICM42688_COM_CFG, 0x10);
    ICM42688P_WriteReg(ICM42688_ACC_RANGE, 0x18);
    ICM42688P_WriteReg(ICM42688_GYR_RANGE, 0x18);

    ICM42688P_WriteReg(ICM42688_ACC_CONF, 0x03);
    ICM42688P_WriteReg(ICM42688_GYR_CONF, 0x03);

    ICM42688P_WriteReg(ICM42688_INT_CFG1, 0x01);

    return 0;
}
