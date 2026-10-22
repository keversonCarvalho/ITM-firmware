# ADR 0004 - N7 Space ECSS para CANopen RET

## Status

Aceita e integrada para bancada. Qualificacao de voo pendente.

## Decisao

Usar N7 Space CANopen v3.5.0, commit `33f62875479028c11907340a88ec9596828bb20b`, como dispositivo CANopen do ITM. O codigo C da N7 Space e incorporado em `third_party/n7space`, com licenca e avisos de autoria preservados. A compilacao usa modo freestanding, sem heap. O adaptador de CAN e timers fica em `src/protocols/n7space.c` e `platform/stm32h723`.

O dicionario estatico deriva do EDS versionado. Os objetos de fabricante ocupam `0x2100` a `0x2106`; `0x2000` fica reservado para redundancia ECSS. A entrada em manutencao usa `0x2103`. A transferencia de firmware ocorre via XCP on CAN no bootloader.

## Limites

Os IDs XCP sao provisorios ate o ICD da RET. A imagem de bancada usa CRC, sem autenticacao. O bootloader exige protecao WRP do setor zero. Antes de voo, definir assinatura, politica de versoes, qualificar o alvo e ensaiar recuperacao apos perda de energia.
