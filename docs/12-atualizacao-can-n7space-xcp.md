# Atualizacao de firmware por CAN: bancada STM32H723

## CANs das baterias na aplicacao

AV-BATT usa FDCAN2 (PB12 RX, PB13 TX) e ACC-BATT usa FDCAN3 (PD12 RX,
PD13 TX). Ambos operam em CAN classico a 500 kbit/s, com identificadores
estendidos de 29 bits definidos em `config/can/Battery.dbc`. O firmware aceita
somente os oito IDs desse DBC em cada porta, valida DLC e payload pelo codec
gerado e atualiza os 26 sinais por bateria no snapshot de telemetria. Essas duas portas
nao executam CANopen nem XCP e nao recebem atualizacoes de firmware.

Na compilacao de bancada `ITM_BUILD_UPDATE_BENCH=ON`, cada porta tambem aceita
uma consulta de verificacao CAN classica `0x7A0` com `ITM?` e nonce de quatro
bytes. Responde em `0x7A1` com nonce e contador de quadros DBC decodificados,
e em `0x7A2` com tensao em mV, SOC em centesimos de porcento e validade. A consulta
`ITMS` em `0x7A0` solicita um ID do catalogo e retorna metadados em `0x7A3` e
o valor inteiro de 64 bits em `0x7A4`. Esse
recurso fica fora das compilacoes sem a opcao de bancada e nao deve ser usado
como protocolo de bateria de voo.

Com AV-BATT no canal 2 do CANalyst-II e ACC-BATT no canal 1, sem bateria real
conectada aos respectivos barramentos, teste cada porta separadamente:

```powershell
python -m pip install -r tools/requirements-dbc.txt
python tools/simulate_battery_can.py --bus av --channel 2
python tools/simulate_battery_can.py --bus acc --channel 1
python tools/simulate_battery_can.py --bus av --channel 2 --dry-run
```

O simulador usa `cantools` para codificar diretamente as oito mensagens do DBC
como quadros estendidos. A versao 41.0.2 e fixada para instalacao reproduzivel;
o script confere vetores de referencia em vez de depender da metadata de versao.
Preenche todos os sinais, faz round-trip local
de cada payload, consulta o contador antes e depois e confere os 26 sinais
recebidos no snapshot do ITM. `--dry-run` mostra ID, DLC, bytes e sinais decodificados
sem transmitir. O perfil padrao usa amostras fisicas representativas e ignora
apenas os limites min/max inconsistentes do DBC (por exemplo, SOC [0|1]% e
tensao de celula [0|1] V). Para simular os limites literais do DBC, use
`--strict-dbc-limits`; nesse caso o SOC padrao e 0,5%.
O bitrate de 500 kbit/s e uma configuracao provisoria desta bancada; confirme
com o ICD dos BMS antes de integrar baterias reais.

Esta integracao e uma demonstracao de bancada: CANopen N7 Space v3.5.0 na
aplicacao e XCP on CAN no bootloader. XCP e CANopen compartilham o FDCAN1 da
RET, mas nao encapsulam um ao outro. O SDO `0x2103:0` solicita manutencao; o
bootloader recebe a imagem por XCP. A atualizacao nao deve ser usada em voo:
o pacote atual verifica CRC-32 e endereco, sem assinatura digital.

## Mapa de 1 MiB de flash interna

| Setor | Endereco | Uso |
| --- | --- | --- |
| 0 | `0x08000000` | Bootloader, 128 KiB, WRP obrigatorio |
| 1 | `0x08020000` | Copia 1 do journal de boot e configuracao, 128 KiB (v7) |
| 2 | `0x08040000` | Copia 2 do journal de boot e configuracao, 128 KiB (v7) |
| 3-4 | `0x08060000` | Imagem A, 256 KiB; vetor em `0x08060200` |
| 5-6 | `0x080A0000` | Imagem B, 256 KiB; vetor em `0x080A0200` |
| 7 | `0x080E0000` | Reservado; nao programado pelo atualizador |

O journal tem duas copias, cada uma em setor independente. Uma nova copia so
substitui a anterior apos a gravacao da segunda palavra de confirmacao. O
bootloader marca a candidata como tentada **antes** de saltar. A aplicacao
confirma apos dois segundos de execucao e inicializacao do CANopen. Reset antes
da confirmacao seleciona a imagem anterior. Falha das duas imagens mantem o
equipamento em recuperacao XCP, com as saidas seguras.
O formato compartilhado e a migracao do bootloader v6 para v7 estao descritos
em `docs/13-journal-configuracao.md`. A aplicacao v7 exige gravar o bootloader
v7 por SWD antes da atualizacao pela RET.

## Compilar

```powershell
cmake -S . -B build-update -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi-gcc.cmake `
  -DITM_HARDWARE=ITM_REV00 -DITM_BUILD_TESTS=OFF `
  -DITM_BUILD_UPDATE_BENCH=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-update --target itm_boot.elf itm_app_a.elf itm_app_b.elf
python tools/generate_n7space_od.py --check
```

Os binarios ficam em `build-update/platform/stm32h723/`. Cada aplicacao e
ligada para um endereco fixo. **Nunca envie o binario A para o slot B.** O
script `pack` verifica os vetores e o endereco antes de criar o pacote.

## Preparar a placa

1. Grave `itm_boot.hex` por SWD com o atuador desenergizado e as saidas em
   estado seguro. Verifique a imagem gravada antes de proteger o setor 0.
2. Configure WRP para o setor 0 e releia os option bytes para confirmar
   `WRPS0=0`. O driver de gravacao do atualizador recusa apagamento sem essa
   protecao. A gravacao de option bytes deve seguir um procedimento de
   laboratorio aprovado; o build nao modifica esses bytes.
3. Configure o transceptor RET em 500 kbit/s, CAN classico, 11 bits. Confirme
   terminacao e clock HSE de 8 MHz na placa Rev00.
4. No primeiro provisionamento, o journal esta apagado e nao ha aplicacao
   confirmada. O bootloader aguarda XCP. Envie uma imagem A inicial com
   `--already-in-bootloader`; apos iniciar, ela se confirma em dois segundos.

IDs XCP de bancada: recepcao `0x6F0`, resposta `0x6F1`. Esses IDs precisam ser
reservados no ICD da RET antes de uso em uma rede compartilhada. Node-ID
CANopen inicial: 10. O bootloader aguarda cinco segundos apos cada reset para
permitir conexao XCP. A ferramenta usa Python e `python-can` apenas no host.
Instale a dependencia fixada com
`python -m pip install -r tools/requirements-update.txt`.

## Atualizar a aplicacao a partir do PC

No PowerShell, dentro da pasta do projeto, use os dois comandos abaixo. O
primeiro recompila o codigo atual para os dois enderecos de aplicacao e gera
`.bin` e `.itmf` de mesma versao em `out/firmware/v6/`. Escolha um numero de
versao maior que o atualmente instalado. O segundo le pelo CANopen o slot e a
versao ativos, seleciona automaticamente o pacote do slot inativo, solicita
entrada no bootloader por SDO, envia a imagem por XCP on CAN e aguarda a
confirmacao da nova aplicacao.

```powershell
python -m pip install -r tools/requirements-update.txt
python tools/build_itm_update.py --version 6
python tools/update_itm.py out/firmware/v6
```

Os defaults do segundo comando correspondem a esta bancada: CANalyst-II,
canal 2, Node-ID 10 e DLL em `.tools/canalystii/ControlCAN.dll`. Se a DLL
estiver em outro lugar, passe `--driver "C:\caminho\ControlCAN.dll"`.
`python tools/update_itm.py --status` apenas consulta a placa;
`python tools/update_itm.py out/firmware/v6 --dry-run` valida o pacote e a
selecao sem reiniciar nem gravar. Para outro adaptador suportado por
`python-can`, passe `--interface` e `--channel`.

O pacote contem CRC-32 e verifica endereco e vetores antes do envio. O
bootloader grava apenas o slot inativo, valida a imagem candidata e preserva
a imagem confirmada para rollback caso a nova aplicacao nao se confirme. O
script retorna erro se nao observar a confirmacao. Este fluxo atualiza a
**aplicacao**; alteracoes no bootloader protegido exigem gravacao por SWD.

### Comandos de baixo nivel

```powershell
python tools/firmware_update.py pack `
  build-update/platform/stm32h723/itm_app_b.bin `
  build-update/platform/stm32h723/itm_app_b.itmf --slot 1 --version 2
python tools/firmware_update.py send `
  build-update/platform/stm32h723/itm_app_b.itmf `
  --interface pcan --channel PCAN_USBBUS1 --node 10
```

O argumento `--interface` segue os nomes aceitos por `python-can`; outros
adaptadores podem usar outro canal. Para o CANalyst-II, use `--interface
canalystii --channel 2 --driver <caminho-para-ControlCAN.dll>`; os canais
sao numerados 1 e 2, e o driver e a DLL de 64 bits do fabricante no Windows.
O programa de baixo nivel valida o pacote localmente,
envia o SDO de manutencao, conecta por XCP, confere o slot inativo, transfere
por `PROGRAM` em quadros CAN de ate seis bytes, manda a verificacao final e
reinicia. Depois le `0x2104` (slot), `0x2105` (versao) e `0x2106`
(confirmado). Perda de resposta interrompe a ferramenta; reconecte e reinicie
a transferencia completa. Nao ha retomada parcial.

## Ensaio de interrupcao do transporte

Com uma imagem confirmada em execucao, crie um pacote para o slot inativo e
execute `tools/bench_interrupt_update.py` com `--driver`, `--channel` e
`--bytes 12000`. O script inicia a programacao por XCP, envia apenas 12.000
bytes e encerra a conexao sem `PROGRAM` final. Apos o timeout XCP de 10 s,
consulta os objetos CANopen `0x2104` a `0x2106` e exige que a imagem anterior
continue ativa e confirmada. Em seguida, `firmware_update.py send` pode
recomecar a transferencia integral do slot inativo. Este ensaio verifica
interrupcao da comunicacao; queda de alimentacao durante erase/programacao
requer um teste separado com controle fisico da fonte.

## Verificacao e restricoes

`tools/test_host.py` compila os testes com GCC, AddressSanitizer e
UndefinedBehaviorSanitizer. Eles cobrem SDO real da N7 Space, heartbeat,
quadros XCP, validacao de imagem, falhas simuladas de gravacao, primeiro boot,
confirmacao e rollback. O build ARM cobre os tres linkers e a colocacao da
rotina de flash em DTCM. Ainda sao necessarios ensaios na placa, incluindo
quedas de alimentacao em cada fase, timing FDCAN, watchdog, duracao de erase,
WRP, reset e imunidade a erros de flash.

Para voo, substituir a politica CRC de bancada por assinatura verificada no
bootloader, gerir chaves e versoes, fixar os IDs no ICD e qualificar pilha,
driver, ferramenta de solo e procedimento de recuperacao no alvo real. O
parametro `ITM_BUILD_UPDATE_BENCH` e a definicao
`ITM_UPDATE_BENCH_CRC_ONLY` deixam esse limite explicito no artefato.
