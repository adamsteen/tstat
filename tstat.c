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
#include <stdarg.h>
#include <string.h>
#include <err.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "tstat.h"

#define D_V "0.1"
#define D_YR "2019"

char *d_warn(const char *s) {
    warn("%s", s);
    return (char *)s;
}

char *d_fmt(char *s, size_t sz, const char *fmt, ...) {
    va_list va;
    int r;

    va_start(va, fmt), r = vsnprintf(s, sz, fmt, va), va_end(va);
    if (r < 0 || (size_t)r >= sz)
        return d_warn("vsnprintf failed");
    return s;
}

char *d_time(void) {
    static char s[D_BUF];
    struct tm *tm;
    time_t ts;

    if ((ts = time(NULL)) == ((time_t) - 1))
        return d_warn("time failed");
    if (!(tm = localtime(&ts)))
        return d_warn("localtime failed");
    if (!strftime(s, sizeof(s), "%Y-%m-%d %H:%M", tm))
        return d_warn("strftime failed");
    return s;
}

static void d_run(const char *ifn) {
    char s[LINE_MAX];

#ifdef __OpenBSD__
    d_fmt(s, sizeof(s), "%s | %s | %s | %s | %s ",
        d_net(ifn), d_cpu(ifn), d_bat(), d_temp(), d_time());
#else
    /* no unprivileged source for cpu freq, temperature or wifi rssi on macOS */
    d_fmt(s, sizeof(s), "%s | %s | %s | %s | %s ",
        d_net(ifn), d_cpu(ifn), d_mem(), d_bat(), d_time());
#endif
    /* one write per run, after both collectors have recorded their readings */
    d_state_save(ifn);
    printf("%s\n", s);
}

int main(int argc, char *argv[]) {
#ifdef TSTAT_TEST
    void d_scaled_test(void);

    d_scaled_test();
#endif
    if (argc == 2 && !strcmp(argv[1], "version")) {
        puts("tstat "D_V" (c) "D_YR" Adam Steen, dstat 0.6 (c) 2015-2019 Joerg Jung");
        return 0;
    }
    if (argc != 2)
        errx(1, "usage: tstat <if>\n%14ststat version", "");
    if (setpriority(PRIO_PROCESS, getpid(), 10))
        err(1, "setpriority failed");
    if (setvbuf(stdout, NULL, _IONBF, 0)) /* allow unbuffered pipe output to others */
        err(1, "setvbuf failed");
    d_run(argv[1]);
    return 0;
}
