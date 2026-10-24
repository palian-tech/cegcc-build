CeGCC for Windows CE on ARM and SH-4
====================================

CeGCC toolchains (GCC, binutils, mingwrt, w32api) for Windows CE, with two
targets built into one prefix:

* ``arm-mingw32ce`` -- ARM Windows CE.
* ``sh4-mingw32ce`` -- Hitachi/Renesas SH-4 Windows CE 5.0, following the
  calling convention of the Microsoft SH-4 compiler (eVC4). The convention is
  tested by linking real Microsoft-compiled code with GCC code and calling in
  both directions (``docs/sh4-abi.md``).

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
``releases/gcc-16``    GCC 16.2, binutils 2.43.1. Default
                       branch, new work goes here. Release tags
                       ``v16.2.0-ce.N``
``releases/gcc-9``     GCC 9.3, binutils 2.34. Maintenance
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
                          functions through thunks (``docs/sh4-abi.md``). No
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
the Microsoft compiler's ``/Qsh4`` (``docs/sh4-abi.md``), in the next
``v16.2.0-ce.N`` and in ``edge``. Link extra libraries as usual, e.g.
``-lcommctrl`` or ``-lceddk``.

Building from source
====================

The source trees (``palian-tech/gcc``, ``binutils-gdb``, ``mingwrt``,
``w32api``) are submodules on both lines, pinned to the commits a release
builds from; ``.gitmodules`` names the fork branch of each line
(``ce/gcc-16.2.0``, ``ce/binutils-2.43.1``, ``ce/mingwrt-16``, ``ce/w32api``
on ``releases/gcc-16``; ``ce/gcc-9.3.0``, ``ce/binutils-2.34``,
``ce/mingwrt-9``, ``ce/w32api`` on ``releases/gcc-9``)::

 git clone --recurse-submodules --shallow-submodules \
     -b releases/gcc-16 https://github.com/palian-tech/cegcc-build
 cd cegcc-build
 docker build --network host -t cegcc .                     # the image
 docker build --network host --target programs \
     --output type=local,dest=out .                          # tests, test EXEs

or without Docker, as the 16.2 ``Dockerfile`` does it::

 export GCC_THREADS=no BINUTILS_FLAGS="--disable-gdb --disable-gdbserver --disable-sim"
 ./build_cf.sh /where/to/install arm-mingw32ce
 ./build_cf.sh /where/to/install sh4-mingw32ce

Use ``-b releases/gcc-9`` for the 9.3 line (there plain
``./build_cf.sh /where/to/install TARGET``).

Each release tag ``vX`` also tags the commits it builds from in the four
forks as ``cegcc-vX`` (``.github/scripts/tag-forks.sh``, run by the
release workflow with the secret ``FORKS_TOKEN``).

Tests
=====

The Docker ``test`` stage runs them all; each prints ``PASS``. With the
tools on ``PATH`` (``python3`` and ``qemu-sh4`` for the SH-4 ones):

* ``tests/gcc16/sh4-abi-gate.sh``: SH-4 binutils test, the calling
  convention against the Microsoft compiler's code
  (``tests/sh4/abi/ref``) under qemu-sh4 in both call directions, the
  coredll floating-point thunks, ``-m4-single`` against ``/Qsh4`` code,
  each with mutation controls that must fail.
* ``tests/sh4/prog/run-hello.sh``: ``hello.c`` under qemu-sh4 with real
  startup code and a stand-in coredll.
* ``tests/gcc16/runtime-imports.sh``: the runtime libraries import only
  functions of the CE 5.0 SDK's coredll (``tests/sh4/coredll``).
* ``tests/gcc16/build.sh TARGET OUTDIR``: builds ``cetest`` (C++20 and
  C++23; on SH-4 also ``-m4-single``; and a ``-g`` C++20 build) and
  checks PE headers and coredll imports. ``cetest`` is the program to run
  on a device: one ``PASS``/``FAIL``/``XFAIL``/``SKIP`` line per check and
  a ``DONE`` line, in ``cetest.txt`` next to the EXE.
* ``tests/gcc16/gdb-check.sh TARGET OUTDIR``: the target's gdb reads the
  ``-g`` build: line info for ``WinMain`` and a check function, and the
  type and frame location of two of its locals.

Known limitations
=================

* Threads (``--enable-threads=win32``) use only coredll functions of
  Windows CE 5.0. Destructors of ``thread_local`` objects run when a
  ``std::thread`` ends and, for the main thread, at ``exit``; a thread
  started with ``CreateThread`` does not run them. ``std::timed_mutex``
  is built from a condition variable. ``std::chrono::steady_clock`` and
  timed waits count milliseconds of ``GetTickCount``; ``system_clock``
  is the system time advanced by ``GetTickCount`` between its seconds.
* Debug info: ``-g`` images carry DWARF that the image's
  ``arm-mingw32ce-gdb`` and ``sh4-mingw32ce-gdb`` read (link without
  ``-s``). There is no gdbserver for Windows CE; attach gdb to an
  emulator's gdb stub. On ARM, an 8-byte local (``double``,
  ``long long``) in a function whose frame GCC realigns has no location
  at ``-O0`` outside its first use.
* Exceptions are SJLJ and the runtime is linked statically, so an
  exception cannot cross a DLL boundary.
* ``dllimport`` is ignored on SH-4; use ``__imp_`` pointers for data
  exported by DLLs.

Status and plans: the issues `CeGCC 16.2 status
<https://github.com/palian-tech/cegcc-build/issues/15>`_ and `CeGCC 9.3
status <https://github.com/palian-tech/cegcc-build/issues/14>`_.
