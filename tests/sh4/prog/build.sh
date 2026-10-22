#!/bin/bash
# Builds the M4 test programs for one target and checks their PE headers
# and imports.
#
#   tests/sh4/prog/build.sh TARGET [OUTDIR]
#
# TARGET is sh4-mingw32ce or arm-mingw32ce; TARGET-gcc must be on PATH.
# Writes OUTDIR/hello-TARGET.exe and OUTDIR/mapio-TARGET.exe (default
# OUTDIR: out/ in the current directory).
set -eu

T=$1
OUT=${2:-out}
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
fail() { echo "FAIL: $*"; exit 1; }

case $T in
sh4-mingw32ce) MACHINE=0x01a6 FMT=pei-sh4-wince-little ;;
arm-mingw32ce) MACHINE=0x01c0 FMT=pei-arm-wince-little ;;
*) fail "unknown target $T" ;;
esac

CFLAGS="-O2 -Wall -Werror -D_WIN32_WCE=0x500"
LDFLAGS="-Wl,--image-base,0x10000 -Wl,--subsystem,wince:5.00 -s
  -Wl,--no-insert-timestamp"

"$T-gcc" $CFLAGS "$HERE/hello.c" -o "$OUT/hello-$T.exe" $LDFLAGS
"$T-gcc" $CFLAGS "$HERE/mapio.c" -o "$OUT/mapio-$T.exe" $LDFLAGS -lceddk

# Exact DLL list of each program.
check() {
  local exe=$1 dlls=$2 p
  p=$("$T-objdump" -p "$exe")
  grep -q "file format $FMT" <<<"$p" || fail "$exe: format"
  # Machine type from the PE file header.
  local pe m
  pe=$(od -An -tu4 -j 60 -N4 "$exe" | tr -d ' ')
  m=$(od -An -tx2 -j $((pe + 4)) -N2 "$exe" | tr -d ' ')
  [[ 0x$m = $MACHINE ]] || fail "$exe: machine 0x$m"
  grep -q '^ImageBase[[:space:]]*00010000$' <<<"$p" || fail "$exe: image base"
  grep -q '^Subsystem[[:space:]]*00000009' <<<"$p" || fail "$exe: subsystem"
  grep -q '^MajorSubsystemVersion[[:space:]]*5$' <<<"$p" \
    && grep -q '^MinorSubsystemVersion[[:space:]]*0$' <<<"$p" \
    || fail "$exe: subsystem version"
  local got
  got=$(sed -n 's/^[[:space:]]*DLL Name: //p' <<<"$p" | sort | tr '\n' ' ')
  [[ $got = "$dlls " ]] || fail "$exe: imports '$got', want '$dlls '"
  echo "ok: $exe ($got)"
}
check "$OUT/hello-$T.exe" "COREDLL.dll"
check "$OUT/mapio-$T.exe" "COREDLL.dll ceddk.dll"
echo "PASS: M4 programs for $T"
