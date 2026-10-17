/* Funcoes auxiliares: leitura de arquivos, comandos, formatacao, cores. */
#include "cosmofetch.h"

#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

Config g_cfg;

#define BLOCK_FULL "\xe2\x96\x88"
#define BLOCK_LIGHT "\xe2\x96\x91"

/* ---------- strings ---------- */
void scpy(char *dst, size_t n, const char *src) {
    if (!n) return;
    size_t l = strlen(src);
    if (l >= n) l = n - 1;
    memcpy(dst, src, l);
    dst[l] = 0;
}

void sapp(char *dst, size_t n, const char *fmt, ...) {
    size_t l = strlen(dst);
    if (l + 1 >= n) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(dst + l, n - l, fmt, ap);
    va_end(ap);
}

char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

int is_junk(const char *s) {
    static const char *junk[] = {"To Be Filled By O.E.M.", "To be filled by O.E.M.", "Default string",
                                 "None", "System Product Name", "System Version", "Not Specified",
                                 "Not Applicable", "Unknown", "N/A", "O.E.M.", "Default", "Type1ProductConfigId",
                                 NULL};
    if (!s || !*s) return 1;
    for (int i = 0; junk[i]; i++)
        if (!strcasecmp(s, junk[i])) return 1;
    return 0;
}

unsigned lcg_next(unsigned *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s >> 8;
}

/* ---------- saida dos modulos ---------- */
static Entry *out_new(Out *o) {
    if (o->n == o->cap) {
        o->cap = o->cap ? o->cap * 2 : 64;
        o->e = realloc(o->e, (size_t)o->cap * sizeof(Entry));
        if (!o->e) {
            perror("cosmofetch: realloc");
            exit(1);
        }
    }
    Entry *e = &o->e[o->n++];
    memset(e, 0, sizeof *e);
    scpy(e->mod, sizeof e->mod, o->curmod);
    return e;
}

void out_add(Out *o, const char *key, const char *fmt, ...) {
    Entry *e = out_new(o);
    scpy(e->key, sizeof e->key, key);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->val, sizeof e->val, fmt, ap);
    va_end(ap);
}

void out_raw(Out *o, const char *fmt, ...) {
    Entry *e = out_new(o);
    e->raw = 1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->val, sizeof e->val, fmt, ap);
    va_end(ap);
}

/* ---------- arquivos e comandos ---------- */
int read_line(const char *path, char *out, size_t n) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char *r = fgets(out, (int)n, f);
    fclose(f);
    if (!r) {
        out[0] = 0;
        return 0;
    }
    out[strcspn(out, "\r\n")] = 0;
    char *t = trim(out);
    if (t != out) memmove(out, t, strlen(t) + 1);
    return out[0] != 0;
}

int read_all(const char *path, char *buf, size_t n) {
    FILE *f = fopen(path, "r");
    if (!f) {
        buf[0] = 0;
        return 0;
    }
    size_t r = fread(buf, 1, n - 1, f);
    fclose(f);
    buf[r] = 0;
    return (int)r;
}

int file_key(const char *path, const char *key, char *out, size_t n) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[2048];
    size_t kl = strlen(key);
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, key, kl) != 0) continue;
        char *v = line + kl;
        if (*v != '=' && *v != ':' && *v != ' ' && *v != '\t') continue;
        while (*v == ' ' || *v == '\t') v++;
        if (*v == '=' || *v == ':') v++;
        v = trim(v);
        size_t l = strlen(v);
        if (l >= 2 && (v[0] == '"' || v[0] == '\'') && v[l - 1] == v[0]) {
            v[l - 1] = 0;
            v++;
        }
        scpy(out, n, v);
        fclose(f);
        return out[0] != 0;
    }
    fclose(f);
    return 0;
}

int run_cmd(const char *cmd, char *out, size_t n) {
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    char *r = fgets(out, (int)n, p);
    pclose(p);
    if (!r) {
        out[0] = 0;
        return 0;
    }
    out[strcspn(out, "\r\n")] = 0;
    return out[0] != 0;
}

int run_cmd_all(const char *cmd, char *out, size_t n) {
    FILE *p = popen(cmd, "r");
    if (!p) {
        out[0] = 0;
        return 0;
    }
    size_t t = 0;
    while (t < n - 1) {
        size_t r = fread(out + t, 1, n - 1 - t, p);
        if (r == 0) break;
        t += r;
    }
    out[t] = 0;
    pclose(p);
    return (int)t;
}

int cmd_exists(const char *name) {
    const char *path = getenv("PATH");
    if (!path) path = "/usr/local/bin:/usr/bin:/bin";
    char buf[4096];
    scpy(buf, sizeof buf, path);
    char *save = NULL;
    for (char *d = strtok_r(buf, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
        char full[4400];
        snprintf(full, sizeof full, "%s/%s", d, name);
        if (access(full, X_OK) == 0) return 1;
    }
    return 0;
}

void shell_quote(const char *s, char *out, size_t n) {
    size_t j = 0;
    if (n < 3) {
        if (n) out[0] = 0;
        return;
    }
    out[j++] = '\'';
    for (; *s && j + 5 < n; s++) {
        if (*s == '\'') {
            memcpy(out + j, "'\\''", 4);
            j += 4;
        } else {
            out[j++] = *s;
        }
    }
    out[j++] = '\'';
    out[j] = 0;
}

int count_entries(const char *path, int dirs_only) {
    DIR *d = opendir(path);
    if (!d) return 0;
    int c = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (dirs_only) {
            if (e->d_type == DT_DIR) {
                c++;
            } else if (e->d_type == DT_UNKNOWN || e->d_type == DT_LNK) {
                char full[1024];
                struct stat st;
                snprintf(full, sizeof full, "%s/%s", path, e->d_name);
                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) c++;
            }
        } else {
            c++;
        }
    }
    closedir(d);
    return c;
}

int get_comm(int pid, char *out, size_t n) {
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/comm", pid);
    return read_line(path, out, n);
}

int get_ppid(int pid) {
    char path[64], buf[1024];
    snprintf(path, sizeof path, "/proc/%d/stat", pid);
    if (!read_all(path, buf, sizeof buf)) return 0;
    char *p = strrchr(buf, ')');
    if (!p) return 0;
    int ppid = 0;
    char st;
    if (sscanf(p + 2, "%c %d", &st, &ppid) != 2) return 0;
    return ppid;
}

const char *username(void) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name) return pw->pw_name;
    const char *u = getenv("USER");
    return u ? u : "user";
}

int term_cols(void) {
    struct winsize ws;
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    const char *c = getenv("COLUMNS");
    if (c && atoi(c) > 0 && isatty(STDOUT_FILENO)) return atoi(c);
    return 0;
}

/* ---------- formatacao ---------- */
void human_bytes(double b, char *out, size_t n) {
    static const char *u[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    int i = 0;
    while (b >= 1024.0 && i < 5) {
        b /= 1024.0;
        i++;
    }
    if (i == 0)
        snprintf(out, n, "%.0f %s", b, u[0]);
    else
        snprintf(out, n, "%.2f %s", b, u[i]);
}

/* ---------- cores ---------- */
const char *fg(int c) {
    static char ring[8][24];
    static int idx;
    if (!g_cfg.color || c < 0) return "";
    char *b = ring[idx++ & 7];
    snprintf(b, 24, "\033[38;5;%dm", c);
    return b;
}

const char *fgb(int c) {
    static char ring[8][24];
    static int idx;
    if (!g_cfg.color || c < 0) return "";
    char *b = ring[idx++ & 7];
    snprintf(b, 24, "\033[1;38;5;%dm", c);
    return b;
}

const char *rst(void) { return g_cfg.color ? "\033[0m" : ""; }

void bar_str(double pct, int width, char *out, size_t n) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    if (width < 4) width = 4;
    int fill = (int)(pct * width / 100.0 + 0.5);
    int col = pct < 50 ? 76 : (pct < 80 ? 220 : 196);
    out[0] = 0;
    sapp(out, n, "[%s", fg(col));
    for (int i = 0; i < fill; i++) sapp(out, n, "%s", BLOCK_FULL);
    sapp(out, n, "%s", fg(240));
    for (int i = fill; i < width; i++) sapp(out, n, "%s", BLOCK_LIGHT);
    sapp(out, n, "%s]", rst());
}

/* largura visivel ignorando sequencias ANSI; conta pontos de codigo UTF-8 */
int visible_width(const char *s) {
    int w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p == 0x1b) {
            p++;
            if (*p == '[') {
                p++;
                while (*p && !(*p >= 0x40 && *p <= 0x7e)) p++;
                if (*p) p++;
            } else if (*p == ']' || *p == '_') {
                while (*p && *p != 0x07 && !(*p == 0x1b && p[1] == '\\')) p++;
                if (*p == 0x07) p++;
                else if (*p == 0x1b) p += 2;
            } else if (*p) {
                p++;
            }
            continue;
        }
        if ((*p & 0xC0) != 0x80) w++;
        p++;
    }
    return w;
}

void truncate_visible(char *s, size_t cap, int maxw) {
    if (maxw <= 0) return;
    int w = 0;
    unsigned char *p = (unsigned char *)s;
    while (*p) {
        if (*p == 0x1b) {
            p++;
            if (*p == '[') {
                p++;
                while (*p && !(*p >= 0x40 && *p <= 0x7e)) p++;
                if (*p) p++;
            } else if (*p) {
                p++;
            }
            continue;
        }
        if ((*p & 0xC0) != 0x80) {
            if (w >= maxw) {
                *p = 0;
                size_t l = strlen(s);
                if (g_cfg.color && l + 5 < cap) memcpy(s + l, "\033[0m", 5);
                return;
            }
            w++;
        }
        p++;
    }
}

void json_escape(const char *s, char *out, size_t n) {
    size_t j = 0;
    for (; *s && j + 8 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c == '\n') {
            out[j++] = '\\';
            out[j++] = 'n';
        } else if (c == '\t') {
            out[j++] = '\\';
            out[j++] = 't';
        } else if (c < 0x20) {
            j += (size_t)snprintf(out + j, n - j, "\\u%04x", c);
        } else {
            out[j++] = (char)c;
        }
    }
    out[j] = 0;
}
