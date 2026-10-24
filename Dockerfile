# CeGCC toolchain image: arm-mingw32ce and sh4-mingw32ce in /opt/cegcc.
#
#   git submodule update --init
#   docker build --network host -t cegcc .
#
# Other stages:
#   --target test       also runs the sh4 binutils, ABI and qemu tests and
#                       builds the test programs of tests/sh4/prog for
#                       every target
#   --target programs   only those programs, with SHA256SUMS
#                       (use with --output type=local,dest=DIR)
#
# The build context is this repository including the submodules. Each
# submodule is copied in its own layer, so a GCC-only change does not
# rebuild binutils.

# ubuntu:24.04, pinned by digest (docker.io/library/ubuntu, same digest on
# mirror.gcr.io/library/ubuntu).
ARG BASE_IMAGE=ubuntu:24.04@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac

FROM ${BASE_IMAGE} AS build-base
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential flex bison texinfo file \
        libgmp-dev libmpfr-dev libmpc-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY build_cf.sh ./

FROM build-base AS binutils
ARG TARGETS="arm-mingw32ce sh4-mingw32ce"
ARG PREFIX=/opt/cegcc
COPY binutils binutils
RUN set -e; for t in ${TARGETS}; do \
        ./build_cf.sh ${PREFIX} $t binutils > /src/binutils-$t.log 2>&1 \
            || { tail -n 60 /src/binutils-$t.log; exit 1; }; \
        tail -n 3 /src/binutils-$t.log; \
    done; rm -rf build-*

FROM binutils AS gcc
ARG TARGETS="arm-mingw32ce sh4-mingw32ce"
ARG PREFIX=/opt/cegcc
COPY mingw mingw
COPY w32api w32api
COPY gcc gcc
RUN set -e; for t in ${TARGETS}; do \
        ./build_cf.sh ${PREFIX} $t gcc > /src/gcc-$t.log 2>&1 \
            || { tail -n 60 /src/gcc-$t.log; exit 1; }; \
        tail -n 3 /src/gcc-$t.log; \
    done; rm -rf build-*

FROM ${BASE_IMAGE} AS image
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        make git libgmp10 libmpfr6 libmpc3 zlib1g ca-certificates \
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
RUN set -e; \
    tests/sh4/binutils-test.sh; \
    tests/sh4/abi/run-abi-test.sh /tmp/abi; \
    tests/sh4/prog/run-hello.sh /tmp/hello; \
    tests/sh4/fpu/run-fpu-test.sh /tmp/fpu; \
    for t in ${TARGETS}; do tests/sh4/prog/build.sh $t /out; done; \
    cd /out && sha256sum *.exe | tee SHA256SUMS

FROM scratch AS programs
COPY --from=test /out/ /

FROM image
