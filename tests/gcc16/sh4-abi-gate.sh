#!/bin/bash
# Gate 3 check of an sh4-mingw32ce toolchain: the binutils test, the
# runtime ABI test against the Microsoft compiler's code (reference and
# open questions, tests/sh4/abi) under qemu-sh4, and a mutation control:
# the same ABI test with GCC forced to return every struct in memory
# (-fpcc-struct-return) must fail, which shows that the harness detects
# a calling convention mismatch. Then the FPU thunk test (tests/sh4/fpu):
# mingwrt's coredll floating-point thunks against Microsoft /Qsh4 code.
# Last the -m4-single ABI test (tests/sh4/abi/run-fpu-abi-test.sh) against
# the Microsoft compiler's /Qsh4 code, with its mutation controls.
#
#   tests/gcc16/sh4-abi-gate.sh [OUTDIR]
#
# Needs sh4-mingw32ce-gcc and binutils on PATH, python3 and qemu-sh4.
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$(mktemp -d)}
T=sh4-mingw32ce-
mkdir -p "$OUT"

"$HERE/../sh4/binutils-test.sh" "$T"

echo "== ABI test"
"$HERE/../sh4/abi/run-abi-test.sh" "$OUT/abi" | tee "$OUT/abi.txt"

echo "== mutation control (-fpcc-struct-return)"
if CC="${T}gcc -fpcc-struct-return" \
     "$HERE/../sh4/abi/run-abi-test.sh" "$OUT/abi-mutant" \
     > "$OUT/abi-mutant.txt" 2>&1; then
  cat "$OUT/abi-mutant.txt"
  echo "FAIL: the mutated ABI test passed; the harness detects nothing"
  exit 1
fi
grep '^FAIL' "$OUT/abi-mutant.txt" || true
echo "PASS: mutated ABI test fails as expected"

echo "== FPU thunk test"
"$HERE/../sh4/fpu/run-fpu-test.sh" "$OUT/fpu" | tee "$OUT/fpu.txt"

echo "== FPU ABI test (-m4-single)"
"$HERE/../sh4/abi/run-fpu-abi-test.sh" "$OUT/fpu-abi" | tee "$OUT/fpu-abi.txt"

# Mutation controls: each mutant breaks one rule on the Microsoft side
# (run-fpu-abi-test.sh) and must fail.
n=$("$HERE/../sh4/abi/run-fpu-abi-test.sh" --count-mutants)
for m in $(seq 1 "$n"); do
  echo "== FPU ABI mutation control $m"
  if "$HERE/../sh4/abi/run-fpu-abi-test.sh" --mutate "$m" "$OUT/fpu-abi-mutant$m" \
       > "$OUT/fpu-abi-mutant$m.txt" 2>&1; then
    cat "$OUT/fpu-abi-mutant$m.txt"
    echo "FAIL: FPU ABI mutant $m passed; the harness misses that rule"
    exit 1
  fi
  grep '^mutant\|^FAIL' "$OUT/fpu-abi-mutant$m.txt" || true
done
echo "PASS: all $n FPU ABI mutants fail as expected"
echo "PASS: sh4-mingw32ce ABI gate"
