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
- Servico CANopen RET com identidade CiA 301, Vendor-ID Concert Space,
  configuracao por SDO e persistencia do Node-ID.
- EDS inicial do ITM-100 preparado para geracao estatica pela Lely `dcf2c`.
- Registro estatico de telemetria com catalogo, qualidade, freshness, snapshots
  consistentes, normalizacao inteira e ring buffers de capacidade fixa.
- Contratos abstratos para relogio, saidas, flash, CAN, UART e protocolos.
- Testes de host e mocks de hardware.

Os parsers BC/CEB/BMS e o mapeamento PDO da RET nao foram implementados porque
seus ICDs ainda nao estao definidos. Lely Core foi selecionada para CANopen,
mas o port STM32/FDCAN ainda depende da versao e configuracao embarcada da pilha.

## Compilar E Testar No Host

Pre-requisitos: CMake 3.20 ou superior e um compilador C11 (GCC, Clang ou MSVC).

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Como alternativa no ambiente atual, a suite foi compilada com GCC 11.4 no WSL,
usando `-Wall -Wextra -Wpedantic -Werror`. Os 24 testes passaram.

Quando `dcf2c` da Lely estiver instalado, o CMake disponibiliza o alvo opcional
que gera o Object Dictionary C estatico:

```powershell
cmake --build build --target itm_lely_od
```

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



