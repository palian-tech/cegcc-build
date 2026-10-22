#!/bin/bash
# Runs hello.c under qemu-sh4: the real crt3 startup code, mingwrt and
# libgcc, with fakecoredll.c in place of coredll.dll.  Checks that WinMain
# computes PASS, that the division goes through all four coredll helpers
# and that the exit code reaches TerminateProcess.
#
#   tests/sh4/prog/run-hello.sh [OUTDIR]
#
# Needs sh4-mingw32ce-gcc on PATH, python3 and qemu-sh4.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$(mktemp -d)}
T=sh4-mingw32ce-
mkdir -p "$OUT"
fail() { echo "FAIL: $*"; exit 1; }

${T}as "$HERE/../abi/rt.s" -o "$OUT/rt.o"
${T}gcc -O2 -Wall -D_WIN32_WCE=0x500 -c "$HERE/hello.c" -o "$OUT/hello.o"
${T}gcc -O2 -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns \
    -c "$HERE/fakecoredll.c" -o "$OUT/fakecoredll.o"
# The libraries of the normal link (LIBGCC_SPEC) without -lcoredll.
${T}gcc -nostdlib -nostartfiles -Wl,-e,_start -o "$OUT/hello-qemu.exe" \
    "$OUT/rt.o" "$(${T}gcc -print-file-name=crt3.o)" "$OUT/hello.o" \
    "$OUT/fakecoredll.o" -lmingw32 -lgcc -lceoldname -lmingwex -lgcc
python3 -I "$HERE/../abi/pe2elf.py" "$OUT/hello-qemu.exe" "$OUT/hello-qemu.elf"
chmod +x "$OUT/hello-qemu.elf"

rc=0
qemu-sh4 "$OUT/hello-qemu.elf" > "$OUT/hello-qemu.txt" || rc=$?
cat "$OUT/hello-qemu.txt"
[[ $rc = 0 ]] || fail "exit code $rc"
grep -q '^PASS$' "$OUT/hello-qemu.txt" || fail "WinMain result"
grep -q '^division helper calls: _divls _divlu _modls _modlu$' \
    "$OUT/hello-qemu.txt" || fail "division helpers"
grep -q '^exit 0$' "$OUT/hello-qemu.txt" || fail "TerminateProcess"
echo "PASS: hello under qemu-sh4"
