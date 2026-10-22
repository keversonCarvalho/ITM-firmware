#include "itm_board.h"
#include "stm32h7xx_hal.h"
#ifdef ITM_UPDATE_APPLICATION
#include "itm_update_platform.h"
extern const uint32_t g_pfnVectors[];
#endif

void itm_system_clock_config(void);
static void fatal_error(void) __attribute__((noreturn));

int main(void)
{
    uint32_t last_toggle;
    bool led_active = false;

#ifdef ITM_UPDATE_APPLICATION
    SCB->VTOR = (uint32_t)g_pfnVectors;
    __DSB(); __ISB(); __enable_irq();
#endif
    HAL_Init();
    itm_board_force_safe_outputs();
    itm_system_clock_config();
    itm_board_init();
#ifdef ITM_UPDATE_APPLICATION
    itm_update_app_init();
#endif

    last_toggle = HAL_GetTick();
    for (;;) {
        const uint32_t now = HAL_GetTick();
#ifdef ITM_UPDATE_APPLICATION
        itm_update_app_poll();
#endif
        if ((uint32_t)(now - last_toggle) >= 500U) {
            last_toggle = now;
            led_active = !led_active;
            itm_board_led_set(led_active);
        }
        __WFI();
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
