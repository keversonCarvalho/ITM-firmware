# Controle de alimentação da CEB e do CB em bancada

Na aplicação de bancada v17, a CAN RET aceita comandos por SDO. `MFET2`
(PE3, nível alto) aciona `+28V_CEB`; `MFET1` (PE2, nível alto) aciona
`BOB_CONT_+`. O comando `0` desliga ambos, `1` liga somente a CEB, `2`
liga ambos e `3` liga somente o CB. Os dois acionamentos são independentes:
um não exige que o outro esteja ligado e EM1/EM2 não bloqueiam as saídas.
A aplicação inicia com ambos desligados. Se um comando ativo não for
renovado em 1 s, solicita o desligamento. Falha de escrita do GPIO ou perda
da permissão de comando também desliga as duas saídas; leitura inválida de
EM1/EM2 não desliga nenhuma delas.

Quando a sessão PPP/XRCE está ativa, o Banco de Controle tem prioridade e a
escrita de `0x2120:0` é recusada para impedir duas autoridades simultâneas.
Quando a sessão não está ativa, o SDO permanece habilitado para ensaio. Ele
pode ser retirado de um build operacional com
`-DITM_ENABLE_BENCH_POWER_SDO=OFF`; as leituras de diagnóstico permanecem.
Para este ensaio independente,
a ausência do Banco de Controle não impede a confirmação da imagem candidata
após 2 s de execução da aplicação e da CANopen; isso não valida o enlace BC.

Os sinais `E_ALIM` (EM1, montante) e `S_ALIM` (EM2, jusante) são lidos pelo
ADC1 diferencial, canais 3 e 4. A conversão nominal adotada é

`V_monitor = (ADC - 32768) × (2 × VREF / 65535) × (95,5 / 20,5)`.

`VREF` está fixado provisoriamente em 3,3 V. Pequenos desvios negativos
do zero (até 640 contagens, cerca de 0,30 V no monitor) são tratados
como zero. Uma leitura mais negativa ou uma entrada estimada acima de
2,0 V no AMC3311 (aproximadamente 9,32 V no monitor) é inválida. O
primeiro divisor externo entrega ao ITM até 9,13 V quando a bateria está
em 176,4 V. O divisor do ITM reduz essa tensão para a entrada do AMC3311.
Assim, `V_bateria = V_monitor × 176,4 / 9,13`. O limiar fixo escolhido
para a tabela verdade é 7,304 V no monitor, equivalentes a 141,12 V na
bateria (80% de 176,4 V). Até 7,304 V inclusive, EM1/EM2 valem `0`;
acima, valem `1`. A
classificação precisa permanecer estável durante 40 ms após qualquer
mudança das entradas **ou do comando CB**. Durante essa janela, a condição
é `PENDING`; conversão ADC inválida produz `INVALID`. Como o limiar é
fixo, uma bateria presente abaixo de 7,304 V também será classificada
como `0` por este critério.

| Comando CB | EM1 | EM2 | Condição publicada |
| --- | --- | --- | --- |
| 0 | 0 | 0 | `BATTERY_DISCONNECTED` (ACT_BATT desconectada) |
| 0 | 0 | 1 | `MONITOR_FAULT` (erro no monitoramento) |
| 0 | 1 | 0 | `NORMAL` |
| 0 | 1 | 1 | `STUCK_ON` (contator travado ligado) |
| 1 | 0 | 0 | `BATTERY_DISCONNECTED` (ACT_BATT desconectada) |
| 1 | 0 | 1 | `MONITOR_FAULT` (erro no monitoramento) |
| 1 | 1 | 0 | `STUCK_OFF` (contator travado desligado) |
| 1 | 1 | 1 | `NORMAL` |

A condição também é lida pelo SDO `0x2127:0` e integra a amostra
`itm::telemetry::SnapshotV2` no tópico `ITM_Snapshot_v2`. Esse campo
de condição não é um dos 70 sinais originais de bateria/CEB.

| Objeto | Acesso | Significado |
| --- | --- | --- |
| `0x2120:0` | RW, U8 | Pedido: 0=desligar, 1=CEB somente, 2=ambos, 3=CB somente |
| `0x2121:0` | RO, U32 | Estado lógico do comando de potência |
| `0x2122:0` | RW, U16 | Debounce diagnóstico de EM1/EM2, ms |
| `0x2123:0` | RW, U16 | Atraso para declarar CEB_ON após comando, ms |
| `0x2124:0` | RW, U16 | Atraso para declarar CB_ON após comando, ms |
| `0x2125:0` | RO, U32 | EM1 em mV; `0xFFFFFFFF` se inválido |
| `0x2126:0` | RO, U32 | EM2 em mV; `0xFFFFFFFF` se inválido |
| `0x2127:0` | RO, U8 | Condição do CB: 0 pendente, 1 normal, 2 bateria desconectada, 3 falha de monitor, 4 preso ligado, 5 preso desligado, 6 inválida |
| `0x2128:0` | RO, U32 | EM1: contagem bruta do ADC diferencial de 16 bits; `0xFFFFFFFF` se a conversão falhou |
| `0x2129:0` | RO, U32 | EM2: contagem bruta do ADC diferencial de 16 bits; `0xFFFFFFFF` se a conversão falhou |

Os três tempos aceitam 1–5000 ms e são voláteis. As saídas mudam assim que
o comando é processado; os atrasos de 0x2123/0x2124 afetam apenas quando o
estado lógico passa de STARTING para ON. EM1/EM2 e seu debounce são somente
diagnósticos. Os estados `CEB_ON`, `CB_ON` e `BOTH_ON` indicam saídas
comandadas, sem confirmação elétrica independente.

Com a CAN RET no canal 2 do CANalyst-II:

```powershell
python tools/test_cb_power.py analog --channel 2 --seconds 10 --interval 0.5
```

O comando `analog` lê `0x2125:0`, `0x2126:0`, `0x2128:0` e `0x2129:0` por SDO
e imprime EM1/E_ALIM e EM2/S_ALIM em volts, milivolts e contagens brutas do
ADC a cada intervalo. A diferenca estimada entre os pinos do ADC usa
`VREF = 3,3 V` nominal; com saida negativa do AMC em falha, ela nao
representa a tensao de entrada medida.
Ele não altera o estado das saídas; se já houver um comando ativo, o script
mostra um aviso. As duas conversões e tensões são publicadas separadamente;
o diagnóstico do CB continua exigindo ambas válidas.

Para testar as saídas digitais separadamente, quando houver instrumentação
adequada nos conectores:

```powershell
python tools/test_cb_power.py status --channel 2
python tools/test_cb_power.py config --channel 2 --debounce-ms 40 --ceb-delay-ms 50 --cb-timeout-ms 50
python tools/test_cb_power.py outputs --target ceb --channel 2 --seconds 5
python tools/test_cb_power.py outputs --target cb --channel 2 --seconds 5
python tools/test_cb_power.py outputs --target both --channel 2 --seconds 5
python tools/test_cb_power.py off --channel 2
```

O comando `outputs` exige a escolha explícita de `--target`; `ceb` aciona
somente a CEB, `cb` somente o CB e `both` ambos. Os nomes antigos `ceb` e
`on` continuam aceitos. Os comandos de saída renovam o
pedido enquanto executam e enviam
`off` ao terminar, inclusive em caso de interrupção do script. Se a
conexão for perdida, o lease de 1 s no firmware desliga as saídas.
O script verifica a versão do firmware e recusa testes de saída na v16, que
ainda intertravava o CB com a CEB e EM1/EM2. A v17 precisa ser instalada antes
dos novos ensaios.
Para validar fisicamente, medir `+28V_CEB` e `BOB_CONT_+` separadamente.
Aplicar níveis conhecidos em EM1/EM2 testa apenas o monitoramento analógico;
uma resposta SDO confirma o comando lógico, mas não substitui a medição nos
conectores.

Com `VREF = 3,3 V` nominal, a tensão mostrada é calculada por
`V_monitor = (ADC − 32768) × (2 × 3,3 / 65535) × (95,5 / 20,5)`.
O firmware arredonda o resultado para milivolts e retorna `0xFFFFFFFF`
quando a amostra é inválida. A validação atual rejeita tensão negativa
e tensão estimada acima de 2,0 V na entrada do AMC3311, equivalente a
aproximadamente 9,32 V em E_ALIM/S_ALIM. Portanto, o ponto de 10 V
aparece como inválido nesta versão; não deve ser interpretado como 0 V.
Cada canal agora conserva sua própria leitura bruta e validade. Uma falha
de conversão aparece como `ADC=conversion failed`; contagens entre 32128
e 32767 são tratadas como zero por tolerância de offset; abaixo de 32128
indicam entrada diferencial negativa inválida. Acima da faixa linear do
AMC3311 a tensão de engenharia é inválida, embora a contagem bruta ainda
esteja disponível. O diagnóstico do CB requer as duas entradas válidas.
