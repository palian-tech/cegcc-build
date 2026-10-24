CeGCC for Windows CE on ARM and SH-4
====================================

CeGCC toolchains (GCC, binutils, mingwrt, w32api) for Windows CE, with two
targets built into one prefix:

* ``arm-mingw32ce`` -- ARM Windows CE.
* ``sh4-mingw32ce`` -- Hitachi/Renesas SH-4 Windows CE 5.0, following the
  calling convention of the Microsoft SH-4 compiler (eVC4). The convention is
  tested by linking real Microsoft-compiled code with GCC code and calling in
  both directions (``docs/sh4/ABI.md``).

They are published as Docker images (below), so Windows CE programs for
both CPUs can be built in CI without proprietary tools.

Based on `enlyze/cegcc-build <https://github.com/enlyze/cegcc-build>`_,
`MaxKellermann/cegcc-build <https://github.com/MaxKellermann/cegcc-build>`_
and salman-javed-nz's GCC 14.2 port for ARM Windows CE.

Branches and tags
=================

=====================  ======================================================
Branch                 Use
=====================  ======================================================
``releases/gcc-16``    GCC 16.2, binutils 2.43.1 (``docs/gcc16``). Default
                       branch, new work goes here. Release tags
                       ``v16.2.0-ce.N``
``releases/gcc-9``     GCC 9.3, binutils 2.34 (``docs/sh4``). Maintenance
                       line, fixes only. Release tags ``v9.3.0-ce.N``
=====================  ======================================================

Use a ``v16.2.0-ce.N`` image for new code (C++20/C++23); use a
``v9.3.0-ce.N`` image to keep building with the 9.3 compiler.

Docker images (arm-mingw32ce and sh4-mingw32ce)
===============================================

Both Windows CE targets are in ``/opt/cegcc`` (on ``PATH``) of every
image. The images are published on ghcr.io as
``ghcr.io/palian-tech/cegcc`` and on Docker Hub as ``paliantech/cegcc``:

========================  ===================================================
Tag                       Content
========================  ===================================================
``v16.2.0-ce.1``          GCC 16.2, binutils 2.43.1. C++20 and C++23 for both
                          targets; SH-4 calls into coredll's floating-point
                          functions through thunks (``docs/sh4/ABI.md``). No
                          threads (``--disable-threads``); SH-4 floating
                          point in software (``-m4-nofpu``); debug info in
                          linked images not usable yet
``v9.3.0-ce.3``           GCC 9.3, binutils 2.34: SH-4 calls into coredll's
                          floating-point functions through thunks, so
                          ``printf`` ``%f`` and coredll math work on SH-4
``v9.3.0-ce.2``           as ``v9.3.0-ce.1``, also on Docker Hub
``v9.3.0-ce.1``           GCC 9.3, binutils 2.34, first release with
                          ``sh4-mingw32ce``
``latest``                the newest ``v16.2.0-ce.N`` (up to
                          ``v9.3.0-ce.3`` it was the newest 9.3 release)
``edge``                  every push to ``releases/gcc-16`` (GCC 16.2)
``gcc9``                  the newest ``v9.3.0-ce.N``
``gcc9-edge``             every push to ``releases/gcc-9``
========================  ===================================================

Pull one and compile for each target::

 docker pull ghcr.io/palian-tech/cegcc:v16.2.0-ce.1
 docker run --rm -v "$PWD":/w -w /w ghcr.io/palian-tech/cegcc:v16.2.0-ce.1 \
     sh4-mingw32ce-g++ -std=c++20 -O2 -D_WIN32_WCE=0x500 hello.cpp -o hello-sh4.exe
 docker run --rm -v "$PWD":/w -w /w ghcr.io/palian-tech/cegcc:v16.2.0-ce.1 \
     arm-mingw32ce-g++ -std=c++20 -O2 -D_WIN32_WCE=0x500 hello.cpp -o hello-arm.exe

``sh4-mingw32ce`` produces Windows CE 5.00 SH-4 images (PE machine 0x01A6,
image base 0x10000) following the calling convention of the Microsoft SH-4
compiler. Floating point is in software by default (``-m4-nofpu``); since
GCC 16.2 ``-m4-single`` uses the SH-4 FPU with the calling convention of
the Microsoft compiler's ``/Qsh4`` (``docs/sh4/ABI.md``), in the next
``v16.2.0-ce.N`` and in ``edge``. Link extra libraries as usual, e.g.
``-lcommctrl`` or ``-lceddk``.

Building from source
====================

GCC 16.2 (``releases/gcc-16``): the ``Dockerfile`` fetches the trees by commit
(``palian-tech/gcc`` ``ce-16.2.0``, ``binutils-gdb`` ``ce-2.43.1``,
``mingwrt`` and ``w32api`` ``ce-16``) and builds both targets with
``build_cf.sh``::

 docker build --network host -t cegcc:16 .

GCC 9.3 (``releases/gcc-9``, its own ``Dockerfile``): the trees are
submodules::

 git clone -b releases/gcc-9 https://github.com/palian-tech/cegcc-build
 cd cegcc-build
 git submodule update --init
 ./build_cf.sh /where/to/install arm-mingw32ce
 ./build_cf.sh /where/to/install sh4-mingw32ce

or ``docker build --network host -t cegcc .`` there.

See ``docs/gcc16/STATUS.md`` (16.2) and ``docs/sh4/STATUS.md`` (9.3) for
the build steps, tests and current status.
