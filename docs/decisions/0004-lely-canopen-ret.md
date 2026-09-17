# ADR 0004 - Lely Core Para CANopen RET

## Status

Aceita como direcao de integracao; port embarcado ainda bloqueado.

## Contexto

O projeto CANopus utiliza Lely Core como pilha CANopen no lado mestre/gateway.
Sua implementacao atual e C++20 para Linux, usa SocketCAN e ainda possui pontos
de integracao Lely marcados como pendentes. O ITM sera um dispositivo CANopen
embarcado, ligado a RET por FDCAN do STM32.

O registro CiA fornecido atribui a Concert, departamento Space, o Vendor-ID
`0x000006A9`.

## Decisao

- Usar a biblioteca C `liblely-co` da familia Lely Core no ITM.
- Manter tipos Lely dentro de `platform/lely`; aplicacao e servicos usam apenas
  contratos proprios.
- Descrever o Object Dictionary em EDS e gerar a descricao C estatica com
  `dcf2c`, evitando parser de EDS no microcontrolador.
- Operar o ITM como dispositivo/slave CANopen, nao reutilizar o mestre C++ e o
  transporte SocketCAN do CANopus.
- Fixar Vendor-ID `0x000006A9` em `0x1018:01`.

## Consequencias

- CANopus podera validar o ITM como mestre de bancada usando a mesma familia de
  pilha e o mesmo EDS.
- A versao/commit da Lely, licenca incorporada, branch ECSS/N7, configuracao sem
  alocacao dinamica e adaptador FDCAN precisam ser definidos antes do port.
- O EDS atual nao possui PDOs: o mapeamento depende do ICD da RET.
