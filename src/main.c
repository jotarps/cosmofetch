/* cosmofetch - ponto de entrada: CLI, execucao dos modulos e layout. */
#include "cosmofetch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------- ajuda ---------- */
static void usage(void) {
    puts("cosmofetch " CF_VERSION " - informacoes do sistema com logo, em C puro\n"
         "\n"
         "Uso: cosmofetch [opcoes]\n"
         "\n"
         "Geral:\n"
         "  -h, --help                 mostra esta ajuda\n"
         "  -v, --version              mostra a versao\n"
         "  --list-modules             lista todos os modulos\n"
         "  --list-logos               lista os logos embutidos\n"
         "  --gen-config               imprime um config.conf de exemplo\n"
         "  --config ARQ               usa outro arquivo de configuracao\n"
         "  --no-config                ignora o arquivo de configuracao\n"
         "\n"
         "Conteudo:\n"
         "  --structure LISTA          modulos separados por ':' (ex.: title:os:cpu:memory)\n"
         "  --json                     saida em JSON (sem logo e sem cores)\n"
         "  --bars                     mostra barras de porcentagem\n"
         "  --bar-width N              largura das barras (padrao 10)\n"
         "  --weather-location LOCAL   cidade para o modulo weather\n"
         "\n"
         "Aparencia:\n"
         "  --theme NOME               cosmo, ocean, sunset, forest, mono, neon\n"
         "  --key-color COR            cor dos rotulos (0-255 ou nome)\n"
         "  --user-color / --host-color / --sep-color COR\n"
         "  --separator TEXTO          separador entre rotulo e valor (padrao \": \")\n"
         "  --key-width N              alinha valores em coluna\n"
         "  --color / --no-color       forca cores ligadas ou desligadas\n"
         "\n"
         "Logo e imagem:\n"
         "  --logo NOME                planet, galaxy, rocket, mountains, terminal, random, none\n"
         "  --logo-file ARQ            usa um arquivo de texto como logo (aceita ANSI)\n"
         "  --logo-color LISTA         cor unica ou degrade: 213,177,141,105\n"
         "  --logo-position POS        left, right, top, none\n"
         "  --logo-padding N / --logo-gap N\n"
         "  --logo-only                mostra so o logo\n"
         "  --no-logo                  esconde o logo\n"
         "  --seed N                   semente dos logos procedurais\n"
         "  --image ARQ                imagem PNG de verdade (kitty/WezTerm/Ghostty) ou via chafa\n"
         "  --image-width N            largura da imagem em colunas (padrao 30)\n"
         "  --image-protocol P         auto, kitty ou chafa");
}

static void list_modules(void) {
    const char *group = "";
    for (int i = 0; i < g_nmodules; i++) {
        if (strcmp(group, g_modules[i].group)) {
            group = g_modules[i].group;
            printf("\n[%s]\n", group);
        }
        printf("  %-14s %s\n", g_modules[i].name, g_modules[i].desc);
    }
    printf("\n%d modulos. Use em --structure ou na chave 'structure' do config.\n", g_nmodules);
}

/* ---------- execucao dos modulos ---------- */
static void run_structure(Out *o) {
    char buf[sizeof g_cfg.structure];
    scpy(buf, sizeof buf, g_cfg.structure);
    char *save = NULL;
    for (char *tok = strtok_r(buf, ":,", &save); tok; tok = strtok_r(NULL, ":,", &save)) {
        tok = trim(tok);
        if (!*tok) continue;
        const Module *m = module_find(tok);
        if (!m) {
            fprintf(stderr, "cosmofetch: modulo desconhecido '%s' (veja --list-modules)\n", tok);
            continue;
        }
        scpy(o->curmod, sizeof o->curmod, m->name);
        m->run(o);
    }
}

static void print_json(const Out *o) {
    printf("[\n");
    int first = 1;
    for (int i = 0; i < o->n; i++) {
        if (o->e[i].raw) continue;
        char k[300], v[2200];
        json_escape(o->e[i].key, k, sizeof k);
        json_escape(o->e[i].val, v, sizeof v);
        printf("%s  {\"module\": \"%s\", \"key\": \"%s\", \"value\": \"%s\"}", first ? "" : ",\n", o->e[i].mod, k, v);
        first = 0;
    }
    printf("\n]\n");
}

/* ---------- linhas de informacao ---------- */
static char **build_info(const Out *o, int *n) {
    char **lines = calloc((size_t)o->n + 1, sizeof(char *));
    *n = 0;
    for (int i = 0; i < o->n; i++) {
        const Entry *e = &o->e[i];
        char *s = malloc(2400);
        if (e->raw) {
            scpy(s, 2400, e->val);
        } else {
            s[0] = 0;
            sapp(s, 2400, "%s%s%s", fgb(g_cfg.key_color), e->key, rst());
            int kw = visible_width(e->key);
            for (int p = kw; p < g_cfg.key_width; p++) sapp(s, 2400, " ");
            sapp(s, 2400, "%s%s%s%s", fg(g_cfg.sep_color), g_cfg.separator, rst(), e->val);
        }
        lines[(*n)++] = s;
    }
    return lines;
}

static void spaces(int n) {
    for (int i = 0; i < n; i++) putchar(' ');
}

static void print_clipped(char *line, int avail) {
    if (g_cfg.truncate && avail > 0) truncate_visible(line, 2400, avail);
    fputs(line, stdout);
    if (g_cfg.color) fputs("\033[0m", stdout);
}

static void expand_home(char *path, size_t n) {
    if (path[0] != '~' || (path[1] && path[1] != '/')) return;
    const char *h = getenv("HOME");
    if (!h) return;
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s%s", h, path + 1);
    scpy(path, n, tmp);
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    config_defaults();

    char cfgpath[512] = "";
    int no_config = 0, explicit_cfg = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--no-config")) no_config = 1;
        else if (!strcmp(argv[i], "--config") && i + 1 < argc) {
            scpy(cfgpath, sizeof cfgpath, argv[++i]);
            explicit_cfg = 1;
        } else if (!strncmp(argv[i], "--config=", 9)) {
            scpy(cfgpath, sizeof cfgpath, argv[i] + 9);
            explicit_cfg = 1;
        }
    }
    if (!no_config) {
        if (!cfgpath[0]) config_default_path(cfgpath, sizeof cfgpath);
        if (cfgpath[0] && config_load(cfgpath) && explicit_cfg) {
            fprintf(stderr, "cosmofetch: nao consegui abrir %s\n", cfgpath);
            return 1;
        }
    }

    int logo_only = 0, no_logo = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage();
            return 0;
        } else if (!strcmp(a, "-v") || !strcmp(a, "--version")) {
            puts("cosmofetch " CF_VERSION);
            return 0;
        } else if (!strcmp(a, "--list-modules")) {
            list_modules();
            return 0;
        } else if (!strcmp(a, "--list-logos")) {
            logo_list();
            return 0;
        } else if (!strcmp(a, "--gen-config")) {
            fputs(config_example(), stdout);
            return 0;
        } else if (!strcmp(a, "--no-config")) {
            continue;
        } else if (!strcmp(a, "--no-color")) {
            g_cfg.color = 0;
        } else if (!strcmp(a, "--color")) {
            g_cfg.color = 1;
        } else if (!strcmp(a, "--json")) {
            g_cfg.json = 1;
        } else if (!strcmp(a, "--bars")) {
            g_cfg.bars = 1;
        } else if (!strcmp(a, "--no-logo")) {
            no_logo = 1;
        } else if (!strcmp(a, "--logo-only")) {
            logo_only = 1;
        } else if (!strncmp(a, "--", 2) && a[2]) {
            char key[64], val[1024];
            const char *eq = strchr(a, '=');
            size_t kl = eq ? (size_t)(eq - a - 2) : strlen(a + 2);
            if (kl >= sizeof key) kl = sizeof key - 1;
            memcpy(key, a + 2, kl);
            key[kl] = 0;
            for (char *p = key; *p; p++)
                if (*p == '-') *p = '_';
            if (!strcmp(key, "config")) {
                if (!eq) i++;
                continue;
            }
            if (eq) {
                scpy(val, sizeof val, eq + 1);
            } else if (i + 1 < argc) {
                scpy(val, sizeof val, argv[++i]);
            } else {
                fprintf(stderr, "cosmofetch: a opcao %s precisa de um valor\n", a);
                return 2;
            }
            if (config_set(key, val)) {
                fprintf(stderr, "cosmofetch: opcao ou valor invalido: %s %s\n", a, val);
                return 2;
            }
        } else {
            fprintf(stderr, "cosmofetch: argumento inesperado '%s' (veja --help)\n", a);
            return 2;
        }
    }

    /* cor: automatico = terminal interativo e sem NO_COLOR */
    if (g_cfg.color < 0) g_cfg.color = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
    if (g_cfg.json) {
        g_cfg.color = 0;
        g_cfg.bars = 0;
    }
    if (logo_only) g_cfg.structure[0] = 0;
    expand_home(g_cfg.logo_file, sizeof g_cfg.logo_file);
    expand_home(g_cfg.image, sizeof g_cfg.image);

    Out out;
    memset(&out, 0, sizeof out);
    run_structure(&out);

    if (g_cfg.json) {
        print_json(&out);
        free(out.e);
        return 0;
    }

    int nout;
    char **info = build_info(&out, &nout);

    /* ---------- escolha do logo ---------- */
    int hide = no_logo || !strcmp(g_cfg.logo_position, "none") || !strcmp(g_cfg.logo, "none");
    Logo logo = {0};
    int use_kitty = 0;
    if (!hide && g_cfg.image[0]) {
        int tty = isatty(STDOUT_FILENO);
        int want_kitty = !strcmp(g_cfg.image_protocol, "kitty") ||
                         (!strcmp(g_cfg.image_protocol, "auto") && image_kitty_supported());
        if (access(g_cfg.image, R_OK) != 0) {
            fprintf(stderr, "cosmofetch: nao consegui ler a imagem '%s'\n", g_cfg.image);
        } else if (want_kitty && tty && image_is_png(g_cfg.image)) {
            use_kitty = 1;
        } else if (g_cfg.color && logo_from_chafa(g_cfg.image, g_cfg.image_width, g_cfg.image_height, &logo) == 0) {
            /* ok: imagem convertida pelo chafa */
        } else {
            fprintf(stderr, "cosmofetch: imagem indisponivel (precisa de terminal kitty/WezTerm/Ghostty com PNG, "
                            "ou do chafa instalado); usando logo embutido\n");
        }
    }
    int tw = term_cols();
    int pad = g_cfg.logo_padding > 0 ? g_cfg.logo_padding : 0;
    int gap = g_cfg.logo_gap > 0 ? g_cfg.logo_gap : 0;
    if (!hide && !use_kitty && logo.n == 0 && tw > 0 && !strcmp(g_cfg.logo_position, "left")) {
        /* terminal estreito: encolhe o logo procedural para sobrar espaco ao texto */
        int need = 44; /* largura minima razoavel para as linhas de info */
        int room = tw - pad - gap - need;
        logo_set_scale(room >= 58 ? 1.0 : (double)room / 58.0);
    }
    if (!hide && !use_kitty && logo.n == 0) {
        if (g_cfg.logo_file[0]) {
            if (logo_from_file(g_cfg.logo_file, &logo)) {
                fprintf(stderr, "cosmofetch: nao consegui ler o logo '%s'\n", g_cfg.logo_file);
                hide = 1;
            }
        } else if (logo_build(g_cfg.logo, &logo)) {
            fprintf(stderr, "cosmofetch: logo desconhecido '%s' (veja --list-logos)\n", g_cfg.logo);
            hide = 1;
        }
    }

    /* logo largo demais para o terminal: some em vez de empurrar o texto para fora */
    if (!hide && logo.n > 0 && tw > 0 && !strcmp(g_cfg.logo_position, "left") && logo.width + pad + gap + 24 > tw) {
        logo_free(&logo);
        hide = 1;
    }
    putchar('\n');

    if (use_kitty) {
        int rows = 0;
        spaces(pad);
        if (image_kitty_emit(g_cfg.image, g_cfg.image_width, g_cfg.image_height, &rows) != 0) {
            putchar('\r');
            rows = 0;
            use_kitty = 0;
        }
        if (use_kitty) {
            int total = rows > nout ? rows : nout;
            int off = (g_cfg.logo_center && rows > nout) ? (rows - nout) / 2 : 0;
            int avail = tw > 0 ? tw - (pad + g_cfg.image_width + gap) : 0;
            for (int i = 0; i < total; i++) {
                int move = g_cfg.image_width + gap + (i == 0 ? 0 : pad);
                if (move > 0) printf("\033[%dC", move);
                if (i - off >= 0 && i - off < nout) print_clipped(info[i - off], avail);
                putchar('\n');
            }
            goto done;
        }
    }

    if (hide || logo.n == 0) {
        for (int i = 0; i < nout; i++) {
            print_clipped(info[i], tw > 0 ? tw - pad : 0);
            putchar('\n');
        }
    } else if (!strcmp(g_cfg.logo_position, "top")) {
        for (int i = 0; i < logo.n; i++) {
            spaces(pad);
            fputs(logo.lines[i], stdout);
            putchar('\n');
        }
        putchar('\n');
        for (int i = 0; i < nout; i++) {
            spaces(pad);
            print_clipped(info[i], tw > 0 ? tw - pad : 0);
            putchar('\n');
        }
    } else if (!strcmp(g_cfg.logo_position, "right")) {
        int iw = 0;
        for (int i = 0; i < nout; i++) {
            int w = visible_width(info[i]);
            if (w > iw) iw = w;
        }
        if (tw > 0 && g_cfg.truncate && iw > tw - (pad + gap + logo.width)) iw = tw - (pad + gap + logo.width);
        if (iw < 10) iw = 10;
        int total = logo.n > nout ? logo.n : nout;
        int off = (g_cfg.logo_center && logo.n > nout) ? (logo.n - nout) / 2 : 0;
        for (int i = 0; i < total; i++) {
            spaces(pad);
            int w = 0;
            if (i - off >= 0 && i - off < nout) {
                print_clipped(info[i - off], iw);
                w = visible_width(info[i - off]);
                if (w > iw) w = iw;
            }
            spaces(iw - w + gap);
            if (i < logo.n) fputs(logo.lines[i], stdout);
            putchar('\n');
        }
    } else { /* left */
        int total = logo.n > nout ? logo.n : nout;
        int off = (g_cfg.logo_center && logo.n > nout) ? (logo.n - nout) / 2 : 0;
        int avail = tw > 0 ? tw - (pad + logo.width + gap) : 0;
        for (int i = 0; i < total; i++) {
            spaces(pad);
            int w = 0;
            if (i < logo.n) {
                fputs(logo.lines[i], stdout);
                w = visible_width(logo.lines[i]);
            }
            spaces(logo.width - w + gap);
            if (i - off >= 0 && i - off < nout) print_clipped(info[i - off], avail);
            putchar('\n');
        }
    }

done:
    putchar('\n');
    for (int i = 0; i < nout; i++) free(info[i]);
    free(info);
    logo_free(&logo);
    free(out.e);
    return 0;
}
