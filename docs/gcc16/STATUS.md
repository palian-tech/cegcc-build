# gcc16: status

Specification: [SPEC.md](SPEC.md).

| Gate | State |
|---|---|
| 0 — survey, plan, estimate | done, accepted by the owner (2026-10-23) |
| 1 — ARM on GCC 14.2 (salman), probe on an ARM CE 6/7 device | built; artifact `cetest-gate1-arm-gcc14` ([run](https://github.com/palian-tech/cegcc-build/actions/runs/40035217009)) |
| 2 — ARM on GCC 16.2 | accepted (2026-10-24): device run C++20/C++23 without FAILs |
| 3 — SH-4 on GCC 16.2 | device run: all PASS except `snprintf` `%f` → Gate 3b |
| 3b — SH-4 `double` calls into coredll (`/Qsh4`) | mingwrt thunks + FPU thunk test under qemu-sh4; device run pending (owner) |
| 3c — the same fix on 9.3, tag `v9.3.0-ce.3` | not started (after the 3b device run) |
| 4 — release `v16.2.0-ce.1` | not started; also moves CI actions off Node 20 |
| 5 — SH-4 FPU code generation (MSVC `/Qsh4`) | after Gate 4; cost estimate first |
| 6 — threads on 16.2, both targets | after Gate 5; cost estimate first |
| 7 — usable debug info, both targets | after Gate 6; cost estimate first |

Gates 3b–7 are defined in [SPEC.md](SPEC.md#gates) (owner, 2026-10-24).
From Gate 5 on, each gate starts with a cost estimate and stops if the
budget (ends 2026-11-05) would not cover it.

Owner pre-approval (2026-10-23 night): continue from Gate 1 into Gate 2
and from Gate 2 into Gate 3 (if the ABI harness and `open-questions.c`
pass) without waiting for device runs; stop before Gate 4. Device
acceptance runs come later and may send work back.

## Resume here

Work branch: `claude/project-thread-1if7mo` (pull request into `gcc16`).

| Tree | Branch | Commit | Content |
|---|---|---|---|
| `palian-tech/gcc` | `ce-16.2.0` | `43596e5d45` | `releases/gcc-16.2.0` + salman's 21 `ce-14.2.0` commits + our 4 SH-4 commits + 7 GCC 16 fixes |
| `palian-tech/binutils-gdb` | `ce-2.43.1` | `a9e4309579` | salman's `ce-2.43.1` + our 3 SH-4 commits + TRUE→true fix (`a817d8b8ee`, used by `arm-gcc16`) + SH-4 weak/linkonce fix (used by `sh4-gcc16`) |
| `palian-tech/mingwrt` | `ce-16` | `87bc83d594` | salman `7c35691` + our `0e274ec` + `struct timespec` (`72d79b48e5`, used by `arm-gcc16`) + SH-4 coredll FPU thunks (used by `sh4-gcc16`) |
| `palian-tech/w32api` | `ce-16` | `9a0d7c127d` | = our `sh4-mingw32ce` (enlyze `51de0ad` + `9a0d7c1`) |

Salman's trees are `github.com/salman-javed-nz/{gcc,binutils-gdb,mingwrt}`.
Build: `.github/workflows/gcc16.yml`, one job per tree set (`trees`
build args pin the commits), `gcc16/Dockerfile`.

## Gate 1 (2026-10-23)

* `gcc16/Dockerfile` fetches salman's trees by commit and builds them with
  our `build_cf.sh` (`GCC_THREADS=no`), binutils without gdb/sim.
* `tests/gcc16/cetest.cpp` built as C++20 and C++23 with salman's GCC
  14.2 in this session: both compile, link `-static`, import only
  `COREDLL.dll` (two descriptors: mingwrt's and w32api's import libs;
  harmless for the loader, tolerated by `build.sh`). Not run: no CE
  device or emulator here.
* Expected on the device: `XFAIL thread_local with CreateThread`,
  `SKIP std::thread and std::mutex`, `SKIP exception across a DLL`.
  Everything else should `PASS`; any `FAIL` is a finding.
* `swprintf` is not declared by salman's mingwrt for CE; the test uses
  `_snwprintf` (coredll). `small` is a macro from `rpcndr.h`
  (via `windows.h`) — avoid it as an identifier.

## Gate 2 — ARM on GCC 16.2

* gcc: salman's 21 commits cherry-picked onto `releases/gcc-16.2.0`; 3
  conflicts resolved by hand: `arm-protos.h` (PE prototypes get their own
  `#ifdef TREE_CODE`), `system_error.cc` (16.2's NULL check kept, strerror
  skipped on CE), `c++23/print.cc` (whole file under `#ifndef UNDER_CE`).
* Build fixes found by building (one commit each on `ce-16.2.0`):
  * `arm.cc`: GCC 16 removed the arm-pe target and with it the
    `#ifndef ARM_PE` around `arm_encode_section_info`/`arm_file_end` that
    the WinCE commit relies on; restored, and `TARGET_ENCODE_SECTION_INFO`
    is `arm_pe_encode_section_info` on PE.
  * `config.gcc`: `i386/t-cygming` moved to `mingw/t-cygming` (provides
    `msformat-c.o`); dropped the never-existing `arm/t-cygming`,
    `arm/t-mingw32`.
  * libstdc++ `system_error.cc`: CE has no `FormatMessageA`; use
    `FormatMessageW` + `WideCharToMultiByte`.
  * libstdc++ `tzdb.cc`: no `GetModuleHandleExA` /
    `GetDynamicTimeZoneInformation` on CE; Windows zone detection off
    (`current_zone()` falls back to UTC).
  * mingwrt `time.h`: `struct timespec` (C11), needed by libstdc++'s
    `this_thread::sleep_for` since GCC 15; it sleeps with `Sleep()`.
* The C++ `std` module (`import std;`) does not compile against the CE
  runtime (no `long double` math, `rewind`, `system`, …); libstdc++'s own
  fallback builds without it. Not a goal for CE.
* cetest with GCC 16.2: C++20 and C++23 compile, import only
  `COREDLL.dll`, same set of coredll functions as with GCC 14.2.
* binutils: **2.47 is released (2026-07-26) but no 2.47 source was
  reachable from the session** (sourceware, GNU mirrors and the GitHub
  binutils mirror are blocked or need auth). Gates 2 and 3 use salman's
  2.43.1 (+ our SH-4 port). GCC 16 does not need newer binutils for
  ARMv4/v5 PE. Moving to 2.47 is a separate step: rebase `ce-2.43.1`
  (18 commits) onto `binutils-2_47` from a machine that can reach
  sourceware.

## Gate 3 — SH-4 on GCC 16.2

* binutils SH-4 port 2.34 → 2.43.1: conflicts in `dlltool.c` (new
  `MAARCH64` machine, extra bool field in `mtable`),
  `binutils/configure{,.ac}`, `ld/Makefile.{am,in}`, `ld/emultempl/pe.em`
  (`IMAGE_SUBSYSTEM_WINDOWS_CE_GUI` branch), `TRUE`→`true`. Generated
  files edited by hand (same lines as `configure.ac` / `Makefile.am`).
  `tests/sh4/binutils-test.sh` passes.
* gcc: our 4 SH-4 commits onto `ce-16.2.0`: `sh.c`→`sh.cc`,
  `sh-pe.c`→`sh-pe.cc`; the attribute table is a `TARGET_GNU_ATTRIBUTES`
  macro now (dllimport/dllexport added through `SH_PE_ATTRIBUTES`);
  `sh_setup_incoming_varargs` takes `function_arg_info` and skips the
  advance for `f(...)` (C23 `TYPE_NO_NAMED_ARGS_STDARG_P`);
  `DBX_REGISTER_NUMBER` → `DEBUGGER_REGNO`; `dbxcoff.h` gone; `.opt`
  without `Report`, plus `mingw32ce.opt.urls`; no `i386/t-gthr-win32`.
* `UNDER_CE` was defined only in GNU modes (`builtin_define_std`), so
  with `-std=c++20` libstdc++'s CE guards were skipped and `<cstdlib>` did
  not compile. Now defined in ISO modes too, as on ARM. **The 9.3 SH-4
  toolchain has the same bug** (not touched here).
* `tests/gcc16/sh4-abi-gate.sh` (run by the `sh4-gcc16` job): binutils
  test, `tests/sh4/abi/run-abi-test.sh` (reference functions and
  `open-questions.c` functions, MSVC code ↔ GCC 16 code) — all checks
  pass under qemu-sh4; mutation control: the same test with
  `-fpcc-struct-return` must fail, and does (`retS4`, `retS3`).
  `tests/sh4/prog/run-hello.sh` passes (division helpers `_divls`,
  `_divlu`, `_modls`, `_modlu`).
* `cetest` on SH-4 did not link: undefined weak symbols
  (`__cxa_pure_virtual`, `_ITM_*` from libgcc/libstdc++) were reported
  as errors by `coff-sh.c`, and `.linkonce` sections were not merged.
  binutils `a9e4309579`: undefined weak resolves to its default (or 0),
  and the SH-4 CE vector gets `SEC_LINK_ONCE | SEC_LINK_DUPLICATES`. A
  two-TU COMDAT test links on both targets; `cetest` C++20 and C++23 now
  build for SH-4 and import only `COREDLL.dll`.

## Gate 3b — SH-4 `double` calls into coredll (2026-10-24)

Device run of Gate 3: `FAIL snprintf: got "-17|str|ff|z| 0.00"`. mingwex's
`snprintf` gets its digits from coredll's `_ecvt`/`_fcvt`; the SH-4 coredll
is `/Qsh4` code and takes the `double` in `dr4`, while `-m4-nofpu` code
passes it in `r4:r5`. The 9.3 toolchain has the same bug.

* The `/Qsh4` convention, decoded from the MSVC listings already in the
  repository (`fl`, `callfl`, `calldbl`): [ABI.md](../sh4/ABI.md#calls-into-qsh4-code-coredll-floating-point).
  `double` arguments in `dr4`, `dr6` (even register = high word), still
  taking their integer slots; `double` result in `dr0`; `FPSCR.PR` =
  `SZ` = 0 at calls.
* mingwrt `87bc83d594` (`coredll_fpu.S`, `.lst`, `.sh`): thunks replace
  the jump stubs in `libcoredll.a` (51 functions) and `libceoldname.a` (15
  old names). Saved/restored: `PR`, `FPSCR`.
* w32api installs its own `libcoredll.a` after mingwrt's (the first CI
  run had no thunks; the FPU test caught it). `build_cf.sh` now runs
  mingwrt's `coredll_fpu.sh` on the installed `libcoredll.a` and
  `libcoredll6.a` for sh4-mingw32ce (idempotent).
* Unsupported (listed in ABI.md): `_cabs`/`cabs`, `_frnd`, `_fsqrt`, and
  `%f`-style conversions in coredll's own `printf`/`scanf` family called
  directly (`_snprintf`, `_snwprintf`, `swprintf`, ...).
* `tests/sh4/fpu/run-fpu-test.sh` (in `sh4-abi-gate.sh`): the installed
  thunks against Microsoft's `/Qsh4` `calldbl` and `fl` and a recording
  callee, under qemu-sh4 with `FPSCR.PR` set: 26 checks pass; swapping the
  word order fails it. `sh4-abi-gate.sh` now uses `pipefail` (a failing
  test piped into `tee` did not fail the gate before).
* cetest: new check `coredll double functions` (`sqrt`, `floor`, `pow`,
  `fmod`, `atan2`, `ldexp`, `frexp`, `modf`, `strtod`, `_ecvt`, `sqrtf`
  through volatile pointers). The SH-4 link map shows the thunks linked.

## Gate 0 findings (short)

* `releases/gcc-16.2.0` exists (tag `1831ac03`).
* Salman's CE series: 20 commits on 14.2.0; replayed onto 16.2.0 with
  `git merge-tree`: 17 clean, 3 textual conflicts (`arm-protos.h`,
  `system_error.cc`, `c++23/print.cc`). Semantic breakage unknown until
  a build.
* Salman's 14.2 is built `--disable-threads` (GCC 13+ `gthr-win32` needs
  Vista APIs). Expect `std::thread`/`std::mutex` = `SKIP` and
  `thread_local` across `CreateThread` = `FAIL` unless we add a CE
  `gthr` (estimated separately below).
* Exceptions: SJLJ on both 9.3 and 14.2; cross-DLL throw likely not
  supported with static libgcc.
* binutils: salman's patch set is 14 small commits on 2.43.1. Latest
  upstream release could not be listed from the container (sourceware
  blocked, GitHub mirror requires auth) — decide at Gate 2 start.

## Cost estimate (session credit, USD)

| Gate | Work | Estimate | Main risk |
|---|---|---|---|
| 1 | Dockerfile stage for salman's trees, test program, Actions workflow with artifacts (builds run on Actions, not in the session) | 8–12 | test program needs CE workarounds in salman's runtime |
| 2 | Forward-port 20 commits + 14 binutils commits, iterate on build errors (ARM backend, libstdc++ C++23 bits) | 12–20 | `arm.cc`/`pe.cc` hook drift; each rebuild cycle ~20 min of CI |
| 2b (optional) | CE `gthr` (CRITICAL_SECTION + TLS) to get `std::thread`/`std::mutex`/per-thread `thread_local` | 6–10 | gthreads API surface (`__gthread_cond_*` for `condition_variable`) |
| 3 | Port SH-4 GCC (4 commits) + binutils (3 commits) to 16.2, ABI harness + mutation control, artifacts | 12–20 | `sh.cc` drift; binutils SH PE on newer bfd |
| 4 | Image tags, README, CI | 3–5 | registry credentials already in CI |
| **Total** | without 2b | **35–57** | |
| | with 2b | **41–67** | |

Budget ~$69 (expires 2026-11-05). Without 2b it fits with margin; with
2b it is at the limit. Recommendation: decide on 2b after the Gate 1
run, when we know whether threads matter on-device.

## Cost so far

| Gate | Spent |
|---|---|
| 0 | ~3 (estimate; survey only, no builds) |
| 1–3 | not metered from inside the session; check account usage (one long session, local GCC 16 builds for both targets) |
