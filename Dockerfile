# Reproducible CeGCC toolchain image.
#
#   docker build --network host -t cegcc .
#   docker build --build-arg TARGETS="arm-mingw32ce sh4-mingw32ce" -t cegcc .
#
# The submodules must be checked out (git submodule update --init) before
# building; the build context is this repository including them.

# ubuntu:24.04, pinned by digest (docker.io/library/ubuntu, same digest on
# mirror.gcr.io/library/ubuntu).
ARG BASE_IMAGE=ubuntu:24.04@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac

FROM ${BASE_IMAGE} AS build
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential flex bison texinfo file \
        libgmp-dev libmpfr-dev libmpc-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
ARG TARGETS="arm-mingw32ce"
ARG PREFIX=/opt/cegcc
RUN set -e; mkdir -p ${PREFIX}; for t in ${TARGETS}; do \
        ./build_cf.sh ${PREFIX}/$t $t > /src/build-$t.log 2>&1 \
            || { tail -n 60 /src/build-$t.log; exit 1; }; \
        tail -n 3 /src/build-$t.log; \
    done

FROM ${BASE_IMAGE}
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        make git libgmp10 libmpfr6 libmpc3 zlib1g ca-certificates \
    && rm -rf /var/lib/apt/lists/*
ARG TARGETS="arm-mingw32ce"
ARG PREFIX=/opt/cegcc
COPY --from=build ${PREFIX} ${PREFIX}
RUN for t in ${TARGETS}; do ln -s ${PREFIX}/$t/bin/$t-* /usr/local/bin/; done
