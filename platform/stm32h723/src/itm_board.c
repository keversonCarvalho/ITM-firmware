#include "itm_board.h"

static void configure_output(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &init);
}

void itm_board_force_safe_outputs(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* Preload inactive levels before changing the pins to outputs. */
    HAL_GPIO_WritePin(ITM_MFET1_PORT, ITM_MFET1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ITM_MFET2_PORT, ITM_MFET2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ITM_CAN1_TERM_PORT, ITM_CAN1_TERM_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ITM_UART4_DE_PORT, ITM_UART4_DE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ITM_UART5_DE_PORT, ITM_UART5_DE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ITM_LED_PORT, ITM_LED_PIN, GPIO_PIN_SET);

    configure_output(ITM_MFET1_PORT, ITM_MFET1_PIN);
    configure_output(ITM_MFET2_PORT, ITM_MFET2_PIN);
    configure_output(ITM_CAN1_TERM_PORT, ITM_CAN1_TERM_PIN);
    configure_output(ITM_UART4_DE_PORT, ITM_UART4_DE_PIN);
    configure_output(ITM_UART5_DE_PORT, ITM_UART5_DE_PIN);
    configure_output(ITM_LED_PORT, ITM_LED_PIN);
}

void itm_board_init(void)
{
    GPIO_InitTypeDef input = {0};

    itm_board_force_safe_outputs();

    input.Pin = ITM_FAULT1_PIN | ITM_FAULT2_PIN;
    input.Mode = GPIO_MODE_INPUT;
    input.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &input);
}

void itm_board_led_set(bool active)
{
    HAL_GPIO_WritePin(ITM_LED_PORT, ITM_LED_PIN,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool itm_board_fault1_level(void)
{
    return HAL_GPIO_ReadPin(ITM_FAULT1_PORT, ITM_FAULT1_PIN) == GPIO_PIN_SET;
}

bool itm_board_fault2_level(void)
{
    return HAL_GPIO_ReadPin(ITM_FAULT2_PORT, ITM_FAULT2_PIN) == GPIO_PIN_SET;
}
