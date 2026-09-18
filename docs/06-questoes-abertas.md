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

- Microcontrolador confirmado: STM32H723VGT6, encapsulamento LQFP100. A pinagem
  Rev00 foi revisada contra o datasheet e o CubeMX, mas ainda requer validacao
  eletrica na placa e aprovacao formal para ser congelada.
- O esquema Rev00 define FDCAN1-3, UART4/5/7, ADC1 e GPIOs. Faltam canais DMA,
  prioridades de interrupcao, timers e watchdog.
- Quais regioes de memoria receberao stacks, buffers DMA, Object Dictionary e
  telemetria, e qual sera a politica de MPU/cache do Cortex-M7?
- Quais saidas fisicas comandam a alimentacao da CEB e do CB?
- Confirmar a nomenclatura funcional das saidas `BOB_CONT_+` e `+28V_CEB`,
  comandadas por `MFET1` e `MFET2`, antes de associa-las definitivamente aos
  estados CB/CEB no firmware.
- Existem leituras de confirmacao de contatores/reles?
- Existem entradas de intertravamento ou emergencia?
- Qual sinal confirma `ceb_power_good`? Afeta `app/power_control` e
  REQ-ITM29/35/36. A decisao provisoria e manter a confirmacao como entrada
  abstrata obrigatoria para completar a energizacao.
- A placa usa definitivamente a alimentacao interna `PWR_LDO_SUPPLY`? Afeta a
  inicializacao de clock em `platform/stm32h723/src/main.c`. A configuracao e
  provisoria e deve ser confirmada contra BOM/layout antes do bring-up.
- Qual e a polaridade logica aprovada de `FAULT1`, `FAULT2`, `MFET1`, `MFET2` e
  `CAN1_TERM`? O port expoe FAULT como nivel eletrico bruto e mantem as saidas
  em nivel baixo no boot; nenhuma semantica operacional foi associada ainda.
- Quais bancos SRAM serao usados alem do DTCM? O linker inicial usa somente
  128 KiB de DTCM. Isso afeta buffers, stacks e futura operacao com DMA/cache.
- Na STM32H735G-DK, o smoke test usa HSE direto a 25 MHz, USART3 a 115200 8N1 e
  FDCAN1 em loopback interno a 250 kbit/s. Esses valores sao exclusivos de
  bancada e nao resolvem nenhuma temporizacao do ITM.
- O FDCAN interno da DK deve ser seguido por teste com o transceptor e conector
  CAN da placa? Isso afeta a evidencia de camada fisica, mas nao deve bloquear o
  primeiro smoke test.

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
- Qual primitiva protegera o store no STM32: secao critica FreeRTOS, mutex ou
  mascaramento de interrupcao? E necessario definir se produtores executam em
  ISR e medir o pior tempo de bloqueio do snapshot.
- Qual e o limite aprovado para duracao da secao critica e quantos sinais podem
  ser copiados por snapshot sem afetar FDCAN/UART?
- O clock de producao, divisores e fonte dos clocks de perifericos ainda devem
  ser definidos. O bring-up usa HSE direto a 8 MHz, sem PLL, como escolha
  conservadora e nao como configuracao final.

## Protocolos

- Qual baud rate e formato da RS-422 com o Banco de Controle?
- Qual baud rate e formato da RS-485 da CEB?
- Confirmar se as interfaces CEB TLM e CEB CMD operam como RS-485 full-duplex
  ou RS-422. O esquema usa transceptores ISOW1432 e pares TX/RX separados, com
  `DE` controlado pelo MCU.
- Quais IDs CAN dos BMS 28 V e 150 V?
- Qual baud rate das redes CAN dos BMS?
- Quais baud rates, sample points, modos FDCAN e temporizacoes UART devem ser
  aplicados no `.ioc`? O CubeMX sugere defaults ao normalizar o arquivo, mas
  eles foram deliberadamente excluidos por nao constituirem requisitos.
- Quais campos, enquadramento, CRC, ACK/NACK e contador de sequencia compoem o
  protocolo BC? Afeta `protocols/bc` e REQ-ITM28/29/30/31; parser permanece
  bloqueado ate receber o ICD.
- Qual formato Skyroot usado pela CEB? Afeta `protocols/ceb` e REQ-ITM26/27;
  parser permanece bloqueado ate receber o ICD.
- Quais objetos e mapeamentos CANopen ECSS devem compor o OD? Afeta
  `services/canopen_ret` e REQ-ITM15-24/34/40. O OD basico foi criado, mas PDOs
  permanecem bloqueados ate receber o ICD da RET.
- Qual Product Code deve ser publicado em `0x1018:02`? Afeta identidade, EDS e
  testes de integracao. O valor permanece zero no EDS e injetavel no firmware.
- Como a revisao semantica do firmware deve ser codificada em `0x1018:03`?
  Provisoriamente usa-se `major << 16 | minor << 8 | patch`, resultando em
  `0x00010200` para 1.2.0.
- Node-ID inicial confirmado: 10. Ainda falta confirmar qual objeto SDO oficial
  o altera; `0x2001` permanece provisorio e o servico aceita a faixa 1..127.
- Qual heartbeat producer time e quais consumidores devem ser configurados?
  Afeta `0x1017`, supervisao de rede e comportamento NMT.
- Quais telemetrias entram em TPDO/RPDO, seus tipos, escalas, COB-IDs, modos de
  transmissao, inhibit times e event timers? Afeta REQ-ITM15/34/40.
- A taxa de 100 Hz de REQ-ITM40 se aplica a todos os TPDOs, a um subconjunto ou
  ao ciclo de aquisicao? Provisoriamente foi exposto periodo nominal de 10 ms em
  objeto fabricante read-only `0x2002`, sem configurar PDO.
- Qual versao ou commit da Lely Core deve ser qualificado? O CANopus nao fixa
  versao e sua integracao real ainda possui placeholders.
- Deve ser usada a Lely upstream ou a variante N7/ECSS mencionada no material
  de referencia? Afeta conformidade ECSS, memoria e manutencao.
- Quais opcoes da Lely serao usadas para operacao sem heap e quais limites
  estaticos serao aprovados? Afeta RAM, determinismo e analise de pior caso.
- Como adaptar `liblely-co` ao STM32 FDCAN, timers e FreeRTOS? O CANopus usa
  SocketCAN/Linux, que nao e reutilizavel no microcontrolador.
- O EDS sera a fonte de verdade e o C gerado por `dcf2c` sera versionado ou
  gerado no build? Afeta reproducibilidade e qualificacao.
- Quais Error Register/EMCY codes e Error Behavior devem representar falhas do
  ITM? Afeta `0x1001`, EMCY e diagnosticos.
- O DDS Micro XRCE e requisito confirmado ou apenas possibilidade de arquitetura?

## Dados

- O ITM deve publicar medidas escaladas ou valor bruto com escala/offset/unidade?
- Qual timestamp deve ser usado para telemetria?
- Como representar validade, stale data e falha de origem?
- Qual granularidade de diagnostico e eventos?
- Qual e a lista definitiva de sinais de AV_BATT, ACT_BATT, CEB e CB? Afeta
  `app/telemetry_catalog`, memoria, testes e mapeamento CANopen.
- Para cada sinal, quais sao tipo bruto, unidade, escala, offset, faixa valida,
  taxa de aquisicao e timeout de stale? Os oito sinais atuais e seus valores sao
  exemplos provisorios centralizados no catalogo.
- Timestamp igual ao anterior representa amostra valida, duplicata ou erro?
  Provisoriamente apenas timestamps estritamente anteriores sao rejeitados.
- Como tratar reset ou salto da base monotonicamente crescente? A comparacao
  atual suporta wrap de 32 bits quando a distancia e menor que `2^31` ms.
- Quais consumidores precisam de historico e qual capacidade de cada ring?
- Qual politica de overflow pertence a cada fluxo: `DROP_OLDEST`,
  `REJECT_NEWEST` ou `SIGNAL_FAULT`?
- O valor anterior deve ser mantido quando uma nova amostra falha na validacao?
  Provisoriamente ele e preservado e apenas o contador de rejeicao aumenta.
- A divisao da normalizacao deve truncar, arredondar ou saturar? Provisoriamente
  a divisao inteira trunca em direcao a zero e overflow e rejeitado.
- O snapshot completo sera publicado em um ciclo ou dividido entre TPDOs com
  diferentes cadencias? O formato interno de 16 bytes por sinal nao define TPDO.

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
- Versoes iniciais validadas: Arm GNU Toolchain 12.2.1, Ninja 1.13.2,
  STM32CubeH7 V1.13.0 e STM32CubeMX 6.18.0-RC3. Definir e qualificar as versoes
  oficiais de producao, especialmente substituir a versao RC do CubeMX.
- A opcao legada `ITM_BUILD_STM32H723` sera removida quando scripts externos
  migrarem para `ITM_HARDWARE=ITM_REV00`.
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
