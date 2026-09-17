# ITM Firmware

Projeto de firmware do ITM-100, Interface de Tubeira Movel, usado para integrar o Sistema de Tubeira Movel ao Banco de Controle e a Rede de Telemetria.

O repositorio contem um nucleo portatil em C11 para desenvolver e testar as
regras do equipamento antes da integracao com STM32, FreeRTOS e protocolos
reais. Requisitos, arquitetura, decisoes e testes permanecem rastreaveis ao
material de referencia.

## Estado Atual

- Supervisor de comunicacao com o BC e bloqueio apos perdas configuraveis.
- Maquina de estados de potencia com sequenciamento CEB/CB e fail-safe.
- Monitor de EM1/EM2 com limiares e debounce configuraveis.
- Diagnosticos em memoria, sem alocacao dinamica.
- Persistencia abstrata em dois slots com CRC-32 e commit transacional.
- Contratos abstratos para relogio, saidas, flash, CAN, UART e protocolos.
- Testes de host e mocks de hardware.

Os parsers BC/CEB/BMS e CANopen nao foram implementados porque seus ICDs ou a
selecao da pilha ainda nao estao definidos.

## Compilar E Testar No Host

Pre-requisitos: CMake 3.20 ou superior e um compilador C11 (GCC, Clang ou MSVC).

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Neste computador o CMake foi encontrado, mas nenhum compilador C estava
disponivel no terminal durante a criacao desta versao.

## Documentos De Projeto

- [Visao do projeto](docs/01-visao-do-projeto.md)
- [Requisitos iniciais](docs/02-requisitos-iniciais.md)
- [Arquitetura de firmware](docs/03-arquitetura-firmware.md)
- [Plano de desenvolvimento](docs/04-plano-desenvolvimento.md)
- [Questoes abertas](docs/06-questoes-abertas.md)
- [Matriz de rastreabilidade inicial](docs/07-matriz-rastreabilidade-inicial.md)
- [Decisoes de arquitetura](docs/decisions/)

## Referencias Locais

- `CON-ML-ET-0006A-Especificacoes de Requisitos do Modulo de Interface (ITM).pdf`
- `Interface Tubeira Movel-Firmware V1_0.pptx`



