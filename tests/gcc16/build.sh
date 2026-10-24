#!/bin/bash
# Builds the gcc16 test program cetest.cpp for one target, once as C++20
# and once as C++23, and checks the PE headers and imports.
#
#   tests/gcc16/build.sh TARGET [OUTDIR]
#
# TARGET is arm-mingw32ce or sh4-mingw32ce; TARGET-g++ must be on PATH.
# Writes OUTDIR/cetest-TARGET-gccN-c++20.exe and ...-c++23.exe, N being
# the GCC major version (default OUTDIR: out/ in the current directory). A C++23 build that does
# not compile is reported and skipped; the C++20 build must succeed.
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
  if [[ $T = sh4-mingw32ce ]]; then
    # Every imported function must be one the SH-4 CE 5.0 coredll exports
    # (coredll-sh4.txt). Of the float math functions it has only ceilf,
    # floorf and fmodf.
    local f bad=""
    for f in $(awk '/^\t[0-9a-f]+ +<none> +[0-9a-f]+ /{print $4}' <<<"$p" | sort -u); do
      case $f in
      ceilf|floorf|fmodf) ;;
      *f)
        case ${f%f} in
        acos|asin|atan|atan2|cos|cosh|exp|fabs|frexp|ldexp|log|log10|modf|\
        pow|sin|sinh|sqrt|tan|tanh|_isnan|_isunordered|_copysign|_chgsign|\
        _logb|_nextafter|_scalb|_hypot|_finite|_fpclass)
          bad="$bad $f"; continue ;;
        esac ;;
      esac
      grep -qx "$f" "$HERE/coredll-sh4.txt" || bad="$bad $f"
    done
    [[ -z $bad ]] || fail "$exe: imports not known to exist in the SH-4 CE 5.0 coredll:$bad (see tests/gcc16/coredll-sh4.txt)"
  fi
  echo "ok: $exe ($got, $(stat -c %s "$exe") bytes)"
}

"$T-g++" --version | head -n 1
GCCV=gcc$("$T-g++" -dumpversion | cut -d. -f1)
for std in c++20 c++23; do
  base=$OUT/cetest-$T-$GCCV-$std
  exe=$base.exe
  if "$T-g++" -std=$std $CXXFLAGS "$HERE/cetest.cpp" -o "$exe" $LDFLAGS \
       2> "$base.log"; then
    check "$exe"
  else
    cat "$base.log"
    [[ $std = c++23 ]] || fail "cetest $std does not compile for $T"
    echo "SKIP: cetest $std does not compile for $T (log kept)"
  fi
  [[ -s $base.log ]] || rm -f "$base.log"
done
echo "PASS: cetest for $T"
