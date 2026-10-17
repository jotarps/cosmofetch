/* Logos: planeta e galaxia gerados por codigo (ray-shading em ASCII),
 * logos estaticos, logo a partir de arquivo e imagem via chafa. */
#include "cosmofetch.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---------- paletas padrao ---------- */
static const int PAL_PLANET[] = {54, 55, 91, 97, 133, 169, 175, 181, 217, 223, 230};
static const int PAL_GALAXY[] = {17, 18, 54, 55, 91, 97, 133, 139, 175, 217, 225, 231};
static const int PAL_ROCKET[] = {231, 255, 252, 213, 177, 141, 105, 69, 39, 45, 208, 202, 196};
static const int PAL_MOUNT[] = {231, 153, 117, 75, 69, 63, 57, 55, 54};
static const int PAL_TERM[] = {82, 46, 40, 34, 28, 22};

/* ---------- logos estaticos ---------- */
static const char *ART_ROCKET[] = {
    "          /\\",
    "         /  \\",
    "        /    \\",
    "       |  /\\  |",
    "       | (  ) |",
    "       |  \\/  |",
    "      /|      |\\",
    "     / |  []  | \\",
    "    /  |  []  |  \\",
    "   /___|______|___\\",
    "       |  /\\  |",
    "       | /  \\ |",
    "        \\ || /",
    "         \\||/",
    "        _\\||/_",
    "       (_/\\/\\_)",
    "         ' ` '",
    NULL};

static const char *ART_MOUNTAINS[] = {
    "                 .          *",
    "        *                  /\\       .",
    "                 /\\       /  \\",
    "        .       /  \\  /\\ /    \\",
    "               / ^^ \\/  \\  /\\  \\     *",
    "         /\\   /      \\   \\/  \\  \\",
    "    /\\  /  \\ /        \\  /    \\  \\",
    "   /  \\/    \\          \\/      \\  \\",
    "  /    \\     \\                  \\  \\",
    " /      \\     \\                  \\  \\",
    "/________\\_____\\__________________\\__\\",
    NULL};

static const char *ART_TERMINAL[] = {
    " .------------------------------.",
    " | o o o                    _ x |",
    " |------------------------------|",
    " | $ cosmofetch                 |",
    " | > planetas, estrelas, bytes  |",
    " | $ _                          |",
    " |                              |",
    " |                              |",
    " '------------------------------'",
    "       __|______________|__",
    "      /____________________\\",
    NULL};

typedef struct {
    const char *name;
    const char *desc;
    const char **lines; /* NULL = procedural */
    int kind;           /* 0 estatico, 1 planeta, 2 galaxia */
    const int *pal;
    int npal;
} LogoDef;

static const LogoDef LOGOS[] = {
    {"planet", "planeta com aneis, sombreado em tempo real", NULL, 1, PAL_PLANET, 11},
    {"galaxy", "galaxia espiral gerada por codigo", NULL, 2, PAL_GALAXY, 12},
    {"rocket", "foguete (arte estatica)", ART_ROCKET, 0, PAL_ROCKET, 13},
    {"mountains", "montanhas e estrelas (arte estatica)", ART_MOUNTAINS, 0, PAL_MOUNT, 9},
    {"terminal", "janela de terminal (arte estatica)", ART_TERMINAL, 0, PAL_TERM, 6},
};
#define NLOGOS ((int)(sizeof LOGOS / sizeof LOGOS[0]))

const char *logo_name_at(int i) { return i >= 0 && i < NLOGOS ? LOGOS[i].name : NULL; }

void logo_list(void) {
    for (int i = 0; i < NLOGOS; i++) printf("  %-10s %s\n", LOGOS[i].name, LOGOS[i].desc);
    printf("  %-10s %s\n", "random", "sorteia um dos logos acima");
    printf("  %-10s %s\n", "none", "sem logo");
}

static double g_scale = 1.0;
void logo_set_scale(double s) { g_scale = s < 0.3 ? 0.3 : (s > 1.0 ? 1.0 : s); }

/* ---------- utilidades ---------- */
static void logo_alloc(Logo *L, int n) {
    L->lines = calloc((size_t)n, sizeof(char *));
    L->n = 0;
    L->width = 0;
}

void logo_free(Logo *L) {
    for (int i = 0; i < L->n; i++) free(L->lines[i]);
    free(L->lines);
    L->lines = NULL;
    L->n = L->width = 0;
}

static const int *active_palette(const LogoDef *d, int *n) {
    if (g_cfg.n_logo_colors > 0) {
        *n = g_cfg.n_logo_colors;
        return g_cfg.logo_colors;
    }
    if (g_cfg.n_theme_palette > 0) {
        *n = g_cfg.n_theme_palette;
        return g_cfg.theme_palette;
    }
    *n = d->npal;
    return d->pal;
}

static int pal_at(const int *pal, int n, double t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int i = (int)(t * (n - 1) + 0.5);
    return pal[i];
}

static int row_visible_len(const char *s) { return visible_width(s); }

static void push_line(Logo *L, char *s) {
    L->lines[L->n++] = s;
    int w = row_visible_len(s);
    if (w > L->width) L->width = w;
}

/* converte uma grade de caracteres+cores em uma linha com ANSI */
static char *grid_row(const char *chars, const int *cols, int W) {
    int end = W;
    while (end > 0 && chars[end - 1] == ' ') end--;
    size_t cap = (size_t)W * 20 + 32;
    char *s = malloc(cap);
    size_t l = 0;
    int last = -2;
    for (int i = 0; i < end; i++) {
        if (g_cfg.color && chars[i] != ' ' && cols[i] != last) {
            l += (size_t)snprintf(s + l, cap - l, "\033[38;5;%dm", cols[i]);
            last = cols[i];
        }
        s[l++] = chars[i];
    }
    if (g_cfg.color && last != -2) l += (size_t)snprintf(s + l, cap - l, "\033[0m");
    s[l] = 0;
    return s;
}

static unsigned seed_value(void) {
    if (g_cfg.seed_set) return g_cfg.seed;
    return 42u;
}

/* ---------- planeta ---------- */
static void build_planet(Logo *L, const LogoDef *d) {
    enum { WMAX = 64 };
    const int W = (int)(58 * g_scale + 0.5), H = (int)(23 * g_scale + 0.5);
    const double sc = g_scale;
    int npal;
    const int *pal = active_palette(d, &npal);
    unsigned rng = seed_value();
    const double cx = (W - 1) / 2.0, cy = (H - 1) / 2.0;
    const double R = 13.5;
    const double phi = -0.30, sint = 0.40;
    double lx = -0.55, ly = -0.60, lz = 0.58;
    double ln = sqrt(lx * lx + ly * ly + lz * lz);
    lx /= ln;
    ly /= ln;
    lz /= ln;
    static const char ramp[] = " .:;+*oO#@";
    const int rl = (int)sizeof ramp - 1;
    logo_alloc(L, H);
    for (int r = 0; r < H; r++) {
        char chars[WMAX + 1];
        int cols[WMAX];
        for (int c = 0; c < W; c++) {
            double x = (c - cx) / sc, y = (r - cy) * 2.0 / sc;
            char ch = ' ';
            int col = pal[npal - 1];
            int sphere = x * x + y * y < R * R;
            double xr = x * cos(phi) + y * sin(phi);
            double yr = -x * sin(phi) + y * cos(phi);
            double dd = sqrt(xr * xr + (yr / sint) * (yr / sint));
            int ring = dd > 16.5 && dd < 25.0 && !(dd > 21.0 && dd < 22.0);
            int front = yr > 0;
            if (ring && (!sphere || front)) {
                double v = 0.5 + 0.5 * sin(dd * 2.3);
                ch = v > 0.55 ? '=' : '-';
                col = pal_at(pal, npal, 0.30 + 0.62 * v + (front ? 0.08 : 0.0));
            } else if (sphere) {
                double z = sqrt(R * R - x * x - y * y);
                double b = (x / R) * lx + (y / R) * ly + (z / R) * lz;
                double band = 0.10 * sin(y * 0.75 + 1.8 * sin(x * 0.18 + 0.7));
                double v = b * 1.05 + band;
                if (v < 0) v = 0;
                if (v > 1) v = 1;
                int ri = (int)(v * (rl - 1) + 0.5);
                if (ri < 1) ri = 1; /* lado escuro ainda desenha o contorno */
                ch = ramp[ri];
                col = pal_at(pal, npal, v);
            } else if ((lcg_next(&rng) % 1000) < 28) {
                static const char st[] = ".'`,";
                ch = st[lcg_next(&rng) % 4];
                col = pal[npal - 1 - (int)(lcg_next(&rng) % (npal > 2 ? 2 : 1))];
            } else {
                lcg_next(&rng);
            }
            chars[c] = ch;
            cols[c] = col;
        }
        chars[W] = 0;
        push_line(L, grid_row(chars, cols, W));
    }
}

/* ---------- galaxia ---------- */
static void build_galaxy(Logo *L, const LogoDef *d) {
    enum { WMAX = 64 };
    const int W = (int)(50 * g_scale + 0.5), H = (int)(22 * g_scale + 0.5);
    const double sc = g_scale;
    int npal;
    const int *pal = active_palette(d, &npal);
    unsigned rng = seed_value() + 7u;
    const double cx = (W - 1) / 2.0, cy = (H - 1) / 2.0;
    const double ang = 0.55, squash = 0.55;
    static const char ramp[] = ".:-=+*#%@";
    const int rl = (int)sizeof ramp - 1;
    logo_alloc(L, H);
    for (int r = 0; r < H; r++) {
        char chars[WMAX + 1];
        int cols[WMAX];
        for (int c = 0; c < W; c++) {
            double x = (c - cx) / sc, y = (r - cy) * 2.0 / sc;
            double xr = x * cos(ang) + y * sin(ang);
            double yr = (-x * sin(ang) + y * cos(ang)) / squash;
            double rad = sqrt(xr * xr + yr * yr);
            double th = atan2(yr, xr);
            double arm = 0.5 + 0.5 * cos(2.0 * th - 3.4 * log(rad + 1.0));
            double inten = exp(-rad / 10.0) * (0.04 + 1.5 * pow(arm, 5.0)) + 1.2 * exp(-rad * rad / 14.0);
            double noise = 0.45 + 1.1 * ((lcg_next(&rng) % 1000) / 1000.0);
            inten *= noise;
            char ch = ' ';
            int col = pal[0];
            if (inten > 0.14) {
                double v = inten > 1 ? 1 : inten;
                ch = ramp[(int)(v * (rl - 1) + 0.5)];
                col = pal_at(pal, npal, v);
            } else if ((lcg_next(&rng) % 1000) < 22) {
                ch = (lcg_next(&rng) % 3) ? '.' : '+';
                col = pal[npal / 2];
            }
            chars[c] = ch;
            cols[c] = col;
        }
        chars[W] = 0;
        push_line(L, grid_row(chars, cols, W));
    }
}

/* ---------- logos estaticos e arquivos ---------- */
static char *colorize_line(const char *line, int color) {
    size_t cap = strlen(line) + 32;
    char *s = malloc(cap);
    if (g_cfg.color && line[0]) snprintf(s, cap, "\033[38;5;%dm%s\033[0m", color, line);
    else snprintf(s, cap, "%s", line);
    return s;
}

static void build_static(Logo *L, const LogoDef *d) {
    int npal;
    const int *pal = active_palette(d, &npal);
    int n = 0;
    while (d->lines[n]) n++;
    logo_alloc(L, n);
    for (int i = 0; i < n; i++) {
        int idx = n > 1 ? (int)((double)i * (npal - 1) / (n - 1) + 0.5) : 0;
        push_line(L, colorize_line(d->lines[i], pal[idx]));
    }
}

int logo_build(const char *name, Logo *L) {
    memset(L, 0, sizeof *L);
    if (!strcmp(name, "random")) {
        unsigned s = g_cfg.seed_set ? g_cfg.seed : (unsigned)time(NULL);
        name = LOGOS[lcg_next(&s) % NLOGOS].name;
    }
    for (int i = 0; i < NLOGOS; i++) {
        if (strcmp(LOGOS[i].name, name)) continue;
        if (LOGOS[i].kind == 1) build_planet(L, &LOGOS[i]);
        else if (LOGOS[i].kind == 2) build_galaxy(L, &LOGOS[i]);
        else build_static(L, &LOGOS[i]);
        return 0;
    }
    return -1;
}

int logo_from_file(const char *path, Logo *L) {
    memset(L, 0, sizeof *L);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char **tmp = NULL;
    int n = 0, cap = 0, has_esc = 0;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strchr(line, 0x1b)) has_esc = 1;
        if (n == cap) {
            cap = cap ? cap * 2 : 32;
            tmp = realloc(tmp, (size_t)cap * sizeof(char *));
        }
        tmp[n++] = strdup(line);
    }
    fclose(f);
    if (!n) {
        free(tmp);
        return -1;
    }
    static const int PAL_DEFAULT[] = {51, 45, 39, 33, 27};
    int npal = 5;
    const int *pal = PAL_DEFAULT;
    if (g_cfg.n_logo_colors > 0) {
        pal = g_cfg.logo_colors;
        npal = g_cfg.n_logo_colors;
    } else if (g_cfg.n_theme_palette > 0) {
        pal = g_cfg.theme_palette;
        npal = g_cfg.n_theme_palette;
    }
    logo_alloc(L, n);
    for (int i = 0; i < n; i++) {
        if (has_esc) {
            size_t cl = strlen(tmp[i]) + 8;
            char *s = malloc(cl);
            snprintf(s, cl, "%s%s", tmp[i], g_cfg.color ? "\033[0m" : "");
            if (!g_cfg.color) {
                /* remove ANSI quando a cor esta desligada */
                char *w = s;
                for (char *r = tmp[i]; *r;) {
                    if (*r == 0x1b && r[1] == '[') {
                        r += 2;
                        while (*r && !(*r >= 0x40 && *r <= 0x7e)) r++;
                        if (*r) r++;
                    } else {
                        *w++ = *r++;
                    }
                }
                *w = 0;
            }
            push_line(L, s);
        } else {
            int idx = n > 1 ? (int)((double)i * (npal - 1) / (n - 1) + 0.5) : 0;
            push_line(L, colorize_line(tmp[i], pal[idx]));
        }
        free(tmp[i]);
    }
    free(tmp);
    return 0;
}

int logo_from_chafa(const char *file, int cols, int rows, Logo *L) {
    memset(L, 0, sizeof *L);
    if (!cmd_exists("chafa")) return -1;
    char q[1100], cmd[1400];
    shell_quote(file, q, sizeof q);
    if (rows <= 0) rows = cols / 2;
    snprintf(cmd, sizeof cmd, "timeout 10 chafa -f symbols --animate=off -s %dx%d %s 2>/dev/null", cols, rows, q);
    static char buf[1 << 18];
    int len = run_cmd_all(cmd, buf, sizeof buf);
    if (len <= 0) return -1;
    int n = 1;
    for (int i = 0; i < len; i++)
        if (buf[i] == '\n') n++;
    logo_alloc(L, n + 1);
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        size_t cl = strlen(line) + 8;
        char *s = malloc(cl);
        snprintf(s, cl, "%s\033[0m", line);
        push_line(L, s);
    }
    return L->n ? 0 : -1;
}
