

#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

#ifdef __cplusplus
extern "C"
{
#endif

    void NMI_Handler(void);
    void HardFault_Handler(void);
    void MemManage_Handler(void);
    void BusFault_Handler(void);
    void UsageFault_Handler(void);
    void SVC_Handler(void);
    void DebugMon_Handler(void);
    void PendSV_Handler(void);
    void SysTick_Handler(void);
    void EXTI2_IRQHandler(void);
    void DMA1_Stream0_IRQHandler(void);
    void DMA1_Stream1_IRQHandler(void);
    void DMA1_Stream3_IRQHandler(void);
    void DMA1_Stream4_IRQHandler(void);
    void EXTI9_5_IRQHandler(void);
    void TIM3_IRQHandler(void);
    void SPI2_IRQHandler(void);
    void USART3_IRQHandler(void);
    void TIM6_DAC_IRQHandler(void);
    void OTG_FS_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif
