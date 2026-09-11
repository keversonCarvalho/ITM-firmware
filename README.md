# ITM Firmware

Projeto de firmware do ITM-100, Interface de Tubeira Movel, usado para integrar o Sistema de Tubeira Movel ao Banco de Controle e a Rede de Telemetria.

Este repositorio comeca pela documentacao de projeto antes da implementacao. A intencao e manter requisitos, arquitetura, decisoes tecnicas, backlog e plano de validacao sempre rastreaveis ao material de referencia.

## Documentos De Projeto

- [Visao do projeto](docs/01-visao-do-projeto.md)
- [Requisitos iniciais](docs/02-requisitos-iniciais.md)
- [Arquitetura de firmware](docs/03-arquitetura-firmware.md)
- [Plano de desenvolvimento](docs/04-plano-desenvolvimento.md)
- [Questoes abertas](docs/06-questoes-abertas.md)
- [Matriz de rastreabilidade inicial](docs/07-matriz-rastreabilidade-inicial.md)

## Referencias Locais

- `CON-ML-ET-0006A-Especificacoes de Requisitos do Modulo de Interface (ITM).pdf`
- `Interface Tubeira Movel-Firmware V1_0.pptx`

Observacao: o PDF de requisitos foi extraido com senha fornecida pelo usuario e incorporado como referencia formal inicial. A apresentacao segue como referencia de arquitetura e contexto.

## Proximo Marco

O proximo marco recomendado e transformar estes documentos em um projeto de firmware versionado:

1. Inicializar Git e convencoes de contribuicao.
2. Definir alvo exato do microcontrolador STM32G473 e toolchain.
3. Criar esqueleto do firmware com camadas `app`, `services`, `protocols`, `drivers`, `platform` e `tests`.
4. Selecionar RTOS, pilha CANopen ECSS/CiA 301, estrategia de testes e biblioteca de mocks.
5. Construir simuladores/mocks para RS-422 do BC, RS-485 da CEB, CAN dos BMS e CANopen da RET antes de integrar hardware real.
