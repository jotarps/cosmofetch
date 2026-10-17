/* Modulos de hardware: cpu, gpu, memoria, disco, bateria, temperatura, tela... */
#include "cosmofetch.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <unistd.h>

/* ---------- helpers ---------- */
static void replace_all(char *s, size_t cap, const char *from, const char *to) {
    char *p;
    size_t fl = strlen(from), tl = strlen(to);
    while ((p = strstr(s, from))) {
        size_t tail = strlen(p + fl);
        if (strlen(s) - fl + tl + 1 > cap) return;
        memmove(p + tl, p + fl, tail + 1);
        memcpy(p, to, tl);
    }
}

static void collapse_spaces(char *s) {
    char *w = s;
    int sp = 0;
    for (char *r = s; *r; r++) {
        if (isspace((unsigned char)*r)) {
            if (!sp && w != s) *w++ = ' ';
            sp = 1;
        } else {
            *w++ = *r;
            sp = 0;
        }
    }
    if (w > s && w[-1] == ' ') w--;
    *w = 0;
}

static int sysfs_int(const char *path, long *v) {
    char b[64];
    if (!read_line(path, b, sizeof b)) return 0;
    char *end;
    *v = strtol(b, &end, 10);
    return end != b;
}

static void maybe_bar(char *val, size_t n, double pct) {
    if (!g_cfg.bars) return;
    char b[256];
    bar_str(pct, g_cfg.bar_width, b, sizeof b);
    sapp(val, n, " %s", b);
}

/* ---------- CPU ---------- */
void mod_cpu(Out *o) {
    char model[256] = "";
    if (!file_key("/proc/cpuinfo", "model name", model, sizeof model) &&
        !file_key("/proc/cpuinfo", "Hardware", model, sizeof model) &&
        !file_key("/proc/cpuinfo", "Processor", model, sizeof model) &&
        !file_key("/proc/cpuinfo", "cpu model", model, sizeof model))
        return;
    char *at = strstr(model, " @");
    if (at) *at = 0;
    replace_all(model, sizeof model, "(R)", "");
    replace_all(model, sizeof model, "(TM)", "");
    replace_all(model, sizeof model, "(tm)", "");
    replace_all(model, sizeof model, " CPU", "");
    replace_all(model, sizeof model, " Processor", "");
    collapse_spaces(model);
    int cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    double ghz = 0;
    long khz;
    if (sysfs_int("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", &khz)) {
        ghz = khz / 1e6;
    } else {
        char mhz[64] = "";
        if (file_key("/proc/cpuinfo", "cpu MHz", mhz, sizeof mhz)) ghz = atof(mhz) / 1000.0;
    }
    if (ghz > 0) out_add(o, "CPU", "%s (%d) @ %.2f GHz", model, cores, ghz);
    else out_add(o, "CPU", "%s (%d)", model, cores);
}

static int read_cpu_times(unsigned long long *total, unsigned long long *idle) {
    char buf[512];
    if (!read_line("/proc/stat", buf, sizeof buf)) return 0;
    unsigned long long v[8] = {0};
    if (sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5],
               &v[6], &v[7]) < 4)
        return 0;
    *total = 0;
    for (int i = 0; i < 8; i++) *total += v[i];
    *idle = v[3] + v[4];
    return 1;
}

void mod_cpuusage(Out *o) {
    unsigned long long t1, i1, t2, i2;
    if (!read_cpu_times(&t1, &i1)) return;
    usleep(200000);
    if (!read_cpu_times(&t2, &i2) || t2 <= t1) return;
    double pct = 100.0 * (double)((t2 - t1) - (i2 - i1)) / (double)(t2 - t1);
    char v[300];
    snprintf(v, sizeof v, "%.0f%%", pct);
    maybe_bar(v, sizeof v, pct);
    out_add(o, "CPU Usage", "%s", v);
}

void mod_cpucache(Out *o) {
    char v[256] = "";
    for (int i = 0; i < 10; i++) {
        char p[256], lvl[16] = "", type[32] = "", size[32] = "";
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/level", i);
        if (!read_line(p, lvl, sizeof lvl)) break;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/type", i);
        read_line(p, type, sizeof type);
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/size", i);
        if (!read_line(p, size, sizeof size)) continue;
        const char *suf = "";
        if (!strcmp(type, "Data")) suf = "d";
        else if (!strcmp(type, "Instruction")) suf = "i";
        sapp(v, sizeof v, "%sL%s%s: %s", v[0] ? ", " : "", lvl, suf, size);
    }
    if (v[0]) out_add(o, "CPU Cache", "%s", v);
}

/* ---------- GPU ---------- */
static const char *vendor_name(const char *id) {
    if (!strcmp(id, "0x10de")) return "NVIDIA";
    if (!strcmp(id, "0x1002")) return "AMD";
    if (!strcmp(id, "0x8086")) return "Intel";
    if (!strcmp(id, "0x1af4")) return "Red Hat (virtio)";
    if (!strcmp(id, "0x15ad")) return "VMware";
    if (!strcmp(id, "0x1234")) return "QEMU";
    if (!strcmp(id, "0x1414")) return "Microsoft";
    if (!strcmp(id, "0x1b36")) return "Red Hat (QXL)";
    if (!strcmp(id, "0x5143")) return "Qualcomm";
    return NULL;
}

void mod_gpu(Out *o) {
    int found = 0;
    if (cmd_exists("lspci")) {
        char buf[8192];
        run_cmd_all("lspci 2>/dev/null", buf, sizeof buf);
        char *save = NULL;
        for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            if (!strstr(line, "VGA compatible controller") && !strstr(line, "3D controller") &&
                !strstr(line, "Display controller"))
                continue;
            char *p = strstr(line, "controller: ");
            if (!p) continue;
            p += 12;
            char name[256];
            scpy(name, sizeof name, p);
            char *rev = strstr(name, " (rev");
            if (rev) *rev = 0;
            replace_all(name, sizeof name, "Advanced Micro Devices, Inc. [AMD/ATI]", "AMD");
            replace_all(name, sizeof name, "NVIDIA Corporation", "NVIDIA");
            replace_all(name, sizeof name, "Intel Corporation", "Intel");
            replace_all(name, sizeof name, "Red Hat, Inc.", "Red Hat");
            out_add(o, "GPU", "%s", name);
            found++;
        }
    }
    if (found) return;
    for (int i = 0; i < 8; i++) {
        char p[256], ven[32] = "", dev[32] = "";
        snprintf(p, sizeof p, "/sys/class/drm/card%d/device/vendor", i);
        if (!read_line(p, ven, sizeof ven)) continue;
        snprintf(p, sizeof p, "/sys/class/drm/card%d/device/device", i);
        read_line(p, dev, sizeof dev);
        const char *vn = vendor_name(ven);
        char drv[64] = "";
        snprintf(p, sizeof p, "/sys/class/drm/card%d/device/driver", i);
        char link[256];
        ssize_t n = readlink(p, link, sizeof link - 1);
        if (n > 0) {
            link[n] = 0;
            char *b = strrchr(link, '/');
            scpy(drv, sizeof drv, b ? b + 1 : link);
        }
        out_add(o, "GPU", "%s [%s:%s]%s%s%s", vn ? vn : ven, ven + 2, dev[0] ? dev + 2 : "????",
                drv[0] ? " (" : "", drv, drv[0] ? ")" : "");
    }
}

/* ---------- memoria ---------- */
static void mem_line(Out *o, const char *label, const char *tkey, const char *fkey, int is_swap) {
    char t[64] = "", f[64] = "";
    file_key("/proc/meminfo", tkey, t, sizeof t);
    file_key("/proc/meminfo", fkey, f, sizeof f);
    double total = atof(t) * 1024.0, freev = atof(f) * 1024.0;
    if (total <= 0) {
        if (is_swap) out_add(o, label, "Disabled");
        return;
    }
    double used = total - freev;
    char us[32], ts[32], v[400];
    human_bytes(used, us, sizeof us);
    human_bytes(total, ts, sizeof ts);
    snprintf(v, sizeof v, "%s / %s (%.0f%%)", us, ts, used * 100.0 / total);
    maybe_bar(v, sizeof v, used * 100.0 / total);
    out_add(o, label, "%s", v);
}

void mod_memory(Out *o) { mem_line(o, "Memory", "MemTotal", "MemAvailable", 0); }
void mod_swap(Out *o) { mem_line(o, "Swap", "SwapTotal", "SwapFree", 1); }

/* ---------- discos ---------- */
static void unescape_mount(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) {
        if (r[0] == '\\' && isdigit((unsigned char)r[1]) && isdigit((unsigned char)r[2]) && isdigit((unsigned char)r[3])) {
            *w++ = (char)(((r[1] - '0') << 6) | ((r[2] - '0') << 3) | (r[3] - '0'));
            r += 3;
        } else {
            *w++ = *r;
        }
    }
    *w = 0;
}

void mod_disk(Out *o) {
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return;
    static const char *okfs[] = {"ext2", "ext3", "ext4", "btrfs", "xfs", "f2fs", "vfat", "exfat", "ntfs",
                                 "ntfs3", "fuseblk", "zfs", "jfs", "reiserfs", "nilfs2", "bcachefs", "overlay",
                                 "9p", "virtiofs", "tmpfs-root", NULL};
    char line[2048];
    long long seen[32][2];
    int nseen = 0, shown = 0;
    while (fgets(line, sizeof line, f) && shown < 16) {
        char dev[512], mp[512], ty[64];
        if (sscanf(line, "%511s %511s %63s", dev, mp, ty) != 3) continue;
        int ok = 0;
        for (int i = 0; okfs[i]; i++)
            if (!strcmp(ty, okfs[i])) ok = 1;
        if (!ok) continue;
        unescape_mount(mp);
        if (!strncmp(mp, "/var/lib/docker", 15) || !strncmp(mp, "/snap", 5) || !strncmp(mp, "/run", 4) ||
            !strncmp(mp, "/sys", 4) || !strncmp(mp, "/proc", 5) || !strncmp(mp, "/dev", 4) ||
            !strncmp(mp, "/etc/", 5) || !strncmp(mp, "/usr/lib/", 9))
            continue;
        if (!strcmp(ty, "overlay") && strcmp(mp, "/")) continue;
        struct statfs sf;
        struct statvfs sv;
        if (statfs(mp, &sf) || statvfs(mp, &sv) || sf.f_blocks == 0) continue;
        long long a = ((long long)sf.f_fsid.__val[0]), b = ((long long)sf.f_fsid.__val[1]);
        int dup = 0;
        for (int i = 0; i < nseen; i++)
            if (seen[i][0] == a && seen[i][1] == b) dup = 1;
        if (dup) continue;
        if (nseen < 32) {
            seen[nseen][0] = a;
            seen[nseen][1] = b;
            nseen++;
        }
        double total = (double)sf.f_blocks * (double)sf.f_bsize;
        double used = total - (double)sf.f_bfree * (double)sf.f_bsize;
        char us[32], ts[32], key[600], v[600];
        human_bytes(used, us, sizeof us);
        human_bytes(total, ts, sizeof ts);
        snprintf(key, sizeof key, "Disk (%s)", mp);
        snprintf(v, sizeof v, "%s / %s (%.0f%%) - %s%s", us, ts, used * 100.0 / total, ty,
                 (sv.f_flag & ST_RDONLY) ? " [Read-only]" : "");
        maybe_bar(v, sizeof v, used * 100.0 / total);
        out_add(o, key, "%s", v);
        shown++;
    }
    fclose(f);
}

void mod_physicaldisk(Out *o) {
    DIR *d = opendir("/sys/block");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        const char *n = e->d_name;
        if (n[0] == '.' || !strncmp(n, "loop", 4) || !strncmp(n, "ram", 3) || !strncmp(n, "zram", 4) ||
            !strncmp(n, "dm-", 3) || !strncmp(n, "md", 2) || !strncmp(n, "sr", 2) || !strncmp(n, "fd", 2) ||
            !strncmp(n, "nbd", 3))
            continue;
        char p[300], model[128] = "", vendor[64] = "";
        long sectors = 0, rot = -1, rem = 0;
        snprintf(p, sizeof p, "/sys/block/%s/size", n);
        if (!sysfs_int(p, &sectors) || sectors <= 0) continue;
        snprintf(p, sizeof p, "/sys/block/%s/device/model", n);
        read_line(p, model, sizeof model);
        snprintf(p, sizeof p, "/sys/block/%s/device/vendor", n);
        read_line(p, vendor, sizeof vendor);
        snprintf(p, sizeof p, "/sys/block/%s/queue/rotational", n);
        sysfs_int(p, &rot);
        snprintf(p, sizeof p, "/sys/block/%s/removable", n);
        sysfs_int(p, &rem);
        char sz[32], key[320], name[256] = "";
        human_bytes((double)sectors * 512.0, sz, sizeof sz);
        if (vendor[0] && model[0]) snprintf(name, sizeof name, "%s %s", vendor, model);
        else if (model[0]) scpy(name, sizeof name, model);
        else scpy(name, sizeof name, n);
        collapse_spaces(name);
        const char *type = !strncmp(n, "nvme", 4) ? "NVMe" : (rot == 1 ? "HDD" : (rot == 0 ? "SSD" : "Disk"));
        snprintf(key, sizeof key, "Physical Disk (%s)", n);
        out_add(o, key, "%s (%s, %s%s)", name, sz, type, rem ? ", Removable" : "");
    }
    closedir(d);
}

/* ---------- energia ---------- */
void mod_battery(Out *o) {
    DIR *d = opendir("/sys/class/power_supply");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[400], type[32] = "";
        snprintf(p, sizeof p, "/sys/class/power_supply/%s/type", e->d_name);
        if (!read_line(p, type, sizeof type) || strcmp(type, "Battery")) continue;
        char sc[64] = "", status[64] = "", model[96] = "";
        long cap;
        snprintf(p, sizeof p, "/sys/class/power_supply/%s/capacity", e->d_name);
        if (!sysfs_int(p, &cap)) continue;
        snprintf(p, sizeof p, "/sys/class/power_supply/%s/status", e->d_name);
        read_line(p, status, sizeof status);
        snprintf(p, sizeof p, "/sys/class/power_supply/%s/model_name", e->d_name);
        read_line(p, model, sizeof model);
        (void)sc;
        long full = 0, design = 0;
        snprintf(p, sizeof p, "/sys/class/power_supply/%s/energy_full", e->d_name);
        if (!sysfs_int(p, &full)) {
            snprintf(p, sizeof p, "/sys/class/power_supply/%s/charge_full", e->d_name);
            sysfs_int(p, &full);
        }
        snprintf(p, sizeof p, "/sys/class/power_supply/%s/energy_full_design", e->d_name);
        if (!sysfs_int(p, &design)) {
            snprintf(p, sizeof p, "/sys/class/power_supply/%s/charge_full_design", e->d_name);
            sysfs_int(p, &design);
        }
        char key[160], v[400];
        if (model[0]) snprintf(key, sizeof key, "Battery (%s)", model);
        else snprintf(key, sizeof key, "Battery");
        snprintf(v, sizeof v, "%ld%%", cap);
        if (status[0]) sapp(v, sizeof v, " [%s]", status);
        if (full > 0 && design > 0) sapp(v, sizeof v, " (health %.0f%%)", 100.0 * full / design);
        maybe_bar(v, sizeof v, (double)cap);
        out_add(o, key, "%s", v);
    }
    closedir(d);
}

void mod_poweradapter(Out *o) {
    DIR *d = opendir("/sys/class/power_supply");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[400], type[32] = "";
        long online;
        snprintf(p, sizeof p, "/sys/class/power_supply/%s/type", e->d_name);
        if (!read_line(p, type, sizeof type) || strcmp(type, "Mains")) continue;
        snprintf(p, sizeof p, "/sys/class/power_supply/%s/online", e->d_name);
        if (!sysfs_int(p, &online)) continue;
        out_add(o, "Power Adapter", "%s", online ? "Connected" : "Disconnected");
        break;
    }
    closedir(d);
}

/* ---------- sensores ---------- */
void mod_temperature(Out *o) {
    static const char *pref[] = {"coretemp", "k10temp", "zenpower", "cpu_thermal", "cpu-thermal", "acpitz", NULL};
    for (int pi = 0; pref[pi]; pi++) {
        for (int h = 0; h < 16; h++) {
            char p[300], name[64] = "";
            snprintf(p, sizeof p, "/sys/class/hwmon/hwmon%d/name", h);
            if (!read_line(p, name, sizeof name) || strcmp(name, pref[pi])) continue;
            long t;
            snprintf(p, sizeof p, "/sys/class/hwmon/hwmon%d/temp1_input", h);
            if (sysfs_int(p, &t)) {
                out_add(o, "Temperature", "%.1f\xc2\xb0""C (%s)", t / 1000.0, name);
                return;
            }
        }
    }
    for (int z = 0; z < 16; z++) {
        char p[300], type[64] = "";
        long t;
        snprintf(p, sizeof p, "/sys/class/thermal/thermal_zone%d/temp", z);
        if (!sysfs_int(p, &t)) continue;
        snprintf(p, sizeof p, "/sys/class/thermal/thermal_zone%d/type", z);
        read_line(p, type, sizeof type);
        out_add(o, "Temperature", "%.1f\xc2\xb0""C%s%s%s", t / 1000.0, type[0] ? " (" : "", type, type[0] ? ")" : "");
        return;
    }
}

void mod_fans(Out *o) {
    for (int h = 0; h < 16; h++) {
        for (int f = 1; f <= 6; f++) {
            char p[300];
            long rpm;
            snprintf(p, sizeof p, "/sys/class/hwmon/hwmon%d/fan%d_input", h, f);
            if (!sysfs_int(p, &rpm)) continue;
            char key[64];
            snprintf(key, sizeof key, "Fan %d", f);
            out_add(o, key, "%ld RPM", rpm);
        }
    }
}

void mod_brightness(Out *o) {
    DIR *d = opendir("/sys/class/backlight");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[300];
        long cur, max;
        snprintf(p, sizeof p, "/sys/class/backlight/%s/brightness", e->d_name);
        if (!sysfs_int(p, &cur)) continue;
        snprintf(p, sizeof p, "/sys/class/backlight/%s/max_brightness", e->d_name);
        if (!sysfs_int(p, &max) || max <= 0) continue;
        char v[300];
        snprintf(v, sizeof v, "%.0f%%", 100.0 * cur / max);
        maybe_bar(v, sizeof v, 100.0 * cur / max);
        out_add(o, "Brightness", "%s", v);
        break;
    }
    closedir(d);
}

/* ---------- audio / bluetooth ---------- */
void mod_sound(Out *o) {
    FILE *f = fopen("/proc/asound/cards", "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        if (line[0] != ' ' || !isdigit((unsigned char)line[1])) continue;
        char *dash = strstr(line, " - ");
        if (!dash) continue;
        char *name = trim(dash + 3);
        out_add(o, "Sound", "%s", name);
    }
    fclose(f);
}

void mod_bluetooth(Out *o) {
    DIR *d = opendir("/sys/class/bluetooth");
    int adapters = 0;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)))
            if (!strncmp(e->d_name, "hci", 3) && !strchr(e->d_name, ':')) adapters++;
        closedir(d);
    }
    if (!adapters) return;
    char v[512] = "";
    if (cmd_exists("bluetoothctl")) {
        char buf[2048];
        run_cmd_all("timeout 2 bluetoothctl devices Connected 2>/dev/null", buf, sizeof buf);
        char *save = NULL;
        for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            if (strncmp(line, "Device ", 7)) continue;
            char *name = strchr(line + 7, ' ');
            if (name) sapp(v, sizeof v, "%s%s", v[0] ? ", " : "", name + 1);
        }
    }
    out_add(o, "Bluetooth", "%s", v[0] ? v : "On (no connected devices)");
}

/* ---------- telas ---------- */
static void parse_edid(const char *path, char *name, size_t nn, double *inches) {
    unsigned char e[256];
    name[0] = 0;
    *inches = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    size_t n = fread(e, 1, sizeof e, f);
    fclose(f);
    if (n < 128) return;
    if (e[21] && e[22]) *inches = sqrt((double)e[21] * e[21] + (double)e[22] * e[22]) / 2.54;
    for (int off = 54; off <= 108; off += 18) {
        if (e[off] == 0 && e[off + 1] == 0 && e[off + 3] == 0xFC) {
            char b[16];
            memcpy(b, e + off + 5, 13);
            b[13] = 0;
            b[strcspn(b, "\n")] = 0;
            scpy(name, nn, trim(b));
            return;
        }
    }
}

static int display_xrandr(Out *o) {
    if (!getenv("DISPLAY") || !cmd_exists("xrandr")) return 0;
    char buf[16384];
    if (!run_cmd_all("timeout 2 xrandr --current 2>/dev/null", buf, sizeof buf)) return 0;
    int count = 0;
    char cur[64] = "";
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (line[0] != ' ') {
            cur[0] = 0;
            if (strstr(line, " connected")) sscanf(line, "%63s", cur);
            continue;
        }
        if (!cur[0] || !strchr(line, '*')) continue;
        char res[64] = "";
        sscanf(line, " %63s", res);
        char *star = strchr(line, '*');
        char *s = star;
        while (s > line && !isspace((unsigned char)s[-1])) s--;
        char hz[32];
        size_t l = (size_t)(star - s);
        if (l >= sizeof hz) l = sizeof hz - 1;
        memcpy(hz, s, l);
        hz[l] = 0;
        char key[128];
        snprintf(key, sizeof key, "Display (%s)", cur);
        out_add(o, key, "%s @ %s Hz", res, hz);
        count++;
        cur[0] = 0;
    }
    return count;
}

void mod_display(Out *o) {
    if (display_xrandr(o)) return;
    DIR *d = opendir("/sys/class/drm");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "card", 4) || !strchr(e->d_name, '-')) continue;
        char p[400], st[32] = "", mode[64] = "";
        snprintf(p, sizeof p, "/sys/class/drm/%s/status", e->d_name);
        if (!read_line(p, st, sizeof st) || strcmp(st, "connected")) continue;
        snprintf(p, sizeof p, "/sys/class/drm/%s/modes", e->d_name);
        read_line(p, mode, sizeof mode);
        if (!mode[0]) continue;
        char nm[96] = "", key[160];
        double in;
        snprintf(p, sizeof p, "/sys/class/drm/%s/edid", e->d_name);
        parse_edid(p, nm, sizeof nm, &in);
        const char *conn = strchr(e->d_name, '-') + 1;
        snprintf(key, sizeof key, "Display (%s)", nm[0] ? nm : conn);
        if (in > 0) out_add(o, key, "%s in %.0f\" [%s]", mode, in, conn);
        else out_add(o, key, "%s [%s]", mode, conn);
    }
    closedir(d);
}
