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
mensagens sem acessar HAL. `itm_dbcc_ingest` atualiza apenas tensao total e SOC,
os sinais que ja possuem entradas distintas para AV_BATT e ACT_BATT no
catalogo. Os demais valores ficam disponiveis na saida normalizada do decoder,
mas nao sao publicados antes de aprovar faixas e semantica.

Os cinco `SIG_VALTYPE_` IEEE-754 sao desempacotados pelo codec gerado. Na
fronteira manual, NaN, infinito e overflow sao rejeitados, e valores validos
sao convertidos para inteiros com escala de milesimos. Nenhum `float` e
armazenado na telemetria.

Todos os IDs sao estendidos: `0x260A` a `0x2D0A`. Sete mensagens possuem DLC 8;
`CAN_PACKET_BMS_HUM` (`0x2C0A`) possui DLC 6. O DBC nao informa periodos.

## Memoria E Limites

O codigo gerado nao aloca memoria dinamica. Uma instancia de `itm_dbcc_t`
contem um ponteiro para o store e oito contadores de 32 bits: estimativa de 36
bytes em ARM de 32 bits, alem das variaveis automaticas limitadas. Os testes de
host nao substituem bitrate, filtros FDCAN, temporizacoes, vetores oficiais nem
ensaio nas duas redes fisicas.

Com Arm GNU 12.2.1 e `-Os`, os objetos antes de eliminacao pelo linker mediram
2348 bytes de texto para o codec gerado e 1592 bytes para o wrapper, total de
3940 bytes de flash e zero bytes de dados/BSS. O executavel de bring-up ainda
nao chama o DBCC, portanto `--gc-sections` pode remover parte ou todo esse codigo;
o consumo final deve ser medido novamente quando o FDCAN estiver integrado.
