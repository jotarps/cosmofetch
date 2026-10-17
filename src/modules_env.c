/* Modulos de ambiente: shell, terminal, DE, WM, tema, fonte, cursor, ... */
#include "cosmofetch.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void mod_shell(Out *o) {
    const char *s = getenv("SHELL");
    if (!s || !*s) return;
    const char *b = strrchr(s, '/');
    b = b ? b + 1 : s;
    char q[1024], cmd[1200], v[256] = "";
    shell_quote(s, q, sizeof q);
    snprintf(cmd, sizeof cmd, "timeout 2 %s --version 2>/dev/null | head -n1", q);
    run_cmd(cmd, v, sizeof v);
    char *p = v;
    while (*p && !isdigit((unsigned char)*p)) p++;
    char ver[64] = "";
    sscanf(p, "%63[0-9.]", ver);
    if (ver[0]) out_add(o, "Shell", "%s %s", b, ver);
    else out_add(o, "Shell", "%s", b);
}

void mod_editor(Out *o) {
    const char *e = getenv("VISUAL");
    if (!e || !*e) e = getenv("EDITOR");
    if (!e || !*e) return;
    const char *b = strrchr(e, '/');
    out_add(o, "Editor", "%s", b ? b + 1 : e);
}

void mod_terminal(Out *o) {
    static const char *skip[] = {"bash", "zsh", "fish", "sh", "dash", "ksh", "tcsh", "nu", "xonsh", "elvish",
                                 "cosmofetch", "sudo", "su", "tmux: server", "tmux", "screen", "make", "env",
                                 "strace", "claude", "node", NULL};
    char comm[64];
    int pid = getppid();
    for (int i = 0; i < 8 && pid > 1; i++) {
        if (!get_comm(pid, comm, sizeof comm)) break;
        int skipit = 0;
        for (int k = 0; skip[k]; k++)
            if (!strcmp(comm, skip[k])) skipit = 1;
        if (!skipit) {
            if (!strncmp(comm, "gnome-terminal", 14)) scpy(comm, sizeof comm, "gnome-terminal");
            const char *ver = getenv("TERM_PROGRAM_VERSION");
            if (ver && *ver) out_add(o, "Terminal", "%s %s", comm, ver);
            else out_add(o, "Terminal", "%s", comm);
            return;
        }
        pid = get_ppid(pid);
    }
    const char *t = getenv("TERM_PROGRAM");
    if (!t || !*t) t = getenv("TERM");
    if (t && *t) out_add(o, "Terminal", "%s", t);
}

void mod_de(Out *o) {
    const char *de = getenv("XDG_CURRENT_DESKTOP");
    const char *st = getenv("XDG_SESSION_TYPE");
    if (!de || !*de) de = getenv("DESKTOP_SESSION");
    if (!de || !*de) return;
    const char *last = strrchr(de, ':');
    if (last) de = last + 1;
    char ver[128] = "";
    if (strcasestr(de, "gnome") && cmd_exists("gnome-shell")) {
        run_cmd("timeout 2 gnome-shell --version 2>/dev/null", ver, sizeof ver);
        char *d = ver;
        while (*d && !isdigit((unsigned char)*d)) d++;
        memmove(ver, d, strlen(d) + 1);
    } else if (strcasestr(de, "kde") && cmd_exists("plasmashell")) {
        run_cmd("timeout 2 plasmashell --version 2>/dev/null", ver, sizeof ver);
        char *d = ver;
        while (*d && !isdigit((unsigned char)*d)) d++;
        memmove(ver, d, strlen(d) + 1);
    }
    char v[256];
    snprintf(v, sizeof v, "%s%s%s", de, ver[0] ? " " : "", ver);
    if (st && *st && strcmp(st, "tty")) sapp(v, sizeof v, " (%s)", st);
    out_add(o, "DE", "%s", v);
}

void mod_wm(Out *o) {
    static const struct {
        const char *proc, *pretty;
    } wms[] = {{"Hyprland", "Hyprland"}, {"sway", "sway"},         {"river", "river"},     {"labwc", "labwc"},
               {"wayfire", "Wayfire"},   {"niri", "niri"},         {"kwin_wayland", "KWin (Wayland)"},
               {"kwin_x11", "KWin (X11)"}, {"gnome-shell", "Mutter"}, {"mutter", "Mutter"},
               {"cinnamon", "Muffin"},   {"muffin", "Muffin"},     {"xfwm4", "Xfwm4"},     {"marco", "Marco"},
               {"i3", "i3"},             {"bspwm", "bspwm"},       {"openbox", "Openbox"}, {"awesome", "awesome"},
               {"dwm", "dwm"},           {"qtile", "Qtile"},       {"herbstluftwm", "herbstluftwm"},
               {"xmonad-x86_64-linux", "xmonad"}, {"fluxbox", "Fluxbox"}, {"icewm", "IceWM"},
               {"weston", "Weston"},     {"cage", "cage"},         {NULL, NULL}};
    DIR *d = opendir("/proc");
    if (!d) return;
    static char seen[2048][20];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < 2048) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        char comm[64];
        if (!get_comm(atoi(e->d_name), comm, sizeof comm)) continue;
        scpy(seen[n++], sizeof seen[0], comm);
    }
    closedir(d);
    for (int i = 0; wms[i].proc; i++) {
        for (int k = 0; k < n; k++) {
            if (!strcmp(seen[k], wms[i].proc)) {
                out_add(o, "WM", "%s", wms[i].pretty);
                return;
            }
        }
    }
}

/* ---------- GTK / GNOME settings ---------- */
static int gnome_like(void) {
    const char *de = getenv("XDG_CURRENT_DESKTOP");
    return de && (strcasestr(de, "gnome") || strcasestr(de, "cinnamon") || strcasestr(de, "budgie") ||
                  strcasestr(de, "unity") || strcasestr(de, "pantheon"));
}

static int from_gsettings(const char *gskey, char *out, size_t n) {
    if (!cmd_exists("gsettings")) return 0;
    char cmd[256], v[256] = "";
    snprintf(cmd, sizeof cmd, "timeout 2 gsettings get org.gnome.desktop.interface %s 2>/dev/null", gskey);
    if (!run_cmd(cmd, v, sizeof v)) return 0;
    char *s = v;
    size_t l = strlen(s);
    if (l >= 2 && s[0] == '\'' && s[l - 1] == '\'') {
        s[l - 1] = 0;
        s++;
    }
    if (!*s) return 0;
    scpy(out, n, s);
    return 1;
}

static int from_gtk_files(const char *key, char *out, size_t n) {
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char p[600];
    if (xdg && *xdg) {
        snprintf(p, sizeof p, "%s/gtk-3.0/settings.ini", xdg);
        if (file_key(p, key, out, n)) return 1;
    }
    if (home) {
        snprintf(p, sizeof p, "%s/.config/gtk-3.0/settings.ini", home);
        if (file_key(p, key, out, n)) return 1;
        snprintf(p, sizeof p, "%s/.config/gtk-4.0/settings.ini", home);
        if (file_key(p, key, out, n)) return 1;
        snprintf(p, sizeof p, "%s/.gtkrc-2.0", home);
        if (file_key(p, key, out, n)) return 1;
    }
    return 0;
}

static int gtk_get(const char *key, const char *gskey, char *out, size_t n) {
    if (gnome_like()) return from_gsettings(gskey, out, n) || from_gtk_files(key, out, n);
    return from_gtk_files(key, out, n) || from_gsettings(gskey, out, n);
}

void mod_theme(Out *o) {
    char v[128] = "";
    if (gtk_get("gtk-theme-name", "gtk-theme", v, sizeof v)) out_add(o, "Theme", "%s [GTK]", v);
}

void mod_icons(Out *o) {
    char v[128] = "";
    if (gtk_get("gtk-icon-theme-name", "icon-theme", v, sizeof v)) out_add(o, "Icons", "%s [GTK]", v);
}

void mod_font(Out *o) {
    char v[128] = "";
    if (gtk_get("gtk-font-name", "font-name", v, sizeof v)) out_add(o, "Font", "%s [GTK]", v);
}

void mod_cursor(Out *o) {
    char v[128] = "", sz[32] = "";
    if (!gtk_get("gtk-cursor-theme-name", "cursor-theme", v, sizeof v)) return;
    if (gtk_get("gtk-cursor-theme-size", "cursor-size", sz, sizeof sz) && atoi(sz) > 0)
        out_add(o, "Cursor", "%s (%dpx)", v, atoi(sz));
    else
        out_add(o, "Cursor", "%s", v);
}
