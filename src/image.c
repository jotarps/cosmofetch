/* Imagens de verdade no terminal via protocolo grafico do kitty
 * (kitty, WezTerm, Ghostty). So PNG: o arquivo e enviado como esta. */
#include "cosmofetch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *b64_encode(const unsigned char *in, size_t n, size_t *outn) {
    char *out = malloc(4 * ((n + 2) / 3) + 4);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i += 3) {
        unsigned v = (unsigned)in[i] << 16;
        if (i + 1 < n) v |= (unsigned)in[i + 1] << 8;
        if (i + 2 < n) v |= in[i + 2];
        out[j++] = B64[(v >> 18) & 63];
        out[j++] = B64[(v >> 12) & 63];
        out[j++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
        out[j++] = i + 2 < n ? B64[v & 63] : '=';
    }
    out[j] = 0;
    *outn = j;
    return out;
}

static int png_dims(const unsigned char *d, size_t n, int *w, int *h) {
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (n < 24 || memcmp(d, sig, 8) != 0) return 0;
    *w = (int)((d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19]);
    *h = (int)((d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23]);
    return *w > 0 && *h > 0;
}

int image_is_png(const char *file) {
    unsigned char b[24];
    FILE *f = fopen(file, "rb");
    if (!f) return 0;
    size_t n = fread(b, 1, sizeof b, f);
    fclose(f);
    int w, h;
    return png_dims(b, n, &w, &h);
}

int image_kitty_supported(void) {
    const char *t = getenv("TERM");
    const char *tp = getenv("TERM_PROGRAM");
    if (getenv("KITTY_WINDOW_ID") || getenv("GHOSTTY_RESOURCES_DIR")) return 1;
    if (t && (strstr(t, "kitty") || strstr(t, "ghostty"))) return 1;
    if (tp && (!strcmp(tp, "WezTerm") || !strcmp(tp, "ghostty") || !strcmp(tp, "rio"))) return 1;
    return 0;
}

/* Envia o PNG ao terminal. O cursor NAO se move (C=1); quem chama posiciona
 * o texto ao lado. Devolve 0 em sucesso e as linhas ocupadas em rows_out. */
int image_kitty_emit(const char *file, int cols, int rows_hint, int *rows_out) {
    FILE *f = fopen(file, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 24 || sz > 32L * 1024 * 1024) {
        fclose(f);
        return -1;
    }
    unsigned char *d = malloc((size_t)sz);
    if (!d || fread(d, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        free(d);
        return -1;
    }
    fclose(f);
    int w, h;
    if (!png_dims(d, (size_t)sz, &w, &h)) {
        free(d);
        return -1;
    }
    int rows = rows_hint > 0 ? rows_hint : (int)((double)cols * h / w / 2.0 + 0.5);
    if (rows < 1) rows = 1;
    size_t blen;
    char *b64 = b64_encode(d, (size_t)sz, &blen);
    free(d);
    if (!b64) return -1;
    size_t pos = 0;
    int first = 1;
    while (pos < blen) {
        size_t chunk = blen - pos > 4096 ? 4096 : blen - pos;
        int more = pos + chunk < blen;
        if (first) printf("\033_Ga=T,f=100,t=d,c=%d,r=%d,C=1,q=2,m=%d;", cols, rows, more);
        else printf("\033_Gm=%d;", more);
        fwrite(b64 + pos, 1, chunk, stdout);
        fputs("\033\\", stdout);
        pos += chunk;
        first = 0;
    }
    free(b64);
    *rows_out = rows;
    return 0;
}
