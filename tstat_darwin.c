/* Copyright (c) 2019 Adam Steen <adam@adamsteen.com.au>
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
#include <stdint.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

#include "tstat.h"

#define D_SCALED_SZ 8

/* stand-in for OpenBSD fmt_scaled(3), which lives in -lutil */
static char *d_scaled(uint64_t b, char *buf, size_t sz) {
    const char *u = "BKMGTPE";
    double v = (double)b;
    int i = 0;

    while (v >= 1024 && u[i + 1])
        v /= 1024, i++;
    if (!i)
        return d_fmt(buf, sz, "%lluB", (unsigned long long)b);
    return d_fmt(buf, sz, v < 10 ? "%.1f%c" : "%.0f%c", v, u[i]);
}

char *d_net(const char *ifn) {
    static char s[D_BUF];
    /* ponytail: one-shot process zeroes these, so rate reads 0B/s; needs a state file */
    static uint64_t in, out;
    struct ifaddrs *ifas, *ifa;
    struct if_data *ifd;
    uint64_t ib = 0, ob = 0;
    char is[D_SCALED_SZ], os[D_SCALED_SZ], f = 0;

    if (getifaddrs(&ifas) == -1)
        return d_warn("getifaddrs failed");
    for (ifa = ifas; ifa; ifa = ifa->ifa_next)
        /* only the AF_LINK entry carries byte counters */
        if (!strcmp(ifa->ifa_name, ifn) && ifa->ifa_addr &&
            ifa->ifa_addr->sa_family == AF_LINK &&
            (ifd = (struct if_data *)ifa->ifa_data)) {
            ib += ifd->ifi_ibytes, ob += ifd->ifi_obytes, f = 1;
#ifdef TSTAT_TEST
            printf("raw ib=%llu ob=%llu\n", (unsigned long long)ib,
                (unsigned long long)ob);
#endif
        }
    freeifaddrs(ifas);
    if (!f)
        return "interface failed";
    /* counters are u_int32_t and wrap at 4GB; mask the delta back into range */
    d_scaled(in ? (ib - in) & 0xffffffff : 0, is, sizeof(is));
    d_scaled(out ? (ob - out) & 0xffffffff : 0, os, sizeof(os));
    return (in = ib, out = ob,
        d_fmt(s, sizeof(s), "↑ %s/s ↓ %s/s", os, is));
}

char *d_cpu(void) {
    static char s[D_BUF];
    /* ponytail: same one-shot delta ceiling as d_net */
    static natural_t cpu[CPU_STATE_MAX];
    host_cpu_load_info_data_t d;
    mach_msg_type_number_t cnt = HOST_CPU_LOAD_INFO_COUNT;
    natural_t busy, idle;
    int p;

    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
        (host_info_t)&d, &cnt) != KERN_SUCCESS)
        return d_warn("host_statistics failed");
    busy = (d.cpu_ticks[CPU_STATE_USER] - cpu[CPU_STATE_USER]) +
           (d.cpu_ticks[CPU_STATE_SYSTEM] - cpu[CPU_STATE_SYSTEM]) +
           (d.cpu_ticks[CPU_STATE_NICE] - cpu[CPU_STATE_NICE]);
    idle = d.cpu_ticks[CPU_STATE_IDLE] - cpu[CPU_STATE_IDLE];
    memmove(cpu, d.cpu_ticks, sizeof(cpu));
    if (!(busy + idle))
        return "cpu failed";
    p = busy / (double)(busy + idle) * 100;
    return d_fmt(s, sizeof(s), "CPU %d%%", p);
}

char *d_bat(void) {
    static char s[D_BUF];
    CFTypeRef blob;
    CFArrayRef list;
    CFDictionaryRef desc;
    CFNumberRef n;
    CFStringRef st;
    int cap = 0, mins = -1, ac;

    if (!(blob = IOPSCopyPowerSourcesInfo()))
        return d_warn("IOPSCopyPowerSourcesInfo failed");
    if (!(list = IOPSCopyPowerSourcesList(blob)))
        return CFRelease(blob), d_warn("IOPSCopyPowerSourcesList failed");
    if (!CFArrayGetCount(list))
        return CFRelease(list), CFRelease(blob), "no battery";
    desc = IOPSGetPowerSourceDescription(blob, CFArrayGetValueAtIndex(list, 0));
    if ((n = CFDictionaryGetValue(desc, CFSTR(kIOPSCurrentCapacityKey))))
        CFNumberGetValue(n, kCFNumberIntType, &cap);
    if ((n = CFDictionaryGetValue(desc, CFSTR(kIOPSTimeToEmptyKey))))
        CFNumberGetValue(n, kCFNumberIntType, &mins);
    st = CFDictionaryGetValue(desc, CFSTR(kIOPSPowerSourceStateKey));
    ac = st && CFEqual(st, CFSTR(kIOPSACPowerValue));
    CFRelease(list), CFRelease(blob);
    if (ac)
        return d_fmt(s, sizeof(s), "⚡ %d%% [A/C]", cap);
    /* -1 while estimating, 0 means not yet known */
    if (mins <= 0)
        return d_fmt(s, sizeof(s), "⚡ %d%% [--:--]", cap);
    return d_fmt(s, sizeof(s), "⚡ %d%% [%u:%02u]", cap, mins / 60, mins % 60);
}

#ifdef TSTAT_TEST
#include <assert.h>

void d_scaled_test(void) {
    char b[D_SCALED_SZ];

    assert(!strcmp(d_scaled(0, b, sizeof(b)), "0B"));
    assert(!strcmp(d_scaled(999, b, sizeof(b)), "999B"));
    assert(!strcmp(d_scaled(1024, b, sizeof(b)), "1.0K"));
    assert(!strcmp(d_scaled(1536, b, sizeof(b)), "1.5K"));
    assert(!strcmp(d_scaled(5UL * 1024 * 1024 * 1024, b, sizeof(b)), "5.0G"));
    assert(!strcmp(d_scaled(20UL * 1024 * 1024 * 1024, b, sizeof(b)), "20G"));
    puts("d_scaled ok");
}
#endif
