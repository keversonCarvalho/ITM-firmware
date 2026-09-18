#include "itm_dk_board.h"
#include "itm_dk_ports.h"
#include "stm32h7xx_hal.h"

UART_HandleTypeDef g_itm_dk_uart;
static FDCAN_HandleTypeDef fdcan;

static void system_clock_config(void);
static void uart_init(void);
static bool fdcan_loopback_test(void);
static void fatal_error(void) __attribute__((noreturn));

int main(void)
{
    static const uint8_t ready[] = "ITM DK smoke test: PASS\r\n";
    itm_serial_port_t serial;
    uint32_t last_toggle;
    bool led_active = false;
    bool button_was_pressed = false;

    HAL_Init();
    itm_dk_board_init();
    system_clock_config();
    uart_init();

    if (!fdcan_loopback_test()) {
        fatal_error();
    }

    serial = itm_dk_vcp_port();
    if (serial.write(serial.context, ready, sizeof(ready) - 1U) != ITM_OK) {
        fatal_error();
    }

    last_toggle = HAL_GetTick();
    for (;;) {
        uint8_t byte;
        size_t received;
        const uint32_t now = HAL_GetTick();
        const bool button_pressed = itm_dk_button_pressed();

        if ((uint32_t)(now - last_toggle) >= 500U) {
            last_toggle = now;
            led_active = !led_active;
            itm_dk_led_ok_set(led_active);
        }
        if (serial.read(serial.context, &byte, 1U, &received) == ITM_OK &&
            received == 1U) {
            (void)serial.write(serial.context, &byte, 1U);
        }
        if (button_pressed && !button_was_pressed) {
            static const uint8_t pressed[] = "USER button\r\n";
            (void)serial.write(serial.context, pressed, sizeof(pressed) - 1U);
        }
        button_was_pressed = button_pressed;
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

static void uart_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    gpio.Pin = ITM_DK_VCP_TX_PIN | ITM_DK_VCP_RX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = ITM_DK_VCP_GPIO_AF;
    HAL_GPIO_Init(GPIOD, &gpio);

    g_itm_dk_uart.Instance = ITM_DK_VCP_UART;
    g_itm_dk_uart.Init.BaudRate = 115200U;
    g_itm_dk_uart.Init.WordLength = UART_WORDLENGTH_8B;
    g_itm_dk_uart.Init.StopBits = UART_STOPBITS_1;
    g_itm_dk_uart.Init.Parity = UART_PARITY_NONE;
    g_itm_dk_uart.Init.Mode = UART_MODE_TX_RX;
    g_itm_dk_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_itm_dk_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    g_itm_dk_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    g_itm_dk_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    if (HAL_UART_Init(&g_itm_dk_uart) != HAL_OK) {
        fatal_error();
    }
}

static bool fdcan_loopback_test(void)
{
    static uint8_t tx_data[8] = {0x49U, 0x54U, 0x4DU, 0x2DU,
                                 0x44U, 0x4BU, 0x00U, 0x01U};
    uint8_t rx_data[8] = {0};
    FDCAN_FilterTypeDef filter = {0};
    FDCAN_TxHeaderTypeDef tx = {0};
    FDCAN_RxHeaderTypeDef rx = {0};
    RCC_PeriphCLKInitTypeDef peripheral_clock = {0};
    uint32_t start;
    size_t index;

    peripheral_clock.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    peripheral_clock.FdcanClockSelection = RCC_FDCANCLKSOURCE_HSE;
    if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock) != HAL_OK) {
        return false;
    }
    __HAL_RCC_FDCAN_CLK_ENABLE();

    fdcan.Instance = FDCAN1;
    fdcan.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    fdcan.Init.Mode = FDCAN_MODE_INTERNAL_LOOPBACK;
    fdcan.Init.AutoRetransmission = DISABLE;
    fdcan.Init.TransmitPause = DISABLE;
    fdcan.Init.ProtocolException = DISABLE;
    /* Bench-only 250 kbit/s from the 25 MHz HSE kernel clock. */
    fdcan.Init.NominalPrescaler = 5U;
    fdcan.Init.NominalSyncJumpWidth = 1U;
    fdcan.Init.NominalTimeSeg1 = 15U;
    fdcan.Init.NominalTimeSeg2 = 4U;
    fdcan.Init.DataPrescaler = 5U;
    fdcan.Init.DataSyncJumpWidth = 1U;
    fdcan.Init.DataTimeSeg1 = 15U;
    fdcan.Init.DataTimeSeg2 = 4U;
    fdcan.Init.MessageRAMOffset = 0U;
    fdcan.Init.StdFiltersNbr = 1U;
    fdcan.Init.ExtFiltersNbr = 0U;
    fdcan.Init.RxFifo0ElmtsNbr = 1U;
    fdcan.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
    fdcan.Init.RxFifo1ElmtsNbr = 0U;
    fdcan.Init.RxBuffersNbr = 0U;
    fdcan.Init.TxEventsNbr = 0U;
    fdcan.Init.TxBuffersNbr = 0U;
    fdcan.Init.TxFifoQueueElmtsNbr = 1U;
    fdcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    fdcan.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
    if (HAL_FDCAN_Init(&fdcan) != HAL_OK) {
        return false;
    }

    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0U;
    filter.FilterType = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0x123U;
    filter.FilterID2 = 0x7FFU;
    if (HAL_FDCAN_ConfigFilter(&fdcan, &filter) != HAL_OK) {
        return false;
    }

    tx.Identifier = 0x123U;
    tx.IdType = FDCAN_STANDARD_ID;
    tx.TxFrameType = FDCAN_DATA_FRAME;
    tx.DataLength = FDCAN_DLC_BYTES_8;
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.BitRateSwitch = FDCAN_BRS_OFF;
    tx.FDFormat = FDCAN_CLASSIC_CAN;
    tx.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    if ((HAL_FDCAN_Start(&fdcan) != HAL_OK) ||
        (HAL_FDCAN_AddMessageToTxFifoQ(&fdcan, &tx, tx_data) != HAL_OK)) {
        return false;
    }

    start = HAL_GetTick();
    while (HAL_FDCAN_GetRxFifoFillLevel(&fdcan, FDCAN_RX_FIFO0) == 0U) {
        if ((uint32_t)(HAL_GetTick() - start) >= 20U) {
            return false;
        }
    }
    if (HAL_FDCAN_GetRxMessage(&fdcan, FDCAN_RX_FIFO0, &rx, rx_data) !=
        HAL_OK) {
        return false;
    }
    for (index = 0U; index < sizeof(tx_data); ++index) {
        if (rx_data[index] != tx_data[index]) {
            return false;
        }
    }
    return rx.Identifier == tx.Identifier;
}

static void fatal_error(void)
{
    itm_dk_led_ok_set(false);
    itm_dk_led_error_set(true);
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
