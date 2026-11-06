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
#ifdef ITM_BC_PPP_XRCE
    oscillator.OscillatorType |= RCC_OSCILLATORTYPE_HSI |
                                 RCC_OSCILLATORTYPE_HSI48;
    oscillator.HSIState = RCC_HSI_ON;
    oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscillator.HSI48State = RCC_HSI48_ON;
#endif
    oscillator.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        Error_Handler();
    }

    clock.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                      RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                      RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
#ifdef ITM_BC_PPP_XRCE
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
#else
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
#endif
    clock.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clock.AHBCLKDivider = RCC_HCLK_DIV1;
    clock.APB3CLKDivider = RCC_APB3_DIV1;
    clock.APB1CLKDivider = RCC_APB1_DIV1;
    clock.APB2CLKDivider = RCC_APB2_DIV1;
    clock.APB4CLKDivider = RCC_APB4_DIV1;
    if (HAL_RCC_ClockConfig(&clock,
#ifdef ITM_BC_PPP_XRCE
                            FLASH_LATENCY_2
#else
                            FLASH_LATENCY_0
#endif
                            ) != HAL_OK) {
        Error_Handler();
    }
#ifdef ITM_BC_PPP_XRCE
    {
        RCC_PeriphCLKInitTypeDef peripheral = {0};
        peripheral.PeriphClockSelection = RCC_PERIPHCLK_RNG;
        peripheral.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
        if (HAL_RCCEx_PeriphCLKConfig(&peripheral) != HAL_OK) Error_Handler();
    }
#endif
}
