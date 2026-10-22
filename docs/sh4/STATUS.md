# sh4-mingw32ce: status

Branch `sh4-mingw32ce` of `palian-tech/cegcc-build`. Specification:
[SPEC.md](SPEC.md). Calling convention: [ABI.md](ABI.md).

| Milestone | State |
|---|---|
| M0 — ARM toolchain from source in Docker | done |
| M1 — binutils `sh4-mingw32ce` | done |
| M2 — GCC `sh4-mingw32ce`, Microsoft ABI, ABI tests | done |
| M3 — runtime (mingwrt, w32api) | not started |
| M4 — test programs | not started |
| M5 — one image with both targets, CI | not started |

## Commits per fork

All forks are in `palian-tech`; the submodule pins in this repository are
authoritative.

| Submodule | Fork, branch | Commits on top of the base branch |
|---|---|---|
| `binutils` | `binutils-gdb`, `sh4-mingw32ce` (base `ce-2.34`) | `bef2f830` add the sh4-mingw32ce target (PE machine 0x01a6, `sh4_wince_pe` emulation, `.rva` in SH gas, dlltool `sh4-wince`); `010f8502` numeric local labels in SH PE gas |
| `gcc` | `gcc`, `sh4-mingw32ce` (base `ce-9.3.0-x86`) | `fd2b8155` add the sh4-mingw32ce target (`sh/pe.h`, `sh/sh-pe.c`, `sh/mingw32ce.h`, `TARGET_MSABI` in `sh.c`); `f313aead` settled open ABI questions (3-byte struct in `r0`, `__divls`/`__modls`, fp-bit without MS bit-fields) |
| `mingw` | `mingwrt`, `master` | none |
| `w32api` | `w32api`, `wip` | none |

## Build

Everything is built by `build_cf.sh PREFIX TARGET` (binutils, gcc +
libgcc, mingwrt, w32api, rest of gcc). Submodules first:

```sh
git submodule update --init
```

Docker (the reproducible recipe; the base image is pinned by digest):

```sh
docker build --network host -t cegcc .                                   # arm only
docker build --network host --build-arg TARGETS="arm-mingw32ce sh4-mingw32ce" -t cegcc .
```

Native (Ubuntu 24.04: `build-essential flex bison texinfo file
libgmp-dev libmpfr-dev libmpc-dev zlib1g-dev`):

```sh
./build_cf.sh /opt/cegcc/sh4-mingw32ce sh4-mingw32ce
```

To iterate on GCC only, re-run `make all-gcc install-gcc` (or
`make all-target-libgcc install-target-libgcc`) in `gcc-build/`; the
script itself always starts from scratch.

## Tests

Tools on `PATH` (`export PATH=/opt/cegcc/sh4-mingw32ce/bin:$PATH`).

| Test | Command | Needs |
|---|---|---|
| binutils: object/image machine 0x01a6, PE header, dlltool import library, jump stub | `tests/sh4/binutils-test.sh` | sh4 binutils |
| ABI against the Microsoft compiler's code, both call directions, under qemu | `tests/sh4/abi/run-abi-test.sh [OUTDIR]` | sh4 binutils + gcc, python3, `qemu-sh4` (`qemu-user`) |

Both print `PASS` on success. `run-abi-test.sh` also writes `gcc-t.dis`
(GCC's code for the reference `t.c`) into OUTDIR.

## Known limitations

* No FPU code: the default and only supported mode is `-m4-nofpu`.
  `float`/`double` go through libgcc fp-bit.
* Debug info in linked images is not usable (SH gas has no `.secrel32`).
* `dllimport` on functions is ignored (calls go through import stubs).

## Open points

* M0 consumer smoke test numbers (LeafSDTools ARM) were reported in the
  session only, as required by the scope rules.
* The full `build_cf.sh` run for `sh4-mingw32ce` (mingwrt, w32api,
  libstdc++) is M3.
