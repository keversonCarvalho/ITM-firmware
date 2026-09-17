# Bring-Up STM32H723VGT6

## Escopo

Esta etapa produz um firmware bare-metal minimo para a placa ITM-100 Rev00. Ele
configura clock, GPIOs seguros, relogio monotonico de milissegundos e secao
critica por PRIMASK. Nao habilita ADC, FDCAN, UART, flash de parametros, DMA,
RTOS ou CANopen.

## Dependencias Fixadas

- STM32CubeH7 V1.13.0, fonte oficial de CMSIS, HAL e startup.
- Arm GNU Toolchain 12.2.1 (`arm-none-eabi-gcc`).
- CMake 3.20 ou superior.
- Ninja 1.13.2 ou compativel.
- STM32CubeMX 6.18.0-RC3 foi usado somente para validar o `.ioc`; uma versao
  estavel deve ser selecionada antes de congelar a ferramenta.

Defina `STM32_CUBE_H7_PATH` quando o pacote nao estiver no diretorio padrao
`%USERPROFILE%/STM32Cube/Repository/STM32Cube_FW_H7_V1.13.0`.

## Compilacao

```powershell
cmake -S . -B build-stm32h723 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-gcc.cmake `
  -DITM_BUILD_STM32H723=ON -DITM_BUILD_TESTS=OFF `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-stm32h723
```

O build gera `itm_stm32h723.elf`, `.bin`, `.hex` e `.map` em
`build-stm32h723/platform/stm32h723`. A verificacao de pinagem e executada como
dependencia do alvo quando PowerShell esta disponivel; tambem pode ser chamada
diretamente:

```powershell
pwsh -File tools/verify_stm32_pinmap.ps1
```

## Memoria Medida

Build Release com GCC 12.2.1:

| Regiao | Uso | Capacidade declarada | Percentual |
| --- | ---: | ---: | ---: |
| Flash | 4.964 bytes | 1.048.576 bytes | 0,47% |
| DTCM RAM | 2.096 bytes | 131.072 bytes | 1,60% |

O relatorio `size` apresentou `text=4948`, `data=16` e `bss=2080`. O mapa
inicial nao disponibiliza AXI SRAM nem SRAM1-4. Isso e intencional ate a politica
de MPU/cache/DMA ser definida. A verificacao de simbolos nao encontrou
`malloc`, `calloc`, `realloc`, `free` ou `_sbrk` na imagem.

## Gravacao E Primeiro Ensaio

Com ST-LINK conectado por SWD e STM32CubeProgrammer instalado:

```powershell
STM32_Programmer_CLI.exe -c port=SWD -w `
  build-stm32h723/platform/stm32h723/itm_stm32h723.hex -v -rst
```

Antes de energizar cargas, desconecte-as ou use fonte limitada em corrente.
Confirme com osciloscopio/analisador logico que MFET1, MFET2, UART4_DE,
UART5_DE e CAN1_TERM permanecem baixos desde o reset. O LED PC6 deve alternar a
cada 500 ms e SWD deve permanecer acessivel.

## Evidencias E Pendencias

Validado sem placa: compilacao ARM com warnings como erros, geracao dos quatro
artefatos, carga/salvamento do `.ioc` no CubeMX, mapa de pinos automatizado,
consumo de memoria e ausencia de simbolos de alocacao dinamica.

Pendente em hardware: tensoes de alimentacao, reset/PGOOD, startup do HSE,
estados eletricos durante reset, LED, SWD, nivel e polaridade de FAULT1/2 e
comportamento do `Error_Handler`. Compilacao nao substitui essas medicoes.
