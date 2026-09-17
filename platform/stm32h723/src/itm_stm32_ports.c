#include "itm_stm32_ports.h"

#include <stddef.h>
#include "itm_board.h"
#include "stm32h7xx_hal.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    bool active_high;
} gpio_context_t;

static gpio_context_t mfet1 = {ITM_MFET1_PORT, ITM_MFET1_PIN, true};
static gpio_context_t mfet2 = {ITM_MFET2_PORT, ITM_MFET2_PIN, true};
static gpio_context_t fault1 = {ITM_FAULT1_PORT, ITM_FAULT1_PIN, true};
static gpio_context_t fault2 = {ITM_FAULT2_PORT, ITM_FAULT2_PIN, true};

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

static itm_result_t gpio_write(void *context, bool active)
{
    gpio_context_t *gpio = context;
    GPIO_PinState state;

    if (gpio == NULL) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    state = (active == gpio->active_high) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(gpio->port, gpio->pin, state);
    return ITM_OK;
}

static itm_result_t gpio_read(void *context, bool *active)
{
    gpio_context_t *gpio = context;
    bool level;

    if ((gpio == NULL) || (active == NULL)) {
        return ITM_ERROR_INVALID_ARGUMENT;
    }
    level = HAL_GPIO_ReadPin(gpio->port, gpio->pin) == GPIO_PIN_SET;
    *active = (level == gpio->active_high);
    return ITM_OK;
}

static itm_result_t adc_not_ready(void *context, uint8_t channel,
                                  int32_t *value)
{
    (void)context;
    (void)channel;
    (void)value;
    return ITM_ERROR_NOT_READY;
}

static itm_result_t can_send_not_ready(void *context,
                                       const itm_can_frame_t *frame)
{
    (void)context;
    (void)frame;
    return ITM_ERROR_NOT_READY;
}

static itm_result_t can_receive_not_ready(void *context,
                                          itm_can_frame_t *frame)
{
    (void)context;
    (void)frame;
    return ITM_ERROR_NOT_READY;
}

static itm_result_t serial_write_not_ready(void *context, const uint8_t *data,
                                           size_t size)
{
    (void)context;
    (void)data;
    (void)size;
    return ITM_ERROR_NOT_READY;
}

static itm_result_t serial_read_not_ready(void *context, uint8_t *data,
                                          size_t capacity, size_t *received)
{
    (void)context;
    (void)data;
    (void)capacity;
    if (received != NULL) {
        *received = 0U;
    }
    return ITM_ERROR_NOT_READY;
}

static itm_result_t flash_read_not_ready(void *context, uint8_t slot,
                                         size_t offset, void *data, size_t size)
{
    (void)context;
    (void)slot;
    (void)offset;
    (void)data;
    (void)size;
    return ITM_ERROR_NOT_READY;
}

static itm_result_t flash_erase_not_ready(void *context, uint8_t slot)
{
    (void)context;
    (void)slot;
    return ITM_ERROR_NOT_READY;
}

static itm_result_t flash_write_not_ready(void *context, uint8_t slot,
                                          size_t offset, const void *data,
                                          size_t size)
{
    (void)context;
    (void)slot;
    (void)offset;
    (void)data;
    (void)size;
    return ITM_ERROR_NOT_READY;
}

itm_clock_port_t itm_stm32_clock_port(void)
{
    return (itm_clock_port_t){NULL, clock_now_ms};
}

itm_critical_section_port_t itm_stm32_critical_section_port(void)
{
    return (itm_critical_section_port_t){NULL, critical_enter, critical_exit};
}

itm_digital_output_port_t itm_stm32_mfet1_port(void)
{
    return (itm_digital_output_port_t){&mfet1, gpio_write};
}

itm_digital_output_port_t itm_stm32_mfet2_port(void)
{
    return (itm_digital_output_port_t){&mfet2, gpio_write};
}

itm_digital_input_port_t itm_stm32_fault1_level_port(void)
{
    return (itm_digital_input_port_t){&fault1, gpio_read};
}

itm_digital_input_port_t itm_stm32_fault2_level_port(void)
{
    return (itm_digital_input_port_t){&fault2, gpio_read};
}

itm_adc_port_t itm_stm32_adc_stub(void)
{
    return (itm_adc_port_t){NULL, adc_not_ready};
}

itm_can_port_t itm_stm32_fdcan_stub(void)
{
    return (itm_can_port_t){NULL, can_send_not_ready, can_receive_not_ready};
}

itm_serial_port_t itm_stm32_serial_stub(void)
{
    return (itm_serial_port_t){NULL, serial_write_not_ready,
                               serial_read_not_ready};
}

itm_flash_port_t itm_stm32_flash_stub(void)
{
    return (itm_flash_port_t){NULL, 0U, flash_read_not_ready,
                              flash_erase_not_ready, flash_write_not_ready};
}
