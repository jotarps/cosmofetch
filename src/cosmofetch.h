/* cosmofetch - fetch de sistema para Linux, escrito em C puro.
 * Tipos e prototipos compartilhados entre todos os arquivos. */
#ifndef COSMOFETCH_H
#define COSMOFETCH_H

#include <stddef.h>

#define CF_VERSION "1.0.0"
#define MAX_PALETTE 32

#if defined(__GNUC__)
#define CF_PRINTF(a, b) __attribute__((format(printf, a, b)))
#else
#define CF_PRINTF(a, b)
#endif

/* ---------- saida dos modulos ---------- */
typedef struct {
    char mod[32];   /* nome do modulo que gerou a linha */
    char key[96];   /* rotulo (ex.: "CPU") */
    char val[1024]; /* valor */
    int raw;        /* 1 = imprimir val sem rotulo (title, colors, ...) */
} Entry;

typedef struct {
    Entry *e;
    int n, cap;
    char curmod[32];
} Out;

void out_add(Out *o, const char *key, const char *fmt, ...) CF_PRINTF(3, 4);
void out_raw(Out *o, const char *fmt, ...) CF_PRINTF(2, 3);

/* ---------- configuracao ---------- */
typedef struct {
    int color;      /* -1 auto, 0 desligado, 1 ligado */
    int json;
    int bars;
    int bar_width;
    int key_color, user_color, host_color, sep_color;
    int key_width;
    int truncate;
    char separator[32];
    char structure[2048];
    char theme[32];
    char logo[64];
    char logo_file[512];
    char logo_position[16]; /* left, right, top, none */
    int logo_padding, logo_gap, logo_center;
    int logo_colors[MAX_PALETTE];
    int n_logo_colors;
    int theme_palette[MAX_PALETTE];
    int n_theme_palette;
    char image[512];
    char image_protocol[16]; /* auto, kitty, chafa */
    int image_width, image_height;
    unsigned seed;
    int seed_set;
    char weather_location[64];
    char custom[9][1024];
} Config;

extern Config g_cfg;

void config_defaults(void);
int config_set(const char *key, const char *val); /* 0 ok, -1 chave invalida */
int config_load(const char *path);                /* 0 ok, -1 nao abriu */
void config_default_path(char *out, size_t n);
const char *config_example(void);

/* ---------- utilidades ---------- */
void scpy(char *dst, size_t n, const char *src);
void sapp(char *dst, size_t n, const char *fmt, ...) CF_PRINTF(3, 4);
char *trim(char *s);
int read_line(const char *path, char *out, size_t n);
int read_all(const char *path, char *buf, size_t n);
int file_key(const char *path, const char *key, char *out, size_t n);
int run_cmd(const char *cmd, char *out, size_t n);
int run_cmd_all(const char *cmd, char *out, size_t n);
int cmd_exists(const char *name);
void shell_quote(const char *s, char *out, size_t n);
int count_entries(const char *path, int dirs_only);
int get_ppid(int pid);
int get_comm(int pid, char *out, size_t n);
int is_junk(const char *s);
void human_bytes(double b, char *out, size_t n);
void bar_str(double pct, int width, char *out, size_t n);
int visible_width(const char *s);
void truncate_visible(char *s, size_t cap, int maxw);
void json_escape(const char *s, char *out, size_t n);
unsigned lcg_next(unsigned *s);
const char *fg(int c);  /* cor 256 */
const char *fgb(int c); /* cor 256 em negrito */
const char *rst(void);
const char *username(void);
int term_cols(void);

/* ---------- logos e imagens ---------- */
typedef struct {
    char **lines;
    int n;
    int width;
} Logo;

void logo_set_scale(double s); /* 0.3..1.0, afeta os logos procedurais */
int logo_build(const char *name, Logo *L);
int logo_from_file(const char *path, Logo *L);
int logo_from_chafa(const char *file, int cols, int rows, Logo *L);
void logo_free(Logo *L);
void logo_list(void);
const char *logo_name_at(int i);

int image_kitty_supported(void);
int image_is_png(const char *file);
int image_kitty_emit(const char *file, int cols, int rows_hint, int *rows_out);

/* ---------- modulos ---------- */
typedef struct {
    const char *name;
    const char *group;
    const char *desc;
    void (*run)(Out *);
} Module;

extern const Module g_modules[];
extern const int g_nmodules;
const Module *module_find(const char *name);

#endif
