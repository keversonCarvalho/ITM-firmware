# ADR 0006 - STM32H723VGT6 Como Alvo

## Estado

Aceita.

## Decisao

O firmware embarcado do ITM tera como alvo o STM32H723VGT6, Arm Cortex-M7 em
encapsulamento LQFP100. O nucleo portatil C11 e seus testes de host permanecem
independentes do microcontrolador.

Os adaptadores de hardware ficarao em uma camada STM32 separada. A configuracao
de CubeMX, HAL, FreeRTOS, linker e startup nao deve introduzir dependencias no
nucleo portatil.

## Consequencias

- O alvo disponibiliza tres controladores FDCAN, coerentes com a necessidade de
  tres redes CAN do ITM.
- O port deve tratar explicitamente MPU, cache de dados e coerencia de buffers
  acessados por DMA.
- O mapa de memoria deve separar codigo, parametros persistentes, stacks e
  buffers de comunicacao.
- A alocacao de pinos e perifericos permanece bloqueada ate o recebimento do
  esquematico ou pinout aprovado.
- Clock, erratas aplicaveis e configuracoes de alimentacao devem ser revisados
  contra a revisao vigente do datasheet e do reference manual antes do bring-up.

## Referencias

- STMicroelectronics DS13313, datasheet do STM32H723xE/G.
- STMicroelectronics RM0468, reference manual da familia STM32H723/733.
