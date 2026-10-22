# Consulta e configuracao do OD por SDO

O script `tools/itm_od.py` usa a CAN RET e o CANalyst-II a 500 kbit/s. Instale
`python -m pip install -r tools/requirements-update.txt` antes do uso. O canal
padrao e 2 e o Node-ID padrao e 10; `--channel`, `--node` e `--driver` podem ser
passados depois do subcomando.

```powershell
python tools/itm_od.py list
python tools/itm_od.py show --channel 2 --node 10
python tools/itm_od.py read 0x1018:4 --channel 2 --node 10
python tools/itm_od.py configure --node-id 11 --heartbeat-ms 750 --channel 2 --node 10 --dry-run
python tools/itm_od.py configure --node-id 11 --heartbeat-ms 750 --channel 2 --node 10
```

`list` mostra as definicoes do EDS local, sem acessar a placa. `show` consulta
por SDO todos os objetos folha do EDS e indica indisponibilidade quando a
implementacao nao responde. `read` consulta um indice e subindice especifico.
O comando `configure` escreve apenas `0x2101:0` (Node-ID) e `0x1017:0`
(heartbeat) e envia a assinatura CANopen `save` para `0x1010:1`. Em seguida,
confere que `0x2100:0`, a versao da configuracao, aumentou uma unidade. O novo
Node-ID passa a valer somente no proximo reinicio do STM32; a sessao SDO atual
continua no ID anterior. O comando `save` isolado persiste parametros ja
escritos por outro cliente SDO e tambem incrementa a versao.

O script nao oferece escrita generica de OD. Os numeros de peca (`0x2107` e
`0x2108`) sao constantes da imagem em FLASH e devem ser substituidos no codigo
fonte antes de gerar e instalar uma nova versao do firmware. O numero de serie
(`0x1018:4`) e derivado do UID de fabrica (`0x2109`). A versao do firmware
(`0x100A` e `0x2105`) e o CRC (`0x210A`) vem do cabecalho da imagem ativa. A
versao da configuracao (`0x2100`) e incrementada exclusivamente pelo comando
de persistencia. Esses objetos permanecem somente leitura por SDO, conforme
REQ-ITM 20 e 21. A atualizacao da imagem usa `tools/update_itm.py`, nao uma
escrita SDO nesses objetos.

Na aplicacao v7, o EDS declara `0x1010:0`, mas uma consulta ao subindice retorna
aborto `0x06090011`. O comando exigido `0x1010:1` funciona e foi ensaiado;
essa divergencia entre EDS e firmware deve ser corrigida em uma proxima imagem.

Os ensaios de bancada da v7 verificaram leitura do OD, rejeicao de escritas nos
campos de gestao, mudanca temporaria de Node-ID 10 para 11, heartbeat 1000 para
750 ms, duas gravacoes em FLASH com versao de configuracao 0 -> 1 -> 2,
restauracao dos valores originais e retencao apos desligar e religar o ITM.
Os numeros de peca ainda sao provisorios e o ensaio nao qualificou quedas de
energia durante a gravacao do journal.
