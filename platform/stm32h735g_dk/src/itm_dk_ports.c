#include "itm_dk_ports.h"

#include <stddef.h>
#include <stdint.h>
#include "stm32h7xx_hal.h"

extern UART_HandleTypeDef g_itm_dk_uart;

static uint32_t clock_now_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

static itm_critical_state_t critical_enter(void *context)
{
    const itm_critical_state_t state = __get_PRIMASK();
    (void)context;
    __disable_irq();
    __DSB();
    __ISB();
    return state;
}

static void critical_exit(void *context, itm_critical_state_t state)
{
    (void)context;
    __DSB();
    __ISB();
    if (state == 0U) {
        __enable_irq();
    }
}

static itm_result_t vcp_write(void *context, const uint8_t *data, size_t size)
{
    HAL_StatusTypeDef status;
    (void)context;
    if ((data == NULL) || (size == 0U) || (size > UINT16_MAX)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    status = HAL_UART_Transmit(&g_itm_dk_uart, (uint8_t *)data,
                               (uint16_t)size, 10U);
    return status == HAL_OK ? ITM_OK : ITM_ERROR_IO;
}

static itm_result_t vcp_read(void *context, uint8_t *data, size_t capacity,
                             size_t *received)
{
    (void)context;
    if ((data == NULL) || (capacity == 0U) || (received == NULL)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    *received = 0U;
    if (__HAL_UART_GET_FLAG(&g_itm_dk_uart, UART_FLAG_RXNE) != 0U) {
        if (HAL_UART_Receive(&g_itm_dk_uart, data, 1U, 0U) == HAL_OK) {
            *received = 1U;
        }
    }
    return ITM_OK;
}

itm_clock_port_t itm_dk_clock_port(void)
{
    return (itm_clock_port_t){NULL, clock_now_ms};
}

itm_critical_section_port_t itm_dk_critical_section_port(void)
{
    return (itm_critical_section_port_t){NULL, critical_enter, critical_exit};
}

itm_serial_port_t itm_dk_vcp_port(void)
{
    return (itm_serial_port_t){NULL, vcp_write, vcp_read};
}
