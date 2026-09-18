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
- DBCC de baterias gerado do `Battery.dbc` com `cantools`, wrapper portatil,
  separacao entre as redes de 28 V e 150 V e testes de host.
- Contratos abstratos para relogio, saidas, flash, CAN, UART e protocolos.
- Testes de host e mocks de hardware.
- Alvo bare-metal STM32H723VGT6 com clock inicial de 8 MHz, GPIOs seguros,
  adaptadores basicos e artefatos ELF/BIN/HEX.

Os parsers BC/CEB e o mapeamento PDO da RET nao foram implementados porque seus
ICDs ainda nao estao definidos. O DBCC BMS esta parcial e ainda depende da
validacao das ambiguidades do DBC e dos parametros fisicos da CAN. Lely Core foi selecionada para CANopen,
mas o port STM32/FDCAN ainda depende da versao e configuracao embarcada da pilha.

## DBCC Das Baterias

O build consome código C versionado em `generated/can`; Python nao faz parte do
firmware. Para instalar o gerador fixado, regenerar e verificar os artefatos:

```sh
python -m pip install -r tools/requirements-dbc.txt
python tools/generate_battery_dbc.py --write
python tools/generate_battery_dbc.py --check
```

Detalhes e lacunas estao em `docs/11-dbcc-baterias.md`.

## Compilar Para O STM32H723

Consulte [Bring-up STM32H723](docs/09-bringup-stm32h723.md) para instalar as
ferramentas, compilar com CMake/Ninja, verificar a pinagem e gravar pela porta
SWD. A compilacao nao e evidência de funcionamento eletrico na placa.

## Selecionar O Hardware

O cache CMake `ITM_HARDWARE` seleciona exatamente uma plataforma:

| Valor | Uso |
| --- | --- |
| `HOST` | Nucleo portatil e testes no computador; valor padrao |
| `ITM_REV00` | Placa final ITM-100 com STM32H723VGT6 |
| `STM32H735G_DK` | Development kit para integracao antecipada |

Exemplo para a development kit:

```powershell
cmake -S . -B build-stm32h735g-dk -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-gcc.cmake `
  -DITM_HARDWARE=STM32H735G_DK -DITM_BUILD_TESTS=OFF `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-stm32h735g-dk
```

Use um diretorio de build diferente para cada hardware. Consulte
[Bring-up STM32H735G-DK](docs/10-bringup-stm32h735g-dk.md).

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
- [Mapa de hardware Rev00](docs/08-mapa-hardware-rev00.md)
- [Bring-up STM32H723](docs/09-bringup-stm32h723.md)
- [Bring-up STM32H735G-DK](docs/10-bringup-stm32h735g-dk.md)
- [Decisoes de arquitetura](docs/decisions/)

## Referencias Locais

- `CON-ML-ET-0006A-Especificacoes de Requisitos do Modulo de Interface (ITM).pdf`
- `Interface Tubeira Movel-Firmware V1_0.pptx`
- `ITM-100 Rev00 _ Schematic diagram.pdf`



