# sh4-mingw32ce calling convention

GCC for `sh4-mingw32ce` follows the calling convention of the Microsoft
SH-4 compiler (eVC4 `clsh.exe`). The ground truth is
[`msvc-sh4-abi-reference.txt`](msvc-sh4-abi-reference.txt). The
implementation is the `TARGET_MSABI` code in `gcc/config/sh/` (sh.c, sh.md,
mingw32ce.h) on the `sh4-mingw32ce` branch of `palian-tech/gcc`.

## How it is tested

`tests/sh4/abi/run-abi-test.sh` runs the test. It needs sh4-mingw32ce
binutils and gcc, python3 and qemu-sh4.

1. `msvc2gas.py` turns the Microsoft listings back into assembly,
   instruction for instruction (the literal pools and relocations stay at the
   same offsets). The re-assembled code is the real MSVC code, not a
   re-implementation. The script uses
   [`msvc-sh4-abi-reference.txt`](msvc-sh4-abi-reference.txt) and both
   sections of
   [`msvc-sh4-abi-open-questions.txt`](msvc-sh4-abi-open-questions.txt):
   `/Qsh4 /O2`, plus `calldbl` from the build without `/Qsh4`.
2. `harness.c` is compiled by sh4-mingw32ce-gcc and linked with that code.
   It calls the MSVC functions (GCC → MSVC) and defines the functions the
   MSVC code calls, such as `ext2`, `extv`, `extS3` and `__divls`
   (MSVC → GCC). Each callee records the arguments it received.
3. `pe2elf.py` wraps the PE image for qemu-sh4, and `rt.s` supplies the
   entry point and the exit/write system calls.

Every call runs between canaries, and the test compares the stack pointer
before and after. `home_clobber` (in `rt.s`) overwrites the whole 16-byte
home area, as Microsoft-compiled callees are allowed to do. It also checks
that no live local of the caller lies inside that area.

As a control, mutating the MSVC side shows that the checks catch real
violations: the fifth argument stored at `@(12,r15)`, a varargs callee
without a home area, a struct returned in `r0:r1`, and a callee that demands
32 bytes of home area. Each of these fails the test.

The script also compiles the reference `t.c` with GCC and keeps
`gcc-t.dis`, so the GCC code can be read next to the reference.

## Points proven against the reference

| Rule | Reference functions | GCC side |
|---|---|---|
| The caller always reserves 16 bytes of home area below the stack arguments, and the callee may write into it | `call2`, `call5`, `take64`, `take_r7`, `callv` | `REG_PARM_STACK_SPACE` = 16, `OUTGOING_REG_PARM_STACK_SPACE` |
| Arguments 1–4 go in `r4`..`r7`, the rest at `@(16,r15)` and up. A 64-bit argument takes two registers with no even alignment. An 8-byte struct also takes two registers | `call5`, `take64`, `takeS8`, `callv` | standard SH assignment (no `-mdalign`) |
| A 64-bit argument that starts in `r7` is split: low word in `r7`, high word at `@(16,r15)`, and the next argument at `@(20,r15)`. The same holds for varargs | `call_r7`, `callv_r7`, `take_r7` | `sh_arg_partial_bytes` (SH4 without FPU doubles) |
| Varargs follow the same placement. `va_list` is a plain pointer: the callee spills the anonymous registers into the home area | `callv`, `callv_r7` | `sh_setup_incoming_varargs` and `sh_build_builtin_va_list` |
| Results of up to 4 bytes come back in `r0`. This includes structs of any size up to 4, so a 3-byte struct comes back in the low bytes | `retS4`, `retS3`, `useS3`, `call2` | `sh_return_in_memory`, and `sh_function_value` uses SImode for BLKmode |
| Larger results go through a hidden pointer in `r4`, which the callee returns in `r0`. This covers 64-bit scalars, `double` without FPU, and structs of 5, 6, 8 and 20 bytes. Callers may use the pointer in `r0` | `ret64`, `use64`, `retS5`, `retS6p`, `useS6p`, `retS8`, `useS8`, `retS20`, `calldbl` (no FPU) | `sh_return_in_memory`, `sh_struct_value_rtx` |
| `char` and `short` arguments are extended by the caller. Such results are extended by the callee. MSVC re-extends on both sides | `callc`, `retc`, `takec` | `sh_promote_prototypes`, `sh_promote_function_mode` |
| Without FPU, a `double` argument takes two consecutive integer registers, low word first | `calldbl` (no FPU) | DFmode in general registers |
| `MACH`/`MACL` are call-clobbered: the callee uses `mul.l` without saving them | `keepmac` | GCC's default SH ABI |
| 32-bit `/` and `%` call `__divlu`/`__modlu` (unsigned) and `__divls`/`__modls` (signed) | `divu`, `divs` | `sh_init_libfuncs`; the `udivsi3`/`divsi3` expanders `FAIL` |
| `r8`..`r14` and `PR` are callee-saved | all non-leaf functions | GCC's SH default |

Under `-O2` GCC produces almost the same instruction sequences for `call2`,
`call5`, `use64`, `useS8`, `callv`, `divu`, `retS4` and `retS8`.

libgcc's 64-bit division also ends up calling `__divlu`. `LIBGCC_SPEC` links
`-lcoredll` after `-lgcc`, so coredll provides it.

## Choices not covered by the reference

* **Bit-field layout** is the MS layout by default (`-mms-bitfields`, which
  `-mno-ms-bitfields` turns off), as on arm-mingw32ce. libgcc's fp-bit is
  built with `-mno-ms-bitfields` because it overlays the IEEE formats with
  bit-fields of mixed widths.
* **Floating-point helpers.** Without `/Qsh4`, MSVC calls coredll helpers
  through `__imp_` pointers (`__itod`, `__addd`), passing results and
  operands by address. GCC uses libgcc's fp-bit instead. These helpers are
  internal to each compiler, so no interface depends on the choice.
* **FPU code.** GCC generates `-m4-nofpu` code only. Calls into `/Qsh4`
  code (coredll) go through thunks; see the next section.

## Calls into /Qsh4 code (coredll floating point)

coredll.dll of SH-4 Windows CE 5.0 is built with `/Qsh4`. Its
floating-point functions (`sqrt`, `pow`, `_ecvt`, `strtod`, ...) therefore
take and return `float`/`double` in FPU registers, while `-m4-nofpu` code
passes them in integer registers. Without a conversion they read garbage:
before Gate 3b, `snprintf("%5.2f", 3.14159)` printed ` 0.00` on the device,
because mingwex's `snprintf` gets its digits from coredll's `_ecvt`/`_fcvt`.
The 9.3 toolchain has the same problem.

The `/Qsh4` convention, from `fl`, `callfl`
([reference](msvc-sh4-abi-reference.txt)) and `calldbl`
([open questions](msvc-sh4-abi-open-questions.txt), section 1), with the FPU
words decoded by hand:

| Rule | Evidence |
|---|---|
| A `double` argument goes in `dr4`, the next in `dr6`. The even register holds the high word (`fr4` = bits 63..32, `fr5` = bits 31..0) | `calldbl` builds `(double) x` in `dr0` and moves `fr0→fr4`, `fr1→fr5` before calling `extdbl`; `fl` and `calldbl` load the constant 1.0 as `fr6 = 0x3ff00000`, `fr7 = 0` |
| The `double` still takes its two integer slots: an `int` after it goes in `r6` | `calldbl`: `extdbl (double, int)` gets the `int` in `r6` |
| A `float` argument after a `double` goes in `fr6` | `fl (double x, float y)` reads `y` with `flds fr6,fpul`; `callfl` loads 2.5f into `fr6` |
| A `double` result comes back in `dr0` (`fr0` high, `fr1` low) | `fl` and `calldbl` move their result to `fr0`/`fr1`; `callfl` and `calldbl` read the callee's result from `dr0` |
| `FPSCR.PR` = 0 and `FPSCR.SZ` = 0 at calls and returns. Code sets `PR` around `double` arithmetic and clears it again, keeping the other bits | every `/Qsh4` function: `sts fpscr` / `or` / `lds` … `and` / `lds`; `fmov` moves doubles as two singles |

Not shown by the listings, and not relied on: how a `float` result comes
back (presumably `fr0`), where a first `float` argument goes (`fr4` or
`fr5`), whether a `float` takes an integer slot, and where a `double`
after an `int` goes (`_jn`, `_yn` assume `dr4`, since FPU registers are
allocated apart from integer slots in every listing).

mingwrt (`coredll_fpu.S`, `coredll_fpu.lst`, `coredll_fpu.sh`) replaces the
jump stubs of these functions in `libcoredll.a` and `libceoldname.a` with
thunks. A thunk saves `PR` and `FPSCR`, clears `FPSCR.PR`/`SZ`, moves each
`double` from its integer registers (low word first; the high word of a
second `double` may be on the stack) to `dr4`/`dr6`, drops the hidden
result pointer of a `double` result and moves the other arguments down one
register, copies stack arguments above a fresh 16-byte home area, calls
coredll through `__imp_`, stores `dr0` through the hidden pointer and
restores `FPSCR`.

* Covered (51 in `libcoredll.a`, 15 old names in `libceoldname.a`): the
  `double` math functions (`acos` … `tanh`, `atan2`, `fmod`, `pow`,
  `frexp`, `ldexp`, `modf`, `_hypot`, `_copysign`, `_chgsign`, `_logb`,
  `_nextafter`, `_scalb`, Bessel `_j0` … `_yn`), `atof`, `strtod`,
  `wcstod`, `_ecvt`, `_fcvt`, `_gcvt`, `_finite`, `_isnan`, `_fpclass`,
  `_isunordered`.
* `ceilf`, `floorf`, `sqrtf`, `fmodf`, `_isnanf`, `_isunorderedf` are
  computed by the `double` function (exact for these), so only the proven
  `double` rules are used; `fabsf` clears the sign bit. Their import
  members are removed from the library: the CE 5.0 SH-4 coredll exports
  only `ceilf`, `floorf` and `fmodf` of the `float` math functions, and a
  missing import keeps an EXE from loading without a message.
  `tests/gcc16/runtime-imports.sh` checks that the runtime libraries
  import only functions of the CE 5.0 SDK's coredll
  (`tests/gcc16/coredll/ce5-sdk-sh4.txt`); `tests/gcc16/build.sh` checks
  cetest's imports against the SDK list and a reference device's list.
* Not covered: `_cabs`/`cabs` (struct of two doubles by value), `_frnd`,
  `_fsqrt` (CRT internals), and floating-point conversions in coredll's own
  `printf`/`scanf` family when called directly (`_snprintf`, `swprintf`,
  `_snwprintf`, `fwprintf`, ...): how `/Qsh4` code reads a variadic
  `double` is not in the listings. mingwex's `printf` family and
  `strtod` do not use them.

`tests/sh4/fpu/run-fpu-test.sh` (part of the SH-4 gate) takes the thunks
out of the installed `libcoredll.a`, points their import pointers at
Microsoft's `calldbl` and `fl` and at a recording `/Qsh4`-style callee,
and runs them under qemu-sh4 with `FPSCR.PR` set. Swapping the word order
in the thunks makes it fail.

## Other target notes

* `dllimport` on functions is accepted and ignored. Calls go through the
  jump stubs of the import library, as on arm-mingw32ce. `dllexport` emits
  `-export:` directives into `.drectve`.
* DWARF offsets are plain `.long` values, because SH gas has no
  `.secrel32`. Debug info in linked images is therefore not usable yet.
