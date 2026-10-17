# cosmofetch

Meu clone do fastfetch, feito em C puro. Mostra as informações do sistema do lado de um logo desenhado direto no terminal. Não tem dependência nenhuma além da libc.

Fiz por diversão e pra aprender, então não é uma cópia perfeita do original. Mas já tem bastante coisa: 65 módulos, temas, logos que se adaptam ao tamanho do terminal, saída em JSON e até imagem de verdade em alguns terminais.

## Instalando

Precisa só de `gcc` (ou `clang`) e `make`:

```sh
sudo apt install build-essential   # se ainda não tiver
git clone https://github.com/jotarps/cosmofetch
cd cosmofetch
make
./cosmofetch
```

Se quiser rodar de qualquer lugar, `sudo make install`.

## Como usar

```sh
./cosmofetch                                # do jeito padrão
./cosmofetch --logo galaxy --theme neon     # outro logo e outras cores
./cosmofetch --bars                         # barrinhas de memória, disco e bateria
./cosmofetch --structure title:os:cpu:memory   # só o que você quer ver
./cosmofetch --json                         # saída em JSON
./cosmofetch --list-modules                 # lista todos os módulos
./cosmofetch --help                         # todas as opções
```

## Trocando as cores

O jeito mais rápido é um tema pronto: `cosmo` (o padrão), `ocean`, `sunset`, `forest`, `mono` e `neon`.

```sh
./cosmofetch --theme sunset
```

Dá pra mexer nas cores uma por uma também. Aceita número de 0 a 255 ou nome (`red`, `pink`, `orange`, `aqua`...):

```sh
./cosmofetch --key-color orange --user-color 196
./cosmofetch --logo-color 196,202,208,214,220
```

## Arquivo de configuração

Pra não ficar digitando as opções toda vez:

```sh
mkdir -p ~/.config/cosmofetch
./cosmofetch --gen-config > ~/.config/cosmofetch/config.conf
```

Tudo que existe na linha de comando existe no arquivo também, no formato `chave = valor`. O arquivo gerado já vem comentado.

Você também pode criar seus próprios módulos com qualquer comando do terminal:

```ini
custom1 = IP público | curl -s --max-time 3 https://api.ipify.org
structure = title:separator:os:cpu:custom1:memory
```

## Os logos

O `planet` e o `galaxy` não são arte pronta: são calculados na hora, com sombreamento e tudo, e encolhem sozinhos se o terminal for estreito. Também tem `rocket`, `mountains` e `terminal`, que são desenhos fixos.

Quer usar o seu? `--logo-file arquivo.txt` aceita qualquer arte em texto (com cores ANSI inclusive).

Se o seu terminal for o kitty, WezTerm ou Ghostty, dá pra mostrar uma imagem PNG de verdade no lugar do logo:

```sh
./cosmofetch --image ~/foto.png
```

Em outros terminais isso funciona se você tiver o `chafa` instalado, que converte a imagem em blocos coloridos.

## O que ele mostra

Tem módulo pra quase tudo: sistema (os, kernel, uptime, pacotes, BIOS, placa-mãe...), hardware (cpu, gpu, memória, discos, bateria, temperatura, monitores...), ambiente (shell, terminal, DE, WM, tema GTK, fonte...) e rede (IP, MAC, gateway, DNS, Wi-Fi...). Rode `./cosmofetch --list-modules` pra ver a lista completa com a descrição de cada um.

Dois módulos fazem requisição na internet (`publicip` e `weather`), então não vêm ligados por padrão.

## Testes

```sh
make test    # roda cada módulo, cada logo, cada tema e confere o JSON
make debug   # compila com AddressSanitizer e UBSan, bom pra caçar bug
```

## Licença

MIT. Pode usar, copiar e mexer à vontade.
