# cosmofetch

Ferramenta de linha de comando que exibe informações do sistema ao lado de um logo, no estilo do fastfetch. Escrita em C, sem dependências além da libc.

## Recursos

- 65 módulos (sistema, hardware, ambiente gráfico e rede)
- Logos gerados em tempo de execução, que se ajustam à largura do terminal
- Temas de cores e personalização de cada elemento
- Arquivo de configuração e módulos customizados
- Saída em JSON
- Imagem no lugar do logo (kitty, WezTerm, Ghostty ou via `chafa`)

## Compilação

São necessários um compilador C (`gcc` ou `clang`) e o `make`.

```sh
git clone https://github.com/jotarps/cosmofetch
cd cosmofetch
make
./cosmofetch
```

Para instalar no sistema:

```sh
sudo make install
```

No Debian e no Ubuntu, `sudo apt install build-essential` instala tudo o que é preciso.

## Uso

```
cosmofetch [opções]
```

Exemplos:

```sh
cosmofetch --logo galaxy --theme neon
cosmofetch --bars
cosmofetch --structure title:os:cpu:memory
cosmofetch --json
```

Opções mais usadas:

| Opção | Descrição |
|---|---|
| `--structure LISTA` | Módulos a exibir, separados por `:` |
| `--theme NOME` | `cosmo`, `ocean`, `sunset`, `forest`, `mono` ou `neon` |
| `--logo NOME` | `planet`, `galaxy`, `rocket`, `mountains`, `terminal`, `random` ou `none` |
| `--logo-file ARQ` | Usa um arquivo de texto como logo |
| `--logo-position POS` | `left`, `right`, `top` ou `none` |
| `--image ARQ` | Exibe uma imagem PNG no lugar do logo |
| `--bars` | Mostra barras de porcentagem |
| `--json` | Saída em JSON |
| `--list-modules` | Lista todos os módulos |

A lista completa está em `cosmofetch --help`.

## Cores

As cores dos rótulos, do usuário e do logo podem ser alteradas por opção. Os valores aceitos são números de 0 a 255 ou nomes como `red`, `orange`, `pink` e `aqua`.

```sh
cosmofetch --key-color orange --user-color 196
cosmofetch --logo-color 196,202,208,214,220
```

## Configuração

O programa lê `~/.config/cosmofetch/config.conf`. Para gerar um arquivo de exemplo:

```sh
mkdir -p ~/.config/cosmofetch
cosmofetch --gen-config > ~/.config/cosmofetch/config.conf
```

O formato é `chave = valor`, e cada opção da linha de comando tem uma chave equivalente.

Módulos customizados executam um comando e exibem a primeira linha da saída:

```ini
custom1 = IP público | curl -s --max-time 3 https://api.ipify.org
structure = title:separator:os:cpu:custom1:memory
```

## Logos e imagens

Os logos `planet` e `galaxy` são calculados na hora e redimensionados conforme o terminal. Os demais são arte fixa. Qualquer arquivo de texto pode ser usado com `--logo-file`, inclusive com sequências de cor ANSI.

Para exibir uma imagem, o terminal precisa suportar o protocolo gráfico do kitty (kitty, WezTerm, Ghostty) e o arquivo precisa ser PNG. Nos demais terminais, a imagem é convertida por `chafa`, se estiver instalado.

## Módulos

| Grupo | Módulos |
|---|---|
| Sistema | os, host, board, bios, chassis, kernel, uptime, boottime, loadavg, date, time, datetime, timezone, processes, users, initsystem, bootmode, libc, virt, locale, lang, packages |
| Hardware | cpu, cpuusage, cpucache, gpu, memory, swap, disk, physicaldisk, battery, poweradapter, temperature, fans, brightness, sound, bluetooth, display |
| Ambiente | shell, editor, terminal, de, wm, theme, icons, font, cursor |
| Rede | localip, localip6, mac, gateway, dns, wifi, netio, publicip, weather, media |
| Customizados | custom1 a custom9 |

Os módulos `publicip` e `weather` acessam a internet e por isso não fazem parte da configuração padrão.

## Testes

```sh
make test     # executa cada módulo, logo e tema, e valida o JSON
make debug    # compila com AddressSanitizer e UBSan
```

## Licença

MIT. Veja o arquivo `LICENSE`.
