# ADR 0001 - Nucleo Portatil Em C11

## Status

Aceita para a primeira versao.

## Contexto

O microcontrolador, a HAL e o escalonamento final ainda dependem do projeto de
hardware. As regras de seguranca precisam ser testadas antes dessa integracao.

## Decisao

O nucleo sera escrito em C11, sem alocacao dinamica, e recebera hardware e tempo
por interfaces explicitas. O mesmo codigo sera compilado no host e no alvo.

## Consequencias

- Regras de aplicacao podem ser verificadas sem a placa.
- Adaptadores STM32, FreeRTOS e protocolos reais ficam fora do nucleo.
- Restricoes adicionais da ferramenta alvo poderao exigir reduzir o subconjunto
  de C utilizado, sem alterar os contratos dos modulos.
