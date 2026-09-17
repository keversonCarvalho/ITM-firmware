#ifndef ITM_STM32H723_BOARD_H
#define ITM_STM32H723_BOARD_H

#include <stdbool.h>
#include "stm32h7xx_hal.h"

#define ITM_MFET1_PORT GPIOE
#define ITM_MFET1_PIN GPIO_PIN_2
#define ITM_MFET2_PORT GPIOE
#define ITM_MFET2_PIN GPIO_PIN_3
#define ITM_CAN1_TERM_PORT GPIOE
#define ITM_CAN1_TERM_PIN GPIO_PIN_4
#define ITM_FAULT1_PORT GPIOC
#define ITM_FAULT1_PIN GPIO_PIN_13
#define ITM_FAULT2_PORT GPIOC
#define ITM_FAULT2_PIN GPIO_PIN_14
#define ITM_UART4_DE_PORT GPIOB
#define ITM_UART4_DE_PIN GPIO_PIN_14
#define ITM_UART5_DE_PORT GPIOC
#define ITM_UART5_DE_PIN GPIO_PIN_8
#define ITM_LED_PORT GPIOC
#define ITM_LED_PIN GPIO_PIN_6

void itm_board_init(void);
void itm_board_force_safe_outputs(void);
void itm_board_led_set(bool active);
bool itm_board_fault1_level(void);
bool itm_board_fault2_level(void);

#endif
