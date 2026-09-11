# Plano De Desenvolvimento

## Fase 0 - Consolidacao De Requisitos

Objetivo: transformar documentos de referencia em backlog rastreavel.

Entregaveis:

- Requisitos revisados contra o PDF formal `CON-ML-ET-0006A`.
- Lista de ICDs necessarios: Banco de Controle, CEB/Skyroot, BMS AV_BATT, BMS ACT_BATT, Contactor Box, CANopen RET e, se confirmado, DDS.
- Matriz de sinais e conectores CN1-CN4.
- Estados seguros e intertravamentos.
- Criterios de aceitacao por requisito.

## Fase 1 - Fundacao Do Projeto

Objetivo: criar um firmware compilavel, testavel e automatizavel.

Entregaveis:

- Repositorio Git inicializado.
- Estrutura de pastas.
- Toolchain STM32 definida.
- Build local reproduzivel.
- Test runner em host.
- CI local ou remoto.
- Politica de formatacao e analise estatica.

## Fase 2 - Modelo De Dominio Do ITM

Objetivo: implementar as regras centrais sem depender de hardware real.

Entregaveis:

- Maquina de estados.
- Modelo de comandos.
- Modelo de telemetria.
- Modelo de falhas.
- Regras de permissao para energizacao 28 V e 150 V.
- Testes unitarios.

## Fase 3 - Protocolos E Simuladores

Objetivo: validar comunicacao antes da placa.

Entregaveis:

- Parser/encoder RS-422 do Banco de Controle.
- Parser/encoder RS-485 da CEB.
- Receptor de mensagens CAN dos BMS.
- Mapeamento CANopen OD.
- Contratos DDS/IDL, se confirmados.
- Simuladores de BC, CEB e BMS.
- Testes de integracao em host.

## Fase 4 - Port Para STM32G473

Objetivo: levar o firmware para hardware alvo.

Entregaveis:

- Configuracao de clock, GPIO, CAN/FDCAN, UART, DMA, timers e watchdog.
- Camada HAL encapsulada.
- RTOS ou SafeRTOS integrado.
- Drivers de saida para controle de alimentacao.
- Bring-up incremental por periferico.

## Fase 5 - Integracao Hardware-In-The-Loop

Objetivo: validar comportamento com equipamentos reais ou bancadas.

Entregaveis:

- Testes HIL para energizacao/desenergizacao.
- Testes de perda de comunicacao.
- Testes de timeouts.
- Testes de falha de BMS/CEB/BC.
- Captura de evidencias por requisito.

## Fase 6 - Release E Qualificacao

Objetivo: entregar firmware controlado e reproduzivel.

Entregaveis:

- Build assinado ou identificado.
- Artefatos versionados.
- Release notes.
- Mapa de requisitos verificados.
- Procedimento de gravacao.
- Procedimento de rollback/recuperacao, se aplicavel.

## Backlog Inicial

| Prioridade | Item |
| --- | --- |
| P0 | Transformar REQ-ITM 1 a REQ-ITM71 em matriz de rastreabilidade com criterios e testes. |
| P0 | Confirmar ICDs disponiveis e mensagens de cada interface. |
| P0 | Definir toolchain: STM32CubeIDE/CMake, compilador, RTOS, testes. |
| P0 | Definir estados seguros para 28 V e 150 V. |
| P0 | Definir regra completa de perda de comunicacao com o BC e temporizacao das 3 tentativas. |
| P1 | Criar esqueleto compilavel do firmware. |
| P1 | Implementar maquina de estados em host com testes. |
| P1 | Criar simulador de Banco de Controle RS-422. |
| P1 | Criar simulador CEB RS-485. |
| P1 | Criar simulador BMS CAN. |
| P2 | Integrar CANopen e gerar EDS/DCF/OD. |
| P2 | Confirmar necessidade de DDS e, se aplicavel, definir/gerar contratos IDL. |
| P2 | Criar pipeline de evidencias de teste. |
