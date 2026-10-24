#!/bin/bash
# Writes the release notes (Markdown) of a tag to stdout.
#
#   release-notes.sh TAG SRC [RUN_URL] [PROGRAMS_DIR] [EXTRA_NOTES_FILE]
#
# TAG is v16.* (the 16.2 line) or v9.* (the 9.3 maintenance line), SRC a
# checkout of TAG, RUN_URL the workflow run that built and tested the
# image, PROGRAMS_DIR the test programs attached to the release (with
# SHA256SUMS). The body of the tag message (after the first line) and
# EXTRA_NOTES_FILE, if any, go under "In this release".
set -eu

TAG=$1 SRC=$2 RUN=${3:-} PROGRAMS=${4:-} EXTRA=${5:-}
IMAGE=ghcr.io/palian-tech/cegcc
HUB=paliantech/cegcc

# The 16.2 Dockerfile is at the root since the 9.3 line moved to its own
# branch; tags up to v16.2.0-ce.1 have it in gcc16/.
DOCKERFILE=$SRC/Dockerfile
[ -f "$SRC/gcc16/Dockerfile" ] && DOCKERFILE=$SRC/gcc16/Dockerfile
pin() {  # pin ARG: the default of ARG in the 16.2 Dockerfile
  sed -n "s/^ARG $1=//p" "$DOCKERFILE" | head -n 1
}
sub() {  # sub PATH: the submodule commit at TAG
  git -C "$SRC" ls-tree HEAD "$1" | awk '{print $3}'
}
tree() {  # tree REPO COMMIT
  echo "[\`${1#https://github.com/}\` \`${2:0:10}\`]($1/commit/$2)"
}

case $TAG in
v16.*)
  [ -n "$(pin GCC_COMMIT)" ] || { echo "release-notes.sh: no pins in $DOCKERFILE" >&2; exit 1; }
  cat <<END
CeGCC 16.2: GCC 16.2.0 and binutils 2.43.1 cross toolchains for Windows CE
in one Docker image, both targets in \`/opt/cegcc\` (on \`PATH\`):

* \`arm-mingw32ce\`: ARM, Windows CE 6 and 7
* \`sh4-mingw32ce\`: SH-4, Windows CE 5.0, with the calling convention of the
  Microsoft SH-4 compiler ([docs/sh4/ABI.md](https://github.com/palian-tech/cegcc-build/blob/$TAG/docs/sh4/ABI.md))
END
  if [ -f "$SRC/tests/sh4/abi/run-fpu-abi-test.sh" ]; then
    cat <<END
  * \`-m4-nofpu\` (default): software floating point
  * \`-m4-single\`: FPU code with the convention of the Microsoft compiler's \`/Qsh4\`
END
  fi
  cat <<END

C and C++ (up to C++23), runtime linked statically, threads disabled.

Source trees:

* GCC $(tree "$(pin GCC_REPO)" "$(pin GCC_COMMIT)")
* binutils $(tree "$(pin BINUTILS_REPO)" "$(pin BINUTILS_COMMIT)")
* mingwrt $(tree "$(pin MINGW_REPO)" "$(pin MINGW_COMMIT)")
* w32api $(tree "$(pin W32API_REPO)" "$(pin W32API_COMMIT)")
END
  ;;
v9.*)
  cat <<END
CeGCC 9.3 (maintenance line, branch \`releases/gcc-9\`): GCC 9.3.0 cross
toolchains for Windows CE in one Docker image, both targets in
\`/opt/cegcc\` (on \`PATH\`):

* \`arm-mingw32ce\`: ARM, Windows CE 6 and 7
* \`sh4-mingw32ce\`: SH-4, Windows CE 5.0, with the calling convention of the
  Microsoft SH-4 compiler ([docs/sh4/ABI.md](https://github.com/palian-tech/cegcc-build/blob/$TAG/docs/sh4/ABI.md)),
  software floating point (\`-m4-nofpu\`)

Source trees (submodules):

* GCC \`$(sub gcc | cut -c1-10)\`, binutils \`$(sub binutils | cut -c1-10)\`,
  mingwrt \`$(sub mingw | cut -c1-10)\`, w32api \`$(sub w32api | cut -c1-10)\`
END
  ;;
*)
  echo "release-notes.sh: unknown line for $TAG" >&2
  exit 1
  ;;
esac

body=$(git -C "$SRC" for-each-ref "refs/tags/$TAG" --format='%(contents:body)' \
  | sed -e '/-----BEGIN PGP SIGNATURE-----/,$d')
if [ -n "$EXTRA" ] && [ -s "$EXTRA" ]; then
  body=$(printf '%s\n\n%s' "$body" "$(cat "$EXTRA")")
fi
if [ -n "$(tr -d '[:space:]' <<<"$body")" ]; then
  printf '\n## In this release\n\n%s\n' "$body"
fi

cat <<END

## Image

\`\`\`sh
docker pull $IMAGE:$TAG
docker pull $HUB:$TAG
\`\`\`

\`\`\`sh
docker run --rm -v "\$PWD":/w -w /w $HUB:$TAG \\
    sh4-mingw32ce-gcc -O2 -D_WIN32_WCE=0x500 hello.c -o hello-sh4.exe
\`\`\`

END
case $TAG in
v16.*) echo "Moving tags of this line: \`:latest\` (newest 16.2 release), \`:edge\` (newest build of the 16.2 branch)." ;;
v9.*) echo "Moving tags of this line: \`:gcc9\` (newest 9.3 release), \`:gcc9-edge\` (newest build of \`releases/gcc-9\`)." ;;
esac

echo
echo "## Tests"
echo
if [ -n "$RUN" ]; then
  echo "Run by the [image build]($RUN) before publishing; any failure stops it:"
else
  echo "Run by the image build before publishing; any failure stops it:"
fi
echo
t=$SRC/tests
[ -f "$t/sh4/binutils-test.sh" ] && echo "* SH-4 binutils test (\`tests/sh4/binutils-test.sh\`)"
[ -f "$t/sh4/abi/run-abi-test.sh" ] && echo "* SH-4 calling convention: GCC code linked with the Microsoft SH-4 compiler's code and run under qemu-sh4, both call directions (\`tests/sh4/abi\`)"
[ -f "$t/gcc16/sh4-abi-gate.sh" ] && echo "  with a mutation control that must fail"
[ -f "$t/sh4/fpu/run-fpu-test.sh" ] && echo "* SH-4 coredll floating-point thunks against \`/Qsh4\` code under qemu-sh4 (\`tests/sh4/fpu\`)"
[ -f "$t/sh4/abi/run-fpu-abi-test.sh" ] && echo "* SH-4 \`-m4-single\`: GCC FPU code linked with the Microsoft compiler's \`/Qsh4\` code under qemu-sh4, with six mutation controls that must fail (\`tests/sh4/abi/run-fpu-abi-test.sh\`)"
[ -f "$t/sh4/prog/run-hello.sh" ] && echo "* SH-4 hello program under qemu-sh4 (\`tests/sh4/prog/run-hello.sh\`)"
[ -f "$t/gcc16/runtime-imports.sh" ] && echo "* The runtime libraries import only functions of the CE 5.0 SDK's coredll (\`tests/gcc16/runtime-imports.sh\`)"
if [ -f "$t/gcc16/build.sh" ]; then
  echo "* Test program \`tests/gcc16/cetest.cpp\` built for both targets as C++20 and C++23, PE headers and coredll imports checked"
elif [ -f "$t/sh4/prog/build.sh" ]; then
  echo "* Test programs of \`tests/sh4/prog\` built for both targets, PE headers and coredll imports checked"
fi
echo
echo "Device runs on Windows CE (SH-4 and ARM) are recorded in"
case $TAG in
v16.*) echo "[docs/gcc16/STATUS.md](https://github.com/palian-tech/cegcc-build/blob/$TAG/docs/gcc16/STATUS.md)." ;;
v9.*) echo "the pull requests of the release." ;;
esac

if [ -n "$PROGRAMS" ] && [ -f "$PROGRAMS/SHA256SUMS" ]; then
  echo
  echo "The test programs are attached; run them on the device, they write"
  echo "\`PASS\`/\`FAIL\` lines and a \`DONE\` line to a text file next to the EXE."
  echo
  echo '```'
  cat "$PROGRAMS/SHA256SUMS"
  echo '```'
fi
