$ErrorActionPreference = 'Stop'

$iocPath = Join-Path $PSScriptRoot '..\platform\stm32h723\itm_stm32h723.ioc'
$boardPath = Join-Path $PSScriptRoot '..\platform\stm32h723\include\itm_board.h'
$ioc = Get-Content -Raw -LiteralPath $iocPath
$board = Get-Content -Raw -LiteralPath $boardPath

$iocExpectations = [ordered]@{
    'Mcu.CPN=STM32H723VGT6' = 'STM32H723VGT6 part number'
    'PA13(JTMS/SWDIO).Signal=DEBUG_JTMS-SWDIO' = 'SWD data'
    'PA14(JTCK/SWCLK).Signal=DEBUG_JTCK-SWCLK' = 'SWD clock'
    'PA11.Signal=FDCAN1_RX' = 'FDCAN1 RX'
    'PA12.Signal=FDCAN1_TX' = 'FDCAN1 TX'
    'PB12.Signal=FDCAN2_RX' = 'FDCAN2 RX'
    'PB13.Signal=FDCAN2_TX' = 'FDCAN2 TX'
    'PD12.Signal=FDCAN3_RX' = 'FDCAN3 RX'
    'PD13.Signal=FDCAN3_TX' = 'FDCAN3 TX'
    'PA0.Signal=UART4_TX' = 'UART4 TX'
    'PA1.Signal=UART4_RX' = 'UART4 RX'
    'PC12.Signal=UART5_TX' = 'UART5 TX'
    'PD2.Signal=UART5_RX' = 'UART5 RX'
    'PE7.Signal=UART7_RX' = 'UART7 RX'
    'PE8.Signal=UART7_TX' = 'UART7 TX'
    'PA6.Signal=ADC1_INP3' = 'ADC1 channel 3 positive'
    'PA7.Signal=ADC1_INN3' = 'ADC1 channel 3 negative'
    'PC4.Signal=ADC1_INP4' = 'ADC1 channel 4 positive'
    'PC5.Signal=ADC1_INN4' = 'ADC1 channel 4 negative'
}

if ($ioc.Contains('RCC_LSE')) {
    throw 'Pin map mismatch: LSE must remain disabled because PC14 is FAULT2'
}

$boardExpectations = [ordered]@{
    'ITM_MFET1_PORT GPIOE' = 'MFET1 port'
    'ITM_MFET1_PIN GPIO_PIN_2' = 'MFET1 pin'
    'ITM_MFET2_PORT GPIOE' = 'MFET2 port'
    'ITM_MFET2_PIN GPIO_PIN_3' = 'MFET2 pin'
    'ITM_CAN1_TERM_PIN GPIO_PIN_4' = 'CAN1 termination pin'
    'ITM_FAULT1_PIN GPIO_PIN_13' = 'FAULT1 pin'
    'ITM_FAULT2_PIN GPIO_PIN_14' = 'FAULT2 pin'
    'ITM_UART4_DE_PORT GPIOB' = 'UART4 DE port'
    'ITM_UART4_DE_PIN GPIO_PIN_14' = 'UART4 DE pin'
    'ITM_UART5_DE_PORT GPIOC' = 'UART5 DE port'
    'ITM_UART5_DE_PIN GPIO_PIN_8' = 'UART5 DE pin'
    'ITM_LED_PIN GPIO_PIN_6' = 'LED pin'
}

foreach ($entry in $iocExpectations.GetEnumerator()) {
    if (-not $ioc.Contains($entry.Key)) {
        throw "Pin map mismatch: $($entry.Value)"
    }
}
foreach ($entry in $boardExpectations.GetEnumerator()) {
    if (-not $board.Contains($entry.Key)) {
        throw "Board definition mismatch: $($entry.Value)"
    }
}

Write-Output 'STM32H723 pin map verification passed.'
