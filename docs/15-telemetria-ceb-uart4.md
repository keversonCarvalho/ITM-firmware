# Telemetria serial CEB no snapshot

A aplicação dos slots A/B recebe a telemetria da CEB pela UART4 do STM32H723
(PA1, 460800 baud, 8N1). A linha `UART4_DE` permanece desabilitada: este fluxo
é somente CEB → ITM. A interface elétrica acordada é RS-485 half-duplex;
a polaridade e a ligação dos pares no hardware Rev00 ainda precisam ser
confirmadas no ensaio com a CEB.

A UART4 usa o HSI de 64 MHz como relógio de periférico para reduzir o erro de
baud. Uma interrupção curta lê diretamente o FIFO RX para um buffer circular
estático. O laço da
aplicação decodifica quadros de 44 bytes, com marcadores `5A 5A`, contador de
16 bits, 36 bytes de payload, CRC e marcador final. Quadros com CRC ou
enquadramento inválidos não atualizam o snapshot. O decodificador se
ressincroniza no próximo marcador e conta lacunas no contador de quadros.
Os sinais ficam obsoletos 100 ms após a última atualização válida.

O CRC está implementado provisoriamente como CRC-16/CCITT-FALSE (polinômio
`0x1021`, inicial `0xFFFF`, sem reflexão, xor final zero) sobre os bytes 2 a 39,
38 bytes conforme a decisão de integração. A confirmação exige um quadro real
e o CRC esperado pela CEB.

Os 18 campos do payload e o contador são registrados como sinais `CEB_*` com
origem `ITM_SOURCE_CEB`. As medições `CEB_BATTERY_150V_VOLTAGE` e
`CEB_SUPPLY_28V_VOLTAGE` não substituem `ACT_BATT_VOLTAGE` e
`AV_BATT_VOLTAGE`, que continuam vindo das CANs dos BMS. `CEB_CAN_HEALTH`
representa as redes CAN A/B da própria CEB. As grandezas derivadas das
contagens brutas usam aritmética inteira nas escalas do catálogo: mV, mA,
centésimos de °C e milésimos de mm.

O ensaio local cobre publicação do quadro completo, isolamento das fontes,
CRC inválido, ressincronização, volta do contador de 65535 para zero e
obsolescência. A validação com a CEB real e um vetor de CRC emitido por ela
permanece pendente.

## Simulador no computador

O script `tools/simulate_ceb_tlm.py` transmite o mesmo quadro pela porta
USB/RS-485. Na máquina de bancada atual, o conversor CP210x aparece como
`COM3` e o driver do Windows já funciona. Para o Python, instalar `pyserial`:

```powershell
python -m pip install -r tools/requirements-ceb-serial.txt
python tools/simulate_ceb_tlm.py --dry-run
python tools/simulate_ceb_tlm.py --port COM3 --count 100
```

O envio dura cerca de um segundo. `--count 0` envia continuamente até Ctrl+C;
`--list-fields` mostra os 18 campos e `--raw ceb_health=0x0001` altera uma
contagem bruta para um ensaio de falha. O script usa o contador de 16 bits
incremental e a temporização nominal de 10 ms. O Windows não oferece garantia
de tempo real; o script relata atrasos de agendamento.

A porta serial de telemetria não define resposta da CEB/ITM. Uma escrita bem
sucedida na COM3 não confirma que o ITM recebeu ou publicou o quadro; isso
deve ser verificado por um canal de observação independente durante o ensaio.

## Ensaio de integração de bancada

A aplicação v8 foi instalada pelo fluxo SDO/XCP sobre CAN RET (CANalyst-II,
canal 2) no slot A e confirmada. O OD expõe o registro somente leitura
`0x2110` para diagnosticar a recepção CEB: subíndices 1 a 7 representam,
respectivamente, quadros válidos, erros de CRC, erros de enquadramento,
lacunas do contador, erros UART/ring, último contador e última tensão de
suprimento CEB em mV. É possível consultá-lo com
`python tools/itm_od.py show --channel 2`.

Com `CEB_TLM_RX_p` no borne A e `CEB_TLM_RX_n` no borne B do conversor CP210x,
terminação presente e sem fio de terra ligado, foram transmitidos 1110 quadros
pela COM3 a 460800 8N1, nominalmente a cada 10 ms. O ITM reportou 1110
quadros válidos, último contador 41109, zero erros de CRC, enquadramento,
UART/ring e lacunas. No último lote, `supply_28v=1365` foi decodificado como
14000 mV. Isso demonstra que essa polaridade A/B funciona nessa montagem;
não substitui a qualificação da ligação elétrica com a CEB real.

## Leitura completa no mesmo script das baterias

A aplicação v9 acrescenta o objeto SDO somente leitura `0x2111:0`. Uma leitura
segmentada captura atomicamente os 19 sinais CEB do mesmo snapshot das
baterias. O formato versionado `1;valor:qualidade,...` usa a ordem dos IDs
`ITM_SIGNAL_CEB_*` no catálogo e qualidade 0=invalido, 1=valido, 2=obsoleto.
O script `tools/simulate_battery_can.py` converte as escalas e mostra origem,
nome, unidade e qualidade por sinal.

Com a CAN RET no canal 2 e o conversor RS-485 na COM3:

```powershell
python tools/simulate_battery_can.py --bus ceb --channel 2 --simulate-ceb --ceb-port COM3
```

Esse comando envia 200 quadros de exemplo pela COM3 enquanto consulta o
snapshot por CANopen. Para apenas ler a última amostra, inclusive quando
a CEB real estiver transmitindo:

```powershell
python tools/simulate_battery_can.py --bus ceb --channel 2
```

Quando uma bateria está ligada a outro canal, `--include-ceb --ret-channel 2`
acrescenta os sinais CEB após a verificação do DBC da bateria; por exemplo,
`python tools/simulate_battery_can.py --bus acc --channel 1 --include-ceb --ret-channel 2`.

Na placa de bancada, a v9 foi instalada via CANopen/XCP no slot B e confirmada.
O comando com `--simulate-ceb` exibiu os 19 sinais com qualidade valida, entre
eles 5,000 V no LVDT principal de pitch, 25,00 °C na temperatura do atuador
de pitch e 28,000 V no suprimento CEB. Os modos AV/ACC mantiveram a geracao
DBC no ensaio local; nao foi repetido um teste fisico nas portas das baterias
nesta etapa.

## Snapshot completo por SDO

A aplicação v10 expõe o snapshot completo no objeto somente leitura
`0x2112:0` (OCTET_STRING). A partir da v11, uma única captura do armazenamento
em RAM contém os 71 sinais ativos do catálogo: AV-BATT, ACC-BATT e CEB.
A transferência SDO é segmentada, mas todos os segmentos pertencem
à mesma captura. Cada registro ocupa 16 bytes em little-endian: ID (`u16`),
tipo (`u8`), qualidade (`u8`: 0 inválido, 1 válido, 2 obsoleto), instante da
última amostra em milissegundos desde o boot (`u32`) e valor inteiro com sinal
(`i64`). A escala e a unidade vêm do catálogo de telemetria do firmware.

```powershell
python tools/itm_od.py snapshot --channel 2
```

O comando mostra a origem, o valor físico, a qualidade e o instante da última
amostra de cada sinal. Um sinal sem amostra aparece como `<no sample>`; ao
envelhecer, mantém o último valor com qualidade `stale`. A leitura não grava
FLASH e não cria histórico. O objeto `0x2111:0` permanece disponível para
clientes que já consomem apenas os 19 sinais CEB.
