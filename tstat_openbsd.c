/* $Id: tstat.c 40 2019-01-01 00:30:07Z umaxx $
 * Copyright (c) 2016-2019 Joerg Jung <jung@openbsd.org>
 * Copyright (c) 2019 Adam Steen <adam@adamsteen.com.au>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sensors.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <machine/apmvar.h>
#include <util.h>

#include "tstat.h"

static const char *d_dots(unsigned char q) {
    const char *s[] = { "  ", " .", "..", ".:", "::" };

    return s[((4 * q) / 100)];
}

static char *d_wifi(const char *ifn) {
    static char s[D_BUF];
    struct ieee80211_bssid bssid;
    struct ieee80211_nodereq nr;
    int fd, q;

    memset(&bssid, 0, sizeof(bssid)), memset(&nr, 0, sizeof(nr));
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        return d_warn("socket failed");
    d_fmt(bssid.i_name, sizeof(bssid.i_name), "%s", ifn);
    if ((ioctl(fd, SIOCG80211BSSID, &bssid)) == -1)
        return close(fd), (errno == ENOTTY ? "" : d_warn("ioctl failed"));
    d_fmt(nr.nr_ifname, sizeof(nr.nr_ifname), "%s", ifn);
    memmove(&nr.nr_macaddr, bssid.i_bssid, sizeof(nr.nr_macaddr));
    if ((ioctl(fd, SIOCG80211NODE, &nr)) == -1 && nr.nr_rssi)
        return close(fd), d_warn("ioctl failed");
    if (nr.nr_max_rssi)
        q = IEEE80211_NODEREQ_RSSI(&nr);
    else
        q = nr.nr_rssi >= -50 ? 100 : (nr.nr_rssi <= -100 ? 0 :
            (2 * (nr.nr_rssi + 100)));
    return close(fd), d_fmt(s, sizeof(s), "[%s]", d_dots(q));
}

char *d_net(const char *ifn) {
    static char s[D_BUF];
    /* ponytail: one-shot process zeroes these, so rate reads 0B/s; needs a state file */
    static uint64_t in, out;
    struct ifaddrs *ifas, *ifa;
    struct if_data *ifd;
    uint64_t ib = 0, ob = 0;
    char is[FMT_SCALED_STRSIZE], os[FMT_SCALED_STRSIZE], f = 0, *w;

    if (getifaddrs(&ifas) == -1)
        return d_warn("getifaddrs failed");
    for (ifa = ifas; ifa; ifa = ifa->ifa_next)
        if (!strcmp(ifa->ifa_name, ifn) &&
            (ifd = (struct if_data *)ifa->ifa_data))
            ib += ifd->ifi_ibytes, ob += ifd->ifi_obytes, f = 1;
    freeifaddrs(ifas);
    if (!f)
        return "interface failed";
    if (fmt_scaled(in ? ib - in : 0, is) == -1 ||
        fmt_scaled(out ? ob - out : 0, os) == -1)
        return d_warn("fmt_scaled failed");
    return (in = ib, out = ob, w = d_wifi(ifn), strlen(w) ?
        d_fmt(s, sizeof(s), "↑ %s/s ↓ %s/s %s", os, is, w) :
        d_fmt(s, sizeof(s), "↑ %s/s ↓ %s/s", os, is));
}

static char *d_perf(void) {
    static char s[D_BUF];
    int mib[2] = { CTL_HW, HW_CPUSPEED }, frq, p;
    size_t sz = sizeof(frq);

    if (sysctl(mib, 2, &frq, &sz, NULL, 0) == -1)
        return d_warn("sysctl failed");
    mib[1] = HW_SETPERF, sz = sizeof(p);
    if (sysctl(mib, 2, &p, &sz, NULL, 0) == -1)
        return d_warn("sysctl failed");
    return d_fmt(s, sizeof(s), "%0.1fGHz [%d%%]", frq / (double)1000, p);
}

char *d_cpu(void) {
    static char s[D_BUF];
    /* ponytail: same one-shot delta ceiling as d_net */
    static long cpu[CPUSTATES];
    int mib[2] = { CTL_KERN, KERN_CPTIME }, p;
    long c[CPUSTATES];
    size_t sz = sizeof(c);

    if (sysctl(mib, 2, &c, &sz, NULL, 0) == -1)
        return d_warn("sysctl failed");
    p = (c[CP_USER] - cpu[CP_USER] + c[CP_SYS] - cpu[CP_SYS] +
         c[CP_NICE] - cpu[CP_NICE]) / (double)
        (c[CP_USER] - cpu[CP_USER] + c[CP_SYS] - cpu[CP_SYS] +
         c[CP_NICE] - cpu[CP_NICE] + c[CP_IDLE] - cpu[CP_IDLE]) * 100;
    memmove(cpu, c, sizeof(cpu));
    return d_fmt(s, sizeof(s), "CPU %d%% %s", p, d_perf());
}

char *d_bat(void) {
    static char s[D_BUF], w;
    struct apm_power_info api;
    int fd;

    if ((fd = open("/dev/apm", O_RDONLY)) == -1)
        return d_warn("open failed");
    if (ioctl(fd, APM_IOC_GETPOWER, &api) == -1)
        return close(fd), d_warn("ioctl failed");
    close(fd);
    if (!w && api.ac_state != APM_AC_ON && api.minutes_left <= 10)
        w = 1;
    return (api.ac_state == APM_AC_ON) ? (w = 0,
        d_fmt(s, sizeof(s), "⚡ %d%% [A/C]",
            api.battery_life)) :
        d_fmt(s, sizeof(s), "⚡ %d%% [%u:%02u]",
            api.battery_life,
            api.minutes_left / 60, api.minutes_left % 60);
}

char *d_temp(void) {
    static char s[D_BUF];
    struct sensordev sd;
    struct sensor sn;
    size_t sd_sz = sizeof(sd), sn_sz = sizeof(sn);
    int mib[5] = { CTL_HW, HW_SENSORS, 0, SENSOR_TEMP, 0 };
    int64_t t = -1;

    for (mib[2] = 0; ; mib[2]++) {
        if (sysctl(mib, 3, &sd, &sd_sz, NULL, 0) == -1) {
            if (errno == ENXIO)
                continue;
            else if (errno == ENOENT)
                break;
            return d_warn("sysctl failed");
        }
        for (mib[4] = 0; mib[4] < sd.maxnumt[SENSOR_TEMP]; mib[4]++) {
            if (sysctl(mib, 5, &sn, &sn_sz, NULL, 0) == -1) {
                if (errno == ENXIO)
                    continue;
                else if (errno == ENOENT)
                    break;
                return d_warn("sysctl failed");
            }
            if (sn_sz && !(sn.flags & SENSOR_FINVALID))
                t = sn.value > t ? sn.value : t;
        }
    }
    return t == -1 ? "temperature failed" :
        d_fmt(s, sizeof(s), "T %.1f°C", (t - 273150000) / 1000000.0);
}
