# Questoes Abertas

Cada item abaixo bloqueia uma decisao definitiva. Valores provisorios devem ser
fornecidos por configuracao de teste e nao promovidos para o alvo sem revisao.

## Documentacao

- Existem ICDs formais do Banco de Controle, CEB/Skyroot, BMS, Contactor Box e RET/CANopen?
- Existem esquematicos eletricos, pinout CN1-CN4 e lista de sinais?
- Qual documento define a tabela verdade oficial de EM1/EM2 para CB ligado,
  desligado e em falha? Afeta `app/cb_monitor` e REQ-ITM10/37/38/39/59. A
  decisao provisoria e injetar a tabela esperada por configuracao.

## Hardware

- Qual variante exata do STM32G473 sera usada?
- Quais perifericos serao usados: FDCAN, USART, DMA, timers, ADCs, GPIOs?
- Quais saidas fisicas comandam a alimentacao da CEB e do CB?
- Existem leituras de confirmacao de contatores/reles?
- Existem entradas de intertravamento ou emergencia?
- Qual sinal confirma `ceb_power_good`? Afeta `app/power_control` e
  REQ-ITM29/35/36. A decisao provisoria e manter a confirmacao como entrada
  abstrata obrigatoria para completar a energizacao.

## Tempo Real

- Quais sao os deadlines para comandos do Banco de Controle?
- Qual periodo de publicacao de telemetria?
- Quais timeouts de comunicacao por interface?
- Qual intervalo entre as 3 tentativas de restabelecimento de comunicacao com o BC?
- As "3 tentativas" significam tres periodos sem frame, tres requisicoes ativas
  ou outro mecanismo? Afeta `services/bc_supervisor` e REQ-ITM31. A decisao
  provisoria e contar periodos de supervisao perdidos, com periodo e limite
  configuraveis.
- Quais prioridades de tarefas RTOS?

## Protocolos

- Qual baud rate e formato da RS-422 com o Banco de Controle?
- Qual baud rate e formato da RS-485 da CEB?
- Quais IDs CAN dos BMS 28 V e 150 V?
- Qual baud rate das redes CAN dos BMS?
- Qual Node-ID do ITM?
- Quais campos, enquadramento, CRC, ACK/NACK e contador de sequencia compoem o
  protocolo BC? Afeta `protocols/bc` e REQ-ITM28/29/30/31; parser permanece
  bloqueado ate receber o ICD.
- Qual formato Skyroot usado pela CEB? Afeta `protocols/ceb` e REQ-ITM26/27;
  parser permanece bloqueado ate receber o ICD.
- Quais objetos e mapeamentos CANopen ECSS devem compor o OD? Afeta
  `services/canopen_ret` e REQ-ITM15-24/34/40; a interface permanece abstrata.
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
- A perda do BC deve aceitar recuperacao automatica ou exigir comando/reset?
  Afeta `services/bc_supervisor` e `app/power_control`. Provisoriamente, o link
  pode voltar a conectado, mas a falha de potencia exige limpeza explicita.

## Build E Release

- Toolchain preferida: CMake + Ninja + arm-none-eabi-gcc, usando o STM32CubeMX apenas para gerar/configurar os arquivos iniciais do STM32.
- RTOS preferido: FreeRTOS
- Bootloader: implementacao futura; nao previsto nesta fase, com gravacao direta por ferramenta de desenvolvimento.
- Assinatura/verificacao de imagem: implementacao futura; nao prevista nesta fase.
- Versionamento dos artefatos: usar versionamento semantico para firmware, protocolo, OD, EDS, DCF e IDL.
	- Firmware: 1.2.0
	- Protocolo: 1.0.0
	- OD: 1.1.0
	- EDS: 1.1.0
	- DCF: 1.1.0
	- IDL: 1.0.0

## Persistencia

- Qual regiao, pagina, alinhamento e endurance da memoria de parametros? Afeta
  `services/persistence` e REQ-ITM6/7/14/18/20/23/24. Provisoriamente, o servico
  usa dois slots abstratos com CRC e marcador de commit.
- Quais parametros sao persistentes, seus valores padrao, faixas e politica de
  migracao? Afeta o mesmo servico e o Object Dictionary CANopen.
