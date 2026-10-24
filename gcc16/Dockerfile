# CeGCC on newer GCC (docs/gcc16): toolchains for one or more targets in
# one prefix (/opt/cegcc), from source trees given by repository and
# commit, built with this repository's build_cf.sh, plus the test programs.
#
# Defaults: both targets (TARGETS), the CeGCC 16.2 trees (gcc ce-16.2.0,
# binutils ce-2.43.1, mingwrt ce-16), --disable-threads. From the repository root:
#
#   docker build -f gcc16/Dockerfile --target programs \
#       --output type=local,dest=out .        # tests, test program EXEs
#   docker build -f gcc16/Dockerfile -t cegcc:16 .   # the image
#
# Stages: binutils -> gcc -> image; test runs the sh4 tests (sh4 only)
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
ARG BINUTILS_REPO=https://github.com/palian-tech/binutils-gdb
ARG BINUTILS_COMMIT=a9e430957907734c3fb5329b3f93dcc68439a5f7
ARG BINUTILS_FLAGS="--disable-gdb --disable-gdbserver --disable-sim"
ARG TARGETS="arm-mingw32ce sh4-mingw32ce"
ARG PREFIX=/opt/cegcc
RUN fetch-commit binutils ${BINUTILS_REPO} ${BINUTILS_COMMIT}
COPY build_cf.sh ./
RUN for t in ${TARGETS}; do \
        BINUTILS_FLAGS="${BINUTILS_FLAGS}" ./build_cf.sh ${PREFIX} $t binutils \
            > /src/binutils-$t.log 2>&1 \
            || { tail -n 60 /src/binutils-$t.log; exit 1; }; \
        tail -n 3 /src/binutils-$t.log; \
    done; rm -rf build-* binutils

FROM binutils AS gcc
ARG GCC_REPO=https://github.com/palian-tech/gcc
ARG GCC_COMMIT=f76b763b701136e3149975bfae267c372588419b
ARG MINGW_REPO=https://github.com/palian-tech/mingwrt
ARG MINGW_COMMIT=59474758ca835bc3e7d0c4561d2f1e5553993b2a
ARG W32API_REPO=https://github.com/palian-tech/w32api
ARG W32API_COMMIT=9a0d7c127dac922cb42b770573ce1260a40e8f0e
ARG GCC_THREADS=no
ARG TARGETS="arm-mingw32ce sh4-mingw32ce"
ARG PREFIX=/opt/cegcc
RUN fetch-commit mingw ${MINGW_REPO} ${MINGW_COMMIT} \
    && fetch-commit w32api ${W32API_REPO} ${W32API_COMMIT}
RUN fetch-commit gcc ${GCC_REPO} ${GCC_COMMIT}
RUN for t in ${TARGETS}; do \
        GCC_THREADS=${GCC_THREADS} ./build_cf.sh ${PREFIX} $t gcc \
            > /src/gcc-$t.log 2>&1 \
            || { tail -n 80 /src/gcc-$t.log; exit 1; }; \
        tail -n 3 /src/gcc-$t.log; \
    done; rm -rf build-*

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
ARG TARGETS="arm-mingw32ce sh4-mingw32ce"
WORKDIR /src
COPY docs docs
COPY tests tests
# sh4: binutils test, ABI test against the Microsoft compiler's code under
# qemu-sh4 and its mutation control, FPU thunk test, -m4-single ABI test
# against the Microsoft /Qsh4 code and its mutation controls, the qemu
# hello test (Gates 3, 3b, 5), and the runtime libraries' coredll imports
# against the CE 5.0 SDK list.
RUN case " ${TARGETS} " in *" sh4-mingw32ce "*) \
        tests/gcc16/sh4-abi-gate.sh /tmp/abi \
        && tests/sh4/prog/run-hello.sh /tmp/hello \
        && tests/gcc16/runtime-imports.sh sh4-mingw32ce /tmp/rt;; esac
RUN for t in ${TARGETS}; do tests/gcc16/build.sh $t /out || exit 1; done \
    && cd /out && sha256sum *.exe | tee SHA256SUMS

FROM scratch AS programs
COPY --from=test /out/ /

FROM image
