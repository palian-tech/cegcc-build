#!/bin/bash
# Checks the sh4-mingw32ce binutils: gas object format, a dlltool import
# library for coredll and an ld-linked Windows CE SH-4 executable.
#
#   tests/sh4/binutils-test.sh [TOOL_PREFIX]   (default: sh4-mingw32ce-)
set -eu

T=${1:-sh4-mingw32ce-}
HERE=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
fail() { echo "FAIL: $*"; exit 1; }

# 16-bit little-endian value at file offset $2 of $1.
u16() { od -An -tu2 -j "$2" -N2 "$1" | tr -d ' '; }
u32() { od -An -tu4 -j "$2" -N4 "$1" | tr -d ' '; }

"${T}as" "$HERE/start.s" -o "$W/start.o"
"${T}objdump" -f "$W/start.o" | grep -q 'file format pe-sh4-wince-little' \
    || fail "object format"
[[ $(u16 "$W/start.o" 0) = $((0x1a6)) ]] || fail "object machine"

"${T}dlltool" -d "$HERE/coredll-min.def" -l "$W/libcoredll.a"
"${T}nm" "$W/libcoredll.a" | grep -q ' T _MessageBoxW$' || fail "stub symbol"
"${T}nm" "$W/libcoredll.a" | grep -q ' I __imp__MessageBoxW$' || fail "IAT symbol"

"${T}ld" -e _WinMainCRTStartup "$W/start.o" "$W/libcoredll.a" -o "$W/t.exe"
pe=$(u32 "$W/t.exe" 60)
[[ $(u16 "$W/t.exe" $((pe + 4))) = $((0x1a6)) ]] || fail "image machine"
"${T}objdump" -p "$W/t.exe" > "$W/p.txt"
grep -q 'file format pei-sh4-wince-little' "$W/p.txt" || fail "image format"
grep -q '^ImageBase[[:space:]]*00010000' "$W/p.txt" || fail "image base"
grep -q '^SectionAlignment[[:space:]]*00001000' "$W/p.txt" || fail "alignment"
grep -q '^Subsystem[[:space:]]*00000009' "$W/p.txt" || fail "subsystem"
grep -q '^MajorSubsystemVersion[[:space:]]*5' "$W/p.txt" || fail "subsystem version"
grep -q 'DLL Name: COREDLL.dll' "$W/p.txt" || fail "import"
grep -q 'MessageBoxW' "$W/p.txt" || fail "imported name"

# The jump stub loads the IAT slot and jumps through it.
"${T}objdump" -d "$W/t.exe" | grep -A3 '<_MessageBoxW>:' > "$W/stub.txt"
grep -q 'mov.l.*r0' "$W/stub.txt" && grep -q 'jmp.*@r0' "$W/stub.txt" \
    || fail "jump stub"

echo "PASS: sh4-mingw32ce binutils"
