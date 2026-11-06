# Enlace IP do ITM com o Banco de Controle

## Decisão provisória

O par TX/RX diferencial da UART7 no J4 transporta PPP assíncrono sobre
RS-422 full-duplex, a 460800 bit/s e 8N1. O lwIP negocia IPv4 por IPCP.
Os endereços de integração são ITM `10.44.0.2` e BC `10.44.0.1`.
O cliente Micro XRCE-DDS do ITM abre TCP para o Agent no BC, porta 2018.
O Agent é a ponte para a rede DDS. A CAN RET não transporta DDS; continua
dedicada a CANopen, diagnóstico e atualização de firmware.

O arquivo de contrato [`itm_snapshot.idl`](itm_snapshot.idl) contém
`itm::telemetry::SnapshotV2`, `itm::power::Command` e `itm::power::Status`.
O tópico de telemetria é
[`itm_snapshot.idl`](itm_snapshot.idl), tópico `ITM_Snapshot_v2`. Contém
sequência do snapshot, qualidade codificada em 18 bytes e 70 campos tipados na ordem
do catálogo. Os dados das baterias preservam a representação inteira de
engenharia e o fator decimal do catálogo. Os da CEB transportam as contagens
originais de 16 bits e as equações do ICD da CEB. Qualidade `0` significa
sem amostra, `1` válido e `2` obsoleto; o valor obsoleto é preservado, mas não
representa uma medição atual. Os campos mantêm 16, 32 ou 64 bits conforme
o dicionário existente. Um campo adicional `cb_condition` (`octet`) expõe
o resultado da tabela verdade do CB após 40 ms de estabilidade. O CDR
ocupa 213 bytes; a publicação provisória é
a cada 40 ms.

Os comandos chegam pelo tópico confiável e volátil
`ITM_PowerCommand_v1` (KEEP_LAST, profundidade 1). O ITM publica
`ITM_PowerStatus_v1` com os mesmos QoS ao iniciar a sessão, ao receber um
comando, a cada mudança observada e, no máximo, a cada 250 ms. O `session_id`
é obtido do RNG de hardware a cada sessão XRCE e comandos de outra sessão ou
com `command_id` antigo são rejeitados. Um comando ativo expira após 1 s sem
um novo `command_id`; a queda de PPP/TCP/XRCE solicita imediatamente OFF.
EM1 e EM2 continuam diagnósticos e não bloqueiam diretamente as saídas.

## Implementação no firmware de bancada

As tarefas FreeRTOS separam o serviço existente de CAN/CEB/controle da
entrada PPP e da publicação XRCE. A UART7 usa anéis de RX e TX; o PPPoS do
lwIP recebe os bytes da UART7 e fornece uma interface IP. O transporte
customizado do cliente XRCE usa socket TCP do lwIP com o prefixo de
comprimento de dois bytes exigido pelo transporte TCP do Agent. O cliente
recria sessão e entidades após desconexão. Neste firmware de bancada, a
imagem candidata é confirmada após 2 s de execução da aplicação e CANopen,
mesmo sem BC conectado. A confirmação não demonstra que PPP, TCP ou DDS
tenham sido validados.
Nesse build da aplicação, o núcleo usa HSI a 64 MHz; a fonte HSE de 8 MHz
continua dedicada ao FDCAN, e UART4/UART7 continuam usando HSI. O bootloader
mantém seu clock de 8 MHz.

Enquanto a sessão XRCE está ativa, o BC é a autoridade de potência e escritas
no SDO de bancada `0x2120:0` são recusadas. Sem sessão BC, o SDO mantém os
valores 0=OFF, 1=CEB, 2=ambas e 3=CB, também com lease de 1 s. A opção CMake
`ITM_ENABLE_BENCH_POWER_SDO=OFF` desabilita as escritas nesse objeto sem
remover os objetos SDO de leitura e diagnóstico.

Micro XRCE-DDS Client `v3.0.0` e Micro-CDR `v2.0.1` são preservados em
`third_party`. FreeRTOS e lwIP são obtidos do STM32CubeH7 `V1.13.0`, já
necessário para o HAL. O heap FreeRTOS de 32 KiB fica na AXI SRAM; a memória
crítica anterior permanece na DTCM. Essa escolha deve ser revista depois de
medição de picos de heap e pilhas em bancada.

## Configuração do BC para ensaio

Em um BC Linux com adaptador serial RS-422, um exemplo inicial de PPP é:

```sh
pppd /dev/ttyUSB0 460800 10.44.0.1:10.44.0.2 local noauth nocrtscts persist nodetach
```

Com o enlace ativo, iniciar o Agent em TCP/IPv4:

```sh
MicroXRCEAgent tcp4 -p 2018
```

Os tipos de `itm_snapshot.idl` devem ser conhecidos pelo ambiente DDS/Agent,
pelo publicador de comandos e pelos consumidores do BC. A configuração concreta
de registro dos tipos depende da instalação Fast DDS no BC; ela ainda não foi
ensaiada. O BC também
deve verificar os 70 índices, a condição do CB, as conversões e o critério de obsolescência.

## Limites de validação

O pacote de bancada `out/firmware/v18` contém as duas imagens A/B e o
bootloader. Os binários compilam, mas a negociação PPP, o TCP e a
criação dos tópicos, recepção de comandos e publicação de Status ainda precisam
ser verificados em um BC ou simulador
fisicamente ligado à UART7. Também faltam ensaios de interrupção, reconexão,
uso máximo simultâneo das CANs e UART4, e medição de perda de quadros e
latência. Sem esses ensaios, esta revisão não é uma baseline de voo.
