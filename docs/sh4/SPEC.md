# sh4-mingw32ce: Windows CE SH-4 target for CeGCC

Status: M0–M2 done (toolchain up to GCC + libgcc; ABI in [ABI.md](ABI.md)). Branch: `sh4-mingw32ce`.

## Goal

One open toolchain, built from this repository in one Docker image, that
compiles Windows CE programs for **both**:

| Target | CPU | OS | PE machine | Status |
|---|---|---|---|---|
| `arm-mingw32ce` | ARMv4I (Thumb interworking) | Windows CE 6.0/7.0 | `0x01C2` | exists here |
| `sh4-mingw32ce` | Hitachi/Renesas SH-4 (SH7785-class) | Windows CE 5.0 | **`0x01A6`** | **to add** |

The first consumer is LeafSDTools
(<https://github.com/alexpalyan/leafsdtools>), a small C++ Windows CE tool
for Nissan Leaf head units (Clarion QY8XXX = ARM, QY7XXX = SH-4). Today the
ARM build uses CeGCC 0.59.1 (GCC 4.4); the SH-4 build only works with the
proprietary eVC4 compiler under Wine. The aim is to drop eVC4 entirely.

`i386-mingw32ce` (added by ENLYZE) is the template: it shows every place a
new `*-mingw32ce` architecture has to be wired in.

## Repositories

All submodules have forks in `palian-tech`; point `.gitmodules` at them and
keep the existing upstream branches as the base:

| Submodule | Fork | Base branch |
|---|---|---|
| `binutils` | `palian-tech/binutils-gdb` | `ce-2.34` |
| `gcc` | `palian-tech/gcc` | `ce-9.3.0-x86` |
| `mingw` | `palian-tech/mingwrt` | `master` |
| `w32api` | `palian-tech/w32api` | `wip` |

Create a `sh4-mingw32ce` branch in each fork you change.

## Known facts (verified, do not re-derive)

Ground truth for the calling convention is
[`msvc-sh4-abi-reference.txt`](msvc-sh4-abi-reference.txt): small C test
functions compiled by the Microsoft SH-4 compiler, with disassembly. Every
ABI question must be closed by a test against that file (or a new test of
the same kind), never by guessing.

1. **binutils** already has an `sh-pe` target (as, ld, objdump, dlltool,
   windres build with `--target=sh-pe`), but it only knows machine `0x01A2`
   (SH3). Windows CE SH-4 executables use **`0x01A6`**. `objdump` must read
   it and `ld` must write it. Whether `dlltool` works for `sh` is unknown.
2. **Stack home area: the caller always reserves 16 bytes** below the stack
   arguments, on **every** call, even with fewer than four arguments
   (`call2`: `add #-16,r15` before `jsr`; `call5`: `add #-20`, fifth
   argument at `@(16,r15)`). The callee may write into that area
   (`take64`, `wsprintfW`). Without it, any call into `coredll` can overwrite
   the caller's saved `PR`. GCC has `REG_PARM_STACK_SPACE` /
   `OUTGOING_REG_PARM_STACK_SPACE` for this.
3. **Arguments:** first four words in `r4..r7`, the rest on the stack from
   `@(16,r15)`. A 64-bit argument takes two consecutive registers **without**
   even-register alignment (`take64`: `a` in `r4`, `b` in `r5:r6`, `c` in
   `r7`). An 8-byte struct argument also goes in two registers (`takeS8`).
   Varargs: same rule (`callv`).
4. **Return values:** 32-bit in `r0`. **64-bit values and structs of 8 bytes
   or more are returned through a hidden pointer passed in `r4`**, which is
   also returned in `r0` (`ret64`, `retS8`, `retS20`). A 4-byte struct is
   returned by value in `r0` (`retS4`).
5. **Division** calls `__divlu` / `__modlu` (exported by `coredll`), not
   inline code or libgcc.
6. **FPU:** with `/Qsh4` the Microsoft compiler emits FPU code and toggles
   `FPSCR.PR` around `double` operations (`fl`, `callfl`), i.e. single
   precision is the mode at function boundaries. **For the first milestone
   `-m4-nofpu` is enough** (LeafSDTools uses no floating point). How `float`
   and `double` arguments are passed is not yet established.
7. A previous scouting run produced a working PE by compiling with GCC for
   `sh4-linux`, stripping ELF-only directives (`.type`, `.size`, `.ident`,
   `.note.GNU-stack`) from the assembly, and assembling/linking with
   `sh-pe` binutils. The proper target removes the need for that hack.
8. **ROM loader detail** (from running the result on the device emulator):
   uncompressed sections must start on 4 KiB boundaries. Not a compiler
   concern, but keep default PE section alignment at 0x1000.

## eVC4 reference build of LeafSDTools for SH-4 (flags to match)

```
cl  /W3 /D _WIN32_WCE=500 /D WCE_PLATFORM_STANDARDSDK_500 /D SHx /D SH4 /D _SH4_
    /D UNDER_CE=500 /D UNICODE /D _UNICODE /D NDEBUG /Qsh4 /O2 /MC /c
link /base:0x00010000 /stack:0x10000,0x1000 /entry:WinMainCRTStartup
     /subsystem:windowsce,5.00 /MACHINE:SH4 commctrl.lib coredll.lib ceddk.lib
```

`ceddk.lib` is generated from a two-symbol `.def` (`MmMapIoSpace`,
`MmUnmapIoSpace`). The ARM cegcc build of LeafSDTools lives on the
`ci-cegcc-build` branch of the LeafSDTools fork (Makefile + `compat/`); the
SH-4 build should become a target of the same Makefile.

## Working environment

The work may run in a sandbox without Docker. In that case build the
toolchain natively (Debian/Ubuntu packages as needed) and keep the
Dockerfile as the reproducible recipe; build and publish the image from a
GitHub Actions workflow in this repository (`ghcr.io/palian-tech/...`).
A full GCC build takes tens of minutes: build only the needed languages
(`c,c++`), reuse build directories between iterations, and keep logs short
in reports (tail and error lines only).

## Milestones

1. **M0 — reproduce ARM from source.** `build_cf.sh PREFIX arm-mingw32ce` in a
   Dockerfile in this repo (pin the base image). Build LeafSDTools for ARM
   with it. Record versions and output hashes.
2. **M1 — binutils `sh4-mingw32ce`.** Target triplet wired like
   `i386-mingw32ce`/`arm-mingw32ce`, PE/COFF for SH with machine `0x01A6`,
   `objdump -d` reads the eVC4 LeafSDTools SH-4 EXE, `dlltool` produces a
   working import library for `coredll`.
3. **M2 — GCC `sh4-mingw32ce`.** New target config for SH with the CE/PE
   conventions (patterned on the i386/ARM mingw32ce configs) and the
   Microsoft ABI points 2–5 above. Default `-m4-nofpu` is acceptable for now.
   Provide **ABI tests**: compile the functions from
   `msvc-sh4-abi-reference.txt` with the new GCC and show, function by
   function, that the call sequences and return paths match the reference.
4. **M3 — runtime.** mingwrt/w32api built for SH-4: `crt` startup with
   `WinMainCRTStartup`, import libraries for `coredll` and `ceddk`, headers.
5. **M4 — programs.** (a) a minimal program that calls `MessageBoxW` and
   `wsprintfW`; (b) LeafSDTools for SH-4. Both as PE files with machine
   `0x01A6`, subsystem Windows CE 5.00, image base `0x10000`.
6. **M5 — one Docker image** building both targets, published as
   `ghcr.io/palian-tech/cegcc` (or similar), and a short README section.

## Acceptance

Done in this repository (cloud work):

* M0–M5 build reproducibly in Docker from a clean checkout.
* ABI tests from M2 pass and are part of the repository.
* `objdump -p` of the M4 executables shows machine `0x01A6`, subsystem
  `WindowsCE GUI` 5.00, base `0x10000`, and only `COREDLL.dll` / `ceddk.dll`
  (plus `commctrl.dll` if used) imports.

Done by the maintainer locally (cannot be done in the cloud): run the M4
executables inside the `clarion-qy7` device emulator, and ARM LeafSDTools in
`clarion-qy8`. Report artifacts (EXE files and their hashes) so the
maintainer can run them.

## Out of scope

* FPU-compatible code generation (`-m4-single` or full `FPSCR` handling) —
  after the first milestone.
* Firmware images or anything from the head units — never commit them.
