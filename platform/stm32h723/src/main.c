#include "itm_board.h"
#include "stm32h7xx_hal.h"

static void system_clock_config(void);
static void fatal_error(void) __attribute__((noreturn));

int main(void)
{
    uint32_t last_toggle;
    bool led_active = false;

    HAL_Init();
    itm_board_force_safe_outputs();
    system_clock_config();
    itm_board_init();

    last_toggle = HAL_GetTick();
    for (;;) {
        const uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - last_toggle) >= 500U) {
            last_toggle = now;
            led_active = !led_active;
            itm_board_led_set(led_active);
        }
        __WFI();
    }
}

static void system_clock_config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clock = {0};

    if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) {
        fatal_error();
    }
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        fatal_error();
    }

    clock.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                      RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                      RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    clock.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clock.AHBCLKDivider = RCC_HCLK_DIV1;
    clock.APB3CLKDivider = RCC_APB3_DIV1;
    clock.APB1CLKDivider = RCC_APB1_DIV1;
    clock.APB2CLKDivider = RCC_APB2_DIV1;
    clock.APB4CLKDivider = RCC_APB4_DIV1;
    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_0) != HAL_OK) {
        fatal_error();
    }
}

static void fatal_error(void)
{
    itm_board_force_safe_outputs();
    itm_board_led_set(true);
    __disable_irq();
    for (;;) {
        __NOP();
    }
}

void Error_Handler(void)
{
    fatal_error();
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    fatal_error();
}
#endif
