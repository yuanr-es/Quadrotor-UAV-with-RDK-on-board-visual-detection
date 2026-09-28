#include "GPS.h"
#include "main.h"
#include "Com_debug.h"
#include <string.h>

extern UART_HandleTypeDef huart1;
uint8_t GPS_rx_buf[GPS_BUF_SIZE];
volatile uint8_t GPS_data_ready = 0;
uint16_t GPS_data_len = 0;
void GPS_Init(void)
{
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    HAL_UART_Receive_DMA(&huart1, GPS_rx_buf, GPS_BUF_SIZE);
    debug_printf("GPS Init successful");
}
void GPS_IDLE_Callback(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET)
    {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        HAL_UART_DMAStop(&huart1);
        GPS_data_len = GPS_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);
        GPS_data_ready = 1;
        HAL_UART_Receive_DMA(&huart1, GPS_rx_buf, GPS_BUF_SIZE);
    }
}
void GPS_Process_Data(void)
{
    if (GPS_data_ready == 1)
    {
        GPS_data_ready = 0;

        if (GPS_data_len >= GPS_BUF_SIZE) GPS_data_len = GPS_BUF_SIZE - 1;
        GPS_rx_buf[GPS_data_len] = '\0';

        debug_printf("[GPS RAW]: %s\r\n", GPS_rx_buf);
    }
}
