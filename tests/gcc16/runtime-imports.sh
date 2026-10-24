#!/bin/bash
# Toolchain check: the runtime libraries of TARGET (libstdc++, libgcc,
# mingwrt's libmingw32, libmingwex, libceoldname) import from coredll only
# functions of the CE SDK's coredll (tests/gcc16/coredll/ce5-sdk-*.txt), so
# that any program they are linked into loads on every device of that CE
# version. Links all of them whole into one EXE and reads its imports.
#
#   tests/gcc16/runtime-imports.sh TARGET [OUTDIR]
set -euo pipefail

T=$1
OUT=${2:-$(mktemp -d)}
HERE=$(cd "$(dirname "$0")" && pwd)
case $T in
sh4-mingw32ce) SDK=$HERE/coredll/ce5-sdk-sh4.txt ;;
*) echo "runtime-imports.sh: no SDK list for $T"; exit 1 ;;
esac
mkdir -p "$OUT"

echo 'int main (void) { return 0; }' > "$OUT/main.c"
"$T-gcc" -c "$OUT/main.c" -o "$OUT/main.o"
LIB=$(dirname "$("$T-gcc" -print-file-name=libmingwex.a)")
"$T-g++" -static -o "$OUT/runtime.exe" "$OUT/main.o" -Wl,--whole-archive \
  "$("$T-g++" -print-file-name=libstdc++.a)" "$LIB/libmingwex.a" \
  "$LIB/libmingw32.a" "$LIB/libceoldname.a" "$("$T-gcc" -print-libgcc-file-name)" \
  -Wl,--no-whole-archive -Wl,--allow-multiple-definition \
  -Wl,--unresolved-symbols=ignore-all 2> "$OUT/link.log" \
  || { cat "$OUT/link.log"; exit 1; }
"$T-objdump" -p "$OUT/runtime.exe" \
  | awk '/^\t[0-9a-f]+ +<none> +[0-9a-f]+ /{print $4}' | sort -u > "$OUT/imports.txt"
bad=$(comm -23 "$OUT/imports.txt" <(grep -v '^#' "$SDK" | sort -u) | tr '\n' ' ')
if [[ -n $bad ]]; then
  echo "FAIL: $T runtime imports not in $(basename "$SDK"): $bad"
  exit 1
fi
echo "PASS: $T runtime libraries import $(wc -l < "$OUT/imports.txt") coredll functions, all in $(basename "$SDK")"
