#include "itm_update_platform.h"
#include "itm_board.h"
#include "battery_dbc_generated.h"
#include <string.h>

static FDCAN_HandleTypeDef fdcan;
static FDCAN_HandleTypeDef battery_fdcan[2];
static UART_HandleTypeDef ceb_uart;
static UART_HandleTypeDef bc_uart;
static ADC_HandleTypeDef cb_adc;
static ADC_ChannelConfTypeDef cb_adc_channel;
static RNG_HandleTypeDef random_generator;

bool itm_update_random_u32(uint32_t *value)
{
    static bool initialized;
    if (!value) return false;
    if (!initialized) {
        __HAL_RCC_RNG_CLK_ENABLE();
        random_generator.Instance = RNG;
        random_generator.Init.ClockErrorDetection = RNG_CED_ENABLE;
        if (HAL_RNG_Init(&random_generator) != HAL_OK) return false;
        initialized = true;
    }
    return HAL_RNG_GenerateRandomNumber(&random_generator, value) == HAL_OK;
}

bool itm_update_cb_adc_init(void)
{
    GPIO_InitTypeDef pins = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_CLKP);
    pins.Mode = GPIO_MODE_ANALOG;
    pins.Pull = GPIO_NOPULL;
    pins.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &pins);
    pins.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &pins);
    cb_adc.Instance = ADC1;
    cb_adc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
    cb_adc.Init.Resolution = ADC_RESOLUTION_16B;
    cb_adc.Init.ScanConvMode = DISABLE;
    cb_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    cb_adc.Init.LowPowerAutoWait = DISABLE;
    cb_adc.Init.ContinuousConvMode = DISABLE;
    cb_adc.Init.NbrOfConversion = 1;
    cb_adc.Init.DiscontinuousConvMode = DISABLE;
    cb_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    cb_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    cb_adc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    cb_adc.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    cb_adc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    cb_adc.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&cb_adc) != HAL_OK ||
        HAL_ADCEx_Calibration_Start(&cb_adc, ADC_CALIB_OFFSET_LINEARITY,
                                    ADC_DIFFERENTIAL_ENDED) != HAL_OK)
        return false;
    cb_adc_channel.Rank = ADC_REGULAR_RANK_1;
    cb_adc_channel.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
    cb_adc_channel.SingleDiff = ADC_DIFFERENTIAL_ENDED;
    cb_adc_channel.OffsetNumber = ADC_OFFSET_NONE;
    cb_adc_channel.Channel = ADC_CHANNEL_3;
    return HAL_ADC_ConfigChannel(&cb_adc, &cb_adc_channel) == HAL_OK;
}

static bool cb_adc_sample(uint32_t channel, uint16_t *raw)
{
    cb_adc_channel.Channel = channel;
    if (HAL_ADC_ConfigChannel(&cb_adc, &cb_adc_channel) != HAL_OK ||
        HAL_ADC_Start(&cb_adc) != HAL_OK) return false;
    if (HAL_ADC_PollForConversion(&cb_adc, 2U) != HAL_OK) {
        (void)HAL_ADC_Stop(&cb_adc);
        return false;
    }
    *raw = (uint16_t)HAL_ADC_GetValue(&cb_adc);
    return HAL_ADC_Stop(&cb_adc) == HAL_OK;
}

uint8_t itm_update_cb_adc_read(uint16_t *em1_raw, uint16_t *em2_raw)
{
    uint8_t valid = 0U;
    if (!em1_raw || !em2_raw) return 0U;
    if (cb_adc_sample(ADC_CHANNEL_3, em1_raw)) valid |= 1U;
    if (cb_adc_sample(ADC_CHANNEL_4, em2_raw)) valid |= 2U;
    return valid;
}
#define BC_UART_RING_SIZE 4096U
static volatile uint8_t bc_tx_ring[BC_UART_RING_SIZE];
static volatile uint8_t bc_rx_ring[BC_UART_RING_SIZE];
static volatile uint16_t bc_tx_head, bc_tx_tail;
static volatile uint16_t bc_rx_head, bc_rx_tail;
static volatile uint32_t bc_uart_errors;
static volatile bool bc_tx_busy;
#define CEB_RX_RING_SIZE 512U
static volatile uint8_t ceb_rx_ring[CEB_RX_RING_SIZE];
static volatile uint16_t ceb_rx_head, ceb_rx_tail;
static volatile uint32_t ceb_rx_overflows;

bool itm_update_ceb_uart_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    RCC_PeriphCLKInitTypeDef clocks = {0};
    uint32_t guard = 1000000U;
    /* HSI gives 460800 baud within 0.1%; APB1 at 8 MHz errs by about 2%. */
    __HAL_RCC_HSI_ENABLE();
    while (!__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) && --guard) {}
    if (!guard) return false;
    clocks.PeriphClockSelection = RCC_PERIPHCLK_UART4;
    clocks.Usart234578ClockSelection = RCC_UART4CLKSOURCE_HSI;
    if (HAL_RCCEx_PeriphCLKConfig(&clocks) != HAL_OK) return false;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_UART4_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOA, &gpio);
    ceb_uart.Instance = UART4;
    ceb_uart.Init.BaudRate = 460800;
    ceb_uart.Init.WordLength = UART_WORDLENGTH_8B;
    ceb_uart.Init.StopBits = UART_STOPBITS_1;
    ceb_uart.Init.Parity = UART_PARITY_NONE;
    ceb_uart.Init.Mode = UART_MODE_RX;
    ceb_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    ceb_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    ceb_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    ceb_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    ceb_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&ceb_uart) != HAL_OK ||
        HAL_UARTEx_EnableFifoMode(&ceb_uart) != HAL_OK) return false;
    UART4->ICR = USART_ICR_ORECF | USART_ICR_FECF |
                 USART_ICR_NECF | USART_ICR_PECF;
    UART4->CR3 |= USART_CR3_EIE;
    UART4->CR1 |= USART_CR1_RXNEIE_RXFNEIE;
    NVIC_SetPriority(UART4_IRQn, 5);
    NVIC_EnableIRQ(UART4_IRQn);
    return true;
}

bool itm_update_ceb_uart_read(uint8_t *byte)
{
    const uint16_t tail = ceb_rx_tail;
    if (!byte || tail == ceb_rx_head) return false;
    *byte = ceb_rx_ring[tail];
    ceb_rx_tail = (uint16_t)((tail + 1U) % CEB_RX_RING_SIZE);
    return true;
}
uint32_t itm_update_ceb_uart_errors(void) { return ceb_rx_overflows; }

void UART4_IRQHandler(void)
{
    uint32_t status = UART4->ISR;
    if (status & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) {
        ++ceb_rx_overflows;
        UART4->ICR = USART_ICR_ORECF | USART_ICR_FECF |
                     USART_ICR_NECF | USART_ICR_PECF;
    }
    while (UART4->ISR & USART_ISR_RXNE_RXFNE) {
        const uint8_t byte = (uint8_t)UART4->RDR;
        const uint16_t head = ceb_rx_head;
        const uint16_t next = (uint16_t)((head + 1U) & (CEB_RX_RING_SIZE - 1U));
        if (next == ceb_rx_tail) ++ceb_rx_overflows;
        else {
            ceb_rx_ring[head] = byte;
            ceb_rx_head = next;
        }
    }
}

bool itm_update_bc_uart_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    RCC_PeriphCLKInitTypeDef clocks = {0};
    uint32_t guard = 1000000U;
    __HAL_RCC_HSI_ENABLE();
    while (!__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) && --guard) {}
    if (!guard) return false;
    clocks.PeriphClockSelection = RCC_PERIPHCLK_UART7;
    clocks.Usart234578ClockSelection = RCC_UART7CLKSOURCE_HSI;
    if (HAL_RCCEx_PeriphCLKConfig(&clocks) != HAL_OK) return false;
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_UART7_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_7 | GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_UART7;
    HAL_GPIO_Init(GPIOE, &gpio);
    bc_uart.Instance = UART7;
    bc_uart.Init.BaudRate = 460800;
    bc_uart.Init.WordLength = UART_WORDLENGTH_8B;
    bc_uart.Init.StopBits = UART_STOPBITS_1;
    bc_uart.Init.Parity = UART_PARITY_NONE;
    bc_uart.Init.Mode = UART_MODE_TX_RX;
    bc_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    bc_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    bc_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    bc_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    bc_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&bc_uart) != HAL_OK ||
        HAL_UARTEx_EnableFifoMode(&bc_uart) != HAL_OK ||
        HAL_UARTEx_SetTxFifoThreshold(&bc_uart,
                                     UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
        return false;
    bc_tx_head = bc_tx_tail = bc_rx_head = bc_rx_tail = 0U;
    bc_uart_errors = 0U;
    bc_tx_busy = false;
    UART7->ICR = USART_ICR_ORECF | USART_ICR_FECF |
                 USART_ICR_NECF | USART_ICR_PECF;
    UART7->CR3 |= USART_CR3_EIE;
    UART7->CR1 |= USART_CR1_RXNEIE_RXFNEIE;
    NVIC_SetPriority(UART7_IRQn, 6);
    NVIC_EnableIRQ(UART7_IRQn);
    return true;
}

bool itm_update_bc_uart_idle(void) { return !bc_tx_busy; }

bool itm_update_bc_uart_read(uint8_t *byte)
{
    const uint16_t tail = bc_rx_tail;
    if (!byte || tail == bc_rx_head) return false;
    *byte = bc_rx_ring[tail];
    bc_rx_tail = (uint16_t)((tail + 1U) & (BC_UART_RING_SIZE - 1U));
    return true;
}
uint32_t itm_update_bc_uart_errors(void) { return bc_uart_errors; }

bool itm_update_bc_uart_send(const uint8_t *frame, size_t length)
{
    size_t i;
    if (!frame || length == 0U || length >= BC_UART_RING_SIZE) return false;
    if (length > (BC_UART_RING_SIZE - 1U) -
        ((bc_tx_head - bc_tx_tail) & (BC_UART_RING_SIZE - 1U)))
        return false;
    for (i = 0U; i < length; ++i) {
        bc_tx_ring[bc_tx_head] = frame[i];
        __DMB();
        bc_tx_head = (uint16_t)((bc_tx_head + 1U) & (BC_UART_RING_SIZE - 1U));
    }
    bc_tx_busy = true;
    UART7->ICR = USART_ICR_TCCF;
    UART7->CR3 |= USART_CR3_TXFTIE;
    return true;
}

void UART7_IRQHandler(void)
{
    if (UART7->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) {
        ++bc_uart_errors;
        UART7->ICR = USART_ICR_ORECF | USART_ICR_FECF |
                     USART_ICR_NECF | USART_ICR_PECF;
    }
    while (UART7->ISR & USART_ISR_RXNE_RXFNE) {
        const uint8_t byte = (uint8_t)UART7->RDR;
        const uint16_t next = (uint16_t)((bc_rx_head + 1U) &
                                         (BC_UART_RING_SIZE - 1U));
        if (next == bc_rx_tail) ++bc_uart_errors;
        else { bc_rx_ring[bc_rx_head] = byte; bc_rx_head = next; }
    }
    if ((UART7->CR3 & USART_CR3_TXFTIE) != 0U &&
        (UART7->ISR & USART_ISR_TXFT) != 0U) {
        while ((UART7->ISR & USART_ISR_TXE_TXFNF) != 0U &&
               bc_tx_tail != bc_tx_head) {
            UART7->TDR = bc_tx_ring[bc_tx_tail];
            bc_tx_tail = (uint16_t)((bc_tx_tail + 1U) &
                                     (BC_UART_RING_SIZE - 1U));
        }
        if (bc_tx_tail == bc_tx_head) {
            UART7->CR3 &= ~USART_CR3_TXFTIE;
            UART7->CR1 |= USART_CR1_TCIE;
        }
    }
    if ((UART7->CR1 & USART_CR1_TCIE) != 0U &&
        (UART7->ISR & USART_ISR_TC) != 0U) {
        UART7->CR1 &= ~USART_CR1_TCIE;
        UART7->ICR = USART_ICR_TCCF;
        if (bc_tx_tail == bc_tx_head) bc_tx_busy = false;
    }
}
static const uint32_t battery_ids[] = {
    BATTERY_DBC_GENERATED_CAN_PACKET_BMS_TEMPS_FRAME_ID,
    BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_TOT_FRAME_ID,
    BATTERY_DBC_GENERATED_CAN_PACKET_BMS_I_FRAME_ID,
    BATTERY_DBC_GENERATED_CAN_PACKET_BMS_AH_WH_FRAME_ID,
    BATTERY_DBC_GENERATED_CAN_PACKET_BMS_V_CELL_FRAME_ID,
    BATTERY_DBC_GENERATED_CAN_PACKET_BMS_BAL_FRAME_ID,
    BATTERY_DBC_GENERATED_CAN_PACKET_BMS_SOC_SOH_TEMP_STAT_FRAME_ID,
    BATTERY_DBC_GENERATED_CAN_PACKET_BMS_HUM_FRAME_ID,
};
static const uint32_t addresses[] = {ITM_FW_SLOT_A, ITM_FW_SLOT_B, 0x08020000UL, 0x08040000UL};
static const uint32_t capacities[] = {ITM_FW_SLOT_SIZE, ITM_FW_SLOT_SIZE, 0x20000UL, 0x20000UL};
bool itm_update_boot_protected(void) { return (FLASH->WPSN_CUR1 & 1U) == 0; }
static bool read_region(void *ctx, unsigned region, uint32_t off, void *out, size_t n)
{
    (void)ctx;
    if (region > 3 || off > capacities[region] || n > capacities[region] - off) return false;
    FLASH->CCR1 = FLASH_SR_DBECCERR;
    memcpy(out, (const void *)(uintptr_t)(addresses[region] + off), n);
    return (FLASH->SR1 & FLASH_SR_DBECCERR) == 0;
}

/* Entire busy interval executes from DTCM, with IRQs masked and caches off.
 * No HAL calls, literal strings, flash data or tick-based timeouts here.
 * A stuck controller stops feeding IWDG; it must never return to busy flash. */
__attribute__((section(".ramfunc"), noinline))
static bool flash_operation(uint32_t address, const uint32_t *word, unsigned sector)
{
    uint32_t count = 0;
    unsigned i;
    while (FLASH->SR1 & (FLASH_SR_BSY | FLASH_SR_QW)) {
        if (++count > 32000000U) for (;;) { __NOP(); }
    }
    if (FLASH->CR1 & FLASH_CR_LOCK) {
        FLASH->KEYR1 = 0x45670123UL;
        FLASH->KEYR1 = 0xcdef89abUL;
    }
    if (FLASH->CR1 & FLASH_CR_LOCK) return false;
    FLASH->CCR1 = FLASH_FLAG_ALL_ERRORS_BANK1 | FLASH_SR_EOP;
    if (word) {
        FLASH->CR1 |= FLASH_CR_PG;
        __DSB(); __ISB();
        for (i = 0; i < 8; ++i) ((volatile uint32_t *)(uintptr_t)address)[i] = word[i];
    } else {
        FLASH->CR1 = (FLASH->CR1 & ~FLASH_CR_SNB) | FLASH_CR_SER |
                     (sector << FLASH_CR_SNB_Pos);
        FLASH->CR1 |= FLASH_CR_START;
    }
    __DSB(); __ISB(); count = 0;
    while (FLASH->SR1 & (FLASH_SR_BSY | FLASH_SR_QW)) {
        if (++count > 32000000U) for (;;) { __NOP(); }
        IWDG1->KR = 0xaaaa;
    }
    FLASH->CR1 &= ~(FLASH_CR_PG | FLASH_CR_SER | FLASH_CR_SNB);
    FLASH->CR1 |= FLASH_CR_LOCK;
    return (FLASH->SR1 & FLASH_FLAG_ALL_ERRORS_BANK1) == 0;
}
static bool erase_region(void *ctx, unsigned region)
{
    uint32_t state;
    unsigned first, i, count;
    bool ok = true;
    (void)ctx;
    if (region > 3 || !itm_update_boot_protected()) return false;
    first = (addresses[region] - FLASH_BASE) / 0x20000UL;
    count = capacities[region] / 0x20000UL;
    state = __get_PRIMASK(); __disable_irq();
    for (i = 0; i < count && ok; ++i) ok = flash_operation(0, NULL, first + i);
    __set_PRIMASK(state);
    return ok;
}
static bool program_region(void *ctx, unsigned region, uint32_t off, const void *data, size_t n)
{
    uint32_t word[8], state;
    bool ok;
    (void)ctx;
    if (region > 3 || n != 32 || (off % 32) || off > capacities[region] - 32 ||
        !itm_update_boot_protected()) return false;
    memcpy(word, data, 32);
    state = __get_PRIMASK(); __disable_irq();
    ok = flash_operation(addresses[region] + off, word, 0);
    __set_PRIMASK(state);
    return ok;
}
static bool bench_authorize(void *ctx, unsigned slot, const uint8_t *header)
{
    (void)ctx; (void)slot; (void)header;
#ifdef ITM_UPDATE_BENCH_CRC_ONLY
    return true;
#else
    return false; /* Flight policy is fail-closed until provisioned. */
#endif
}
itm_fw_port_t itm_update_flash_port(void)
{
    return (itm_fw_port_t){NULL, read_region, erase_region, program_region, bench_authorize};
}
static itm_result_t send_frame(void *ctx, const itm_can_frame_t *frame)
{
    FDCAN_TxHeaderTypeDef tx = {0};
    (void)ctx;
    if (!frame || frame->id > 0x7ff || frame->length > 8) return ITM_ERROR_INVALID_ARGUMENT;
    tx.Identifier = frame->id; tx.IdType = FDCAN_STANDARD_ID;
    tx.TxFrameType = FDCAN_DATA_FRAME; tx.DataLength = frame->length;
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE; tx.BitRateSwitch = FDCAN_BRS_OFF;
    tx.FDFormat = FDCAN_CLASSIC_CAN; tx.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    return HAL_FDCAN_AddMessageToTxFifoQ(&fdcan, &tx, (uint8_t *)frame->data) == HAL_OK ? ITM_OK : ITM_ERROR_IO;
}
static itm_result_t receive_frame(void *ctx, itm_can_frame_t *frame)
{
    FDCAN_RxHeaderTypeDef rx;
    uint8_t data[8];
    (void)ctx;
    if (!HAL_FDCAN_GetRxFifoFillLevel(&fdcan, FDCAN_RX_FIFO0)) return ITM_ERROR_NOT_FOUND;
    if (HAL_FDCAN_GetRxMessage(&fdcan, FDCAN_RX_FIFO0, &rx, data) != HAL_OK) return ITM_ERROR_IO;
    if (rx.IdType != FDCAN_STANDARD_ID || rx.RxFrameType != FDCAN_DATA_FRAME ||
        rx.FDFormat != FDCAN_CLASSIC_CAN || rx.DataLength > FDCAN_DLC_BYTES_8) return ITM_ERROR_INVALID_ARGUMENT;
    frame->id = rx.Identifier; frame->length = (uint8_t)rx.DataLength;
    memcpy(frame->data, data, frame->length); return ITM_OK;
}
bool itm_update_can_init(void)
{
    RCC_PeriphCLKInitTypeDef clocks = {0};
    GPIO_InitTypeDef gpio = {0};
    FDCAN_FilterTypeDef filter = {0};
    /* Reset leaves both caches disabled, and neither image enables them.
     * Avoid set/way maintenance on an already disabled D-cache during bring-up. */
    if (SCB->CCR & (SCB_CCR_IC_Msk | SCB_CCR_DC_Msk)) return false;
    clocks.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    clocks.FdcanClockSelection = RCC_FDCANCLKSOURCE_HSE;
    if (HAL_RCCEx_PeriphCLKConfig(&clocks) != HAL_OK) return false;
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_FDCAN_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12; gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF9_FDCAN1; HAL_GPIO_Init(GPIOA, &gpio);
    fdcan.Instance = FDCAN1;
    fdcan.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    fdcan.Init.Mode = FDCAN_MODE_NORMAL;
    fdcan.Init.AutoRetransmission = ENABLE;
    fdcan.Init.TransmitPause = DISABLE; fdcan.Init.ProtocolException = DISABLE;
    /* 8 MHz HSE / (1 * (1 + 13 + 2)) = 500 kbit/s. */
    fdcan.Init.NominalPrescaler = 1; fdcan.Init.NominalSyncJumpWidth = 2;
    fdcan.Init.NominalTimeSeg1 = 13; fdcan.Init.NominalTimeSeg2 = 2;
    fdcan.Init.DataPrescaler = 1; fdcan.Init.DataSyncJumpWidth = 1;
    fdcan.Init.DataTimeSeg1 = 1; fdcan.Init.DataTimeSeg2 = 1;
    fdcan.Init.StdFiltersNbr = 1; fdcan.Init.ExtFiltersNbr = 0;
    fdcan.Init.RxFifo0ElmtsNbr = 16; fdcan.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
    fdcan.Init.TxFifoQueueElmtsNbr = 8; fdcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    fdcan.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
    if (HAL_FDCAN_Init(&fdcan) != HAL_OK) return false;
    filter.IdType = FDCAN_STANDARD_ID; filter.FilterIndex = 0;
    filter.FilterType = FDCAN_FILTER_RANGE; filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0; filter.FilterID2 = 0x7ff;
    return HAL_FDCAN_ConfigFilter(&fdcan, &filter) == HAL_OK &&
           HAL_FDCAN_ConfigGlobalFilter(&fdcan, FDCAN_REJECT, FDCAN_REJECT,
                                        FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) == HAL_OK &&
           HAL_FDCAN_Start(&fdcan) == HAL_OK;
}
static FDCAN_HandleTypeDef *battery_handle(itm_dbcc_bus_t bus)
{
    if (bus == ITM_DBCC_BUS_BATT_28V) return &battery_fdcan[0];
    if (bus == ITM_DBCC_BUS_BATT_150V) return &battery_fdcan[1];
    return NULL;
}
static bool init_battery_channel(FDCAN_HandleTypeDef *handle,
                                 FDCAN_GlobalTypeDef *instance,
                                 uint32_t ram_offset)
{
    FDCAN_FilterTypeDef filter = {0};
    unsigned i;
    handle->Instance = instance;
    handle->Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    handle->Init.Mode = FDCAN_MODE_NORMAL;
    handle->Init.AutoRetransmission = DISABLE;
    handle->Init.TransmitPause = DISABLE;
    handle->Init.ProtocolException = DISABLE;
    handle->Init.NominalPrescaler = 1;
    handle->Init.NominalSyncJumpWidth = 2;
    handle->Init.NominalTimeSeg1 = 13;
    handle->Init.NominalTimeSeg2 = 2; /* 8 MHz HSE / 16 = 500 kbit/s. */
    handle->Init.DataPrescaler = 1;
    handle->Init.DataSyncJumpWidth = 1;
    handle->Init.DataTimeSeg1 = 1;
    handle->Init.DataTimeSeg2 = 1;
    handle->Init.MessageRAMOffset = ram_offset;
#ifdef ITM_UPDATE_BENCH_CRC_ONLY
    handle->Init.StdFiltersNbr = 1;
    handle->Init.TxFifoQueueElmtsNbr = 2;
#else
    handle->Init.StdFiltersNbr = 0;
    handle->Init.TxFifoQueueElmtsNbr = 0;
#endif
    handle->Init.ExtFiltersNbr = sizeof(battery_ids) / sizeof(battery_ids[0]);
    handle->Init.RxFifo0ElmtsNbr = 16;
    handle->Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
    handle->Init.RxFifo1ElmtsNbr = 0;
    handle->Init.RxBuffersNbr = 0;
    handle->Init.TxEventsNbr = 0;
    handle->Init.TxBuffersNbr = 0;
    handle->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    handle->Init.TxElmtSize = FDCAN_DATA_BYTES_8;
    if (HAL_FDCAN_Init(handle) != HAL_OK) return false;
    filter.IdType = FDCAN_EXTENDED_ID;
    filter.FilterType = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID2 = 0x1fffffffU;
    for (i = 0; i < sizeof(battery_ids) / sizeof(battery_ids[0]); ++i) {
        filter.FilterIndex = i;
        filter.FilterID1 = battery_ids[i];
        if (HAL_FDCAN_ConfigFilter(handle, &filter) != HAL_OK) return false;
    }
#ifdef ITM_UPDATE_BENCH_CRC_ONLY
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0;
    filter.FilterID1 = 0x7a0;
    filter.FilterID2 = 0x7ff;
    if (HAL_FDCAN_ConfigFilter(handle, &filter) != HAL_OK) return false;
#endif
    return HAL_FDCAN_ConfigGlobalFilter(handle, FDCAN_REJECT, FDCAN_REJECT,
                                        FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) == HAL_OK &&
           HAL_FDCAN_Start(handle) == HAL_OK;
}
bool itm_update_battery_can_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_FDCAN_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF9_FDCAN2;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Alternate = GPIO_AF5_FDCAN3;
    HAL_GPIO_Init(GPIOD, &gpio);
    /* FDCAN1 occupies 97 words; the following 128-word regions do not overlap. */
    return init_battery_channel(&battery_fdcan[0], FDCAN2, 128) &&
           init_battery_channel(&battery_fdcan[1], FDCAN3, 256);
}
itm_result_t itm_update_battery_can_receive(itm_dbcc_bus_t bus,
                                             itm_dbcc_frame_t *frame)
{
    FDCAN_HandleTypeDef *handle = battery_handle(bus);
    FDCAN_RxHeaderTypeDef rx;
    if (!handle || !frame) return ITM_ERROR_INVALID_ARGUMENT;
    if (!HAL_FDCAN_GetRxFifoFillLevel(handle, FDCAN_RX_FIFO0)) return ITM_ERROR_NOT_FOUND;
    if (HAL_FDCAN_GetRxMessage(handle, FDCAN_RX_FIFO0, &rx, frame->data) != HAL_OK)
        return ITM_ERROR_IO;
    if (rx.RxFrameType != FDCAN_DATA_FRAME || rx.FDFormat != FDCAN_CLASSIC_CAN ||
        rx.DataLength > FDCAN_DLC_BYTES_8) return ITM_ERROR_CORRUPT;
    frame->bus = bus;
    frame->id = rx.Identifier;
    frame->is_extended = rx.IdType == FDCAN_EXTENDED_ID;
    frame->length = (uint8_t)rx.DataLength;
    frame->timestamp_ms = HAL_GetTick();
    return ITM_OK;
}
#ifdef ITM_UPDATE_BENCH_CRC_ONLY
itm_result_t itm_update_battery_can_send(itm_dbcc_bus_t bus, uint32_t id,
                                          const uint8_t *data, uint8_t length)
{
    FDCAN_HandleTypeDef *handle = battery_handle(bus);
    FDCAN_TxHeaderTypeDef tx = {0};
    if (!handle || !data || id > 0x7ff || length > 8) return ITM_ERROR_INVALID_ARGUMENT;
    tx.Identifier = id;
    tx.IdType = FDCAN_STANDARD_ID;
    tx.TxFrameType = FDCAN_DATA_FRAME;
    tx.DataLength = length;
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.BitRateSwitch = FDCAN_BRS_OFF;
    tx.FDFormat = FDCAN_CLASSIC_CAN;
    tx.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    return HAL_FDCAN_AddMessageToTxFifoQ(handle, &tx, (uint8_t *)data) == HAL_OK
               ? ITM_OK : ITM_ERROR_IO;
}
#endif
itm_can_port_t itm_update_can_port(void) { return (itm_can_port_t){NULL, send_frame, receive_frame}; }
bool itm_update_can_tx_idle(void) { return HAL_FDCAN_GetTxFifoFreeLevel(&fdcan) == 8; }
void itm_update_watchdog_feed(void) { IWDG1->KR = 0xaaaa; }
void itm_update_watchdog_start(void)
{
    uint32_t guard = 1000000;
    IWDG1->KR = 0xcccc; IWDG1->KR = 0x5555;
    IWDG1->PR = 6; IWDG1->RLR = 4095; /* ~32 s nominal LSI; measure on board. */
    while (IWDG1->SR && --guard) {}
    if (!guard) for (;;) {}
    itm_update_watchdog_feed();
}
void itm_update_jump(uint32_t vector)
{
    unsigned i;
    HAL_FDCAN_Stop(&fdcan);
    itm_board_force_safe_outputs();
    __disable_irq(); SysTick->CTRL = 0; SysTick->LOAD = 0; SysTick->VAL = 0;
    for (i = 0; i < 8; ++i) { NVIC->ICER[i] = 0xffffffff; NVIC->ICPR[i] = 0xffffffff; }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR = vector; __DSB(); __ISB();
    /* No C stack access after changing MSP. Startup re-enables IRQs in main. */
    __asm volatile("ldr r1, [%0]\n ldr r2, [%0, #4]\n msr msp, r1\n bx r2" :: "r"(vector) : "r1", "r2", "memory");
    __builtin_unreachable();
}
