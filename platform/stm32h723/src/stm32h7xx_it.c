#include "stm32h7xx_hal.h"
#ifdef ITM_BC_PPP_XRCE
#include "FreeRTOS.h"
#include "task.h"
void xPortSysTickHandler(void);
#endif

void Error_Handler(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
#ifdef ITM_BC_PPP_XRCE
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xPortSysTickHandler();
#endif
}

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    Error_Handler();
}

void MemManage_Handler(void)
{
    Error_Handler();
}

void BusFault_Handler(void)
{
    Error_Handler();
}

void UsageFault_Handler(void)
{
    Error_Handler();
}

#ifndef ITM_BC_PPP_XRCE
void SVC_Handler(void) {}
#endif

void DebugMon_Handler(void)
{
}

#ifndef ITM_BC_PPP_XRCE
void PendSV_Handler(void) {}
#endif
