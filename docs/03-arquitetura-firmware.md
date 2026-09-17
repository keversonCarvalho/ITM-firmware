# Arquitetura De Firmware

## Visao Geral

A arquitetura recomendada e em camadas, com regra simples: codigo dependente de hardware deve ficar nas bordas; regras do ITM devem ficar no centro e ser testaveis em PC.

```text
app/
  itm_state_machine
  power_control
  telemetry_router
  diagnostics

services/
  command_service
  ceb_telemetry_service
  bms_service
  canopen_service
  dds_service
  persistence_service

protocols/
  bc_rs422_protocol
  ceb_rs485_protocol
  skyroot_protocol
  canopen_od_mapping
  idl_contracts

drivers/
  relay_or_contactor_driver
  uart_driver
  can_driver
  gpio_driver
  watchdog_driver
  storage_driver

platform/
  stm32g473_hal
  rtos_port
  board_config

tests/
  unit
  integration
  hil
```

## Componentes Principais

### Maquina De Estados Do ITM

Responsavel por coordenar o ciclo de vida do equipamento:

- Inicializacao.
- Autoteste.
- Pronto/desenergizado.
- CEB 28 V energizada.
- Drivers 150 V habilitados.
- Operacao normal.
- Falha recuperavel.
- Falha travada.
- Modo manutencao, se aplicavel.

Todas as transicoes criticas devem ter pre-condicoes explicitas, timeout e evento de diagnostico.
Em especial, comandos externos do BC so devem acionar SSRs apos validacao de protocolo, estado operacional permitido e ausencia de falhas bloqueantes.

### Controle De Alimentacao

Responsavel por comandos fisicos e intertravamentos:

- Acionamento da alimentacao da CE Box em 28 V.
- Acionamento da Contactor Box para 150 V.
- Sequenciamento de ligamento/desligamento.
- Confirmacao do CB por duas entradas analogicas convertidas para `EM1` e `EM2`.
- Debounce e tabela verdade de deteccao de erro do CB.
- Retorno a estado seguro em falha.

### Servico De Comandos RS-422

Responsavel por receber comandos do Banco de Controle:

- Decodificacao de frames.
- Validacao de CRC/checksum, sequencia e permissao.
- Conversao para comandos internos.
- Resposta/ack/nack, se especificado no ICD.
- Deteccao de perda de comunicacao; apos 3 tentativas sem restabelecimento, o controle de alimentacao deve desligar os SSRs.

### Servico De Telemetria CEB RS-485

Responsavel por:

- Leitura do monitoramento da CEB.
- Normalizacao dos dados.
- Publicacao para roteador de telemetria.
- Conversao para objetos CANopen quando aplicavel.

### Servico BMS CAN

Responsavel por:

- Recepcao de telemetria dos BMS de 28 V e 150 V.
- Temperaturas.
- Estado de carga.
- Tensao total.
- Tensao por celula.
- Diagnosticos de comunicacao e validade dos dados.
- Suporte a CAN basico ou CANopen, conforme especificacao dos fornecedores das baterias.

### Servico CANopen

Responsavel por:

- Pilha CANopen.
- EDS/DCF/Object Dictionary.
- Heartbeat e Node-ID.
- Mapeamento de telemetrias para objetos.
- Persistencia de configuracao.
- Objetos read-only para numero de serie, versao de firmware e versao de configuracao.
- Interface da RET em CAN 2.0A, identificador 11 bits e 500 kbit/s.

Primeira implementacao:

- Lely Core (`liblely-co`) selecionada, em alinhamento com o CANopus.
- ITM atua como dispositivo/slave; CANopus atua como mestre/gateway de bancada.
- Object Dictionary inicial em `config/canopen/itm-100.eds`.
- Identidade `0x1018` usa Vendor-ID Concert Space `0x000006A9`.
- Node-ID e heartbeat producer configuraveis por SDO e persistidos por `0x1010`.
- Telemetria RET fixada em 10 ms apenas como periodo nominal derivado de 100 Hz;
  objetos e PDOs permanecem bloqueados pelo ICD.
- Adaptacao de CAN frames, timers e NMT da Lely para FDCAN/FreeRTOS fica restrita
  a futura camada `platform/lely`.

### Servico DDS

Responsavel por comunicacao DDS Micro XRCE, se confirmada:

- Contratos IDL.
- Serializacao/deserializacao.
- Publicacao de telemetria.
- Recepcao de comandos ou configuracoes, se aplicavel.

## Politica De Dados

Uma decisao de arquitetura ainda esta aberta: escalar as medidas no ITM ou transmitir valor bruto com fator de escala, offset e unidade de engenharia.

Recomendacao inicial:

- Internamente, manter valor bruto e metadados de escala quando vierem do protocolo de origem.
- Para telemetria operacional, publicar valor ja convertido em unidade de engenharia.
- Para diagnostico, permitir acesso ao valor bruto.
- Registrar unidade, escala, offset, origem, timestamp e validade em um contrato unico de telemetria.

## Concorrencia E RTOS

O requisito formal pede software multitarefas capaz de tratar todas as interfaces simultaneamente nas taxas maximas. Isso favorece RTOS, filas e DMA, mas a escolha final entre RTOS e SafeRTOS deve ser tomada no projeto detalhado.

Sugestao inicial de tarefas:

| Tarefa | Responsabilidade |
| --- | --- |
| `task_supervisor` | Maquina de estados, watchdog, falhas criticas |
| `task_bc_comm` | Comunicacao RS-422 com Banco de Controle |
| `task_ceb_comm` | Comunicacao RS-485 com CEB |
| `task_bms_can` | Recepcao CAN dos BMS |
| `task_canopen` | Pilha CANopen e OD |
| `task_telemetry` | Agregacao, normalizacao e publicacao |
| `task_diagnostics` | Eventos, contadores e persistencia |

O desenho final deve depender dos prazos reais, prioridades, interrupcoes, DMA e memoria disponivel.

## Principios De Implementacao

- Nao colocar regra de negocio em callbacks de interrupcao.
- Usar filas/eventos para atravessar fronteiras entre ISR, drivers e servicos.
- Validar todo comando externo antes de afetar saidas fisicas.
- Fazer fail-safe por padrao.
- Separar protocolos de transporte fisico.
- Tornar a aplicacao executavel em host para testes.
- Versionar contratos: EDS, DCF, OD, IDL e ICDs.
