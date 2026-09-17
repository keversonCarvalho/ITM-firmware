# Matriz De Rastreabilidade Inicial

Esta matriz e o ponto de partida para desenvolvimento assistido por IA. Ela nao substitui a matriz formal de verificacao do projeto detalhado, mas cria uma ponte entre requisitos, modulos de firmware e testes.

## Modulos De Firmware Por Requisito

| Modulo | Requisitos de entrada | Testes minimos |
| --- | --- | --- |
| `platform/stm32g473` | REQ-ITM 4, 5, 11, 12, 13 | Build alvo, mapa de perifericos, teste de clock/UART/CAN, prova de capacidade por simulacao de carga. |
| `services/persistence` | REQ-ITM 6, 7, 14, 18, 20, 23, 24 | Gravacao independente, power-fail simulado, CRC de parametros, restauracao no boot, incremento de versao. |
| `services/canopen_ret` | REQ-ITM 15, 16, 17, 18, 19, 21, 24, 34, 40 | SDO read/write, persistencia, Node-ID, OD read-only, PDO/telemetria, heartbeat, taxa 500 kbit/s. |
| `services/bms` | REQ-ITM 25 | Recepcao CAN valida, timeout, stale data, escala de sinais, falha de BMS e recuperacao. |
| `services/ceb` | REQ-ITM 26, 27 | Parser RS-485, telemetria CEB, comandos Skyroot, erros de CRC, timeouts e comandos nao permitidos. |
| `services/bc` | REQ-ITM 28, 29, 30, 31 | Parser RS-422, comandos de SSR, comandos de teste, perda de comunicacao, 3 tentativas e desligamento seguro. |
| `app/power_control` | REQ-ITM 8, 9, 29, 31, 35, 36 | Sequenciamento CEB/CB, falhas de SSR, desligamento em falha, intertravamentos e eventos. |
| `app/cb_monitor` | REQ-ITM10, 37, 38, 39, 59 | Conversao ADC para booleano, zona indeterminada, debounce, tabela verdade de falhas e diagnosticos. |
| `app/telemetry_router` | REQ-ITM 15, 25, 26, 34, 40 | Agregacao CEB/BMS, validade, publicacao CANopen/RET, frequencia 100 Hz quando aplicavel. |
| `app/diagnostics` | REQ-ITM 31, 39, 60, 61, 67, 68, 70 | Eventos de falha, contadores, FMEA inicial, estados seguros e evidencias para ensaios. |
| `app/telemetry_catalog` | REQ-ITM15, 25, 26, 40 | Catalogo estatico, tipos, origens, escalas, faixas e freshness por sinal. |
| `services/telemetry_store` | REQ-ITM13, 15, 25, 26, 40 | Concorrencia, validacao, stale, snapshot consistente, serializacao e memoria. |
| `services/telemetry_ring` | REQ-ITM13, 40 | Ordem FIFO, overflow e politicas de perda por fluxo. |

## Estado Da Primeira Implementacao

| Item | Estado | Evidencia atual | Pendencia para conclusao |
| --- | --- | --- | --- |
| `ports/*` | Implementado no host | Contratos C para clock, GPIO, flash, CAN e serial; mocks; mapa Rev00 levantado | Adaptadores STM32, DMA, IRQ e configuracao CubeMX |
| `services/bc_supervisor` | Parcial | Testes aprovados de perda, bloqueio e recuperacao | Semantica e temporizacao oficiais das 3 tentativas |
| `app/cb_monitor` | Parcial | Testes aprovados de limiar, zona indeterminada, debounce e tabela configuravel | Tabela verdade oficial e periodo de amostragem |
| `app/power_control` | Parcial | Testes aprovados de sequenciamento e perda de permissao | Estados seguros, intertravamentos, realimentacoes e tempos oficiais |
| `services/diagnostics` | Parcial | Teste aprovado de fila circular e falha bloqueante | Catalogo, severidades, persistencia e politica de limpeza oficiais |
| `services/persistence` | Parcial | Testes aprovados de duas geracoes e gravacao interrompida | Layout de flash, defaults, migracao e ensaio real de power-fail |
| Protocolos BC/CEB/BMS | Bloqueado | Somente interfaces e mocks de transporte | ICDs e bases de tempo |
| CANopen RET | Parcial | Lely selecionada; EDS, identidade `0x1018`, SDO de configuracao, persistencia e testes | Port Lely/FDCAN, Product Code, Node-ID, heartbeat, PDOs e perfil ECSS detalhado |
| Pipeline de telemetria | Parcial | Catalogo com 8 sinais provisorios, normalizacao inteira, store, snapshot CANopen, ring e testes | ICDs, catalogo final, politica por fluxo, TPDOs e medicao temporal no STM32 |
| STM32H723VGT6/FreeRTOS | Parcial | MCU, encapsulamento e pinagem Rev00 documentados; arquitetura preparada para adaptadores | Aprovacao do esquema, CubeMX, toolchain ARM, DMA/IRQ, mapa de memoria/cache e escalonamento |

`Implementado no host` nao significa qualificado para voo. A conclusao de cada
requisito depende das evidencias de integracao, HIL e qualificacao previstas no
plano de desenvolvimento.

## Primeiras Historias De Desenvolvimento

| ID | Historia | Requisitos |
| --- | --- | --- |
| FW-001 | Como integrador, quero compilar o firmware em host e no alvo para validar a base do projeto. | REQ-ITM 4, 5, 12, 13 |
| FW-002 | Como firmware, quero persistir parametros em flash sem afetar a imagem do programa. | REQ-ITM 6, 7, 14 |
| FW-003 | Como integrador CANopen, quero configurar Node-ID e parametros via SDO e persisti-los. | REQ-ITM 16, 17, 18, 24 |
| FW-004 | Como operador do BC, quero comandar energizacao da CEB e CB via protocolo RS-422. | REQ-ITM 28, 29, 35, 36 |
| FW-005 | Como sistema, quero desligar SSRs se perder comunicacao com o BC apos 3 tentativas. | REQ-ITM 31 |
| FW-006 | Como sistema, quero detectar falhas do CB a partir de EM1/EM2 e comando aplicado. | REQ-ITM 37, 38, 39, 59 |
| FW-007 | Como telemetria, quero publicar dados de BMS e CEB para a RET via CANopen. | REQ-ITM 15, 25, 26, 27, 34, 40 |

## Testes Automatizados Mapeados

| Teste | Requisitos relacionados | Estado |
| --- | --- | --- |
| `test_bc_supervisor_*` | REQ-ITM31 | Aprovado no host |
| `test_cb_monitor_*` | REQ-ITM10/37/38/39/59 | Aprovado no host |
| `test_power_control_*` | REQ-ITM29/31/35/36 | Aprovado no host |
| `test_persistence_*` | REQ-ITM6/7/14/18/20/23/24 | Aprovado no host |
| `test_diagnostics_*` | REQ-ITM31/39/67/68/70 | Aprovado no host |
| `test_canopen_*` | REQ-ITM16/17/18/19/21/24 | Aprovado no host |
| `test_telemetry_*` | REQ-ITM13/15/25/26/40 | 10 testes aprovados no host, incluindo memoria e overflow |

## Evidencias Esperadas

- Log de execucao de testes unitarios em host.
- Log de testes de integracao com simuladores RS-422, RS-485 e CAN.
- Capturas de barramento CAN/CANopen.
- Relatorio de cobertura por requisito.
- Checklist de bring-up por periferico.
- Evidencias HIL para acionamento/desligamento de CEB e CB.
