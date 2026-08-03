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

#define D_BUF 64

/* shared helpers, defined in tstat.c */
char *d_fmt(char *s, size_t sz, const char *fmt, ...);
char *d_warn(const char *s);

/* collectors, defined per platform */
char *d_net(const char *ifn);
char *d_cpu(void);
char *d_bat(void);
char *d_time(void);

#ifdef __OpenBSD__
char *d_temp(void);
#endif

#endif
