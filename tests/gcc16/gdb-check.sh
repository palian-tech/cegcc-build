#!/bin/bash
# Toolchain check: DWARF debug info in a linked image is usable by the
# target's gdb. Reads the -g build of cetest (tests/gcc16/build.sh) with
# TARGET-gdb -batch, without running it, and checks the line info of
# WinMain and of one check function (c_snprintf), and the type and frame
# location of two of its locals. Then connects gdb to a minimal remote
# stub (gdb-stub.py, stopped in c_snprintf, like an emulator's gdb stub:
# no qGetTIBAddr, and for ARM an XML target description of the core
# registers only) and checks that the connection holds, the frame is
# found, and the 8-byte locals of c_snprintf, which ARM keeps in a
# realigned block of the frame, can be written and read back.
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

# gdb must parse XML target descriptions (ARM stubs send the core
# registers only; without expat gdb expects the FPA layout).
grep -q -- '--with-expat' <<<"$("$T-gdb" -batch -nx -ex 'show configuration')" \
  || fail "built without expat (no XML target descriptions)"

pc=$(sed -n 's/^Line [0-9]* of ".*" starts at address \(0x[0-9a-f]*\) <.*c_snprintf.*/\1/p' \
  <<<"$out" | head -n 1)
out=$("$T-gdb" -batch -nx \
  -ex "target remote | python3 $(dirname "$0")/gdb-stub.py ${T%%-*} $pc" \
  -ex 'frame' \
  -ex 'print $sp' \
  -ex 'info address vd' \
  -ex 'set var vd = 2.5' \
  -ex 'set var vll = -1234567890123' \
  -ex 'set var vull = 18446744073709551615' \
  -ex 'print vd' \
  -ex 'print vll' \
  -ex 'print vull' \
  -ex 'print (unsigned long)&vd % 8 + (unsigned long)&vll % 8 + (unsigned long)&vull % 8' \
  "$EXE" 2>&1) || true  # the checks below report what failed
grep -q '^#0  .*c_snprintf () at .*cetest\.cpp:[0-9]' <<<"$out" \
  || fail "remote stub: no frame in c_snprintf"
grep -q '^\$1 = .*0x100000' <<<"$out" || fail "remote stub: wrong registers"
# The value history numbers count only the prints.
grep -qx '\$2 = 2.5' <<<"$out" || fail "remote stub: vd has no location"
grep -qx '\$3 = -1234567890123' <<<"$out" || fail "remote stub: vll has no location"
grep -qx '\$4 = 18446744073709551615' <<<"$out" \
  || fail "remote stub: vull has no location"
if [[ $T = arm-* ]]; then
  # GCC realigns the frame for them (8-byte alignment, 4-byte stack): the
  # location computes the aligned base from the frame base.
  grep -A4 '^Symbol "vd" is' <<<"$out" | grep -q 'DW_OP_and' \
    || fail "remote stub: c_snprintf frame is not realigned"
  grep -qx '\$5 = 0' <<<"$out" || fail "remote stub: 8-byte locals not 8-byte aligned"
fi
if grep -qi 'qGetTIBAddr\|Truncated register\|internal-error' <<<"$out"; then
  fail "remote stub: errors"
fi

echo "PASS: $T gdb reads $(basename "$EXE"): line info for WinMain and c_snprintf, type and frame location of its locals; remote stub connection works, 8-byte locals readable"
