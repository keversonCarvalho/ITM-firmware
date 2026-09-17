# ADR 0003 - Persistencia Transacional Em Dois Slots

## Status

Aceita para o nucleo; layout fisico da flash pendente.

## Contexto

REQ-ITM6, REQ-ITM7 e REQ-ITM14 exigem separar firmware e parametros e evitar
corrupcao durante gravacao independente.

## Decisao

Parametros usam dois slots abstratos. Cada registro possui versao de esquema,
geracao, tamanho, CRC-32 e marcador de commit gravado por ultimo. Uma gravacao
interrompida nao substitui o ultimo registro confirmado.

## Consequencias

- O servico independe da tecnologia de memoria.
- Enderecos, alinhamento, tamanho de pagina, endurance e protecao contra queda
  de energia devem ser definidos na integracao STM32.
