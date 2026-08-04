# macOS build; GNU make prefers this file, OpenBSD make ignores it
# Copyright (c) 2019 Adam Steen <adam@adamsteen.com.au>
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

CC?=cc
INSTALL?=install
RM?=rm -f

PREFIX?=/usr/local

BINDIR?=$(PREFIX)/bin
MANDIR?=$(PREFIX)/share/man

CFLAGS?=-Os
CFLAGS+=-std=c99 -pedantic -Wall -Wextra

LIBS+=-framework CoreFoundation -framework IOKit

OBJECTS=tstat.o tstat_state.o tstat_darwin.o

all: tstat

%.o: %.c tstat.h
	$(CC) -c $(CFLAGS) -o $@ $<

tstat: $(OBJECTS)
	$(CC) $(LDFLAGS) -o tstat $(OBJECTS) $(LIBS)

test: CFLAGS+=-DTSTAT_TEST
test: clean tstat
	./tstat en0

clean:
	$(RM) $(OBJECTS) tstat tstat.core

install: tstat
	$(INSTALL) -m0755 tstat $(BINDIR)
	$(INSTALL) -m0444 tstat.1 $(MANDIR)/man1

uninstall:
	$(RM) $(BINDIR)/tstat $(MANDIR)/man1/tstat.1

.PHONY: all clean install uninstall test
