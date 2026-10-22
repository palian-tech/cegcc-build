#!/bin/bash
# Runtime ABI test: links the Microsoft SH-4 compiler's code for the
# functions of docs/sh4/msvc-sh4-abi-reference.txt (re-assembled by
# msvc2gas.py) with harness.c compiled by sh4-mingw32ce-gcc and runs the
# result under qemu-sh4.  It also writes GCC's code for the same
# functions next to the reference for review.
#
#   tests/sh4/abi/run-abi-test.sh [OUTDIR]
#
# Needs sh4-mingw32ce-gcc and binutils on PATH (or CC/AS/LD/... set),
# python3 and qemu-sh4.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
REF="$HERE/../../../docs/sh4/msvc-sh4-abi-reference.txt"
OUT=${1:-$(mktemp -d)}
T=${TOOL_PREFIX:-sh4-mingw32ce-}
CC=${CC:-${T}gcc}
mkdir -p "$OUT"

# The integer functions; fl and callfl need the FPU (out of scope).
FUNCS="_call2 _call5 _ret64 _use64 _retS8 _retS4 _retS20 _useS8 _takeS8
       _take64 _callv _divu"

python3 -I "$HERE/msvc2gas.py" "$REF" "$OUT/msvc.s" $FUNCS
${T}as "$OUT/msvc.s" -o "$OUT/msvc.o"
${T}as "$HERE/rt.s" -o "$OUT/rt.o"
$CC -O2 -ffreestanding -fno-builtin -c "$HERE/harness.c" -o "$OUT/harness.o"
LIBGCC=$($CC -print-libgcc-file-name)
${T}ld -e _start "$OUT/rt.o" "$OUT/harness.o" "$OUT/msvc.o" "$LIBGCC" \
    -o "$OUT/abi-test.exe"
python3 -I "$HERE/pe2elf.py" "$OUT/abi-test.exe" "$OUT/abi-test.elf"
chmod +x "$OUT/abi-test.elf"

# GCC's code for the reference functions, for comparison.
sed -n '/^## t.c/,/^## t.obj/p' "$REF" | sed '1d;$d' \
    | sed 's/__int64/long long/g' > "$OUT/t.c"
$CC -O2 -c "$OUT/t.c" -o "$OUT/t.o"
${T}objdump -d -r "$OUT/t.o" > "$OUT/gcc-t.dis"

qemu-sh4 "$OUT/abi-test.elf"
