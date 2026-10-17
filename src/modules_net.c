/* Modulos de rede e extras: ip, gateway, dns, wifi, trafego, clima, midia. */
#include "cosmofetch.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int prefix_len_v6(const struct in6_addr *m) {
    int n = 0;
    for (int i = 0; i < 16; i++) n += __builtin_popcount(m->s6_addr[i]);
    return n;
}

void mod_localip(Out *o) {
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa)) return;
    for (p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if ((p->ifa_flags & IFF_LOOPBACK) || !(p->ifa_flags & IFF_UP)) continue;
        char ip[INET_ADDRSTRLEN], key[96];
        struct sockaddr_in *a = (struct sockaddr_in *)p->ifa_addr;
        struct sockaddr_in *m = (struct sockaddr_in *)p->ifa_netmask;
        inet_ntop(AF_INET, &a->sin_addr, ip, sizeof ip);
        snprintf(key, sizeof key, "Local IP (%s)", p->ifa_name);
        out_add(o, key, "%s/%d", ip, m ? __builtin_popcount(m->sin_addr.s_addr) : 0);
    }
    freeifaddrs(ifa);
}

void mod_localip6(Out *o) {
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa)) return;
    for (p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET6) continue;
        if ((p->ifa_flags & IFF_LOOPBACK) || !(p->ifa_flags & IFF_UP)) continue;
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)p->ifa_addr;
        if (IN6_IS_ADDR_LINKLOCAL(&a->sin6_addr)) continue;
        struct sockaddr_in6 *m = (struct sockaddr_in6 *)p->ifa_netmask;
        char ip[INET6_ADDRSTRLEN], key[96];
        inet_ntop(AF_INET6, &a->sin6_addr, ip, sizeof ip);
        snprintf(key, sizeof key, "Local IPv6 (%s)", p->ifa_name);
        out_add(o, key, "%s/%d", ip, m ? prefix_len_v6(&m->sin6_addr) : 0);
    }
    freeifaddrs(ifa);
}

void mod_mac(Out *o) {
    struct ifaddrs *ifa, *p;
    if (getifaddrs(&ifa)) return;
    for (p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_PACKET) continue;
        if ((p->ifa_flags & IFF_LOOPBACK) || !(p->ifa_flags & IFF_UP)) continue;
        char path[300], mac[64], key[96];
        snprintf(path, sizeof path, "/sys/class/net/%s/address", p->ifa_name);
        if (!read_line(path, mac, sizeof mac) || !strcmp(mac, "00:00:00:00:00:00")) continue;
        snprintf(key, sizeof key, "MAC (%s)", p->ifa_name);
        out_add(o, key, "%s", mac);
    }
    freeifaddrs(ifa);
}

void mod_gateway(Out *o) {
    FILE *f = fopen("/proc/net/route", "r");
    if (!f) return;
    char line[512];
    if (!fgets(line, sizeof line, f)) {
        fclose(f);
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char iface[64], dst[16], gw[16];
        if (sscanf(line, "%63s %15s %15s", iface, dst, gw) != 3) continue;
        if (strcmp(dst, "00000000")) continue;
        struct in_addr a;
        a.s_addr = (in_addr_t)strtoul(gw, NULL, 16);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &a, ip, sizeof ip);
        out_add(o, "Gateway", "%s (%s)", ip, iface);
        break;
    }
    fclose(f);
}

void mod_dns(Out *o) {
    FILE *f = fopen("/etc/resolv.conf", "r");
    if (!f) return;
    char line[512], v[256] = "";
    int n = 0;
    while (fgets(line, sizeof line, f) && n < 3) {
        char ns[128];
        if (sscanf(line, "nameserver %127s", ns) == 1) {
            sapp(v, sizeof v, "%s%s", v[0] ? ", " : "", ns);
            n++;
        }
    }
    fclose(f);
    if (v[0]) out_add(o, "DNS", "%s", v);
}

void mod_wifi(Out *o) {
    FILE *f = fopen("/proc/net/wireless", "r");
    if (!f) return;
    char line[512];
    if (!fgets(line, sizeof line, f) || !fgets(line, sizeof line, f)) {
        fclose(f);
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char iface[64], status[16];
        double link = 0;
        if (sscanf(line, " %63[^:]: %15s %lf", iface, status, &link) < 2) continue;
        char ssid[128] = "", cmd[256];
        if (cmd_exists("iwgetid")) {
            snprintf(cmd, sizeof cmd, "timeout 2 iwgetid -r %s 2>/dev/null", iface);
            run_cmd(cmd, ssid, sizeof ssid);
        }
        if (!ssid[0] && cmd_exists("nmcli"))
            run_cmd("timeout 2 nmcli -t -f active,ssid dev wifi 2>/dev/null | grep '^yes' | head -n1 | cut -d: -f2-",
                    ssid, sizeof ssid);
        if (!ssid[0]) continue;
        out_add(o, "Wi-Fi", "%s (%s, %.0f%%)", ssid, iface, link * 100.0 / 70.0 > 100 ? 100 : link * 100.0 / 70.0);
    }
    fclose(f);
}

void mod_netio(Out *o) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return;
    char line[512];
    if (!fgets(line, sizeof line, f) || !fgets(line, sizeof line, f)) {
        fclose(f);
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = 0;
        char *name = line;
        while (*name == ' ') name++;
        if (!strcmp(name, "lo")) continue;
        unsigned long long rx = 0, tx = 0, dummy;
        if (sscanf(colon + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu", &rx, &dummy, &dummy, &dummy, &dummy,
                   &dummy, &dummy, &dummy, &tx) < 9)
            continue;
        if (!rx && !tx) continue;
        char r[32], t[32], key[640];
        human_bytes((double)rx, r, sizeof r);
        human_bytes((double)tx, t, sizeof t);
        snprintf(key, sizeof key, "Net IO (%s)", name);
        out_add(o, key, "RX %s / TX %s", r, t);
    }
    fclose(f);
}

void mod_publicip(Out *o) {
    if (!cmd_exists("curl")) return;
    char v[128] = "";
    if (run_cmd("curl -fsS --max-time 3 https://api.ipify.org 2>/dev/null", v, sizeof v)) out_add(o, "Public IP", "%s", v);
}

void mod_weather(Out *o) {
    if (!cmd_exists("curl")) return;
    char loc[200] = "", cmd[400], v[256] = "";
    for (size_t i = 0, j = 0; g_cfg.weather_location[i] && j + 3 < sizeof loc; i++)
        loc[j++] = g_cfg.weather_location[i] == ' ' ? '+' : g_cfg.weather_location[i];
    char q[300];
    snprintf(cmd, sizeof cmd, "wttr.in/%s?format=3", loc);
    shell_quote(cmd, q, sizeof q);
    snprintf(cmd, sizeof cmd, "curl -fsS --max-time 4 %s 2>/dev/null", q);
    if (run_cmd(cmd, v, sizeof v)) out_add(o, "Weather", "%s", v);
}

void mod_media(Out *o) {
    if (!cmd_exists("playerctl")) return;
    char v[256] = "";
    if (run_cmd("timeout 2 playerctl metadata --format '{{artist}} - {{title}} ({{status}})' 2>/dev/null", v,
                sizeof v) &&
        strlen(v) > 6)
        out_add(o, "Media", "%s", v);
}
