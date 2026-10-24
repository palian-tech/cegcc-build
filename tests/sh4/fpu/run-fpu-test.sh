#!/bin/bash
# FPU thunk test: runs the coredll floating-point thunks of the installed
# libcoredll.a (mingwrt coredll_fpu.S) against the Microsoft compiler's
# /Qsh4 code under qemu-sh4.  See fpu-test.c.
#
#   tests/sh4/fpu/run-fpu-test.sh [OUTDIR]
#
# Needs sh4-mingw32ce-gcc and binutils on PATH (or TOOL_PREFIX), python3
# and qemu-sh4.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ABI="$HERE/../abi"
DOCS="$HERE/../../../docs/sh4"
OUT=${1:-$(mktemp -d)}
T=${TOOL_PREFIX:-sh4-mingw32ce-}
CC=${T}gcc
mkdir -p "$OUT/thunks"

python3 -I "$ABI/msvc2gas.py" --suffix _fpu \
    "$DOCS/msvc-sh4-abi-open-questions.txt" "$OUT/calldbl.s" _calldbl
python3 -I "$ABI/msvc2gas.py" --suffix _fpu \
    "$DOCS/msvc-sh4-abi-reference.txt" "$OUT/fl.s" _fl
for f in calldbl fl; do ${T}as "$OUT/$f.s" -o "$OUT/$f.o"; done
${T}as "$ABI/rt.s" -o "$OUT/rt.o"
${T}as "$HERE/fpu-rt.s" -o "$OUT/fpu-rt.o"
$CC -O2 -ffreestanding -fno-builtin -c "$HERE/fpu-test.c" -o "$OUT/fpu-test.o"

LIB=$($CC -print-file-name=libcoredll.a)
THUNKS="fpu_atof.o fpu_fmod.o fpu__ecvt.o fpu_sqrt.o fpu_sqrtf.o fpu_fabsf.o"
(cd "$OUT/thunks" && ${T}ar x "$LIB" $THUNKS)

${T}ld -e _start "$OUT/rt.o" "$OUT/fpu-rt.o" "$OUT/fpu-test.o" \
    "$OUT/calldbl.o" "$OUT/fl.o" $(cd "$OUT/thunks" && printf "$OUT/thunks/%s " $THUNKS) \
    "$($CC -print-libgcc-file-name)" -o "$OUT/fpu-test.exe"
python3 -I "$ABI/pe2elf.py" "$OUT/fpu-test.exe" "$OUT/fpu-test.elf"
chmod +x "$OUT/fpu-test.elf"
qemu-sh4 "$OUT/fpu-test.elf"
