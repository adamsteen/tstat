# $Id: Makefile 34 2017-06-21 19:57:22Z umaxx $
# Copyright (c) 2016-2017 Joerg Jung <mail@umaxx.net>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

CC?=gcc
INSTALL?=install
RM?=rm -f

PREFIX?=/usr/local

BINDIR?=$(PREFIX)/bin
INCDIR?=$(PREFIX)/include
LIBDIR?=$(PREFIX)/lib
MANDIR?=$(PREFIX)/man

CFLAGS?=-Os
CFLAGS+=-ansi -pedantic -Wall -Wextra
CFLAGS+=-Isrc -I/usr/include -I$(INCDIR)

LDFLAGS+=-L/usr/lib -L$(LIBDIR)

LIBS+=-lutil

OBJECTS=tstat.o

all: tstat

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

tstat: $(OBJECTS)
	$(CC) $(LDFLAGS) -o tstat $(OBJECTS) $(LIBS)

clean:
	$(RM) $(OBJECTS) tstat tstat.core

install: tstat
	$(INSTALL) -m0755 tstat $(BINDIR)
	$(INSTALL) -m0444 tstat.1 $(MANDIR)/man1

uninstall:
	$(RM) $(BINDIR)/tstat $(MANDIR)/man1/tstat.1

.PHONY: all clean install uninstall
