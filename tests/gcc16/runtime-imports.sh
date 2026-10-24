#!/bin/bash
# Toolchain check: the runtime libraries of TARGET (libstdc++, libgcc,
# mingwrt's libmingw32, libmingwex, libceoldname) import from coredll only
# functions of the CE SDK's coredll (tests/sh4/coredll/ce5-sdk-*.txt), so
# that any program they are linked into loads on every device of that CE
# version. Links all of them whole into one EXE and reads its imports, for
# each multilib.
#
#   tests/gcc16/runtime-imports.sh TARGET [OUTDIR]
set -euo pipefail

T=$1
OUT=${2:-$(mktemp -d)}
HERE=$(cd "$(dirname "$0")" && pwd)
case $T in
sh4-mingw32ce) SDK=$HERE/../sh4/coredll/ce5-sdk-sh4.txt ;;
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
done
