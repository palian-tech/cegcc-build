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
# GCC_THREADS (environment) is the --enable-threads model of GCC, default
# win32; "no" builds GCC with --disable-threads (GCC 13 and later need
# Vista APIs for win32 threads, which Windows CE does not have).
# BINUTILS_FLAGS (environment) adds options to the binutils configure,
# e.g. "--disable-gdb --disable-sim".
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
GCC_THREADS="${GCC_THREADS:-win32}"
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
    --without-python ${BINUTILS_FLAGS:-}
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
if [[ "$GCC_THREADS" = "no" ]]; then
       ADDITIONAL_GCC_PARAMETERS+="--disable-threads "
else
       ADDITIONAL_GCC_PARAMETERS+="--enable-threads=${GCC_THREADS} "
fi
if [[ "$TARGET" = "arm-mingw32ce" || "$TARGET" = "sh4-mingw32ce" ]]; then
       ADDITIONAL_GCC_PARAMETERS+="--disable-__cxa_atexit"
fi
# sh4-mingw32ce: GCC 16 has the multilibs -m4-nofpu (default) and
# -m4-single (the FPU convention of Microsoft's /Qsh4, docs/sh4-abi.md).
if [[ "$TARGET" = "sh4-mingw32ce" ]]; then
       ADDITIONAL_GCC_PARAMETERS+=" --enable-multilib"
else
       ADDITIONAL_GCC_PARAMETERS+=" --disable-multilib"
fi

# The binutils of this prefix.
export PATH="${PREFIXDIR}/bin:$OLDPATH"

rm -rf gcc
mkdir gcc
cd gcc
"${SRCDIR}"/gcc/configure --prefix="${PREFIXDIR}" --target="${TARGET}" --with-pkgversion="ENLYZE" \
    --enable-languages=c,c++ --disable-shared --disable-nls \
    --disable-werror --disable-win32-registry --disable-libstdcxx-verbose \
    ${ADDITIONAL_GCC_PARAMETERS}

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


# The runtime once more for each further multilib (sh4-mingw32ce:
# m4-single), into lib/<multilib>. Headers are the same and not installed
# again. -m4-single calls coredll with coredll's own convention: for it,
# coredll_fpu.sh only replaces the float functions that coredll lacks.
for ml in $(${TARGET}-gcc -print-multi-lib | sed -n 's/^\([^.][^;]*\);.*/\1/p'); do
    flags=$(${TARGET}-gcc -print-multi-lib | sed -n "s/^$ml;//p" | sed 's/@/ -/g')
    for c in mingw w32api; do
        rm -rf "$c-$ml"
        mkdir "$c-$ml"
        cd "$c-$ml"
        CC="${TARGET}-gcc$flags" "${SRCDIR}"/$c/configure --prefix="${PREFIXDIR}" \
            --host="${TARGET}" --target="${TARGET}"
        make $J
        make install inst_libdir="${PREFIXDIR}/${TARGET}/lib/$ml" \
            inst_includedir="$PWD/unused/include" inst_bindir="$PWD/unused/bin" \
            inst_docdir="$PWD/unused/doc"
        cd ..
    done
    if [ "$TARGET" = "sh4-mingw32ce" ] && [ -f "${SRCDIR}"/mingw/coredll_fpu.sh ]; then
        for lib in libcoredll.a libcoredll6.a; do
            CC="${TARGET}-gcc$flags" AR=${TARGET}-ar NM=${TARGET}-nm \
                OBJCOPY=${TARGET}-objcopy sh "${SRCDIR}"/mingw/coredll_fpu.sh \
                "${PREFIXDIR}/${TARGET}/lib/$ml/$lib" coredll "${SRCDIR}"/mingw
        done
    fi
done


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
