#ifndef ITM_DK_BOARD_H
#define ITM_DK_BOARD_H

#include <stdbool.h>
#include "stm32h7xx_hal.h"

#define ITM_DK_LED_OK_PORT GPIOC
#define ITM_DK_LED_OK_PIN GPIO_PIN_3
#define ITM_DK_LED_ERROR_PORT GPIOC
#define ITM_DK_LED_ERROR_PIN GPIO_PIN_2
#define ITM_DK_BUTTON_PORT GPIOC
#define ITM_DK_BUTTON_PIN GPIO_PIN_13

#define ITM_DK_VCP_UART USART3
#define ITM_DK_VCP_TX_PORT GPIOD
#define ITM_DK_VCP_TX_PIN GPIO_PIN_8
#define ITM_DK_VCP_RX_PORT GPIOD
#define ITM_DK_VCP_RX_PIN GPIO_PIN_9
#define ITM_DK_VCP_GPIO_AF GPIO_AF7_USART3

void itm_dk_board_init(void);
void itm_dk_led_ok_set(bool active);
void itm_dk_led_error_set(bool active);
bool itm_dk_button_pressed(void);

#endif
