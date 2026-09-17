# ADR 0002 - Controle De Potencia Fail-Safe

## Status

Provisoria, aguardando aprovacao formal dos estados seguros e intertravamentos.

## Contexto

REQ-ITM31 exige desligar SSRs apos perda da comunicacao com o BC. Os tempos,
realimentacoes e sequencia eletrica completos ainda nao foram definidos.

## Decisao

As saidas iniciam desligadas. Falha bloqueante, perda confirmada do BC, timeout
de transicao ou falha de escrita desliga primeiro o CB e depois a CEB e trava a
maquina em falha. Energizacao ocorre na ordem CEB e depois CB, mediante
confirmacoes abstratas e tempos configuraveis.

## Consequencias

- A politica pode ser testada agora e adaptada sem alterar drivers.
- Os valores de timeout nao possuem default de producao.
- A confirmacao `ceb_power_good` e provisoria; sua origem eletrica deve ser
  definida pelo ICD e pelos esquematicos.
