CeGCC for Windows CE on ARM and SH-4
====================================

A CeGCC 9.3.0 toolchain (GCC 9.3, binutils 2.34, mingwrt, w32api) for
Windows CE, with two targets built into one prefix:

* ``arm-mingw32ce`` -- ARM Windows CE.
* ``sh4-mingw32ce`` -- Hitachi/Renesas SH-4 Windows CE 5.0, following the
  calling convention of the Microsoft SH-4 compiler (eVC4). The convention is
  tested by linking real Microsoft-compiled code with GCC code and calling in
  both directions (``docs/sh4/ABI.md``).

It is published as a Docker image (below), so Windows CE programs for both
CPUs can be built in CI without proprietary tools.

Based on `enlyze/cegcc-build <https://github.com/enlyze/cegcc-build>`_ and
`MaxKellermann/cegcc-build <https://github.com/MaxKellermann/cegcc-build>`_;
``i386-mingw32ce`` from the ENLYZE fork still builds the same way.

Building from source::

 git clone https://github.com/palian-tech/cegcc-build
 cd cegcc-build
 git submodule update --init
 ./build_cf.sh /where/to/install arm-mingw32ce
 ./build_cf.sh /where/to/install sh4-mingw32ce

Docker images (arm-mingw32ce and sh4-mingw32ce)
================================================

Both Windows CE targets are in ``/opt/cegcc`` (on ``PATH``) of every
image. The images are published on ghcr.io as
``ghcr.io/palian-tech/cegcc`` and on Docker Hub as ``paliantech/cegcc``:

========================  ===================================================
Tag                       Content
========================  ===================================================
``v16.2.0-ce.1``          GCC 16.2, binutils 2.43.1 (branch ``gcc16``,
                          ``docs/gcc16``). C++20 and C++23 for both targets;
                          SH-4 calls into coredll's floating-point functions
                          through thunks (``docs/sh4/ABI.md``). No threads
                          (``--disable-threads``); SH-4 floating point in
                          software (``-m4-nofpu``); debug info in linked
                          images not usable yet
``v9.3.0-ce.3``           GCC 9.3, binutils 2.34 (branch ``sh4-mingw32ce``):
                          SH-4 calls into coredll's floating-point functions
                          through thunks, so ``printf`` ``%f`` and coredll
                          math work on SH-4
``v9.3.0-ce.2``           as ``v9.3.0-ce.1``, also on Docker Hub
``v9.3.0-ce.1``           GCC 9.3, binutils 2.34, first release with
                          ``sh4-mingw32ce``
``latest``                the newest ``v9.3.0-ce.N``
``edge``                  every push to ``sh4-mingw32ce`` (ghcr.io only)
========================  ===================================================

Pull one and compile for each target::

 docker pull ghcr.io/palian-tech/cegcc:v16.2.0-ce.1
 docker run --rm -v "$PWD":/w -w /w ghcr.io/palian-tech/cegcc:v16.2.0-ce.1 \
     sh4-mingw32ce-g++ -std=c++20 -O2 -D_WIN32_WCE=0x500 hello.cpp -o hello-sh4.exe
 docker run --rm -v "$PWD":/w -w /w ghcr.io/palian-tech/cegcc:v16.2.0-ce.1 \
     arm-mingw32ce-g++ -std=c++20 -O2 -D_WIN32_WCE=0x500 hello.cpp -o hello-arm.exe

``sh4-mingw32ce`` produces Windows CE 5.00 SH-4 images (PE machine 0x01A6,
image base 0x10000) following the calling convention of the Microsoft SH-4
compiler; floating point is software only (``-m4-nofpu``). Link extra
libraries as usual, e.g. ``-lcommctrl`` or ``-lceddk``.

To build an image yourself::

 git submodule update --init
 docker build --network host -t cegcc .                    # 9.3
 docker build --network host -f gcc16/Dockerfile -t cegcc:16 .   # 16.2

See ``docs/sh4/STATUS.md`` (9.3) and ``docs/gcc16/STATUS.md`` (16.2) for
the build steps, tests and current status.
