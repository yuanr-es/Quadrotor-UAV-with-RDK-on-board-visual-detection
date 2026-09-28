#include "stm32f4xx_hal.h"
#include "main.h"
#include "Com_debug.h"
#include "FreeRTOS.h"
extern SPI_HandleTypeDef hspi2;
uint8_t retry_times = 3;
void MySPI_W_CS(uint8_t Bitvalue) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, Bitvalue ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void MySPI_Start(void) { MySPI_W_CS(0); }
void MySPI_Stop(void) { MySPI_W_CS(1); }
uint8_t MySPI_Swapdata(uint8_t SendData)
{
    uint8_t ReceiveData = 0;
    HAL_SPI_TransmitReceive(&hspi2, &SendData, &ReceiveData, 1, HAL_MAX_DELAY);
    return ReceiveData;
}
void MySPI_Init(void)
{
    debug_printf("MySPI_Init: start\r\n");
    MySPI_W_CS(0);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(10));

    MySPI_Start();
    debug_printf("MySPI_Init: doing first SPI read...\r\n");
    uint8_t whoami = MySPI_Swapdata(0x01 | 0x80);
    debug_printf("MySPI_Init: first byte done, second...\r\n");
    whoami = MySPI_Swapdata(0x01 | 0x80);
    debug_printf("MySPI_Init: done\r\n");
    MySPI_Stop();
}
