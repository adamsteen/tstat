/* Copyright (c) 2016-2019 Joerg Jung <jung@openbsd.org>
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
#ifndef TSTAT_H
#define TSTAT_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#define D_BUF 64

/* shared helpers, defined in tstat.c */
char *d_fmt(char *s, size_t sz, const char *fmt, ...);
char *d_warn(const char *s);

/* previous sample, persisted across runs; defined in tstat_state.c */
struct d_state {
    uint32_t magic;
    double t;
    uint64_t in, out;
    uint64_t busy, idle;
};

double d_now(void);
int d_state_load(const char *ifn, struct d_state *st);
void d_state_net(uint64_t in, uint64_t out);
void d_state_cpu(uint64_t busy, uint64_t idle);
void d_state_save(const char *ifn);

/* collectors, defined per platform */
char *d_net(const char *ifn);
char *d_cpu(const char *ifn); /* ifn only selects the state file */
char *d_bat(void);
char *d_time(void);

#ifdef __OpenBSD__
char *d_temp(void);
#else
char *d_mem(void); /* no memory compressor on OpenBSD, so macOS-only */
#endif

#endif
