#!/bin/bash
#
# Builds a Binutils & GCC toolchain for Windows CE software development.
#
# Usage:
#   ./build_cf.sh PREFIX_DIRECTORY TARGET [STEP...]
#
# GNU tools can be confusing enough, so this script is meant to be simple:
# No branches, no options, just rebuilds the entire toolchain in the right order.
# If you want to skip a few steps, name the ones you want:
#   binutils  Binutils
#   gcc       GCC, mingw and w32api (needs the binutils of the same prefix)
#
# Several targets can share one prefix; build them one after the other.
# Build directories are build-TARGET/* in the current directory.
#
# Written by Colin Finck for ENLYZE GmbH
#

set -eu

if [[ $# -lt 2 ]]; then
    echo "Usage: ./build_cf.sh PREFIXDIR TARGET [binutils] [gcc]"
    echo
    echo "Supported targets are":
    echo "  - arm-mingw32ce"
    echo "  - i386-mingw32ce"
    echo "  - sh4-mingw32ce"
    exit 1
fi

J="-j `nproc`"
OLDPATH="$PATH"
PREFIXDIR="$1"
TARGET="$2"
shift 2
STEPS="${*:-binutils gcc}"
SRCDIR="$(cd "$(dirname "$0")" && pwd)"
BUILDDIR="$PWD/build-${TARGET}"

mkdir -p "${PREFIXDIR}" "${BUILDDIR}"
cd "${BUILDDIR}"


if [[ " $STEPS " = *" binutils "* ]]; then

echo "##################################"
echo "# BINUTILS                       #"
echo "##################################"

rm -rf binutils
mkdir binutils
cd binutils
"${SRCDIR}"/binutils/configure --prefix="${PREFIXDIR}" --target="${TARGET}" --with-pkgversion="ENLYZE" \
    --disable-multilib --disable-werror --enable-lto --enable-plugins \
    --with-zlib=yes --disable-nls --disable-unit-tests --disable-shared \
    --without-python
make $J
make install
cd ..

fi


if [[ " $STEPS " = *" gcc "* ]]; then

# --enable-threads=win32 of our GCC build wants to include <windows.h> from w32api.
# But building w32api requires a target GCC.
#
# GCC solves this circular dependency by considering the optional directory "winsup".
# "winsup/mingw/include" and "winsup/w32api/include" are added to the include path when
# building target components.
rm -rf "${SRCDIR}"/gcc/winsup
mkdir "${SRCDIR}"/gcc/winsup
ln -s ../../mingw "${SRCDIR}"/gcc/winsup/mingw
ln -s ../../w32api "${SRCDIR}"/gcc/winsup/w32api


echo "##################################"
echo "# INITIAL GCC                    #"
echo "##################################"

ADDITIONAL_GCC_PARAMETERS=""
if [[ "$TARGET" = "arm-mingw32ce" || "$TARGET" = "sh4-mingw32ce" ]]; then
       ADDITIONAL_GCC_PARAMETERS+="--disable-__cxa_atexit"
fi

# The binutils of this prefix.
export PATH="${PREFIXDIR}/bin:$OLDPATH"

rm -rf gcc
mkdir gcc
cd gcc
"${SRCDIR}"/gcc/configure --prefix="${PREFIXDIR}" --target="${TARGET}" --with-pkgversion="ENLYZE" \
    --enable-languages=c,c++ --disable-shared --disable-multilib --disable-nls \
    --disable-werror --disable-win32-registry --disable-libstdcxx-verbose \
    --enable-threads=win32 ${ADDITIONAL_GCC_PARAMETERS}

# Only build and install GCC so far to let us build low-level CE binaries.
make $J all-gcc
make install-gcc
make install-lto-plugin

# Building anything with GCC also requires libgcc.
make $J all-target-libgcc
make install-target-libgcc

cd ..


echo "##################################"
echo "# MINGW                          #"
echo "##################################"

rm -rf mingw
mkdir mingw
cd mingw
"${SRCDIR}"/mingw/configure --prefix="${PREFIXDIR}" --host="${TARGET}" --target="${TARGET}"
make $J
make install
cd ..


echo "##################################"
echo "# W32API                         #"
echo "##################################"

rm -rf w32api
mkdir w32api
cd w32api
"${SRCDIR}"/w32api/configure --prefix="${PREFIXDIR}" --host="${TARGET}" --target="${TARGET}"
make $J
make install
cd ..

# sh4-mingw32ce: w32api installs its own libcoredll.a over mingw's. Give it
# mingw's thunks for coredll's floating-point functions too
# (mingw/coredll_fpu.sh, docs/sh4-abi.md).
if [ "$TARGET" = "sh4-mingw32ce" ] && [ -f "${SRCDIR}"/mingw/coredll_fpu.sh ]; then
    for lib in libcoredll.a libcoredll6.a; do
        CC=${TARGET}-gcc AR=${TARGET}-ar NM=${TARGET}-nm OBJCOPY=${TARGET}-objcopy \
            sh "${SRCDIR}"/mingw/coredll_fpu.sh "${PREFIXDIR}/${TARGET}/lib/$lib" \
            coredll "${SRCDIR}"/mingw
    done
fi


# Continue building the rest of GCC as before.
# The remaining components (like libstdc++) can now depend on all headers and libraries
# of mingw and w32api.
export PATH="$OLDPATH"

echo "##################################"
echo "# REST OF GCC                    #"
echo "##################################"

cd gcc
make $J
make install
cd ..

fi
