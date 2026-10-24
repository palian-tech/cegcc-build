# CeGCC on newer GCC (docs/gcc16): toolchain for one target from source
# trees given by repository and commit, built with this repository's
# build_cf.sh, plus the test program tests/gcc16/cetest.cpp.
#
# Gate 1 defaults: salman-javed-nz's GCC 14.2 trees, arm-mingw32ce,
# --disable-threads. From the repository root:
#
#   docker build -f gcc16/Dockerfile --target programs \
#       --output type=local,dest=out .
#
# Stages: binutils -> gcc -> image; test runs the sh4 ABI gate (sh4 only)
# and builds the test program, programs holds only the EXEs.

ARG BASE_IMAGE=ubuntu:24.04@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac

FROM ${BASE_IMAGE} AS build-base
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential flex bison texinfo file git ca-certificates \
        libgmp-dev libmpfr-dev libmpc-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
# Fetches one commit of a repository into DIR.
RUN printf '%s\n' '#!/bin/sh' 'set -e' \
        'git init -q "$1"; cd "$1"' \
        'git fetch -q --depth 1 "$2" "$3"' \
        'git checkout -q FETCH_HEAD; git log -1 --format="$1 %H %s"' \
        > /usr/local/bin/fetch-commit \
    && chmod +x /usr/local/bin/fetch-commit

FROM build-base AS binutils
ARG BINUTILS_REPO=https://github.com/salman-javed-nz/binutils-gdb
ARG BINUTILS_COMMIT=f7f94cdd6b4b254d7b30e1832bfbf7f3c9262a47
ARG BINUTILS_FLAGS="--disable-gdb --disable-gdbserver --disable-sim"
ARG TARGET=arm-mingw32ce
ARG PREFIX=/opt/cegcc
RUN fetch-commit binutils ${BINUTILS_REPO} ${BINUTILS_COMMIT}
COPY build_cf.sh ./
RUN BINUTILS_FLAGS="${BINUTILS_FLAGS}" ./build_cf.sh ${PREFIX} ${TARGET} binutils > /src/binutils.log 2>&1 \
        || { tail -n 60 /src/binutils.log; exit 1; }; \
    tail -n 3 /src/binutils.log; rm -rf build-* binutils

FROM binutils AS gcc
ARG GCC_REPO=https://github.com/salman-javed-nz/gcc
ARG GCC_COMMIT=de6e9181807529b3034aa9cc56f97daa46bee303
ARG MINGW_REPO=https://github.com/salman-javed-nz/mingwrt
ARG MINGW_COMMIT=7c35691fba55b5947d40c863ba4d0d1775a02b3a
ARG W32API_REPO=https://github.com/enlyze/w32api
ARG W32API_COMMIT=51de0ad1420f5228fbc340b1fdb057a6b8f710e0
ARG GCC_THREADS=no
ARG TARGET=arm-mingw32ce
ARG PREFIX=/opt/cegcc
RUN fetch-commit mingw ${MINGW_REPO} ${MINGW_COMMIT} \
    && fetch-commit w32api ${W32API_REPO} ${W32API_COMMIT}
RUN fetch-commit gcc ${GCC_REPO} ${GCC_COMMIT}
RUN GCC_THREADS=${GCC_THREADS} ./build_cf.sh ${PREFIX} ${TARGET} gcc \
        > /src/gcc.log 2>&1 || { tail -n 80 /src/gcc.log; exit 1; }; \
    tail -n 3 /src/gcc.log; rm -rf build-*

FROM ${BASE_IMAGE} AS image
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        make libgmp10 libmpfr6 libmpc3 zlib1g \
    && rm -rf /var/lib/apt/lists/*
ARG PREFIX=/opt/cegcc
COPY --from=gcc ${PREFIX} ${PREFIX}
ENV PATH=${PREFIX}/bin:$PATH

FROM image AS test
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 qemu-user \
    && rm -rf /var/lib/apt/lists/*
ARG TARGET=arm-mingw32ce
WORKDIR /src
COPY docs docs
COPY tests tests
# sh4: binutils test, ABI test against the Microsoft compiler's code under
# qemu-sh4 and its mutation control, FPU thunk test, the qemu hello test
# (Gates 3, 3b), and the runtime libraries' coredll imports against the
# CE 5.0 SDK list.
RUN if [ "${TARGET}" = sh4-mingw32ce ]; then \
        tests/gcc16/sh4-abi-gate.sh /tmp/abi \
        && tests/sh4/prog/run-hello.sh /tmp/hello \
        && tests/gcc16/runtime-imports.sh ${TARGET} /tmp/rt; fi
RUN tests/gcc16/build.sh ${TARGET} /out \
    && cd /out && sha256sum *.exe | tee SHA256SUMS

FROM scratch AS programs
COPY --from=test /out/ /

FROM image
