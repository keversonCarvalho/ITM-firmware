# ADR 0002 - Controle De Potencia Fail-Safe

## Status

Provisoria, aguardando aprovacao formal dos estados seguros e intertravamentos.

## Contexto

REQ-ITM31 exige desligar SSRs apos perda da comunicacao com o BC. O usuario
definiu que MFET1/CB e MFET2/CEB sao comandos independentes e que EM1/EM2
servem apenas ao diagnostico; as leituras analogicas nao intertravam saidas.

## Decisao

As saidas iniciam desligadas. O pedido seleciona OFF, CEB somente, CB somente
ou ambas. Cada GPIO muda de acordo com o estado solicitado sem depender do
outro GPIO ou de EM1/EM2. Os tempos configuraveis apenas atrasam a declaracao
do estado logico ON; nao atrasam o acionamento fisico. Falha de escrita ou
perda da permissao desliga ambas as saidas. O lease de comando de bancada de
1 s tambem solicita OFF quando nao e renovado.

## Consequencias

- A politica pode ser testada agora e adaptada sem alterar drivers.
- Os valores de timeout nao possuem default de producao.
- Os estados ON significam nivel GPIO comandado, nao tensao comprovada.
- A condicao calculada de EM1/EM2 permanece disponivel para diagnostico,
  mas nao altera o comando das saidas.
