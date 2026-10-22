# Controle de alimentação da CEB e do CB em bancada

Na aplicação v12, a CAN RET aceita comandos de bancada por SDO. `MFET2`
(PE3, nível alto) aciona `+28V_CEB`; `MFET1` (PE2, nível alto) aciona
`BOB_CONT_+`. O comando `0` desliga ambos, `1` liga somente a CEB e `2`
liga a CEB e, após 50 ms, solicita o CB. A aplicação inicia com ambos
desligados. Se um comando ativo não for renovado em 1 s, solicita o
desligamento. Falhas desligam as duas saídas.

Os sinais `E_ALIM` (EM1, montante) e `S_ALIM` (EM2, jusante) são lidos pelo
ADC1 diferencial, canais 3 e 4. A conversão nominal adotada é

`V_monitor = (ADC - 32768) × (2 × VREF / 65535) × (95,5 / 20,5)`.

`VREF` está fixado provisoriamente em 3,3 V. A leitura é considerada
inválida quando o resultado indica polaridade negativa ou entrada do
AMC3311 maior que 2,0 V (aproximadamente 9,32 V no monitor), fora da
faixa linear especificada para esta interpretação. Abaixo de 5,6 V o
sinal é baixo; acima de 6,7 V é alto; o intervalo intermediário é
indeterminado. As duas entradas precisam permanecer no mesmo estado
durante o tempo de debounce, inicialmente 50 ms.

| Objeto | Acesso | Significado |
| --- | --- | --- |
| `0x2120:0` | RW, U8 | Pedido: 0=desligar, 1=CEB, 2=CEB+CB |
| `0x2121:0` | RO, U32 | Estado da sequência de potência |
| `0x2122:0` | RW, U16 | Debounce de EM1/EM2, ms |
| `0x2123:0` | RW, U16 | Atraso de partida da CEB, ms |
| `0x2124:0` | RW, U16 | Espera adicional de resposta do CB, ms |
| `0x2125:0` | RO, U32 | EM1 em mV; `0xFFFFFFFF` se inválido |
| `0x2126:0` | RO, U32 | EM2 em mV; `0xFFFFFFFF` se inválido |
| `0x2127:0` | RO, U8 | Condição do CB: 0 pendente, 1 normal, 2 bateria desconectada, 3 falha de monitor, 4 preso ligado, 5 preso desligado, 6 inválida |

Os três tempos aceitam 1–5000 ms e são voláteis. O limite de acionamento
do CB é o debounce mais a espera adicional. Não há confirmação elétrica
independente da alimentação da CEB nesta revisão; o estado `CEB_ON`
significa que a saída foi comandada e o atraso configurado transcorreu.

Com a CAN RET no canal 2 do CANalyst-II:

```powershell
python tools/test_cb_power.py status --channel 2
python tools/test_cb_power.py config --channel 2 --debounce-ms 50 --ceb-delay-ms 50 --cb-timeout-ms 50
python tools/test_cb_power.py ceb --channel 2 --seconds 5
python tools/test_cb_power.py on --channel 2 --seconds 5
python tools/test_cb_power.py off --channel 2
```

Os comandos `ceb` e `on` renovam o pedido enquanto executam e enviam
`off` ao terminar, inclusive em caso de interrupção do script. Se a
conexão for perdida, o lease de 1 s no firmware desliga as saídas.
Para validar fisicamente, medir `+28V_CEB` e `BOB_CONT_+` e aplicar
níveis conhecidos em EM1/EM2; uma resposta SDO confirma a lógica digital,
mas não substitui a medição nos conectores.
