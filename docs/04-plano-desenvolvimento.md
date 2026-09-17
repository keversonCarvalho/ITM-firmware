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

Estado: parcialmente concluida. Estrutura CMake, nucleo C11, portas e testes de
host foram criados. A suite foi compilada com GCC 11.4 no WSL com warnings como
erro e os 13 testes passaram. Falta integrar analise estatica/formatacao
automatizada e disponibilizar a toolchain ARM.

## Fase 2 - Modelo De Dominio Do ITM

Objetivo: implementar as regras centrais sem depender de hardware real.

Entregaveis:

- Maquina de estados.
- Modelo de comandos.
- Modelo de telemetria.
- Modelo de falhas.
- Regras de permissao para energizacao 28 V e 150 V.
- Testes unitarios.

Estado: primeira iteracao implementada para supervisao BC, monitor CB, controle
de potencia, diagnosticos e persistencia. Os pontos dependentes de ICD e hardware
continuam configuraveis e constam em `06-questoes-abertas.md`.

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

- Build identificado com a versao do firmware e dos contratos associados.
- Artefatos versionados.
- Release notes.
- Mapa de requisitos verificados.
- Procedimento de gravacao.

Nesta fase nao havera bootloader nem assinatura/verificacao de imagem. A gravacao sera feita diretamente por ferramenta de desenvolvimento. Bootloader e assinatura/verificacao de imagem ficam registrados como implementacoes futuras do projeto.

### Versionamento De Artefatos

EDS, DCF, OD e IDL serao versionados junto com o firmware usando versionamento semantico. A versao inicial registrada para os artefatos e:

| Artefato | Versao |
| --- | --- |
| Firmware | 1.2.0 |
| Protocolo | 1.0.0 |
| OD | 1.1.0 |
| EDS | 1.1.0 |
| DCF | 1.1.0 |
| IDL | 1.0.0 |

Alteracoes incompativeis incrementam MAJOR; novos recursos compativeis incrementam MINOR; correcoes compativeis incrementam PATCH.

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
