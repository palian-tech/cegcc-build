# gcc16: CeGCC on GCC 16.2 — specification

Task T272. A second Windows CE cross toolchain next to the 9.3 one (branch
`sh4-mingw32ce`, image tags `v9.3.0-ce.N`), not instead of it. Status and
cost: [STATUS.md](STATUS.md).

## Targets

| Target | CPU / OS |
|---|---|
| `arm-mingw32ce` | ARM, Windows CE 6 / 7 |
| `sh4-mingw32ce` | SH-4, Windows CE 5.0 |

Compiler: GCC 16.2.0 (`releases/gcc-16.2.0`, tag object `1831ac03`).
Languages: C, C++ (`-std=c++20`, `-std=c++23` where it compiles).

## Components and branches

| Submodule | Fork | New branch | Base |
|---|---|---|---|
| `gcc` | `palian-tech/gcc` | `ce-16.2.0` | `releases/gcc-16.2.0` + salman's `ce-14.2.0` CE patches (forward-ported) + our SH-4 patches |
| `binutils` | `palian-tech/binutils-gdb` | `ce-2.NN` (see below) | latest binutils release + salman's `ce-2.43.1` patches + our SH-4 patches |
| `mingw` | `palian-tech/mingwrt` | `ce-16` | salman's `mingwrt` `master` (`7c35691`) + our `0e274ec` (SH division helpers, `_JBLEN`, DLL entry) |
| `w32api` | `palian-tech/w32api` | `ce-16` | `enlyze/w32api` `wip` (`51de0ad`, same as salman) + our `9a0d7c1` (coredll SH helpers, `ceddk`) |

Gate 1 (feasibility probe) uses salman's trees as they are: gcc `de6e918`
(`ce-14.2.0`), binutils `f7f94cd` (`ce-2.43.1`), mingwrt `7c35691`,
w32api `51de0ad`. Nothing of ours except the Docker layout and build script.

### binutils version

Salman's binutils patch set is small: 14 commits, 12 files, +100/−9
(`bfd/` PE/COFF-ARM link fixes, pseudo-reloc v2, `gas` `.inst` on PE and
soft-VFP default, `ld` `.text` writable, `__text_start__/__text_end__`,
`gcrt3.o`). Ours adds the SH-4 PE target (3 commits). GCC does not pin a
binutils version; take the latest binutils release tag at the start of
Gate 2 (2.46 is the newest this session could confirm offline; the
sourceware and GitHub mirrors were not reachable from the cloud
container, so check `binutils-2_47` first and take it if released). If
2.4x fails on ARM PE, fall back to salman's 2.43.1 for ARM and keep the
port effort for SH-4 only.

## Patch-set survey (Gate 0)

### Our 9.3 vs salman's 14.2

Salman's `ce-14.2.0` is 20 commits on top of `releases/gcc-14.2.0`
(59 files, +2116/−17). It is a clean re-cut of the MaxKellermann series,
not a mechanical rebase of ours. Mapping:

| Area | 9.3 (`ce-9.3.0-x86`, ~40 commits) | 14.2 (salman) |
|---|---|---|
| ARM PE backend (`arm/pe.cc`, `wince-pe.h`, `pe.h`, `coff.h`, `mingw32.h`, `t-wince-pe`, `pe.opt`, `cygming.opt`) | `fbc5eb8` + fixes `ff1e4f6` binds_local_p, `1a34b66` section_type_flags, `4f8cf21` clobber IP, `54f30d7` file_end | one commit `f363d9a` (fixes folded in: `TARGET_BINDS_LOCAL_P`, `TARGET_ASM_FILE_END arm_pe_file_end`, `arm_pe_section_type_flags` present) |
| middle end | cgraph comdat fix, `maybe_thunk_body`, sincos off, errno | same four commits |
| libgcc | WinCE support, no `.cfi_*`, no mprotect, `WIN32_LEAN_AND_MEAN` | `db4740f`, `939f716`, `dccf641`; adds ARM `ieee754-*.S` / `lib1funcs.S` / `bpabi.S` tweaks |
| libstdc++ | ~15 commits: fake locale, no chrono, no multibyte, no errno, stdio removals, iomanip `tm`, TM off, `showmanyc`, filesystem off, `to_wstring` via `_vsnwprintf` | 8 commits: `using` guards in `c_global`/`c_std`, strcoll/strxfrm, strerror, `<intrin.h>`, `__STDC_LIMIT_MACROS`, filesystem off, `std::print` off, C99 forced. Chrono/locale hacks no longer needed (newer mingwrt provides more) |
| other | libquadmath errno | libssp `malloc.h`, libbacktrace `getpid` |
| i386-mingw32ce | `5669eec` | `68f61ca`, `6c29709` |
| SH-4 (ours only) | `fd2b815`, `f313aea`, `5b17b05`, `6b1d153` | — |

Build differences that matter for the test program:

* **Exceptions**: both use SJLJ (`DWARF2_UNWIND_INFO 0` in
  `wince-pe.h`); no SEH/ARM EHABI. Expect exceptions to work across a
  function; across a DLL only if `libgcc`/`libstdc++` are in one shared
  place — both are static (`--disable-shared`), so a cross-DLL throw needs
  both modules to share the SJLJ context chain. Likely `FAIL`/`SKIP`;
  the test reports it honestly.
* **Threads**: 9.3 is built `--enable-threads=win32`; salman's 14.2 is
  `--disable-threads` ("WinCE is missing too much of the Win32 API").
  Since GCC 13 `gthr-win32` uses SRW locks and condition variables
  (Vista+), absent on CE. Consequence on 14.2/16.2: no `std::thread`/
  `std::mutex` (report `SKIP`), and `thread_local` goes through emutls
  without gthreads — one copy shared by all threads, so a
  `CreateThread` + `thread_local` check is expected to `FAIL`. Fix
  options (Gate 2, costed separately): a CE `gthr` built on
  `CRITICAL_SECTION` + `TlsAlloc` (the pre-13 `gthr-win32.h` model).
* `--disable-__cxa_atexit` for ARM (same as ours); static dtors via
  `atexit`.

### 14.2 → 16.2: mechanical probe

Done in this session: each of salman's 20 commits replayed with
`git merge-tree` onto `releases/gcc-16.2.0`. 17 apply cleanly, 3 conflict:

| Commit | Conflict |
|---|---|
| `f363d9a` gcc: add support for WinCE targets | `gcc/config/arm/arm-protos.h` |
| `f8fc076` libstdc++: don't use strerror() | `libstdc++-v3/src/c++11/system_error.cc` |
| `d2e4720` libstdc++: disable std::print | `libstdc++-v3/src/c++23/print.cc` |

Textual conflicts are small; the real risk is semantic. Expected breakage
(to confirm in Gate 2 by building):

* **Target hooks / ARM backend**: `arm.cc` moved on a lot (15/16 added
  PAC/BTI, MVE, new `TARGET_*` hooks, `arm_option_override` changes);
  `pe.cc` uses `default_binds_local_p_1`, `section_type_flags`,
  `asm_out` APIs, `GTY` roots — any signature change breaks the build.
  `arm/semi.h`, `aout.h`, `coff.h` are legacy headers dropped from
  upstream configs; check they still exist in 16.2 (salman patches
  `semi.h`, so it existed in 14).
* **`config.gcc`**: the `arm-*-mingw32*` stanza must still find
  `arm/t-arm-softfp`, `soft-fp/t-softfp`, `i386/t-cygming`; the
  `i386/t-gthr-win32` path is moot with `--disable-threads`.
* **libgcc**: `config.host` stanza, ARM `ieee754-*.S` tweaks for the PE
  symbol style; 16 reworked parts of `lib1funcs.S`.
* **libstdc++**: GCC 15/16 added C++23/26 bits (`<print>`, `<stacktrace>`,
  `<text_encoding>`, `<chrono>` tzdb, `std::format` locale paths) that
  assume POSIX or full Win32; each needs a WinCE guard like
  `print.cc`. 16 makes `-std=gnu++20` the default C++ dialect — not a
  problem but changes what the headers instantiate. `c_global` headers
  were reformatted in 15/16 (stat above), so the `using` guards will need
  re-placement by hand even where git applies them.
* **TLS**: CE has no native TLS; emutls only. Unchanged risk.
* **PE linker**: binutils side, see above.

### SH-4 port to 16.2 (Gate 3)

Our SH-4 changes are confined to new files (`sh/pe.h`, `sh/sh-pe.c`,
`sh/mingw32ce.h`) plus `TARGET_MSABI` branches in `sh.c` (now `sh.cc`,
renamed in GCC 12). The SH backend is in maintenance mode upstream, so
churn is low; expect `.c`→`.cc` renames, `rtx_insn`/hook signature drift
and the `config.gcc` stanza to be the bulk. The ABI harness
`tests/sh4/abi/run-abi-test.sh` and `open-questions.c` are the gate.

## Test program

`tests/gcc16/cetest.cpp` (written in Gate 1), built by
`tests/gcc16/build.sh TARGET OUTDIR` as `-std=c++20` and, separately,
`-std=c++23` (a C++23 build that does not compile is reported, not
fatal), linked `-static`, imports `COREDLL.dll` only. One line per check
through `OutputDebugStringW` and into `cetest.txt` next to the EXE:
`PASS`, `FAIL <reason>`, `XFAIL <reason>` (known limitation of the
build), `XPASS` (a known limitation that passed), `SKIP <reason>`; last
line `DONE pass/run xfail=n skip=n`. Exit code = FAIL + XPASS count.

Checks: static ctors (order), function-local statics, static dtors and
`atexit` order (the EXE runs itself with `--child-dtor` and reads the
file the child writes at exit), `new`/`delete` incl. arrays, nothrow,
over-aligned, smart pointers; containers; vocabulary types
(`optional`, `variant`/`visit`, `any`, `string_view`, `span`;
`expected`, `to_underlying`, `string::contains` in the C++23 build);
lambdas/`std::function`; exceptions (one function, unwinding through
6 frames with destructors, class hierarchy, rethrow, `exception_ptr`,
throw from a member constructor, library exceptions); `thread_local`
in the main thread and with `CreateThread`; `std::thread`/`std::mutex`;
coredll `double` functions called directly (`sqrt`, `pow`, `_ecvt`, ...); `snprintf` (`%f`, C99 truncation, `%lld`), `wcs*`/`_snwprintf`/`swprintf`;
64-bit and 32-bit division, shifts, int64↔double.

Decided by the owner (Gate 0 review):

* Exception across a DLL: `SKIP` (runtime linked statically, no shared
  libgcc/libstdc++).
* `thread_local` with `CreateThread`: `XFAIL` while GCC is built
  `--disable-threads`; `std::thread`/`std::mutex`: `SKIP` then. Both
  become plain checks when `_GLIBCXX_HAS_GTHREADS` is defined.
* For 2b, restoring GCC 12's pre-Vista `gthr-win32` (CRITICAL_SECTION,
  TLS slots) may be cheaper than a new thread layer; cost it first.

## Build layout

`gcc16/Dockerfile` builds the targets in `TARGETS` (default: both) into
one prefix from source trees given as repository + commit build arguments
(defaults: the CeGCC 16.2 trees) with this repository's `build_cf.sh`
(`GCC_THREADS=no` → `--disable-threads`), then the test program. Workflow
`.github/workflows/gcc16.yml` runs it per target and uploads
`cetest-<job>` (EXEs + `SHA256SUMS`); `gcc16-release.yml` builds the image
with both targets and, for a `v16.*` tag, pushes it to ghcr.io and Docker
Hub with `:latest`; pushes to `master` publish `:edge`. The 9.3 line
publishes `:gcc9` (newest `v9.3.0-ce.N`) and `:gcc9-edge` (owner,
2026-10-24). The 9.3 `Dockerfile` is unchanged;
`image.yml` on this branch no longer runs for pull requests into `gcc16`
or for `v16.*` tags.

## Gates

0. Survey, this document, estimate — **stop**.
1. Salman's ARM 14.2 in Docker (our layout) + test program as Actions
   artifacts — **stop**, the owner runs the test program on an ARM CE 6/7
   device. Fail → work ends.
2. ARM on 16.2 (`ce-16.2.0` etc.) — same artifacts — **stop**.
3. SH-4 on 16.2, ABI harness + `open-questions.c` with mutation control,
   artifacts for an SH-4 CE 5.0 device — **stop**.
3b. SH-4: `double` calls from `-m4-nofpu` code into the `/Qsh4` coredll
   (thunks in mingwrt), convention in `docs/sh4/ABI.md`, cetest check —
   **stop** for an SH-4 device run.
3c. The same fix and cetest check on 9.3 (`sh4-mingw32ce` and its mingwrt
   branch; the owner lifted the 9.3 rule for this fix only), separate PR —
   **stop** for a device run; then tag `v9.3.0-ce.3`. Nothing else changes
   in 9.3.
4. Image `v16.2.0-ce.1` on ghcr and Docker Hub next to 9.3 tags; README;
   CI, including moving the actions off Node 20 (checkout,
   upload-artifact, build-push-action, setup-buildx).

After Gate 4 (owner, 2026-10-24). Same rules. Before each gate: a cost
estimate; if the remaining budget (ends 2026-11-05) would not cover it,
stop and leave STATUS.md for another session. At each gate: commits,
artifacts, what to run, cost so far. After each accepted gate: image tag
`v16.2.0-ce.N` (next N) on ghcr and Docker Hub, README says which tag has
which features; the 9.3 tags stay.

5. **SH-4 FPU code generation compatible with MSVC `/Qsh4`.** `float` and
   `double` in FPU registers, interoperable with the `/Qsh4` CE 5.0 system
   DLLs, so coredll math and CRT functions work without thunks and
   floating point is no longer emulated by fp-bit.
   * Convention first, in ABI.md: argument registers for `float`/`double`,
     mixed int/float lists, varargs with `float`/`double`, results,
     callee-saved FPU registers, `FPSCR` at function boundaries (`PR`,
     `SZ`). If `fl`, `callfl`, `calldbl` are not enough: write a C file
     with the cases needed and **stop**; the maintainer compiles it with
     the MSVC SH-4 compiler (`/Qsh4 /O2`) and returns the listings.
   * An option (for example `-m4-single` with the MS ABI); `-m4-nofpu`
     stays the unchanged default unless the owner decides otherwise.
     Runtime libraries (libgcc, libstdc++, mingwrt) for both modes
     (multilib or two sysroots, with the reason).
   * Tests: ABI harness with the FPU cases and a mutation control that
     must fail; cetest in both modes with `float`/`double` arithmetic,
     coredll math called directly, `printf`/`snprintf` `%f`/`%e`/`%g`,
     `strtod`.
   * **Stop** for an SH-4 CE 5.0 device run. 9.3 only on the owner's
     word.
6. **Threads on 16.2, both targets** (2b of Gate 0). `--enable-threads`
   with a gthr layer that works on CE (no SRWLOCK, CONDITION_VARIABLE,
   InitOnce); starting point GCC 12's gthr-win32 adapted to CE.
   `thread_local` per thread (emutls on CE TLS), `std::thread`,
   `std::mutex`, `std::condition_variable`, `std::call_once`.
   * cetest: `thread_local` + `CreateThread` from XFAIL to PASS; checks
     for `std::thread` join, mutex contention, condition variable,
     `call_once`, exceptions thrown and caught inside a thread.
   * **Stop** for device runs (ARM CE 6/7, SH-4 CE 5.0).
7. **Usable debug info, both targets.** DWARF in linked PE images that gdb
   can use: `.secrel32` (or equivalent) in SH gas; check ARM and fix if
   needed; `gdb` for both targets in the image.
   * CI: a `-g` cetest; `gdb -batch` on the EXE shows line info for
     `main` and a check function, and type and location of a local.
   * **Stop** for a device check: gdb attached through a gdb stub,
     breakpoint by source line.

## Rules

Gates build only this repository's own test programs. Application builds
are done outside the repository; no application, product or device names
in this repository.

New branches only (`gcc16` here, `ce-16.2.0` / `ce-16` in forks). The
`sh4-mingw32ce` and 9.3 branches and existing tags are not touched.

Branch layout (owner, 2026-10-24): `sh4-mingw32ce` is renamed to
`releases/gcc-9`, the 9.3 maintenance line (fixes only, tags
`v9.3.0-ce.N`). At Gate 4, `gcc16` is merged into `master`, which becomes
the 16.2 line (tags `v16.2.0-ce.N`). The target name `sh4-mingw32ce`
does not change.
