# Visao Do Projeto

## Objetivo

Desenvolver do zero o firmware do ITM-100-1.0 Mx, modulo que integra o Sistema de Tubeira Movel ao Banco de Controle e a Rede Eletrica de Telemetria.

O firmware deve controlar e supervisionar a alimentacao do sistema, converter e encaminhar telemetrias entre interfaces heterogeneas, receber comandos do Banco de Controle e expor dados por protocolos adequados ao sistema.

## Escopo Inicial Inferido

O ITM-100 possui dois blocos funcionais principais:

1. Alimentacao
   - Alimentacao exclusiva pela bateria de 28 V do sistema de tubeira movel.
   - Controle da alimentacao da CE Box em 28 V.
   - Comando da Contactor Box para alimentacao de 150 V.
   - Controle individual da energizacao do modulo de controle e do modulo de potencia da CEB.

2. Comunicacao
- Conversao/publicacao da telemetria da CEB para a RET via CANopen.
   - Recepcao de dados dos BMS das baterias de 28 V e 150 V via CAN.
   - Recepcao de monitoramento da CEB via RS-485.
   - Envio de informacoes para o Banco de Controle.
   - Recepcao de comandos do Banco de Controle via RS-422 para ligar/desligar CEB e drivers dos atuadores.

## Interfaces Conhecidas

| Interface | Origem/Destino | Papel |
| --- | --- | --- |
| 28 V | Bateria / ITM / CE Box | Alimentacao e controle do modulo de controle |
| 125/150 V | ACT_BATT / Contactor Box / CEB | Alimentacao de potencia dos atuadores, com valor exato a confirmar entre especificacao e apresentacao |
| RS-422 | Banco de Controle / ITM | Comandos de controle |
| RS-485 | CEB / ITM | Telemetria e monitoramento da CEB |
| CAN BMS 28 V | BMS / ITM | Telemetria da bateria 28 V |
| CAN BMS 150 V | BMS / ITM | Telemetria da bateria 150 V |
| CANopen RET | ITM / Rede Eletrica de Telemetria | Publicacao/configuracao de dados padronizados, CAN 2.0A, 11 bits, 500 kbit/s |
| DDS Micro XRCE | ITM / rede superior, a confirmar | Comunicacao por contrato IDL |

## Premissas Tecnicas

- Microcontrolador alvo confirmado: STM32H723VGT6 (Arm Cortex-M7, encapsulamento
  LQFP100).
- Camada de baixo nivel baseada em STM32 HAL.
- Uso de RTOS ou SafeRTOS ainda a decidir.
- Pilha CANopen candidata: N7 CANopen, citada na apresentacao, desde que cubra ECSS-E-ST-50-15C e CiA 301.
- Possivel uso de LwIP e Micro XRCE-DDS aparece na apresentacao, mas nao foi identificado como requisito formal do PDF.
- Protocolo Skyroot e protocolo do Banco de Controle precisam de ICDs formais.

## Definicao De Pronto Do Firmware

O firmware so deve ser considerado pronto quando houver:

- Requisitos rastreados ao documento CON-ML-ET-0006A e demais ICDs aplicaveis.
- Testes unitarios das regras de aplicacao e protocolos.
- Testes de integracao com simuladores de interfaces.
- Testes HIL com hardware real.
- Analise de falhas para energizacao 28 V e 150 V.
- Evidencias de validacao por requisito.
- Procedimento reproduzivel de build, flash, debug e release.
