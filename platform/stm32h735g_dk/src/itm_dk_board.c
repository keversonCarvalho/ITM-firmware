#include "itm_dk_board.h"

static void configure_led(uint16_t pin)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &init);
}

void itm_dk_board_init(void)
{
    GPIO_InitTypeDef button = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Both DK LEDs are active-low; preload them off before output mode. */
    HAL_GPIO_WritePin(GPIOC, ITM_DK_LED_OK_PIN | ITM_DK_LED_ERROR_PIN,
                      GPIO_PIN_SET);
    configure_led(ITM_DK_LED_OK_PIN);
    configure_led(ITM_DK_LED_ERROR_PIN);

    button.Pin = ITM_DK_BUTTON_PIN;
    button.Mode = GPIO_MODE_INPUT;
    button.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ITM_DK_BUTTON_PORT, &button);
}

void itm_dk_led_ok_set(bool active)
{
    HAL_GPIO_WritePin(ITM_DK_LED_OK_PORT, ITM_DK_LED_OK_PIN,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void itm_dk_led_error_set(bool active)
{
    HAL_GPIO_WritePin(ITM_DK_LED_ERROR_PORT, ITM_DK_LED_ERROR_PIN,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool itm_dk_button_pressed(void)
{
    return HAL_GPIO_ReadPin(ITM_DK_BUTTON_PORT, ITM_DK_BUTTON_PIN) ==
           GPIO_PIN_SET;
}
