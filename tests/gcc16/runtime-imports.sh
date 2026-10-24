#!/bin/bash
# Toolchain check: the runtime libraries of TARGET (libstdc++, libgcc,
# mingwrt's libmingw32, libmingwex, libceoldname) import from coredll only
# functions of the CE SDK's coredll (tests/sh4/coredll/ce5-sdk-*.txt), so
# that any program they are linked into loads on every device of that CE
# version. Links all of them whole into one EXE and reads its imports, for
# each multilib. libgcc's thread and atomic objects (gthr-wince.o,
# atomic-wince.o) are also checked on their own against the SDK's list and
# the reference device's (tests/sh4/coredll/ref-device-*.txt).
#
#   tests/gcc16/runtime-imports.sh TARGET [OUTDIR]
set -euo pipefail

T=$1
OUT=${2:-$(mktemp -d)}
HERE=$(cd "$(dirname "$0")" && pwd)
case $T in
sh4-mingw32ce) SDK=$HERE/../sh4/coredll/ce5-sdk-sh4.txt
  DEVICE=$HERE/../sh4/coredll/ref-device-sh4-ce5.txt ;;
*) echo "runtime-imports.sh: no SDK list for $T"; exit 1 ;;
esac
mkdir -p "$OUT"

# Each multilib (sh4-mingw32ce: the default -m4-nofpu and -m4-single).
"$T-gcc" -print-multi-lib | while IFS=';' read -r dir flags; do
  flags=$(sed 's/@/ -/g' <<<"$flags")
  o=$OUT/${dir//\//_}
  mkdir -p "$o"
  echo 'int main (void) { return 0; }' > "$o/main.c"
  "$T-gcc" $flags -c "$o/main.c" -o "$o/main.o"
  LIB=$(dirname "$("$T-gcc" $flags -print-file-name=libmingwex.a)")
  "$T-g++" $flags -static -o "$o/runtime.exe" "$o/main.o" -Wl,--whole-archive \
    "$("$T-g++" $flags -print-file-name=libstdc++.a)" "$LIB/libmingwex.a" \
    "$LIB/libmingw32.a" "$LIB/libceoldname.a" \
    "$("$T-gcc" $flags -print-libgcc-file-name)" \
    -Wl,--no-whole-archive -Wl,--allow-multiple-definition \
    -Wl,--unresolved-symbols=ignore-all 2> "$o/link.log" \
    || { cat "$o/link.log"; exit 1; }
  "$T-objdump" -p "$o/runtime.exe" \
    | awk '/^\t[0-9a-f]+ +<none> +[0-9a-f]+ /{print $4}' | sort -u > "$o/imports.txt"
  bad=$(comm -23 "$o/imports.txt" <(grep -v '^#' "$SDK" | sort -u) | tr '\n' ' ')
  if [[ -n $bad ]]; then
    echo "FAIL: $T$flags runtime imports not in $(basename "$SDK"): $bad"
    exit 1
  fi
  echo "PASS: $T$flags runtime libraries import $(wc -l < "$o/imports.txt") coredll functions, all in $(basename "$SDK")"

  # libgcc's thread and atomic objects (gthr-wince.o, with
  # --enable-threads=win32, and atomic-wince.o) with only what they pull
  # in from the runtime: their
  # imports must be in the SDK's list and in the reference device's, and
  # Vista-only functions such as GetThreadId or GetSystemTimeAsFileTime
  # must not appear (a missing import keeps an EXE from loading).
  t=$o/threads
  mkdir -p "$t"
  (cd "$t" && "$T-ar" x "$("$T-gcc" $flags -print-libgcc-file-name)" \
     atomic-wince.o && "$T-ar" x "$("$T-gcc" $flags -print-libgcc-file-name)" \
     gthr-wince.o 2>/dev/null || true)
  objs=$(ls "$t"/*.o)
  "$T-gcc" $flags -nostartfiles -nostdlib -o "$t/threads.exe" $objs \
    -Wl,-e,0 -L"$LIB" -Wl,--start-group -lmingw32 -lmingwex -lgcc \
    -lcoredll -Wl,--end-group \
    2> "$t/link.log" || { cat "$t/link.log"; exit 1; }
  "$T-objdump" -p "$t/threads.exe" \
    | awk '/^\t[0-9a-f]+ +<none> +[0-9a-f]+ /{print $4}' | sort -u > "$t/imports.txt"
  bad=$(comm -23 "$t/imports.txt" \
    <(comm -12 <(grep -v '^#' "$SDK" | sort -u) <(grep -v '^#' "$DEVICE" | sort -u)) \
    | tr '\n' ' ')
  if [[ -n $bad ]]; then
    echo "FAIL: $T$flags thread/atomic objects import functions not in $(basename "$SDK") and $(basename "$DEVICE"): $bad"
    exit 1
  fi
  if [[ $("$T-gcc" -v 2>&1 | sed -n 's/^Thread model: //p') = win32 ]] \
     && ! grep -qx CreateThread "$t/imports.txt"; then
    echo "FAIL: $T$flags win32 threads, but gthr-wince.o imports no CreateThread"
    exit 1
  fi
  echo "PASS: $T$flags thread/atomic objects ($(cd "$t" && echo *.o)) import $(wc -l < "$t/imports.txt") coredll functions, all in $(basename "$SDK") and $(basename "$DEVICE")"
done
