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
   re-implementation.
2. `harness.c` is compiled by sh4-mingw32ce-gcc and linked with that code.
   It calls the MSVC functions (GCC → MSVC) and defines the functions the
   MSVC code calls, such as `ext2`, `extv` and `__divlu` (MSVC → GCC). Each
   callee records the arguments it received.
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

| # | Rule | Reference functions | GCC side |
|---|---|---|---|
| 2 | The caller always reserves 16 bytes of home area below the stack arguments, and the callee may write into it | `call2`, `call5`, `take64`, `callv` | `REG_PARM_STACK_SPACE` = 16, `OUTGOING_REG_PARM_STACK_SPACE` |
| 3 | Arguments 1–4 go in `r4`..`r7`, the rest at `@(16,r15)` and up. A 64-bit argument takes two registers with no even alignment. An 8-byte struct also takes two registers | `call5`, `take64`, `takeS8`, `callv` | standard SH assignment (no `-mdalign`) |
| 3 | Varargs follow the same placement. `va_list` is a plain pointer: the callee spills the anonymous registers into the home area | `callv` | `sh_setup_incoming_varargs` and `sh_build_builtin_va_list` |
| 4 | 32-bit results come back in `r0`, a 4-byte struct too | `retS4`, `call2` | — |
| 4 | 64-bit values and structs of 8 or more bytes are returned through a hidden pointer in `r4`, which the callee also returns in `r0` | `ret64`, `use64`, `retS8`, `useS8`, `retS20` | `sh_return_in_memory`, `sh_struct_value_rtx` |
| 5 | Unsigned 32-bit `/` and `%` call `__divlu` / `__modlu` | `divu` | `sh_init_libfuncs`; the `udivsi3` expander `FAIL`s |
| — | `r8`..`r14` and `PR` are callee-saved | all non-leaf functions | as GCC's SH default |

Under `-O2` GCC produces almost the same instruction sequences for `call2`,
`call5`, `use64`, `useS8`, `callv`, `divu`, `retS4` and `retS8`.

## Choices not covered by the reference

These are implemented as described, but no reference test proves them yet.
Compiling `tests/sh4/abi/open-questions.c` with eVC4 (`clsh /Qsh4 /O2 /c`)
and adding its listing to the reference would settle them.

* **64-bit argument starting in `r7`.** GCC splits it: low word in `r7`,
  high word at `@(16,r15)`. This keeps the varargs array contiguous, which a
  plain-pointer `va_list` over the home area requires. The harness only
  checks this GCC against itself (`varargs 64-bit in r7/stack`).
* **Structs of 5 to 7 bytes, and odd sizes up to 4 bytes.** GCC returns any
  struct larger than 4 bytes through memory, and BLKmode structs of 3 bytes
  as well.
* **`char`/`short` arguments and results.** The caller widens them (as
  prototyped C promotes), and neither side relies on the widening.
* **MACH/MACL** are call-clobbered, as in GCC's default SH ABI.
* **Bit-field layout** is the MS layout (`-mms-bitfields` behaviour), as on
  arm-mingw32ce.
* **Signed division** uses libgcc's `__sdivsi3_i4i`, which is linked
  statically. The reference only shows the unsigned helpers.
* **`float`/`double`** use the `-m4-nofpu` software floating point. A
  `double` result counts as a 64-bit value and is returned through memory.
  The FPU convention (`fl`, `callfl`, `FPSCR.PR`) is out of scope for now.

## Other target notes

* `dllimport` on functions is accepted and ignored. Calls go through the
  jump stubs of the import library, as on arm-mingw32ce. `dllexport` emits
  `-export:` directives into `.drectve`.
* DWARF offsets are plain `.long` values, because SH gas has no
  `.secrel32`. Debug info in linked images is therefore not usable yet.
