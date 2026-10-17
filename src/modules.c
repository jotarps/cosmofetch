/* Registro de todos os modulos disponiveis. */
#include "cosmofetch.h"

#include <string.h>

#define DECL(n) void mod_##n(Out *);
/* sistema */
DECL(title) DECL(separator) DECL(break) DECL(colors) DECL(version) DECL(os) DECL(host) DECL(board) DECL(bios)
DECL(chassis) DECL(kernel) DECL(uptime) DECL(boottime) DECL(loadavg) DECL(date) DECL(time) DECL(datetime)
DECL(timezone) DECL(processes) DECL(users) DECL(initsystem) DECL(bootmode) DECL(libc) DECL(virt) DECL(locale)
DECL(lang) DECL(packages)
DECL(custom1) DECL(custom2) DECL(custom3) DECL(custom4) DECL(custom5) DECL(custom6) DECL(custom7) DECL(custom8)
DECL(custom9)
/* hardware */
DECL(cpu) DECL(cpuusage) DECL(cpucache) DECL(gpu) DECL(memory) DECL(swap) DECL(disk) DECL(physicaldisk)
DECL(battery) DECL(poweradapter) DECL(temperature) DECL(fans) DECL(brightness) DECL(sound) DECL(bluetooth)
DECL(display)
/* ambiente */
DECL(shell) DECL(editor) DECL(terminal) DECL(de) DECL(wm) DECL(theme) DECL(icons) DECL(font) DECL(cursor)
/* rede e extras */
DECL(localip) DECL(localip6) DECL(mac) DECL(gateway) DECL(dns) DECL(wifi) DECL(netio) DECL(publicip)
DECL(weather) DECL(media)

#define M(n, g, d) {#n, g, d, mod_##n}

const Module g_modules[] = {
    /* layout */
    M(title, "layout", "usuario@host"),
    M(separator, "layout", "linha de tracos abaixo do titulo"),
    M(break, "layout", "linha em branco"),
    M(colors, "layout", "paleta de 16 cores do terminal"),
    M(version, "layout", "versao do cosmofetch"),
    /* sistema */
    M(os, "sistema", "distribuicao e arquitetura"),
    M(host, "sistema", "modelo da maquina"),
    M(board, "sistema", "placa-mae"),
    M(bios, "sistema", "BIOS/UEFI (fabricante, versao, data)"),
    M(chassis, "sistema", "tipo de chassi (desktop, laptop...)"),
    M(kernel, "sistema", "versao do kernel"),
    M(uptime, "sistema", "tempo ligado"),
    M(boottime, "sistema", "data e hora do boot"),
    M(loadavg, "sistema", "carga media (1, 5, 15 min)"),
    M(date, "sistema", "data atual"),
    M(time, "sistema", "hora atual"),
    M(datetime, "sistema", "data e hora atuais"),
    M(timezone, "sistema", "fuso horario"),
    M(processes, "sistema", "quantidade de processos"),
    M(users, "sistema", "usuarios logados"),
    M(initsystem, "sistema", "sistema de init (systemd, openrc...)"),
    M(bootmode, "sistema", "UEFI/BIOS e Secure Boot"),
    M(libc, "sistema", "biblioteca C (glibc, musl)"),
    M(virt, "sistema", "virtualizacao / container"),
    M(locale, "sistema", "locale"),
    M(lang, "sistema", "idioma"),
    M(packages, "sistema", "pacotes (pacman, dpkg, rpm, apk, flatpak, snap, brew...)"),
    /* hardware */
    M(cpu, "hardware", "modelo, threads e frequencia"),
    M(cpuusage, "hardware", "uso de CPU (amostra de 200 ms)"),
    M(cpucache, "hardware", "caches L1/L2/L3"),
    M(gpu, "hardware", "placas de video"),
    M(memory, "hardware", "memoria RAM"),
    M(swap, "hardware", "swap"),
    M(disk, "hardware", "particoes montadas"),
    M(physicaldisk, "hardware", "discos fisicos (modelo, tamanho, SSD/HDD/NVMe)"),
    M(battery, "hardware", "bateria (nivel, status, saude)"),
    M(poweradapter, "hardware", "carregador conectado ou nao"),
    M(temperature, "hardware", "temperatura da CPU"),
    M(fans, "hardware", "ventoinhas (RPM)"),
    M(brightness, "hardware", "brilho da tela"),
    M(sound, "hardware", "placas de som"),
    M(bluetooth, "hardware", "adaptador e dispositivos conectados"),
    M(display, "hardware", "monitores (resolucao, taxa, polegadas)"),
    /* ambiente */
    M(shell, "ambiente", "shell e versao"),
    M(editor, "ambiente", "editor padrao ($VISUAL/$EDITOR)"),
    M(terminal, "ambiente", "emulador de terminal"),
    M(de, "ambiente", "ambiente grafico"),
    M(wm, "ambiente", "gerenciador de janelas / compositor"),
    M(theme, "ambiente", "tema GTK"),
    M(icons, "ambiente", "tema de icones"),
    M(font, "ambiente", "fonte do sistema"),
    M(cursor, "ambiente", "tema e tamanho do cursor"),
    /* rede */
    M(localip, "rede", "IPv4 local de cada interface"),
    M(localip6, "rede", "IPv6 global de cada interface"),
    M(mac, "rede", "endereco MAC"),
    M(gateway, "rede", "gateway padrao"),
    M(dns, "rede", "servidores DNS"),
    M(wifi, "rede", "rede Wi-Fi conectada"),
    M(netio, "rede", "trafego total desde o boot"),
    M(publicip, "rede", "IP publico (faz requisicao de rede, opt-in)"),
    M(weather, "rede", "clima via wttr.in (requisicao de rede, opt-in)"),
    M(media, "rede", "musica tocando (playerctl)"),
    /* customizados */
    M(custom1, "custom", "comando customizado 1 (config: custom1 = Rotulo | comando)"),
    M(custom2, "custom", "comando customizado 2"),
    M(custom3, "custom", "comando customizado 3"),
    M(custom4, "custom", "comando customizado 4"),
    M(custom5, "custom", "comando customizado 5"),
    M(custom6, "custom", "comando customizado 6"),
    M(custom7, "custom", "comando customizado 7"),
    M(custom8, "custom", "comando customizado 8"),
    M(custom9, "custom", "comando customizado 9"),
};

const int g_nmodules = (int)(sizeof g_modules / sizeof g_modules[0]);

const Module *module_find(const char *name) {
    for (int i = 0; i < g_nmodules; i++)
        if (!strcmp(g_modules[i].name, name)) return &g_modules[i];
    return NULL;
}
