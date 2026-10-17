# cosmofetch

Um clone do **fastfetch** escrito em **C puro** (só libc), para Linux.
65 módulos, logos gerados por código, temas, barras de porcentagem, saída JSON,
módulos customizados e suporte a **imagem de verdade** no terminal.

```
                          ####OOooo*+;    =====            root@vm
                       ##########OOOo**+:----   ===--=     -------
                     ##@@@@@@@@@###Ooo**+;.==---==- ==-=    OS: Ubuntu 24.04 x86_64
                    #@@@@@@@@@##OOoo*++;;:..   ==-== =--    Kernel: Linux 6.18
```

## Instalar

```sh
git clone https://github.com/SEU_USUARIO/cosmofetch
cd cosmofetch
make
./cosmofetch
sudo make install        # opcional (PREFIX=/usr/local por padrão)
```

Só precisa de `gcc` ou `clang` e `make`. Sem dependências além da libc.
Opcionais (módulos que usam ferramentas externas, se existirem): `lspci` (GPU),
`xrandr` (monitores no X11), `chafa` (imagens), `playerctl`, `curl`.

## Uso

```sh
cosmofetch                                  # padrão
cosmofetch --logo galaxy --theme neon       # outro logo e tema
cosmofetch --bars                           # barras de memória/disco/bateria
cosmofetch --structure title:os:cpu:memory  # só o que você quiser
cosmofetch --json                           # saída em JSON
cosmofetch --logo-position right
cosmofetch --image ~/foto.png               # imagem real (kitty/WezTerm/Ghostty)
cosmofetch --list-modules                   # todos os módulos
cosmofetch --help
```

## Configuração

```sh
mkdir -p ~/.config/cosmofetch
cosmofetch --gen-config > ~/.config/cosmofetch/config.conf
```

Todas as opções da linha de comando existem também no arquivo (`chave = valor`).
Veja `config/config.example.conf`.

### Módulos customizados

```ini
custom1 = IP Publico | curl -s --max-time 3 https://api.ipify.org
structure = title:separator:os:cpu:custom1:memory
```

## Módulos

| Grupo | Módulos |
|---|---|
| layout | title, separator, break, colors, version |
| sistema | os, host, board, bios, chassis, kernel, uptime, boottime, loadavg, date, time, datetime, timezone, processes, users, initsystem, bootmode, libc, virt, locale, lang, packages |
| hardware | cpu, cpuusage, cpucache, gpu, memory, swap, disk, physicaldisk, battery, poweradapter, temperature, fans, brightness, sound, bluetooth, display |
| ambiente | shell, editor, terminal, de, wm, theme, icons, font, cursor |
| rede | localip, localip6, mac, gateway, dns, wifi, netio, publicip, weather, media |
| custom | custom1 … custom9 |

`publicip` e `weather` fazem requisição de rede e **não** estão na estrutura padrão.

## Logos

`planet` e `galaxy` são **gerados por código** (sombreamento em tempo real, sem
arte pronta); `rocket`, `mountains` e `terminal` são arte estática.
`--logo-file arquivo.txt` usa qualquer arte sua (aceita cores ANSI);
`--logo-color 213,177,141,105` define o degradê; `--seed N` muda os procedurais.

## Imagens

- **kitty / WezTerm / Ghostty**: PNG enviado direto pelo protocolo gráfico do kitty.
- **Outros terminais**: se o `chafa` estiver instalado, a imagem é convertida em blocos coloridos.

## Testes

```sh
make test      # roda cada módulo, cada logo, layouts, temas e valida o JSON
make debug     # build com AddressSanitizer + UBSan
```

## Licença

MIT
