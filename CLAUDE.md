# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`tstat` is a small C utility that prints one line of system statistics to stdout
and exits. It is a fork of Joerg Jung's `dstat` (a dwm status bar tool), tweaked
to feed a **tmux** status line instead.

**Builds on OpenBSD and macOS.** Platform-specific collectors are split into
separate files behind `tstat.h`; the OpenBSD side still uses `sys/sensors.h`,
`machine/apmvar.h`, `net80211/`, `KERN_CPTIME`, `HW_SETPERF`, and `fmt_scaled(3)`
from `-lutil`, none of which exist on macOS.

**macOS omits three fields** — CPU frequency, temperature, and wifi signal.
Not an oversight, and not worth reopening: `HW_CPUSPEED`/`HW_SETPERF` do not
exist on Apple Silicon, SMC temperature needs root (`powermetrics`), and RSSI
would mean linking CoreWLAN and pulling Objective-C into an ANSI C file. macOS
prints `net | CPU | battery | time`; OpenBSD prints those plus temperature, and
its net field carries a wifi signal graph.

## Build

```sh
make                 # builds ./tstat
make clean
make install         # PREFIX?=/usr/local
make uninstall
make test            # macOS only: d_scaled asserts + raw counter print
```

Two makefiles, no `uname` conditionals. GNU make prefers `GNUmakefile` and
OpenBSD make does not look for that name at all, so each platform picks up its
own file with no detection logic. Do not merge them — OpenBSD make and GNU make
3.81 have incompatible conditional syntax (`.if`/`VAR != cmd` vs `ifeq`/`$(shell)`),
and the OpenBSD branch cannot be compile-tested from a Mac.

- `Makefile` (OpenBSD): `tstat.o tstat_openbsd.o`, `-lutil`, `-ansi`.
- `GNUmakefile` (macOS): `tstat.o tstat_darwin.o`, `-framework CoreFoundation
  -framework IOKit`, and `-std=c99` — the mach and IOKit headers are not
  C89-clean, so `-ansi` cannot be used there.

Despite the macOS `-std=c99`, **keep new code C89-clean** (declarations at the
top of blocks, no `//` comments, no mixed declarations) or the OpenBSD `-ansi`
build breaks. `-ansi` is still correct on the OpenBSD Makefile and was left
alone deliberately during the split — the moved collectors are verbatim C89 and
`tstat.h` adds only prototypes. Don't "fix" its absence on the mac side.

No tests beyond `make test`, no linter, no CI.

Run: `tstat <interface>` — `tstat trunk0` on OpenBSD, `tstat en0` on this Mac
(Wi-Fi is `en0`; the default route may be a different `enN`). Or `tstat version`.

## Architecture

Three source files plus a header:

- `tstat.c` — `main`, `d_run`, and the shared helpers `d_fmt`, `d_warn`, `d_time`.
  Platform-neutral; the only `#ifdef __OpenBSD__` is `d_run`'s format string,
  which picks the five-field or four-field line.
- `tstat.h` — the collector contract: `d_net(ifn)`, `d_cpu()`, `d_bat()`,
  `d_time()`, plus `d_temp()` guarded under `#ifdef __OpenBSD__` so the mac
  build cannot reference it. Also `D_BUF`.
- `tstat_openbsd.c` — `d_net`, `d_wifi`, `d_perf`, `d_cpu`, `d_bat`, `d_temp`.
- `tstat_darwin.c` — `d_net`, `d_cpu`, `d_bat`, and `d_scaled`.

`main` → `d_run(ifn)` → one `d_fmt` call joining the collectors with `" | "`:

    OpenBSD: d_net(ifn) | d_cpu() | d_bat() | d_temp() | d_time()
    macOS:   d_net(ifn) | d_cpu() | d_bat() |             d_time()

Key structural facts, most following from the fork's one-shot design:

- **One shot, not a loop.** Upstream `dstat` looped forever with a 1s sleep and
  drove dwm via X11. This fork prints once and exits — tmux re-invokes it on its
  own `status-interval`. The man page (`tstat.1`) is still the inherited `dstat`
  page and describes the old looping/dwm/X11 behaviour; it is stale.
- **Two of the fields carry no live data, on both platforms.** `d_net` and
  `d_cpu` compute deltas against `static` locals that were meant to persist
  across loop iterations. A one-shot process leaves them zeroed, so **throughput
  always prints `0B/s`** and **CPU prints the since-boot average, not current
  load** (verified: reads a flat 8% while three `yes` processes spin). Same dead
  latch for `static char w` in OpenBSD's `d_bat`. Marked with `ponytail:`
  comments at each site. Fixing this needs counters persisted across invocations
  — a state file; the statics cannot do it. Do not "fix" it by tweaking the
  arithmetic.
- **macOS byte counters are 32-bit.** `getifaddrs` hands back `struct if_data`
  (not `if_data64`), whose `ifi_ibytes`/`ifi_obytes` are `u_int32_t` and wrap at
  4GB — `netstat -ibn` reconstructs the high bits, the struct does not. The delta
  is masked with `& 0xffffffff`. `NET_RT_IFLIST2`/`if_msghdr2` was tried and
  returns the same truncated value, so it is not a workaround.
- **macOS `d_net` must filter on `AF_LINK`.** An interface has several `ifaddrs`
  entries and only the link-layer one carries `if_data`. Without the filter the
  counters read zero — and the formatted output would hide it, since the field
  prints `0B/s` regardless. Check the raw values with `make test`.
- **Every collector returns a `char *` that is never freed** — either a
  `static char s[D_BUF]` inside the function or a string literal error message.
  Callers must not hold two results from the same collector at once. `d_fmt`
  wraps `vsnprintf` and returns the buffer, or a warning string on truncation.
- **Errors degrade, they don't exit.** `d_warn(s)` warns to stderr and returns
  the message as the field's value, so a failing sensor shows text in the status
  line and the other fields still print. It must call `warn("%s", s)` — a bare
  `warn(s)` trips `-Wformat-security` under `-Wall`. The only hard `err(1, ...)`
  paths are in `main`.
- **Audio/volume is gone, not disabled.** Commit `e6cb1e7` ("make things work
  after audio changes") commented out `d_vol()`; the macOS port dropped the
  commented block and the orphaned mixer fd (`int m = -1` / `close(m)`) along
  with it. Restoring volume means writing it fresh — see `a094fdf` for the
  original OpenBSD `/dev/mixer` implementation.

## Style

The inherited style is deliberately dense and is worth matching rather than
"cleaning up": comma operators in place of statement sequences
(`memset(&a,...), memset(&b,...)`), returns that do work inline
(`return close(fd), d_fmt(...)`), `d_` prefix on every function, 4-space indent,
`D_BUF`-sized static buffers.

Keep the ISC licence header on files. `$Id: ... $` lines are dead SVN keywords
from upstream — leave them alone.

## Verifying changes

macOS builds and runs here, so verify by building. OpenBSD does not — that side
is verified by reading, and needs a `make` on an OpenBSD box before any change
touching `tstat_openbsd.c` can be called done.

```sh
make clean && make          # must be warning-free
./tstat en0
make test                   # d_scaled asserts + raw counter print
./tstat nosuchif            # must degrade, not crash
```

Cross-check against the system: `pmset -g batt` for battery, `top -l1 -n0` for
CPU, `netstat -ibn -I en0` for the raw counters.

For a tmux end-to-end check, use a **private server** so a mistake cannot touch
the user's live session — `tmux -L probe -f /dev/null`, and `kill-server` on that
socket when done. `display-message -p '#{T:status-right}'` does **not** run
`#()` commands, so a client must actually be attached for the status bar to
render.
