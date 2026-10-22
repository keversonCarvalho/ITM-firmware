# DBCC Das Baterias

## Escopo

O DBCC recebe frames das redes `Batt_28V` e `Batt_150V`, valida ID estendido e
DLC, decodifica conforme `config/can/Battery.dbc` e entrega valores inteiros ao
registro de telemetria. O mesmo conjunto de CAN IDs pode existir nas duas
redes; a origem fisica faz parte do frame abstrato e impede mistura de dados.

O arquivo DBC nao e interpretado no STM32. `cantools` 41.0.2 roda apenas no
computador de desenvolvimento e gera `generated/can/battery_dbc_generated.c`
e `.h`. O firmware usa esses arquivos versionados e o wrapper portatil
`itm_dbcc`.

## Geracao Reproduzivel

```sh
python -m pip install -r tools/requirements-dbc.txt
python tools/generate_battery_dbc.py --write
python tools/generate_battery_dbc.py --check
```

Os alvos CMake equivalentes sao `itm_generate_battery_dbc` e
`itm_check_battery_dbc`. A compilacao normal nao regenera arquivos e nao
depende de Python ou internet. O relatório versionado
`generated/can/battery_dbc_audit.json` registra a versao da ferramenta, o
SHA-256 do DBC e todas as mensagens e sinais importados.

## Contrato

`itm_dbcc_frame_t` carrega barramento de origem, CAN ID, tipo de identificador,
DLC, oito bytes e timestamp monotonico. `itm_dbcc_decode` decodifica as oito
mensagens sem acessar HAL. `itm_dbcc_ingest` atualiza todos os 26 sinais do DBC
em cada rede, totalizando 52 entradas independentes no snapshot. Cada entrada
guarda o ultimo valor aceito, a origem, o timestamp e a qualidade. O snapshot
nao e uma serie temporal: uma mensagem nova substitui o valor anterior daquele
sinal. Em `V_CELL`, `cellPoint` e as tres tensoes correspondem apenas ao ultimo
grupo recebido; o snapshot nao acumula as 48 celulas indexadas.

Os cinco `SIG_VALTYPE_` IEEE-754 sao desempacotados pelo codec gerado. Na
fronteira manual, NaN, infinito e overflow sao rejeitados, e valores validos
sao convertidos para inteiros com escala de milesimos. Nenhum `float` e
armazenado na telemetria. Os demais campos mantem a escala inteira do DBC:
tensoes de celula em mV, tensoes auxiliares e temperatura/umidade em centesimos,
SOC e SOH em centesimos de porcento, contadores sem escala e mascara de
balanceamento em inteiro de 64 bits. As faixas do catalogo para os novos sinais
sao provisorias; os minimos/maximos do DBC tambem contem valores incompativeis
com as escalas de varios sinais e exigem revisao com o ICD da bateria.

Todos os IDs sao estendidos: `0x260A` a `0x2D0A`. Sete mensagens possuem DLC 8;
`CAN_PACKET_BMS_HUM` (`0x2C0A`) possui DLC 6. O DBC nao informa periodos.

## Memoria E Limites

O codigo gerado e o snapshot nao alocam memoria dinamica. Uma instancia de `itm_dbcc_t`
contem um ponteiro para o store e oito contadores de 32 bits: estimativa de 36
bytes em ARM de 32 bits, alem das variaveis automaticas limitadas. Os testes de
host nao substituem bitrate, filtros FDCAN, temporizacoes, vetores oficiais nem
ensaio nas duas redes fisicas.

No build atual de bancada, cada imagem de aplicacao ocupa cerca de 69 KiB de
flash e 39,7 KiB de RAM, incluindo o DBCC, o snapshot, CANopen e a logica de
atualizacao. Os numeros de objetos medidos antes da integracao FDCAN nao
representam mais o firmware completo.
