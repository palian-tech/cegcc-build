#!/bin/bash
# Builds the gcc16 test program cetest.cpp for one target, once as C++20
# and once as C++23 (for sh4-mingw32ce also with -m4-single), and checks
# the PE headers and imports.
#
#   tests/gcc16/build.sh TARGET [OUTDIR]
#
# TARGET is arm-mingw32ce or sh4-mingw32ce; TARGET-g++ must be on PATH.
# Writes OUTDIR/cetest-TARGET-gccN-c++20.exe and ...-c++23.exe (and
# cetest-sh4-mingw32ce-gccN-m4-single-c++20.exe, ...-c++23.exe), N being
# the GCC major version (default OUTDIR: out/ in the current directory). A C++23 build that does
# not compile is reported and skipped; the C++20 build must succeed.
set -eu

T=$1
OUT=${2:-out}
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
fail() { echo "FAIL: $*"; exit 1; }

# coredll export lists (tests/sh4/coredll): the CE SDK's and those of a
# reference device.
case $T in
sh4-mingw32ce) MACHINE=0x01a6 FMT=pei-sh4-wince-little
  SDK=$HERE/../sh4/coredll/ce5-sdk-sh4.txt DEVICE=$HERE/../sh4/coredll/ref-device-sh4-ce5.txt ;;
arm-mingw32ce) MACHINE=0x01c0 FMT=pei-arm-wince-little
  SDK=$HERE/../sh4/coredll/ce5-sdk-armv4i.txt DEVICE=$HERE/../sh4/coredll/ref-device-arm-ce67.txt ;;
*) fail "unknown target $T" ;;
esac
names() { grep -v '^#' "$1" | sort -u; }

CXXFLAGS="-O2 -Wall -D_WIN32_WCE=0x500"
# The runtime is linked statically: the EXE imports COREDLL.dll only.
LDFLAGS="-static -Wl,--image-base,0x10000 -Wl,--subsystem,wince:5.00 -s
  -Wl,--no-insert-timestamp"

check() {
  local exe=$1 p
  p=$("$T-objdump" -p "$exe")
  grep -q "file format $FMT" <<<"$p" || fail "$exe: format"
  local pe m
  pe=$(od -An -tu4 -j 60 -N4 "$exe" | tr -d ' ')
  m=$(od -An -tx2 -j $((pe + 4)) -N2 "$exe" | tr -d ' ')
  [[ 0x$m = $MACHINE ]] || fail "$exe: machine 0x$m"
  grep -q '^Subsystem[[:space:]]*00000009' <<<"$p" || fail "$exe: subsystem"
  # Unique names: the import libraries of mingwrt and w32api can each add
  # a COREDLL.dll descriptor.
  local got
  got=$(sed -n 's/^[[:space:]]*DLL Name: //p' <<<"$p" | sort -u | tr '\n' ' ')
  [[ $got = "COREDLL.dll " ]] || fail "$exe: imports '$got', want 'COREDLL.dll '"
  # Every imported function must be in the CE SDK's coredll and in the
  # reference device's coredll: a missing import keeps the EXE from loading
  # without a message.
  local bad
  bad=$(comm -23 \
    <(awk '/^\t[0-9a-f]+ +<none> +[0-9a-f]+ /{print $4}' <<<"$p" | sort -u) \
    <(comm -12 <(names "$SDK") <(names "$DEVICE")) | tr '\n' ' ')
  [[ -z $bad ]] || fail "$exe: imports not in $(basename "$SDK") and $(basename "$DEVICE"): $bad"
  echo "ok: $exe ($got, $(stat -c %s "$exe") bytes)"
}

"$T-g++" --version | head -n 1
GCCV=gcc$("$T-g++" -dumpversion | cut -d. -f1)
# SH-4: also the FPU build (-m4-single, the /Qsh4 convention).
MODES=-
[[ $T = sh4-mingw32ce ]] && MODES="- m4-single"
for mode in $MODES; do
for std in c++20 c++23; do
  flags=
  base=$OUT/cetest-$T-$GCCV-$std
  if [[ $mode != - ]]; then
    flags=-$mode
    base=$OUT/cetest-$T-$GCCV-$mode-$std
  fi
  exe=$base.exe
  if "$T-g++" $flags -std=$std $CXXFLAGS "$HERE/cetest.cpp" -o "$exe" \
       $LDFLAGS 2> "$base.log"; then
    check "$exe"
  else
    cat "$base.log"
    [[ $std = c++23 ]] || fail "cetest $flags $std does not compile for $T"
    echo "SKIP: cetest $flags $std does not compile for $T (log kept)"
  fi
  [[ -s $base.log ]] || rm -f "$base.log"
done
done
echo "PASS: cetest for $T"
