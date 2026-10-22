# sh4-mingw32ce: status

Branch `sh4-mingw32ce` of `palian-tech/cegcc-build`. Specification:
[SPEC.md](SPEC.md). Calling convention: [ABI.md](ABI.md).

| Milestone | State |
|---|---|
| M0 — ARM toolchain from source in Docker | done |
| M1 — binutils `sh4-mingw32ce` | done |
| M2 — GCC `sh4-mingw32ce`, Microsoft ABI, ABI tests | done |
| M3 — runtime (mingwrt, w32api), both targets in one prefix | done |
| M4 — test programs | not started |
| M5 — CI, published image | not started |

## Commits per fork

All forks are in `palian-tech`, each on a branch `sh4-mingw32ce`; the
submodule pins in this repository are authoritative.

| Submodule | Fork (base branch) | Commits on top of the base branch |
|---|---|---|
| `binutils` | `binutils-gdb` (`ce-2.34`) | `bef2f830` add the sh4-mingw32ce target (PE machine 0x01a6, `sh4_wince_pe` emulation, `.rva` in SH gas, dlltool `sh4-wince`); `010f8502` numeric local labels in SH PE gas; `190deabd` no section-end padding in SH PE gas (it broke the import directory of images importing from two DLLs) |
| `gcc` | `gcc` (`ce-9.3.0-x86`) | `fd2b8155` add the sh4-mingw32ce target (`sh/pe.h`, `sh/sh-pe.c`, `sh/mingw32ce.h`, `TARGET_MSABI` in `sh.c`); `f313aead` settled open ABI questions (3-byte struct in `r0`, `__divls`/`__modls`, fp-bit without MS bit-fields); `5b17b05d` predefine `__declspec`, DLL entry `_DllMainCRTStartup` |
| `mingw` | `mingwrt` (`master`) | `0e274ec` coredll SH division helpers, `_JBLEN` for SH, underscored DLL entry |
| `w32api` | `w32api` (`wip`) | `9a0d7c1` coredll SH division helpers, `ceddk.def` and `<ceddk.h>` |

## Build

`build_cf.sh PREFIX TARGET [binutils] [gcc]` builds binutils, then gcc +
libgcc, mingwrt, w32api and the rest of gcc (libstdc++). Without steps it
does both. Several targets can share one prefix; the build directories are
`build-TARGET/` in the current directory. Submodules first:

```sh
git submodule update --init
```

Docker (the reproducible recipe; base image pinned by digest). The image
has both targets in `/opt/cegcc` (`/opt/cegcc/bin` is on `PATH`):

```sh
docker build --network host -t cegcc .                 # the image
docker build --network host --target test -t cegcc-test .   # + tests
```

Stages: `build-base` (packages) → `binutils` (copies only `binutils/`) →
`gcc` (copies `mingw/`, `w32api/`, `gcc/`) → `image`; `test` runs the
tests on top of `image`. A change in `gcc/`, `mingw/` or `w32api/` does
not rebuild binutils. `--build-arg TARGETS=sh4-mingw32ce` builds one
target only. On a 4-core machine: binutils ~4 min per target, the GCC
layer ~12.5 min for both.

Native (Ubuntu 24.04: `build-essential flex bison texinfo file
libgmp-dev libmpfr-dev libmpc-dev zlib1g-dev`; tests also need
`python3 qemu-user`):

```sh
./build_cf.sh /opt/cegcc sh4-mingw32ce
./build_cf.sh /opt/cegcc arm-mingw32ce
export PATH=/opt/cegcc/bin:$PATH
```

To iterate on one component, re-run `make` + `make install` in its
directory under `build-TARGET/` (`gcc`: `make all-gcc install-gcc`). A
gas change needs mingwrt and w32api rebuilt (`make clean all install`),
since their import libraries are assembled.

## Tests

With the tools on `PATH`:

| Test | Command | Needs |
|---|---|---|
| binutils: object/image machine 0x01a6, PE header, dlltool import library, two-DLL import directory, jump stub | `tests/sh4/binutils-test.sh` | sh4 binutils |
| ABI against the Microsoft compiler's code, both call directions, under qemu | `tests/sh4/abi/run-abi-test.sh [OUTDIR]` | sh4 binutils + gcc, python3, `qemu-sh4` |

Each prints `PASS` on success. `run-abi-test.sh` also writes `gcc-t.dis`
(GCC's code for the reference `t.c`) into OUTDIR. The Docker `test`
stage runs both.

Clean-clone verification (done after each milestone):

```sh
git clone -b sh4-mingw32ce https://github.com/palian-tech/cegcc-build
cd cegcc-build && git submodule update --init
docker build --network host --target test .
```

## Known limitations

* No FPU code: the default and only supported mode is `-m4-nofpu`.
  `float`/`double` go through libgcc fp-bit.
* Debug info in linked images is not usable (SH gas has no `.secrel32`).
* `dllimport` is ignored, for functions (calls go through import stubs,
  as on arm-mingw32ce) and also for data: a dllimported variable is
  referenced directly, not through `__imp_`. Use `__imp_` pointers for
  data exported by DLLs.
* `setjmp`/`longjmp` come from coredll; `_JBLEN` for SH (32 words) is a
  generous guess, the layout is not documented.

## Open points

* **Export names of the division helpers.** The Microsoft compiler calls
  the symbols `__divlu`, `__divls`, `__modlu`, `__modls`. With the
  underscore decoration of SH (C name `_divlu` → symbol `__divlu`, as for
  `_abs64` in coredll) the coredll export names are `_divlu` etc., which
  is what the import library now asks for. Not yet checked against a real
  SH-4 coredll: `objdump -p` of the eVC4 build of a program that divides,
  or of the device's `coredll.dll`, settles it.
* M0 consumer smoke test numbers (LeafSDTools ARM) were reported in the
  session only, as required by the scope rules.
