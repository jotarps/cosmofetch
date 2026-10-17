/* Modulos de sistema: title, os, host, kernel, uptime, packages, ... */
#include "cosmofetch.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

#define DMI "/sys/devices/virtual/dmi/id/"

static int dmi(const char *f, char *out, size_t n) {
    char p[256];
    snprintf(p, sizeof p, DMI "%s", f);
    if (!read_line(p, out, n)) return 0;
    if (is_junk(out)) {
        out[0] = 0;
        return 0;
    }
    return 1;
}

static int in_wsl(void) {
    char b[512];
    if (!read_line("/proc/version", b, sizeof b)) return 0;
    return strcasestr(b, "microsoft") != NULL || strcasestr(b, "wsl") != NULL;
}

/* ---------- title / separator / break / colors / version ---------- */
void mod_title(Out *o) {
    char host[256] = "";
    gethostname(host, sizeof host - 1);
    out_raw(o, "%s%s%s@%s%s%s", fgb(g_cfg.user_color), username(), rst(), fgb(g_cfg.host_color), host, rst());
}

void mod_separator(Out *o) {
    char host[256] = "";
    gethostname(host, sizeof host - 1);
    size_t n = strlen(username()) + 1 + strlen(host);
    char line[512];
    if (n >= sizeof line) n = sizeof line - 1;
    memset(line, '-', n);
    line[n] = 0;
    out_raw(o, "%s%s%s", fg(g_cfg.sep_color), line, rst());
}

void mod_break(Out *o) { out_raw(o, "%s", ""); }

void mod_colors(Out *o) {
    if (!g_cfg.color) return;
    char a[512] = "", b[512] = "";
    for (int i = 0; i < 8; i++) {
        sapp(a, sizeof a, "\033[4%dm   ", i);
        sapp(b, sizeof b, "\033[10%dm   ", i);
    }
    sapp(a, sizeof a, "\033[0m");
    sapp(b, sizeof b, "\033[0m");
    out_raw(o, "%s", a);
    out_raw(o, "%s", b);
}

void mod_version(Out *o) { out_add(o, "Fetch", "cosmofetch %s", CF_VERSION); }

/* ---------- identidade do sistema ---------- */
void mod_os(Out *o) {
    char name[256] = "";
    if (!file_key("/etc/os-release", "PRETTY_NAME", name, sizeof name) &&
        !file_key("/usr/lib/os-release", "PRETTY_NAME", name, sizeof name))
        scpy(name, sizeof name, "Linux");
    struct utsname u;
    uname(&u);
    out_add(o, "OS", "%s %s%s", name, u.machine, in_wsl() ? " (WSL)" : "");
}

void mod_host(Out *o) {
    char name[160] = "", ver[160] = "";
    dmi("product_name", name, sizeof name);
    dmi("product_version", ver, sizeof ver);
    if (!name[0]) {
        char buf[256];
        int n = read_all("/proc/device-tree/model", buf, sizeof buf);
        if (n > 0) {
            buf[strcspn(buf, "\n")] = 0;
            scpy(name, sizeof name, buf);
        }
    }
    if (!name[0]) return;
    if (ver[0]) out_add(o, "Host", "%s (%s)", name, ver);
    else out_add(o, "Host", "%s", name);
}

void mod_board(Out *o) {
    char v[160] = "", n[160] = "", r[160] = "";
    dmi("board_vendor", v, sizeof v);
    dmi("board_name", n, sizeof n);
    dmi("board_version", r, sizeof r);
    if (!n[0]) return;
    char out[512] = "";
    if (v[0]) sapp(out, sizeof out, "%s ", v);
    sapp(out, sizeof out, "%s", n);
    if (r[0]) sapp(out, sizeof out, " (%s)", r);
    out_add(o, "Motherboard", "%s", out);
}

void mod_bios(Out *o) {
    char v[160] = "", n[160] = "", d[160] = "";
    dmi("bios_vendor", v, sizeof v);
    dmi("bios_version", n, sizeof n);
    dmi("bios_date", d, sizeof d);
    if (!v[0] && !n[0]) return;
    char out[512] = "";
    if (v[0]) sapp(out, sizeof out, "%s ", v);
    if (n[0]) sapp(out, sizeof out, "%s", n);
    if (d[0]) sapp(out, sizeof out, " (%s)", d);
    out_add(o, "BIOS", "%s", out);
}

void mod_chassis(Out *o) {
    static const char *t[] = {"", "Other", "Unknown", "Desktop", "Low Profile Desktop", "Pizza Box", "Mini Tower",
                              "Tower", "Portable", "Laptop", "Notebook", "Handheld", "Docking Station",
                              "All in One", "Sub Notebook", "Space-saving", "Lunch Box", "Main Server Chassis",
                              "Expansion Chassis", "SubChassis", "Bus Expansion Chassis", "Peripheral Chassis",
                              "RAID Chassis", "Rack Mount Chassis", "Sealed-case PC", "Multi-system",
                              "CompactPCI", "AdvancedTCA", "Blade", "Blade Enclosure", "Tablet", "Convertible",
                              "Detachable", "IoT Gateway", "Embedded PC", "Mini PC", "Stick PC"};
    char b[32];
    if (!read_line(DMI "chassis_type", b, sizeof b)) return;
    int n = atoi(b);
    if (n < 1 || n >= (int)(sizeof t / sizeof t[0])) return;
    out_add(o, "Chassis", "%s", t[n]);
}

void mod_kernel(Out *o) {
    struct utsname u;
    uname(&u);
    out_add(o, "Kernel", "%s %s", u.sysname, u.release);
}

/* ---------- tempo ---------- */
void mod_uptime(Out *o) {
    struct sysinfo si;
    if (sysinfo(&si)) return;
    long s = si.uptime;
    long d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
    char v[128] = "";
    if (d) sapp(v, sizeof v, "%ld day%s, ", d, d > 1 ? "s" : "");
    if (h) sapp(v, sizeof v, "%ld hour%s, ", h, h > 1 ? "s" : "");
    sapp(v, sizeof v, "%ld min%s", m, m == 1 ? "" : "s");
    out_add(o, "Uptime", "%s", v);
}

void mod_boottime(Out *o) {
    struct sysinfo si;
    if (sysinfo(&si)) return;
    time_t t = time(NULL) - si.uptime;
    struct tm tmv;
    localtime_r(&t, &tmv);
    char b[64];
    strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", &tmv);
    out_add(o, "Boot Time", "%s", b);
}

void mod_loadavg(Out *o) {
    double a, b, c;
    char buf[128];
    if (!read_line("/proc/loadavg", buf, sizeof buf)) return;
    if (sscanf(buf, "%lf %lf %lf", &a, &b, &c) != 3) return;
    out_add(o, "Load Average", "%.2f, %.2f, %.2f", a, b, c);
}

void mod_date(Out *o) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char b[64];
    strftime(b, sizeof b, "%Y-%m-%d", &tmv);
    out_add(o, "Date", "%s", b);
}

void mod_time(Out *o) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char b[64];
    strftime(b, sizeof b, "%H:%M:%S", &tmv);
    out_add(o, "Time", "%s", b);
}

void mod_datetime(Out *o) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char b[96];
    strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", &tmv);
    out_add(o, "Date & Time", "%s", b);
}

void mod_timezone(Out *o) {
    char link[512] = "", zone[128] = "";
    ssize_t n = readlink("/etc/localtime", link, sizeof link - 1);
    if (n > 0) {
        link[n] = 0;
        char *z = strstr(link, "zoneinfo/");
        if (z) scpy(zone, sizeof zone, z + 9);
    }
    if (!zone[0]) read_line("/etc/timezone", zone, sizeof zone);
    if (!zone[0]) {
        const char *tz = getenv("TZ");
        if (tz) scpy(zone, sizeof zone, tz);
    }
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char off[16];
    strftime(off, sizeof off, "%z", &tmv);
    if (zone[0]) out_add(o, "Timezone", "%s (%s)", zone, off);
    else out_add(o, "Timezone", "UTC%s", off);
}

/* ---------- processos, usuarios, init ---------- */
void mod_processes(Out *o) {
    DIR *d = opendir("/proc");
    if (!d) return;
    int c = 0;
    struct dirent *e;
    while ((e = readdir(d)))
        if (isdigit((unsigned char)e->d_name[0])) c++;
    closedir(d);
    out_add(o, "Processes", "%d", c);
}

void mod_users(Out *o) {
    char names[512] = "";
    int count = 0;
    setutent();
    struct utmp *u;
    while ((u = getutent())) {
        if (u->ut_type != USER_PROCESS) continue;
        char nm[UT_NAMESIZE + 1];
        memcpy(nm, u->ut_user, UT_NAMESIZE);
        nm[UT_NAMESIZE] = 0;
        char probe[UT_NAMESIZE + 4];
        snprintf(probe, sizeof probe, "|%s|", nm);
        char hay[600];
        snprintf(hay, sizeof hay, "|%s|", names);
        for (char *p = hay; *p; p++)
            if (*p == ',' || *p == ' ') *p = '|';
        if (strstr(hay, probe)) continue;
        sapp(names, sizeof names, "%s%s", names[0] ? ", " : "", nm);
        count++;
    }
    endutent();
    if (count) out_add(o, "Users", "%s", names);
}

void mod_initsystem(Out *o) {
    char c[64] = "";
    if (!read_line("/proc/1/comm", c, sizeof c)) return;
    if (!strcmp(c, "systemd") && cmd_exists("systemctl")) {
        char v[256] = "";
        if (run_cmd("systemctl --version 2>/dev/null | head -n1", v, sizeof v)) {
            out_add(o, "Init System", "%s", v);
            return;
        }
    }
    out_add(o, "Init System", "%s", c);
}

void mod_bootmode(Out *o) {
    if (access("/sys/firmware", F_OK) != 0) return;
    if (access("/sys/firmware/efi", F_OK) == 0) {
        unsigned char sb[16];
        FILE *f = fopen("/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c", "rb");
        const char *state = "";
        if (f) {
            size_t n = fread(sb, 1, sizeof sb, f);
            fclose(f);
            if (n >= 5) state = sb[4] ? " (Secure Boot: on)" : " (Secure Boot: off)";
        }
        out_add(o, "Boot Mode", "UEFI%s", state);
    } else {
        out_add(o, "Boot Mode", "BIOS (Legacy)");
    }
}

void mod_libc(Out *o) {
#ifdef __GLIBC__
    out_add(o, "libc", "glibc %s", gnu_get_libc_version());
#else
    DIR *d = opendir("/lib");
    if (!d) return;
    struct dirent *e;
    int musl = 0;
    while ((e = readdir(d)))
        if (!strncmp(e->d_name, "ld-musl", 7)) musl = 1;
    closedir(d);
    if (musl) out_add(o, "libc", "musl");
#endif
}

void mod_virt(Out *o) {
    if (access("/.dockerenv", F_OK) == 0) {
        out_add(o, "Virtualization", "Docker");
        return;
    }
    if (access("/run/.containerenv", F_OK) == 0) {
        out_add(o, "Virtualization", "Podman");
        return;
    }
    char v[128] = "";
    if (cmd_exists("systemd-detect-virt") && run_cmd("systemd-detect-virt 2>/dev/null", v, sizeof v) &&
        strcmp(v, "none") != 0) {
        out_add(o, "Virtualization", "%s", v);
        return;
    }
    char flags[4096] = "";
    if (file_key("/proc/cpuinfo", "flags", flags, sizeof flags) && strstr(flags, " hypervisor")) {
        char vend[128] = "", prod[128] = "";
        dmi("sys_vendor", vend, sizeof vend);
        dmi("product_name", prod, sizeof prod);
        char both[300];
        snprintf(both, sizeof both, "%s%s%s", vend, vend[0] && prod[0] ? " " : "", prod);
        out_add(o, "Virtualization", "%s", both[0] ? both : "Hypervisor");
    }
}

/* ---------- locale ---------- */
void mod_locale(Out *o) {
    const char *l = getenv("LC_ALL");
    if (!l || !*l) l = getenv("LANG");
    out_add(o, "Locale", "%s", l && *l ? l : "C");
}

void mod_lang(Out *o) {
    const char *l = getenv("LANGUAGE");
    if (!l || !*l) l = getenv("LANG");
    if (l && *l) out_add(o, "Language", "%s", l);
}

/* ---------- pacotes ---------- */
static int count_prefix_lines(const char *path, const char *prefix) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    size_t pl = strlen(prefix);
    int c = 0;
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, prefix, pl)) c++;
    fclose(f);
    return c;
}

static int count_suffix_files(const char *path, const char *suffix) {
    DIR *d = opendir(path);
    if (!d) return 0;
    int c = 0;
    size_t sl = strlen(suffix);
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t l = strlen(e->d_name);
        if (l > sl && !strcmp(e->d_name + l - sl, suffix)) c++;
    }
    closedir(d);
    return c;
}

static int count_portage(void) {
    DIR *d = opendir("/var/db/pkg");
    if (!d) return 0;
    int c = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[600];
        snprintf(p, sizeof p, "/var/db/pkg/%s", e->d_name);
        c += count_entries(p, 1);
    }
    closedir(d);
    return c;
}

void mod_packages(Out *o) {
    char v[512] = "";
    char tmp[128];
    const char *home = getenv("HOME");
    int c;
#define ADDPKG(n, name)                                                         \
    do {                                                                        \
        int _n = (n);                                                           \
        if (_n > 0) sapp(v, sizeof v, "%s%d (%s)", v[0] ? ", " : "", _n, (name)); \
    } while (0)

    ADDPKG(count_entries("/var/lib/pacman/local", 1), "pacman");
    ADDPKG(count_prefix_lines("/var/lib/dpkg/status", "Status: install ok installed"), "dpkg");
    if ((access("/var/lib/rpm", F_OK) == 0 || access("/usr/lib/sysimage/rpm", F_OK) == 0) && cmd_exists("rpm") &&
        run_cmd("rpm -qa 2>/dev/null | wc -l", tmp, sizeof tmp))
        ADDPKG(atoi(tmp), "rpm");
    ADDPKG(count_prefix_lines("/lib/apk/db/installed", "P:"), "apk");
    if (cmd_exists("xbps-query") && run_cmd("xbps-query -l 2>/dev/null | wc -l", tmp, sizeof tmp))
        ADDPKG(atoi(tmp), "xbps");
    ADDPKG(count_portage(), "portage");

    c = count_entries("/var/lib/flatpak/app", 1);
    if (home) {
        char p[600];
        snprintf(p, sizeof p, "%s/.local/share/flatpak/app", home);
        c += count_entries(p, 1);
    }
    ADDPKG(c, "flatpak");
    ADDPKG(count_suffix_files("/var/lib/snapd/snaps", ".snap"), "snap");
    ADDPKG(count_entries("/home/linuxbrew/.linuxbrew/Cellar", 1), "brew");
    if (home) {
        char p[600];
        snprintf(p, sizeof p, "%s/.cargo/bin", home);
        ADDPKG(count_entries(p, 0), "cargo");
    }
#undef ADDPKG
    if (v[0]) out_add(o, "Packages", "%s", v);
}

/* ---------- custom1..custom9 ---------- */
static void run_custom(Out *o, int i) {
    char spec[1024];
    scpy(spec, sizeof spec, g_cfg.custom[i]);
    if (!spec[0]) return;
    char *bar = strchr(spec, '|');
    const char *label = "Custom";
    char *cmd = spec;
    if (bar) {
        *bar = 0;
        label = trim(spec);
        cmd = trim(bar + 1);
    }
    char out[1024] = "";
    if (run_cmd(cmd, out, sizeof out)) out_add(o, label, "%s", out);
}

#define CUSTOM_FN(n) \
    void mod_custom##n(Out *o) { run_custom(o, n - 1); }
CUSTOM_FN(1)
CUSTOM_FN(2)
CUSTOM_FN(3)
CUSTOM_FN(4)
CUSTOM_FN(5)
CUSTOM_FN(6)
CUSTOM_FN(7)
CUSTOM_FN(8)
CUSTOM_FN(9)
