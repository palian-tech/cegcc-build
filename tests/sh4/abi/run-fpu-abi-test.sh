#!/bin/bash
# Runtime ABI test of -m4-single: links the Microsoft SH-4 compiler's /Qsh4
# code for tests/sh4/abi/fpu-cases.c (docs/sh4/msvc-sh4-fpu-cases.txt,
# re-assembled by msvc2gas.py) with fpu-harness.c compiled by
# sh4-mingw32ce-gcc -m4-single and runs the result under qemu-sh4.
#
#   tests/sh4/abi/run-fpu-abi-test.sh [--mutate N] [OUTDIR]
#
# --mutate N changes one rule on the Microsoft side before assembling
# (see MUTANTS below), as a control: each mutant must fail the test.
#
# Needs sh4-mingw32ce-gcc and binutils on PATH (or TOOL_PREFIX), python3
# and qemu-sh4.
set -eu

# Each mutant is a sed script applied to the instructions of one function
# of the re-assembled listing; every replacement has the same size, so the
# layout and literal pools stay as they are.
MUTANTS=(
  # 1: the int after a float in r4 instead of r5 (floats take slots).
  "_call_fi|s/mov[[:space:]]*#7,r5/mov #7,r4/"
  # 2: no back-fill: the second float after a double in fr8, not fr5.
  "_call_mix|0,/fmov[[:space:]]*@r0,fr5/s//fmov @r0,fr8/"
  # 3: the double after an int read from dr6 instead of dr4.
  "_take_id|s/fmov[[:space:]]*fr5,fr7/fmov fr7,fr7/;s/fmov[[:space:]]*fr4,fr6/fmov fr6,fr6/"
  # 4: the 9th float one slot too low, at @(28,r15).
  "_call_f10|s/mov.l[[:space:]]*r2,@(32,r15)/mov.l r2,@(28,r15)/"
  # 5: FPSCR.PR left set on return.
  "_take_id|s/and[[:space:]]*r2,r1/nop/"
  # 6: a float result in fr1 instead of fr0.
  "_take_f|s/fmov[[:space:]]*fr4,fr0/fmov fr4,fr1/"
)

MUTATE=
if [ "${1:-}" = --mutate ]; then
  MUTATE=$2
  shift 2
fi
if [ "${1:-}" = --count-mutants ]; then
  echo ${#MUTANTS[@]}
  exit 0
fi

HERE=$(cd "$(dirname "$0")" && pwd)
REF="$HERE/../../../docs/sh4/msvc-sh4-fpu-cases.txt"
OUT=${1:-$(mktemp -d)}
T=${TOOL_PREFIX:-sh4-mingw32ce-}
CC=${CC:-${T}gcc}
mkdir -p "$OUT"

python3 -I "$HERE/msvc2gas.py" "$REF" "$OUT/msvc-fpu.s"
if [ -n "$MUTATE" ]; then
  m=${MUTANTS[$((MUTATE - 1))]}
  func=${m%%|*}
  script=${m#*|}
  # Apply the script between the function's label and the next .global.
  sed -i "/^$func:/,/\.global/{$script}" "$OUT/msvc-fpu.s"
  echo "mutant $MUTATE: $func: $script"
fi
${T}as -isa=sh4 "$OUT/msvc-fpu.s" -o "$OUT/msvc-fpu.o"
${T}as "$HERE/rt.s" -o "$OUT/rt.o"
$CC -m4-single -O2 -ffreestanding -fno-builtin -ffp-contract=off \
    -c "$HERE/fpu-harness.c" -o "$OUT/fpu-harness.o"
LIBGCC=$($CC -m4-single -print-libgcc-file-name)
${T}ld -e _start "$OUT/rt.o" "$OUT/fpu-harness.o" "$OUT/msvc-fpu.o" \
    "$LIBGCC" -o "$OUT/fpu-abi-test.exe"
python3 -I "$HERE/pe2elf.py" "$OUT/fpu-abi-test.exe" "$OUT/fpu-abi-test.elf"
chmod +x "$OUT/fpu-abi-test.elf"

# GCC's code for fpu-cases.c, for comparison with the listing.
sed 's/__int64/long long/g' "$HERE/fpu-cases.c" > "$OUT/fpu-cases.c"
$CC -m4-single -O2 -c "$OUT/fpu-cases.c" -o "$OUT/fpu-cases.o"
${T}objdump -d -r "$OUT/fpu-cases.o" > "$OUT/gcc-fpu-cases.dis"

qemu-sh4 "$OUT/fpu-abi-test.elf"
