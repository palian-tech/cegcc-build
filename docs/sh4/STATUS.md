# sh4-mingw32ce: status

Branch `sh4-mingw32ce` of `palian-tech/cegcc-build`. Specification:
[SPEC.md](SPEC.md). Calling convention: [ABI.md](ABI.md).

| Milestone | State |
|---|---|
| M0 — ARM toolchain from source in Docker | done |
| M1 — binutils `sh4-mingw32ce` | done |
| M2 — GCC `sh4-mingw32ce`, Microsoft ABI, ABI tests | done |
| M3 — runtime (mingwrt, w32api), both targets in one prefix | done |
| M4 — test programs (both targets) | done |
| M5 — CI, published image | not started |

## Commits per fork

All forks are in `palian-tech`, each on a branch `sh4-mingw32ce`; the
submodule pins in this repository are authoritative.

| Submodule | Fork (base branch) | Commits on top of the base branch |
|---|---|---|
| `binutils` | `binutils-gdb` (`ce-2.34`) | `bef2f830` add the sh4-mingw32ce target (PE machine 0x01a6, `sh4_wince_pe` emulation, `.rva` in SH gas, dlltool `sh4-wince`); `010f8502` numeric local labels in SH PE gas; `190deabd` no section-end padding in SH PE gas (it broke the import directory of images importing from two DLLs) |
| `gcc` | `gcc` (`ce-9.3.0-x86`) | `fd2b8155` add the sh4-mingw32ce target (`sh/pe.h`, `sh/sh-pe.c`, `sh/mingw32ce.h`, `TARGET_MSABI` in `sh.c`); `f313aead` settled open ABI questions (3-byte struct in `r0`, `__divls`/`__modls`, fp-bit without MS bit-fields); `5b17b05d` predefine `__declspec`, DLL entry `_DllMainCRTStartup`; `6b1d1531` section alignment up to 8192 bytes (`aligned(8)` and up were rejected) |
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
docker build --network host --target programs --output type=local,dest=out .
```

The last one writes the M4 executables of both targets and `SHA256SUMS`
to `out/`.

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
| `hello.c` under qemu: real crt3 startup, WinMain, the four division helpers and the exit path, with `fakecoredll.c` standing in for coredll | `tests/sh4/prog/run-hello.sh [OUTDIR]` | as above |
| M4 programs for one target: builds `hello` and `mapio`, checks machine, image base 0x10000, subsystem Windows CE 5.00 and the exact DLL imports | `tests/sh4/prog/build.sh TARGET [OUTDIR]` | `TARGET-gcc` (`sh4-mingw32ce` or `arm-mingw32ce`) |

Each prints `PASS` on success. `run-abi-test.sh` also writes `gcc-t.dis`
(GCC's code for the reference `t.c`) into OUTDIR. The Docker `test`
stage runs all of them, `build.sh` for both targets.

## Test programs (M4)

`tests/sh4/prog/`, built by `build.sh` as `hello-TARGET.exe` and
`mapio-TARGET.exe`:

* `hello.c`: `WinMain` calls `wsprintfW` and `MessageBoxW`, returns an
  8-byte struct (hidden pointer), passes a 64-bit argument split between
  `r7` and the stack, divides signed and unsigned (`_divls`, `_modls`,
  `_divlu`, `_modlu` from coredll). The box reads `PASS` and the values;
  exit code 0. Imports only `COREDLL.dll`.
* `mapio.c`: maps one page at physical address 0 with `MmMapIoSpace`
  (never touches it), unmaps it, shows the address. Imports `COREDLL.dll`
  and `ceddk.dll`.

Built with `-O2 -D_WIN32_WCE=0x500 -Wl,--image-base,0x10000
-Wl,--subsystem,wince:5.00 -s`. SH-4: machine 0x01a6; ARM: machine 0x01c0
(arm-mingw32ce has always written `IMAGE_FILE_MACHINE_ARM`; such binaries
run on the ARM unit). Running them in the device emulators is the
maintainer's step.

Clean-clone build at cegcc-build `e0f92ea` (sha256):

```
625be32ac6e3dd09fe8ef6499445fb16638195f72c6d3e69b5c9d574a12f376c  hello-sh4-mingw32ce.exe
867d2007e4055a1a990d63e1a649a4181b63df7a2c089e9314eee066b51b7dcd  mapio-sh4-mingw32ce.exe
707429994bea0509794e57015b4c89f84ec1e7043e955994eb4f6fa725b9bafb  hello-arm-mingw32ce.exe
be317e2b9b023f55aeb9b0de5998b13102a4656c4509c633a342578bf1c90e03  mapio-arm-mingw32ce.exe
```

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

* `coredll.def` is the generic Windows CE export list. A unit's coredll
  is built for that device and may lack some exports: the CE 5.0 SH-4
  coredll of the target unit (2507 exports, ~1700 named) has none of 151
  names of the list (Crypt*, services, battery, printing, EnhMetaFile).
  A program that imports one of them links but does not load there.

## Settled

* **Export names of the division helpers**, checked by the maintainer
  against the target unit's CE 5.0 SH-4 coredll: `_divls`, `_divlu`,
  `_modls`, `_modlu` (called as `__divls` etc.) are exported by name, as
  are `MessageBoxW`, `wsprintfW`, `TerminateProcess`, `GetModuleHandleW`,
  `_divi64`, `_divui64`, `_modi64`, `_modui64` and `_divd`. GCC uses
  libgcc for 64-bit division and fp-bit for `double`, not the latter
  helpers.

## Open points

* M0 consumer smoke test numbers (LeafSDTools ARM) were reported in the
  session only, as required by the scope rules.
