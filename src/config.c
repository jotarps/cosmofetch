/* Configuracao: valores padrao, temas, arquivo .conf e opcoes da CLI. */
#include "cosmofetch.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define DEFAULT_STRUCTURE                                                                               \
    "title:separator:os:host:kernel:uptime:packages:shell:display:de:wm:theme:icons:font:cursor:"       \
    "terminal:cpu:gpu:memory:swap:disk:localip:battery:poweradapter:locale:break:colors"

typedef struct {
    const char *name;
    int key, user, host, sep;
    int pal[12];
    int npal;
} Theme;

static const Theme THEMES[] = {
    {"cosmo", 75, 213, 51, 244, {0}, 0},
    {"ocean", 39, 45, 87, 244, {17, 18, 24, 25, 31, 32, 38, 44, 45, 51, 87, 123}, 12},
    {"sunset", 208, 203, 220, 244, {52, 88, 124, 160, 196, 202, 208, 214, 220, 226, 229}, 11},
    {"forest", 71, 118, 154, 244, {22, 28, 34, 40, 46, 82, 118, 154, 190}, 9},
    {"mono", 252, 255, 250, 240, {236, 240, 244, 248, 252, 255}, 6},
    {"neon", 207, 201, 51, 244, {53, 89, 125, 161, 197, 201, 207, 213, 219, 51, 87}, 11},
};

static int apply_theme(const char *name) {
    for (size_t i = 0; i < sizeof THEMES / sizeof THEMES[0]; i++) {
        if (strcasecmp(THEMES[i].name, name)) continue;
        const Theme *t = &THEMES[i];
        g_cfg.key_color = t->key;
        g_cfg.user_color = t->user;
        g_cfg.host_color = t->host;
        g_cfg.sep_color = t->sep;
        g_cfg.n_theme_palette = t->npal;
        for (int k = 0; k < t->npal; k++) g_cfg.theme_palette[k] = t->pal[k];
        scpy(g_cfg.theme, sizeof g_cfg.theme, t->name);
        return 0;
    }
    return -1;
}

void config_defaults(void) {
    memset(&g_cfg, 0, sizeof g_cfg);
    g_cfg.color = -1;
    g_cfg.bar_width = 10;
    g_cfg.truncate = 1;
    g_cfg.logo_padding = 2;
    g_cfg.logo_gap = 4;
    g_cfg.logo_center = 1;
    g_cfg.image_width = 30;
    scpy(g_cfg.separator, sizeof g_cfg.separator, ": ");
    scpy(g_cfg.structure, sizeof g_cfg.structure, DEFAULT_STRUCTURE);
    scpy(g_cfg.logo, sizeof g_cfg.logo, "planet");
    scpy(g_cfg.logo_position, sizeof g_cfg.logo_position, "left");
    scpy(g_cfg.image_protocol, sizeof g_cfg.image_protocol, "auto");
    scpy(g_cfg.weather_location, sizeof g_cfg.weather_location, "");
    apply_theme("cosmo");
}

static int parse_bool(const char *v) {
    return !strcasecmp(v, "1") || !strcasecmp(v, "true") || !strcasecmp(v, "yes") || !strcasecmp(v, "on") ||
           !strcasecmp(v, "sim");
}

static int parse_color(const char *v) {
    static const struct {
        const char *n;
        int c;
    } names[] = {{"black", 0},   {"red", 1},    {"green", 2},  {"yellow", 3}, {"blue", 4},   {"magenta", 5},
                 {"cyan", 6},    {"white", 7},  {"pink", 213}, {"orange", 208}, {"purple", 141}, {"lime", 118},
                 {"aqua", 51},   {"gray", 244}, {"grey", 244}, {"gold", 220},  {"none", -1}};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if (!strcasecmp(names[i].n, v)) return names[i].c;
    char *end;
    long n = strtol(v, &end, 10);
    if (end == v || n < -1 || n > 255) return -2;
    return (int)n;
}

static int parse_color_list(const char *v, int *dst, int *count) {
    char buf[256];
    scpy(buf, sizeof buf, v);
    int n = 0;
    char *save = NULL;
    for (char *t = strtok_r(buf, ", ", &save); t && n < MAX_PALETTE; t = strtok_r(NULL, ", ", &save)) {
        int c = parse_color(t);
        if (c < 0) return -1;
        dst[n++] = c;
    }
    *count = n;
    return 0;
}

int config_set(const char *key, const char *val) {
    if (!strcmp(key, "structure")) {
        scpy(g_cfg.structure, sizeof g_cfg.structure, val);
    } else if (!strcmp(key, "separator")) {
        scpy(g_cfg.separator, sizeof g_cfg.separator, val);
    } else if (!strcmp(key, "theme")) {
        if (apply_theme(val)) return -1;
    } else if (!strcmp(key, "key_color") || !strcmp(key, "user_color") || !strcmp(key, "host_color") ||
               !strcmp(key, "sep_color")) {
        int c = parse_color(val);
        if (c == -2) return -1;
        if (!strcmp(key, "key_color")) g_cfg.key_color = c;
        else if (!strcmp(key, "user_color")) g_cfg.user_color = c;
        else if (!strcmp(key, "host_color")) g_cfg.host_color = c;
        else g_cfg.sep_color = c;
    } else if (!strcmp(key, "logo")) {
        scpy(g_cfg.logo, sizeof g_cfg.logo, val);
        g_cfg.logo_file[0] = 0;
    } else if (!strcmp(key, "logo_file")) {
        scpy(g_cfg.logo_file, sizeof g_cfg.logo_file, val);
    } else if (!strcmp(key, "logo_color") || !strcmp(key, "logo_colors")) {
        if (parse_color_list(val, g_cfg.logo_colors, &g_cfg.n_logo_colors)) return -1;
    } else if (!strcmp(key, "logo_position")) {
        if (strcmp(val, "left") && strcmp(val, "right") && strcmp(val, "top") && strcmp(val, "none")) return -1;
        scpy(g_cfg.logo_position, sizeof g_cfg.logo_position, val);
    } else if (!strcmp(key, "logo_padding")) {
        g_cfg.logo_padding = atoi(val);
    } else if (!strcmp(key, "logo_gap")) {
        g_cfg.logo_gap = atoi(val);
    } else if (!strcmp(key, "logo_center")) {
        g_cfg.logo_center = parse_bool(val);
    } else if (!strcmp(key, "image")) {
        scpy(g_cfg.image, sizeof g_cfg.image, val);
    } else if (!strcmp(key, "image_protocol")) {
        if (strcmp(val, "auto") && strcmp(val, "kitty") && strcmp(val, "chafa")) return -1;
        scpy(g_cfg.image_protocol, sizeof g_cfg.image_protocol, val);
    } else if (!strcmp(key, "image_width")) {
        g_cfg.image_width = atoi(val) > 0 ? atoi(val) : 30;
    } else if (!strcmp(key, "image_height")) {
        g_cfg.image_height = atoi(val) > 0 ? atoi(val) : 0;
    } else if (!strcmp(key, "bars")) {
        g_cfg.bars = parse_bool(val);
    } else if (!strcmp(key, "bar_width")) {
        g_cfg.bar_width = atoi(val) > 3 ? atoi(val) : 10;
    } else if (!strcmp(key, "key_width")) {
        g_cfg.key_width = atoi(val) > 0 ? atoi(val) : 0;
    } else if (!strcmp(key, "truncate")) {
        g_cfg.truncate = parse_bool(val);
    } else if (!strcmp(key, "seed")) {
        g_cfg.seed = (unsigned)strtoul(val, NULL, 10);
        g_cfg.seed_set = 1;
    } else if (!strcmp(key, "weather_location")) {
        scpy(g_cfg.weather_location, sizeof g_cfg.weather_location, val);
    } else if (!strcmp(key, "color")) {
        if (!strcasecmp(val, "auto")) g_cfg.color = -1;
        else g_cfg.color = parse_bool(val);
    } else if (!strncmp(key, "custom", 6) && key[6] >= '1' && key[6] <= '9' && key[7] == 0) {
        scpy(g_cfg.custom[key[6] - '1'], sizeof g_cfg.custom[0], val);
    } else {
        return -1;
    }
    return 0;
}

void config_default_path(char *out, size_t n) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg) snprintf(out, n, "%s/cosmofetch/config.conf", xdg);
    else if (home && *home) snprintf(out, n, "%s/.config/cosmofetch/config.conf", home);
    else out[0] = 0;
}

int config_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[2048];
    int ln = 0;
    while (fgets(line, sizeof line, f)) {
        ln++;
        char *s = trim(line);
        if (!*s || *s == '#' || *s == ';') continue;
        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "cosmofetch: %s:%d: linha invalida\n", path, ln);
            continue;
        }
        *eq = 0;
        char *k = trim(s);
        char *v = trim(eq + 1);
        size_t l = strlen(v);
        if (l >= 2 && (v[0] == '"' || v[0] == '\'') && v[l - 1] == v[0]) {
            v[l - 1] = 0;
            v++;
        }
        if (config_set(k, v))
            fprintf(stderr, "cosmofetch: %s:%d: chave ou valor invalido: %s\n", path, ln, k);
    }
    fclose(f);
    return 0;
}

const char *config_example(void) {
    return "# cosmofetch - arquivo de configuracao\n"
           "# Local: ~/.config/cosmofetch/config.conf\n"
           "# Linhas com # sao comentarios. Formato: chave = valor\n"
           "\n"
           "# --- aparencia ---\n"
           "# temas: cosmo, ocean, sunset, forest, mono, neon\n"
           "theme = cosmo\n"
           "# cores aceitam numero 0-255 ou nome (red, pink, orange, aqua...)\n"
           "#key_color = 75\n"
           "#user_color = 213\n"
           "#host_color = 51\n"
           "separator = \": \"\n"
           "#key_width = 12        # alinha os valores em coluna\n"
           "bars = false           # barras de porcentagem (memoria, disco, bateria, cpu)\n"
           "bar_width = 10\n"
           "color = auto           # auto | true | false\n"
           "truncate = true        # corta linhas maiores que o terminal\n"
           "\n"
           "# --- logo ---\n"
           "# logos: planet, galaxy, rocket, mountains, terminal, random\n"
           "logo = planet\n"
           "#logo_file = ~/meu-logo.txt   # qualquer arquivo de texto (aceita ANSI)\n"
           "#logo_color = 213,177,141,105,69   # degrade (lista) ou cor unica\n"
           "logo_position = left   # left | right | top | none\n"
           "logo_padding = 2\n"
           "logo_gap = 4\n"
           "logo_center = true\n"
           "#seed = 42             # semente dos logos procedurais\n"
           "\n"
           "# --- imagem de verdade (kitty, WezTerm, Ghostty) ou chafa ---\n"
           "#image = ~/foto.png\n"
           "#image_protocol = auto  # auto | kitty | chafa\n"
           "#image_width = 30\n"
           "\n"
           "# --- modulos (veja --list-modules) ---\n"
           "structure = title:separator:os:host:kernel:uptime:packages:shell:display:de:wm:theme:icons:font:"
           "cursor:terminal:cpu:gpu:memory:swap:disk:localip:battery:poweradapter:locale:break:colors\n"
           "\n"
           "# --- modulos customizados: custom1..custom9 = Rotulo | comando ---\n"
           "#custom1 = Kernel Cmd | uname -r\n"
           "#custom2 = IP Publico | curl -s --max-time 3 https://api.ipify.org\n"
           "# depois e so usar na structure: ...:cpu:custom1:memory:...\n"
           "\n"
           "#weather_location = Curitiba\n";
}
