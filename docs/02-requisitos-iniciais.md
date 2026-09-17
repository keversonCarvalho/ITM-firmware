# Requisitos Iniciais

Esta versao foi revisada contra o documento formal `CON-ML-ET-0006A`, revisao A, de 30/06/2026, e complementada pela apresentacao `Interface Tubeira Movel-Firmware V1_0.pptx`.

## Requisitos Formais Mais Relevantes Para Firmware

| ID | Tema | Impacto direto no firmware |
| --- | --- | --- |
| REQ-ITM 4 | Microcontrolador | O alvo deve possuir 3 controladores CAN integrados e 3 UARTs aptas a implementar RS-422/RS-485. |
| REQ-ITM 5 | Capacidade do MCU | O firmware deve sustentar 3 instancias CANopen ate 1 Mbps e 3 protocolos seriais assincronos ate 115200 bps. |
| REQ-ITM 6 | Flash | A memoria nao volatil deve separar area de firmware e area de parametros. |
| REQ-ITM 7 | Gerenciamento de flash | Gravacao de firmware e parametros deve ocorrer sem corromper a outra area. |
| REQ-ITM 8 | SSR com diagnostico | O firmware deve monitorar protecoes/diagnosticos dos MOSFETs inteligentes quando expostos ao MCU. |
| REQ-ITM10 | Monitoramento do CB | Devem existir entradas analogicas para verificar atuacao do Contactor Box. |
| REQ-ITM11 | Programacao/debug | O projeto deve preservar caminho de gravacao e depuracao do firmware. |
| REQ-ITM12 | Linguagem | Firmware em ANSI C/C++. |
| REQ-ITM13 | Multitarefas | O software deve tratar simultaneamente todas as interfaces sem perda de pacotes nas taxas maximas. |
| REQ-ITM14 | Alocacao NVM | Parametros e programa devem ter gravacao independente. |
| REQ-ITM15 | RET | Implementar comunicacao com a RET usando extensao CANopen ECSS. |
| REQ-ITM16 | Configuracao CANopen | Configuracao total do ITM via SDO. |
| REQ-ITM17 | Node-ID | Node-ID configuravel via SDO durante integracao. |
| REQ-ITM18 | Persistencia CANopen | Comando SDO deve persistir parametros do OD em flash. |
| REQ-ITM19 | OD para configuracao | OD deve expor numero de serie do hardware, versao do firmware e versao da configuracao. |
| REQ-ITM20 | Origem dos dados de configuracao | Dados de configuracao devem vir da memoria nao volatil e serem carregados no boot. |
| REQ-ITM21 | Acesso read-only | Dados de gestao de configuracao no OD devem ser somente leitura via SDO. |
| REQ-ITM22 | Numero de serie | Numero de serie deve derivar de identificador unico nao editavel do MCU ou componente equivalente. |
| REQ-ITM23 | Versao do firmware | Versao do firmware deve ficar em area especifica de flash e ser consultavel via OD. |
| REQ-ITM24 | Versao da configuracao | Persistencia da configuracao via SDO deve incrementar automaticamente a versao de configuracao. |
| REQ-ITM25 | Baterias | Comunicar com AV_BATT e ACT_BATT via CAN, usando CAN basico ou CANopen conforme fornecedor. |
| REQ-ITM26 | CEB | Comunicar com a CEB via RS-485. |
| REQ-ITM27 | Protocolo CEB | Implementar protocolo proprietario Skyroot para monitoramento e testes funcionais. |
| REQ-ITM28 | BC | Comunicar com o Banco de Controle usando protocolo proprietario a definir. |
| REQ-ITM29 | SSR pelo BC | Permitir acionamento dos SSR por comando do BC. |
| REQ-ITM30 | Testes em solo | Permitir testes funcionais do STM em solo, incluindo comandos de movimentacao da tubeira. |
| REQ-ITM31 | Perda de BC | Se a comunicacao com o BC for perdida e nao restabelecida apos 3 tentativas, os SSR devem ir para DESLIGADO. |
| REQ-ITM32 | Alimentacao ITM | Entrada de alimentacao do ITM: 28 +/- 4 VDC. |
| REQ-ITM34 | CANopen RET | CAN 2.0A, diferencial isolado, 11 bits, 500 kbit/s, CANopen conforme ECSS. |
| REQ-ITM35 | Saida CEB | Uma saida digital para energizar/desenergizar modulo de controle da CEB. |
| REQ-ITM36 | Saida CB | Uma saida digital para energizar/desenergizar o Contactor Box. |
| REQ-ITM37/38/59 | Entradas CB | Duas entradas analogicas ate 10 V, convertidas em booleanos por limiares abaixo de 5,6 V e acima de 6,7 V. |
| REQ-ITM39 | Falhas CB | Firmware deve confirmar operacao correta do CB por tabela verdade apos debounce. |
| REQ-ITM40 | CAN telemetria | Interface CAN de telemetria configuravel: CAN 2.0A, 11 bits, 500 kbit/s, 100 Hz. |
| REQ-ITM60/61 | Ambiente/EMC | Firmware deve apoiar ensaios e diagnosticos necessarios a qualificacao ambiental e EMC. |
| REQ-ITM67/68/70 | Confiabilidade/falha/FMEA | Projeto deve considerar confiabilidade, falha simples e analise FMEA. |

## Requisitos De Hardware Que Afetam Software

| ID | Impacto |
| --- | --- |
| REQ-ITM 1 | O ITM e alimentado exclusivamente pela AV_BATT no conector C3. |
| REQ-ITM 2 | Interfaces RS-422/RS-485 e CAN/CANopen devem ser galvanicamente isoladas entre si e em relacao a alimentacao. |
| REQ-ITM 3 | Terminacoes das interfaces de comunicacao devem ser selecionaveis. |
| REQ-ITM 8/9 | SSRs devem usar MOSFETs inteligentes e suportar margem de corrente. |
| REQ-ITM33 | Interface de alimentacao deve somar consumo do ITM, margem da CEB e margem da CB. |
| REQ-ITM41 | Referenciais eletricos independentes precisam estar refletidos no diagnostico e no bring-up. |
| REQ-ITM46 | Isolacao galvanica deve cumprir 1,5 kVrms @ 60 Hz, 500 Vrms de trabalho e 1,5 kV/s de imunidade a transitorio. |

## Requisitos Ainda Dependentes De ICD Ou Decisao

| Tema | Falta definir |
| --- | --- |
| Protocolo BC | Formato de frame, comandos, respostas, tentativas, timeout entre tentativas, CRC/checksum e condicoes de "perda de comunicacao". |
| Protocolo Skyroot/CEB | Variaveis monitoradas, comandos de teste, enderecamento, CRC/checksum, temporizacao e limites de movimento. |
| BMS AV_BATT/ACT_BATT | CAN basico ou CANopen, IDs, objetos, escalas, periodos, falhas e timeouts. |
| CANopen RET | OD completo, EDS, DCF, PDOs, SDOs, heartbeat, Node-ID inicial e estrategia de persistencia. |
| Contactor Box | Tempo de debounce, tabela verdade final do REQ-ITM39 e tratamento de estados intermediarios entre 5,6 V e 6,7 V. |
| DDS | Confirmar se Micro XRCE-DDS e requisito do produto ou apenas opcao de arquitetura da apresentacao. |

## Criterios De Aceitacao Iniciais

- Todo parser de protocolo deve ter testes de frame valido, frame truncado, CRC/checksum invalido, campo fora de faixa, timeout e recuperacao.
- Toda saida que aciona CEB ou CB deve passar por maquina de estados, intertravamentos e log de evento.
- A perda de comunicacao com o BC deve produzir desligamento seguro dos SSR apos a regra das 3 tentativas.
- Configuracao CANopen deve ser alteravel por SDO, persistivel sob comando e restaurada no boot.
- OD deve expor numero de serie, versao de firmware e versao de configuracao como read-only.
- Conversao analogica do CB deve cobrir histerese/indeterminacao entre 5,6 V e 6,7 V, mesmo que o requisito ainda nao explicite esse estado.

## Estado Da Rastreabilidade

- `REQ-ITM 1` a `REQ-ITM71` foram identificados no PDF.
- Esta pagina destaca os requisitos com impacto direto em firmware.
- Uma matriz detalhada de requisito, criterio, teste e evidencia deve ser criada antes da implementacao de cada modulo.
