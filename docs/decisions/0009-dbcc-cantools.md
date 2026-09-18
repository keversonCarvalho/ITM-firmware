# ADR 0009 - DBCC Gerado Com Cantools

## Status

Aceita para a primeira integracao; semantica fisica do DBC ainda provisoria.

## Contexto

O ITM precisa decodificar o mesmo protocolo DBC em duas redes CAN de bateria,
sem parser, alocacao dinamica ou estruturas variaveis no microcontrolador.

## Decisao

- Fixar `cantools` 41.0.2, licenca MIT, como ferramenta de desenvolvimento.
- Versionar o C gerado e verificar sua correspondencia com o DBC.
- Nao executar Python nem interpretar DBC no STM32.
- Encapsular o gerado em `itm_dbcc`, que valida transporte, normaliza numeros,
  separa as origens e atualiza o registro estatico.
- Respeitar literalmente o DBC e registrar inconsistencias em vez de corrigi-las
  implicitamente.

## Consequencias

O codec e inspecionavel, reproduzivel e testavel no host e no ARM. Alteracoes no
DBC produzem diferencas revisaveis. O gerador e uma ferramenta nao qualificada;
por isso, vetores independentes, warnings como erros e testes cruzados sao
obrigatorios antes de aceitar uma nova versao.
