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
* **FPU code** (`/Qsh4`: `fl`, `callfl`, `calldbl`, with `FPSCR.PR` switched
  around `double` operations, and `float`/`double` passed in FPU registers)
  is out of scope. The default is `-m4-nofpu`.

## Other target notes

* `dllimport` on functions is accepted and ignored. Calls go through the
  jump stubs of the import library, as on arm-mingw32ce. `dllexport` emits
  `-export:` directives into `.drectve`.
* DWARF offsets are plain `.long` values, because SH gas has no
  `.secrel32`. Debug info in linked images is therefore not usable yet.
