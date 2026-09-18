$ErrorActionPreference = 'Stop'

$iocPath = Join-Path $PSScriptRoot '..\platform\stm32h735g_dk\stm32h735g_dk.ioc'
$boardPath = Join-Path $PSScriptRoot '..\platform\stm32h735g_dk\include\itm_dk_board.h'
$ioc = Get-Content -Raw -LiteralPath $iocPath
$board = Get-Content -Raw -LiteralPath $boardPath

$iocExpectations = [ordered]@{
    'Mcu.CPN=STM32H735IGK6' = 'STM32H735IGK6 part number'
    'PC3.GPIO_Label=LED1_OK' = 'LED1 PC3'
    'PC2.GPIO_Label=LED2_ERROR' = 'LED2 PC2'
    'PC13.GPIO_Label=USER_BUTTON' = 'user button PC13'
    'PD8.Signal=USART3_TX' = 'VCP USART3 TX'
    'PD9.Signal=USART3_RX' = 'VCP USART3 RX'
    'RCC.HSE_VALUE=25000000' = '25 MHz HSE'
}

$boardExpectations = [ordered]@{
    'ITM_DK_LED_OK_PIN GPIO_PIN_3' = 'LED1 board pin'
    'ITM_DK_LED_ERROR_PIN GPIO_PIN_2' = 'LED2 board pin'
    'ITM_DK_BUTTON_PIN GPIO_PIN_13' = 'button board pin'
    'ITM_DK_VCP_TX_PIN GPIO_PIN_8' = 'VCP TX board pin'
    'ITM_DK_VCP_RX_PIN GPIO_PIN_9' = 'VCP RX board pin'
}

foreach ($entry in $iocExpectations.GetEnumerator()) {
    if (-not $ioc.Contains($entry.Key)) {
        throw "STM32H735G-DK pin map mismatch: $($entry.Value)"
    }
}
foreach ($entry in $boardExpectations.GetEnumerator()) {
    if (-not $board.Contains($entry.Key)) {
        throw "STM32H735G-DK board mismatch: $($entry.Value)"
    }
}

Write-Output 'STM32H735G-DK pin map verification passed.'
