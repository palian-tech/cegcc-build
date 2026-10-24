#!/bin/bash
# Toolchain check: DWARF debug info in a linked image is usable by the
# target's gdb. Reads the -g build of cetest (tests/gcc16/build.sh) with
# TARGET-gdb -batch, without running it, and checks the line info of
# WinMain and of one check function (c_snprintf), and the type and frame
# location of two of its locals.
#
#   tests/gcc16/gdb-check.sh TARGET EXEDIR
set -euo pipefail

T=$1
EXE=$(ls "$2"/cetest-"$T"-gcc*-g-c++20.exe)
fail() { echo "FAIL: $T gdb: $*"; echo "$out"; exit 1; }

out=$("$T-gdb" -batch -nx \
  -ex 'show osabi' \
  -ex 'info line WinMain' \
  -ex 'info line c_snprintf' \
  -ex 'info scope c_snprintf' \
  -ex "ptype '(anonymous namespace)::c_snprintf'::buf" \
  -ex "whatis '(anonymous namespace)::c_snprintf'::n" \
  "$EXE" 2>&1)

grep -q 'currently "WindowsCE"' <<<"$out" || fail "OS ABI is not Windows CE"
for f in WinMain c_snprintf; do
  grep -Eq "^Line [0-9]+ of \".*cetest\.cpp\" starts at address 0x[0-9a-f]+ <.*$f" \
    <<<"$out" || fail "no line info for $f"
done
# buf: char[64] in the frame; n: an int in the frame. info scope prints a
# frame-relative location as "a complex DWARF expression: DW_OP_fbreg N"
# (or "a variable at frame base ..."), followed by the length.
frame_local() {
  grep -A2 "^Symbol $1 is " <<<"$out" | tr '\n' ' ' \
    | grep -Eq "(fbreg|frame base).*length $2\." \
    || fail "no frame location of length $2 for $1"
}
frame_local buf 64
frame_local n 4
grep -q '^type = char \[64\]$' <<<"$out" || fail "type of buf is not char [64]"
grep -q '^type = int$' <<<"$out" || fail "type of n is not int"
if grep -qi 'internal-error\|warning: \|Dwarf Error' <<<"$out"; then
  fail "gdb reported errors"
fi
echo "PASS: $T gdb reads $(basename "$EXE"): line info for WinMain and c_snprintf, type and frame location of its locals"
