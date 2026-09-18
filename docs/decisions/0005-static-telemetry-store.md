# ADR 0005 - Armazenamento E Snapshot Estaticos De Telemetria

## Status

Aceita para a infraestrutura portatil; catalogo e tempos permanecem provisorios.

## Contexto

O ITM recebe dados de AV_BATT, ACT_BATT, CEB e Contactor Box e publica o ultimo
estado valido para a RET. Nao existe requisito atual de consulta historica,
armazenamento de campanhas ou processamento relacional no equipamento.

## Decisao

- Formalizar o armazenamento como KV de dominio fechado:
  `Key = itm_telemetry_key_t` (alias de `itm_signal_id_t`) e
  `Value = itm_telemetry_value_t` (alias de `itm_signal_runtime_t`).
- Implementar `put/get` por indexacao direta da chave, com pior caso O(1), sem
  hash, colisoes, busca ou alocacao. Manter `update/read` como aliases de
  compatibilidade.
- Nao permitir que produtores escrevam o valor runtime completo: qualidade e
  contador continuam controlados pelo store depois das validacoes.
- Usar um vetor estatico indexado por `itm_signal_id_t` para o ultimo valor.
- Separar metadados constantes de valores mutaveis em runtime.
- Representar medidas como inteiros escalados e expoente decimal, sem `float`.
- Validar tipo, faixa, origem e ordem temporal antes de aceitar atualizacoes.
- Classificar valores como `INVALID`, `VALID` ou `STALE`.
- Produzir snapshots consistentes sob uma porta abstrata de secao critica.
- Usar ring buffers estaticos apenas para dados que exigem preservacao de ordem.
- Disponibilizar tres politicas explicitas de buffer cheio: descartar o mais
  antigo, rejeitar o novo ou sinalizar falha.
- Entregar snapshots ao CANopen por interface, sem expor internamente o store.

## Concorrencia

Atualizacoes seguram a secao critica durante a validacao e escrita de um unico
sinal. O snapshot segura a mesma secao enquanto copia `ITM_SIGNAL_COUNT` valores.
O custo e limitado e cresce linearmente com a quantidade de sinais. A porta
permite usar mock no host e, futuramente, primitiva FreeRTOS ou mascaramento de
interrupcao aprovado no STM32.

## Memoria

Com oito sinais e ring de 16 eventos, medido em host x86-64:

| Estrutura | RAM |
| --- | ---: |
| `itm_telemetry_store_t` | 256 bytes |
| `itm_telemetry_snapshot_t` | 200 bytes |
| `itm_telemetry_ring_t` | 592 bytes |
| Total de uma instancia de cada | 1048 bytes |

Estimativa para ABI ARM de 32 bits: aproximadamente 240 bytes para store, 200
bytes para snapshot e 568 bytes por ring, totalizando cerca de 1008 bytes. Um
buffer que serialize todos os oito sinais acrescenta 128 bytes. O catalogo
constante ocupa aproximadamente 384 bytes de flash, nao RAM mutavel.

Formulas de dimensionamento:

```text
store    = N * sizeof(signal_runtime) + statistics + sequence + critical_port
snapshot = N * sizeof(signal_runtime) + 8 bytes + alinhamento
ring     = C * sizeof(telemetry_event) + indices + statistics + critical_port
wire     = N * 16 bytes
```

`N` e a quantidade de sinais e `C` e a capacidade do ring.

## Consequencias

- Uso de memoria e tempo de acesso sao limitados e calculaveis.
- Inclusao de sinais altera memoria em compilacao e exige nova validacao.
- O tempo maximo da secao critica do snapshot deve ser medido no STM32.
- O formato serializado e interno para testes; nao define TPDO nem ICD da RET.
