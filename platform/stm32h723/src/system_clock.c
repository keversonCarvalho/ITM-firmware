#include "stm32h7xx_hal.h"
void Error_Handler(void);
void itm_system_clock_config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clock = {0};

    if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) {
        Error_Handler();
    }
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        Error_Handler();
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
        Error_Handler();
    }
}
