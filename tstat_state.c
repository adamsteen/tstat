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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "tstat.h"

#define D_MAGIC 0x54535431 /* 'TST1', bump if struct d_state changes */
#define D_STALE 60.0

static struct d_state cur;
static int cur_set;

double d_now(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return -1;
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

static char *d_state_path(const char *ifn, char *s, size_t sz) {
    const char *d = getenv("TMPDIR");

    return d_fmt(s, sz, "%stmp-tstat-%u-%s.state", d && *d ? d : "/tmp/",
        (unsigned)getuid(), ifn);
}

int d_state_load(const char *ifn, struct d_state *st) {
    char p[PATH_MAX];
    struct stat sb;
    double age;
    int fd, r;

    if (d_state_path(ifn, p, sizeof(p)) != p)
        return -1;
    if ((fd = open(p, O_RDONLY | O_NOFOLLOW)) == -1)
        return -1;
    /* refuse anything not a plain file we own; /tmp is shared on OpenBSD */
    if (fstat(fd, &sb) == -1 || !S_ISREG(sb.st_mode) || sb.st_uid != getuid())
        return close(fd), -1;
    r = read(fd, st, sizeof(*st));
    close(fd);
    if (r != (int)sizeof(*st) || st->magic != D_MAGIC)
        return -1;
    age = d_now() - st->t;
    if (age < 0 || age > D_STALE)
        return -1;
    return 0;
}

void d_state_net(uint64_t in, uint64_t out) {
    cur.in = in, cur.out = out, cur_set = 1;
}

void d_state_cpu(uint64_t busy, uint64_t idle) {
    cur.busy = busy, cur.idle = idle, cur_set = 1;
}

void d_state_save(const char *ifn) {
    char p[PATH_MAX], t[PATH_MAX];
    int fd;

    if (!cur_set)
        return;
    if (d_state_path(ifn, p, sizeof(p)) != p)
        return;
    if (d_fmt(t, sizeof(t), "%s.XXXXXX", p) != t)
        return;
    if ((fd = mkstemp(t)) == -1)
        return;
    cur.magic = D_MAGIC, cur.t = d_now();
    /* rename onto the target replaces a planted symlink rather than following it */
    if (write(fd, &cur, sizeof(cur)) != (ssize_t)sizeof(cur) ||
        close(fd) == -1 || rename(t, p) == -1)
        unlink(t);
}
