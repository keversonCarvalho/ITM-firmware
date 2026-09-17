# ADR 0007 - Bring-Up Bare-Metal Conservador

## Estado

Aceita para a primeira etapa de bring-up.

## Decisao

O STM32H723VGT6 inicia sem RTOS, sem DMA e sem habilitar interfaces de
comunicacao. O HSE de 8 MHz alimenta diretamente o SYSCLK, sem PLL. As saidas de
potencia, habilitacao de transmissores e terminacao CAN recebem nivel inativo no
registrador de saida antes de seus GPIOs mudarem para modo output.

CMSIS e HAL vem do pacote oficial STM32CubeH7 V1.13.0 e ficam isolados em
`platform/stm32h723`. O linker usa 1 MiB de flash e, inicialmente, apenas os
128 KiB de DTCM. Heap minimo do linker e zero; o projeto nao usa alocacao
dinamica.

## Consequencias

- O LED ativo em nivel baixo indica vida por alternancia nao bloqueante.
- `Error_Handler` e asserts levam imediatamente as saidas ao estado de boot
  seguro e mantem o LED aceso.
- FAULT1/2 sao expostos somente como niveis eletricos brutos ate a polaridade
  funcional ser aprovada.
- ADC, FDCAN, UART e flash permanecem stubs explicitamente indisponiveis.
- O clock de 8 MHz e adequado para bring-up, mas nao prova capacidade temporal.
- MPU, cache e outros bancos SRAM devem ser definidos antes de qualquer DMA.

## Alternativas Rejeitadas Nesta Etapa

- Operar imediatamente na frequencia maxima.
- Aceitar baud rates e temporizacoes sugeridos automaticamente pelo CubeMX.
- Integrar FreeRTOS ou CANopen antes da validacao eletrica basica.
