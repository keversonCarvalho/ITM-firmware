# Bring-Up STM32H735G-DK

## Hardware Revisado

A development kit usa STM32H735IGK6 em UFBGA176+25, HSE de 25 MHz e ST-LINK-V3E
integrado. O BSP oficial STM32CubeH7 V1.13.0 define:

| Recurso | MCU | Comportamento |
| --- | --- | --- |
| LED1 verde | PC3 | Ativo em nivel baixo |
| LED2 vermelho | PC2 | Ativo em nivel baixo |
| Botao USER | PC13 | Pressionado em nivel alto |
| ST-LINK VCP TX/RX | USART3 PD8/PD9 | UART de bancada |

## Selecionar E Compilar

Use um diretorio de build exclusivo da DK:

```powershell
cmake -S . -B build-stm32h735g-dk -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-gcc.cmake `
  -DITM_HARDWARE=STM32H735G_DK -DITM_BUILD_TESTS=OFF `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-stm32h735g-dk
```

Para voltar ao firmware da placa ITM, configure outro diretorio:

```powershell
cmake -S . -B build-stm32h723 -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-gcc.cmake `
  -DITM_HARDWARE=ITM_REV00 -DITM_BUILD_TESTS=OFF `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-stm32h723
```

Para testes no computador, omita `ITM_HARDWARE` ou use explicitamente
`-DITM_HARDWARE=HOST`. Nao altere apenas a variavel dentro de um build existente;
diretorios separados evitam usar startup e linker da placa errada.

## Gravar

Conecte o cabo na porta USB ST-LINK da DK:

```powershell
STM32_Programmer_CLI.exe -c port=SWD -w `
  build-stm32h735g-dk/platform/stm32h735g_dk/itm_stm32h735g_dk.hex `
  -v -rst
```

## Resultado Esperado

1. LED2 vermelho permanece apagado.
2. O FDCAN1 executa uma transmissao e recepcao em loopback interno no boot.
3. Se o loopback passar, LED1 verde alterna a cada 500 ms.
4. A porta virtual envia `ITM DK smoke test: PASS` em 115200 8N1.
5. Cada caractere recebido pela VCP e retransmitido.
6. Pressionar USER envia `USER button` uma vez por acionamento.
7. Falha de clock, UART ou FDCAN acende LED2 e interrompe a aplicacao.

O loopback usa classic CAN a 250 kbit/s derivado do HSE de 25 MHz. E um valor de
bancada, nao o bitrate do ITM. Como o modo e interno, ele nao valida transceptor,
conector, terminacao nem camada fisica CAN.

## Memoria E Evidencias

O build Release inicial usa 13.928 bytes de flash e 2.416 bytes dos 128 KiB de
DTCM declarados no linker. Foram gerados ELF, BIN, HEX e mapa. A verificacao de
pinagem confere o `.ioc` com as definicoes manuais da placa.

Ainda pendem evidencias na DK real: gravacao, oscilador, LEDs, botao, VCP e
resultado do loopback. Esses ensaios nao substituem o bring-up posterior da
placa ITM-100.

## Referencias Oficiais

- [Pagina do produto STM32H735G-DK](https://www.st.com/en/evaluation-tools/stm32h735g-dk.html)
- [Manual UM2679 da STM32H735G-DK](https://www.st.com/resource/en/user_manual/um2679-discovery-kit-with-stm32h735ig-mcu-stmicroelectronics.pdf)
- STM32CubeH7 V1.13.0, BSP `Drivers/BSP/STM32H735G-DK` e exemplo
  `Projects/STM32H735G-DK/Examples/FDCAN/FDCAN_Loopback`.
