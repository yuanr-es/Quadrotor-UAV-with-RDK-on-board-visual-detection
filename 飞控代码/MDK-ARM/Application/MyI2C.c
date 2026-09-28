#include "stm32f4xx_hal.h"
#include "main.h"
#include "MyI2C.h"

static void I2C_Delay(void)
{
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U && (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U)
    {
        uint32_t start = DWT->CYCCNT;
        uint32_t cycles = SystemCoreClock / 500000U;
        while ((uint32_t)(DWT->CYCCNT - start) < cycles)
        {
        }
    }
    else
    {
        for (volatile uint32_t i = 0; i < 32U; ++i)
        {
            __NOP();
        }
    }
}

void I2C_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    I2C_SDA(1);
    I2C_SCL(1);
}
void I2C_SDA(uint8_t DataBit)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, (DataBit) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    I2C_Delay();
}
void I2C_SCL(uint8_t DataBit)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, (DataBit) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    I2C_Delay();
}
uint8_t READ_SDA(void)
{
    uint8_t DataBit = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);
    I2C_Delay();
    return DataBit;
}
void I2C_Start(void)
{
    I2C_SDA(1);
    I2C_SCL(1);
    I2C_SDA(0);
    I2C_SCL(0);
}
void I2C_Stop(void)
{
    I2C_SDA(0);
    I2C_SCL(1);
    I2C_SDA(1);
}
uint8_t I2C_Read_Ack(void)
{
    uint8_t ack;
    I2C_SDA(1);
    I2C_SCL(1);
    ack = READ_SDA();
    I2C_SCL(0);
    return ack;
}
void I2C_SendAck(uint8_t ack)
{
    I2C_SDA(ack);
    I2C_SCL(1);
    I2C_SCL(0);
}
void I2C_Send_Byte(uint8_t txd)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        I2C_SDA(txd & (0x80 >> i));
        I2C_SCL(1);
        I2C_SCL(0);
    }
}
uint8_t I2C_Read_Byte(uint8_t ack)
{
    uint8_t Received_Data = 0;
    I2C_SDA(1);
    for (uint8_t i = 0; i < 8; i++)
    {
        I2C_SCL(1);
        Received_Data = (Received_Data << 1) | READ_SDA();
        I2C_SCL(0);
    }
    I2C_SendAck(ack);
    return Received_Data;
}

uint8_t Soft_I2C_Mem_Write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *pData, uint16_t Size)
{
    if (pData == NULL && Size != 0U)
    {
        return 1U;
    }

    I2C_Start();
    I2C_Send_Byte(dev_addr);
    if (I2C_Read_Ack())
    {
        I2C_Stop();
        return 1;
    }
    I2C_Send_Byte(reg_addr);
    if (I2C_Read_Ack())
    {
        I2C_Stop();
        return 1;
    }
    while (Size > 0)
    {
        I2C_Send_Byte(*(pData++));
        if (I2C_Read_Ack())
        {
            I2C_Stop();
            return 1;
        }
        Size--;
    }
    I2C_Stop();
    return 0;
}
uint8_t Soft_I2C_Mem_Read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *pData, uint16_t Size)
{
    if (pData == NULL || Size == 0U)
    {
        return 1U;
    }

    I2C_Start();
    I2C_Send_Byte(dev_addr);
    if (I2C_Read_Ack())
    {
        I2C_Stop();
        return 1;
    }
    I2C_Send_Byte(reg_addr);
    if (I2C_Read_Ack())
    {
        I2C_Stop();
        return 1;
    }
    I2C_Start();
    I2C_Send_Byte(dev_addr | 0x01);
    if (I2C_Read_Ack())
    {
        I2C_Stop();
        return 1;
    }

    while (Size > 0)
    {
        if (Size > 1)
        {
            *pData = I2C_Read_Byte(0);
        }
        if (Size == 1)
        {
            *pData = I2C_Read_Byte(1);
        }
        pData++;
        Size--;
    }
    I2C_Stop();
    return 0;
}
