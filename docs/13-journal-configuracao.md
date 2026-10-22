# Journal compartilhado de boot e configuracao (v7)

## Mapa e contrato

O STM32H723VGT6 conserva os enderecos de imagem existentes. O setor 0 contem o
bootloader com WRP; os setores 1 e 2 (`0x08020000` e `0x08040000`) guardam copias
alternadas do mesmo registro transacional; os setores 3-4 contem a imagem A, os
setores 5-6 a imagem B, e o setor 7 continua reservado. Gravar configuracao nao
apaga nenhuma imagem. Gravar imagem nao apaga a configuracao.

Cada registro ocupa duas palavras fisicas de 32 bytes. A primeira contem magic,
formato, geracao, estado de boot (`confirmed`, `candidate`, `tried`), Node-ID,
heartbeat, versao da configuracao e CRC-32. A segunda contem marcador de commit,
geracao e CRC repetidos. A segunda palavra e gravada por ultimo. Na partida, o
bootloader e a aplicacao escolhem a copia valida de maior geracao. Se a nova
escrita for interrompida, a outra copia permanece valida. Um erro de leitura em
uma copia nao impede tentar a outra; ambas invalidas impedem qualquer apagamento
automatico de uma imagem desconhecida.

O formato preserva o magic e o esquema do journal v6 para permitir a leitura do
estado de boot anterior. Os bytes antes reservados passam a ter um marcador de
configuracao; o bootloader v7 converte um journal antigo gravando a copia oposta
antes de iniciar a aplicacao. A configuracao padrao de migracao e Node-ID 10,
heartbeat 1000 ms e versao 0. A aplicacao v7 recusa a persistencia se ainda nao
encontrar o marcador de migracao.

## SDO e informacoes de gestao

`0x2101:0` aceita Node-ID entre 1 e 127. `0x1017:0` configura o periodo do
heartbeat. `0x1010:1` com assinatura CANopen `save` grava os dois parametros e
incrementa a versao `0x2100:0` somente apos verificar a nova copia na FLASH. O
novo Node-ID passa a valer no proximo reset; o SDO atual continua no ID antigo.
O comando de persistencia so e aceito depois da confirmacao da imagem ativa.

O OD expõe, somente para leitura, o numero de serie `0x1018:4` derivado do UID
de 96 bits e o UID completo em hexadecimal em `0x2109`. A versao real da imagem
esta em `0x2105` e `0x100A`; `0x210A` fornece o CRC-32 do corpo da imagem,
gravado no cabecalho do slot ativo. `0x2107` e `0x2108` contêm numeros de peca
provisorios de hardware e firmware; trocar pelos codigos oficiais antes de uma
linha de base de configuracao. O CRC-32 de bancada nao autentica firmware.

## Migracao da placa v6

**Nao envie a aplicacao v7 com o bootloader v6.** O pacote
`out/firmware/v7/` inclui `itm_boot.hex` e `itm_boot.bin` para gravacao por SWD,
alem das duas imagens e pacotes XCP. A ferramenta `update_itm.py` recusa esse
pacote ate que o operador indique `--bootloader-migrated`.

O procedimento de bancada deve primeiro copiar e conferir os setores 0, 1, 2,
3-4 e 5-6, confirmar a alimentacao, desabilitar temporariamente WRP do setor 0,
gravar **apenas** o novo `itm_boot.hex` por SWD, verificar byte a byte e voltar
a habilitar WRP. Nao executar mass erase nem apagar os setores 1 e 2. Depois do
reset, o novo bootloader converte o journal v6 e inicia a aplicacao v6. So
entao conectar a RET e executar:

```powershell
python tools/update_itm.py out/firmware/v7 --channel 2 --dry-run
python tools/update_itm.py out/firmware/v7 --channel 2 --bootloader-migrated
```

A migracao do bootloader nao pode ocorrer pela atualizacao CAN atual, pois o
setor 0 esta protegido. Este procedimento requer revisao do estado real dos
option bytes e ensaio de interrupcao de energia antes da gravacao na placa.

## Limite operacional

A implementacao atual alterna os setores e apaga o destino a cada commit.
No STM32H723, um erase de 128 KiB pode demorar segundos; durante a operacao o
driver executa em RAM com interrupcoes mascaradas. Portanto, a persistencia SDO
e um procedimento de integracao/manutencao, nao uma escrita durante comunicacao
em carga maxima. Ainda faltam ensaios de queda de energia em cada etapa,
endurance, ECC, watchdog e perda de quadros antes de qualificar este fluxo para
voo.
