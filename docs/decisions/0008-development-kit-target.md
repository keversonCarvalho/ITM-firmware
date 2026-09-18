# ADR 0008 - STM32H735G-DK Como Plataforma De Desenvolvimento

## Estado

Aceita.

## Decisao

A STM32H735G-DK sera usada para integracao antecipada enquanto a placa ITM-100
nao estiver disponivel. Ela tem um STM32H735IGK6, portanto nao compartilha
pinagem, encapsulamento, clock ou board support com o STM32H723VGT6 do produto.

O CMake seleciona a plataforma por `ITM_HARDWARE`: `HOST`, `ITM_REV00` ou
`STM32H735G_DK`. Cada alvo embarcado possui startup, linker, `.ioc` e definicoes
de placa independentes, mas liga o mesmo `itm_core`.

## Configuracoes De Bancada

- SYSCLK direto do HSE de 25 MHz, sem PLL.
- USART3 PD8/PD9 a 115200 8N1 para ST-LINK VCP.
- FDCAN1 em loopback interno classic CAN a 250 kbit/s.
- LED1 PC3 para heartbeat e LED2 PC2 para falha, ambos ativos em nivel baixo.
- Botao de usuario em PC13 lido por polling.

Os baud rates acima nao sao configuracoes do produto nem requisitos de
protocolo. Servem somente para smoke test reproduzivel na DK.

## Consequencias

- A DK valida toolchain, inicializacao, HAL, relogio, secao critica e integracao
  inicial de UART/FDCAN.
- Nenhum resultado na DK comprova pinagem, estados seguros, isolamento,
  transceptores, ADC ou potencia da placa ITM.
- Builds de placas diferentes nao devem compartilhar diretorio de cache CMake.
