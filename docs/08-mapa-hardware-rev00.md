# Mapa De Hardware Rev00

## Fonte E Estado

Este levantamento foi feito a partir de `ITM-100 Rev00 _ Schematic diagram.pdf`,
emitido em 23/07/2026, com quatro folhas. O PDF e referencia local e nao deve ser
adicionado ao repositorio.

O mapa permite iniciar a configuracao do alvo, mas permanece sujeito a revisao
e aprovacao pelo responsavel de hardware. Nomes de protocolo, baud rates,
temporizacoes e comportamento funcional nao sao inferidos do esquema.

## Microcontrolador

- Componente: STM32H723VGT6.
- Encapsulamento: LQFP100.
- Oscilador externo: cristal de 8 MHz.
- Debug: SWDIO e SWCLK expostos no conector J3.
- Reset: NRST exposto no conector J3.
- Alimentacao logica: 3,3 V; dominio analogico VDDA filtrado.

## Mapa De Perifericos

| Funcao | Periferico | Pino MCU | Net |
| --- | --- | --- | --- |
| RET CAN RX | FDCAN1 RX | PA11 | `CAN1_RX` |
| RET CAN TX | FDCAN1 TX | PA12 | `CAN1_TX` |
| Terminacao RET | GPIO output | PE4 | `CAN1_TERM` |
| AV_BATT CAN RX | FDCAN2 RX | PB12 | `CAN2_RX` |
| AV_BATT CAN TX | FDCAN2 TX | PB13 | `CAN2_TX` |
| ACC_BATT CAN RX | FDCAN3 RX | PC8 | `CAN3_RX` |
| ACC_BATT CAN TX | FDCAN3 TX | PC9 | `CAN3_TX` |
| CEB TLM TX | UART4 TX | PA0 | `UART4_TX` |
| CEB TLM RX | UART4 RX | PA1 | `UART4_RX` |
| CEB TLM driver enable | GPIO output | PC7 | `UART4_DE` |
| CEB CMD TX | UART5 TX | PC12 | `UART5_TX` |
| CEB CMD RX | UART5 RX | PD2 | `UART5_RX` |
| CEB CMD driver enable | GPIO output | PA10 | `UART5_DE` |
| Banco de Controle TX | UART7 TX | PE8 | `UART7_TX` |
| Banco de Controle RX | UART7 RX | PE7 | `UART7_RX` |
| Entrada analogica E_ALIM + | ADC1 channel 18 differential + | PA4 | `ADC1_INP3` |
| Entrada analogica E_ALIM - | ADC1 channel 18 differential - | PA5 | `ADC1_INN3` |
| Entrada analogica S_ALIM + | ADC1 channel 3 differential + | PA6 | `ADC1_INP4` |
| Entrada analogica S_ALIM - | ADC1 channel 3 differential - | PA7 | `ADC1_INN4` |
| Acionamento de potencia 1 | GPIO output | PE2 | `MFET1` |
| Acionamento de potencia 2 | GPIO output | PE3 | `MFET2` |
| Diagnostico de potencia 1 | GPIO input | PC13 | `FAULT1` |
| Diagnostico de potencia 2 | GPIO input | PC14 | `FAULT2` |
| Indicador | GPIO output | PA9 | `LED` |

## Interfaces Externas

### CAN

- FDCAN1 chega ao conector J4 e corresponde a rede RET.
- FDCAN2 chega ao conector J5 e corresponde a AV_BATT.
- FDCAN3 chega ao conector J5 e corresponde a ACC_BATT.
- As tres interfaces usam transceptores isolados ADM3055E.
- A terminacao de FDCAN1 e comutavel por `CAN1_TERM` e rele de estado solido.
- AV_BATT e ACC_BATT possuem resistores de terminacao de 120 ohms desenhados.

### Seriais

- UART7 chega ao J4 como quatro fios `BC_TX_p/n` e `BC_RX_p/n`, usando
  transceptor isolado ISOW1432.
- UART4 chega ao J4 como `CEB_TLM_TX_p/n` e `CEB_TLM_RX_p/n`.
- UART5 chega ao J4 como `CEB_CMD_TX_p/n` e `CEB_CMD_RX_p/n`.
- UART4 e UART5 possuem nets de driver enable; a polaridade e a sequencia de
  habilitacao devem ser confirmadas contra o datasheet do ISOW1432.

### Potencia E Medicao

- U11, comandado por `MFET1`, produz `BOB_CONT_+` e fornece `FAULT1` e `SNS1`.
- U13, comandado por `MFET2`, produz `+28V_CEB` e fornece `FAULT2` e `SNS2`.
- O esquema liga `FAULT1/2` ao MCU; `SNS1/2` aparecem apenas como pontos de
  teste e nao como entradas do MCU.
- E_ALIM e S_ALIM sao medidos por amplificadores isolados AMC3311 e pares
  diferenciais do ADC1.
- A entrada dos amplificadores foi dimensionada no esquema para no maximo
  1,81 V com uma fonte de ate 176,4 V.

## Restricoes Para O Firmware

- Todas as saidas de potencia devem iniciar desabilitadas e somente mudar de
  estado depois da inicializacao dos GPIOs e da maquina de estados.
- A configuracao de ADC deve preservar o modo diferencial usado no esquema.
- Buffers de FDCAN/UART usados por DMA devem ser posicionados em memoria
  acessivel ao DMA e tratados segundo a politica de cache/MPU do Cortex-M7.
- A rede RET pode exigir controle de terminacao; as redes de bateria nao devem
  receber controle de terminacao por software segundo esta revisao.
- `FAULT1/2` devem ser tratados como diagnosticos digitais; o sentido ativo
  ainda deve ser confirmado no datasheet e validado em bancada.
- `FAULT2` usa PC14, compartilhado com OSC32_IN. O firmware nao pode habilitar
  o LSE enquanto esse pino for usado como GPIO.

## Achados Para Revisao

### Nomes Das Nets ADC

Os nomes das nets analogicas nao correspondem aos numeros de canal do
STM32H723VGT6:

- PA4/PA5 formam o par diferencial ADC1 canal 18, mas o esquema os chama de
  `ADC1_INP3` e `ADC1_INN3`.
- PA6/PA7 formam o par diferencial ADC1 canal 3, mas o esquema os chama de
  `ADC1_INP4` e `ADC1_INN4`.

O hardware pode estar eletricamente correto, mas esses nomes apresentam risco
de configuracao incorreta no CubeMX e no firmware. Recomenda-se corrigir as nets
na proxima revisao ou manter uma nota formal de equivalencia.

### Correspondencia Com O Diagrama Funcional

O esquema detalhado deve prevalecer sobre o diagrama funcional anterior:

- Banco de Controle: UART7, nao UART4.
- CEB TLM: UART4.
- CEB CMD: UART5.
- RET: FDCAN1; AV_BATT: FDCAN2; ACC_BATT: FDCAN3.

Essa correspondencia ainda precisa ser confirmada pelo responsavel do sistema,
pois o esquema eletrico nao define o conteudo dos protocolos.

## Pendencias Antes Do Bring-Up

- Aprovar formalmente a revisao e confirmar se Rev00 corresponde a placa
  fabricada.
- Validar alternate functions no STM32CubeMX sem remapeamentos conflitantes.
- Definir clocks, baud rates, sample points, DMA, IRQs e prioridades.
- Confirmar polaridade e estados de reset de `MFET1`, `MFET2`, `UART4_DE`,
  `UART5_DE` e `CAN1_TERM`.
- Confirmar associacao de `BOB_CONT_+` com o Contactor Box e a finalidade de
  E_ALIM/S_ALIM na tabela verdade de monitoramento.
- Resolver ou aceitar formalmente a divergencia dos nomes de canal ADC.
- Revisar as erratas do STM32H723 e dos componentes isoladores aplicaveis.
