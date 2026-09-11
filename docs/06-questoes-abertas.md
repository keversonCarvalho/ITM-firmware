# Questoes Abertas

## Documentacao

- Existem ICDs formais do Banco de Controle, CEB/Skyroot, BMS, Contactor Box e RET/CANopen?
- Existem esquematicos eletricos, pinout CN1-CN4 e lista de sinais?

## Hardware

- Qual variante exata do STM32G473 sera usada?
- Quais perifericos serao usados: FDCAN, USART, DMA, timers, ADCs, GPIOs?
- Quais saidas fisicas comandam a alimentacao da CEB e do CB?
- Existem leituras de confirmacao de contatores/reles?
- Existem entradas de intertravamento ou emergencia?

## Tempo Real

- Quais sao os deadlines para comandos do Banco de Controle?
- Qual periodo de publicacao de telemetria?
- Quais timeouts de comunicacao por interface?
- Qual intervalo entre as 3 tentativas de restabelecimento de comunicacao com o BC?
- Quais prioridades de tarefas RTOS?

## Protocolos

- Qual baud rate e formato da RS-422 com o Banco de Controle?
- Qual baud rate e formato da RS-485 da CEB?
- Quais IDs CAN dos BMS 28 V e 150 V?
- Qual baud rate das redes CAN dos BMS?
- Qual Node-ID do ITM?
- O DDS Micro XRCE e requisito confirmado ou apenas possibilidade de arquitetura?

## Dados

- O ITM deve publicar medidas escaladas ou valor bruto com escala/offset/unidade?
- Qual timestamp deve ser usado para telemetria?
- Como representar validade, stale data e falha de origem?
- Qual granularidade de diagnostico e eventos?

## Seguranca E Falhas

- Qual e o estado seguro de cada saida?
- O que deve acontecer em perda de Banco de Controle?
- O que deve acontecer em perda da CEB?
- O que deve acontecer em perda de BMS 28 V ou 150 V?
- Quais falhas sao recuperaveis e quais exigem reset/intervencao?

## Build E Release

- Toolchain preferida: STM32CubeIDE, CMake, Make, Ninja, IAR, Keil ou outra?
- RTOS preferido: FreeRTOS, SafeRTOS ou bare metal?
- Havera bootloader?
- Havera assinatura/verificacao de imagem?
- Como serao versionados EDS, DCF, OD e IDL?
