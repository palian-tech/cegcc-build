CeGCC for Windows CE on ARM and SH-4
====================================

A CeGCC 9.3.0 toolchain (GCC 9.3, binutils 2.34, mingwrt, w32api) for
Windows CE, with two targets built into one prefix:

* ``arm-mingw32ce`` -- ARM Windows CE.
* ``sh4-mingw32ce`` -- Hitachi/Renesas SH-4 Windows CE 5.0, following the
  calling convention of the Microsoft SH-4 compiler (eVC4). The convention is
  tested by linking real Microsoft-compiled code with GCC code and calling in
  both directions (`docs/sh4-abi.md <docs/sh4-abi.md>`_).

It is published as a Docker image (below), so Windows CE programs for both
CPUs can be built in CI without proprietary tools.

Based on `enlyze/cegcc-build <https://github.com/enlyze/cegcc-build>`_ and
`MaxKellermann/cegcc-build <https://github.com/MaxKellermann/cegcc-build>`_;
``i386-mingw32ce`` from the ENLYZE fork still builds the same way.

Building from source: the source trees are submodules, pinned to the
commits a release builds from (``.gitmodules`` names the fork branches of
this line: ``ce/gcc-9.3.0``, ``ce/binutils-2.34``, ``ce/mingwrt-9``,
``ce/w32api``)::

 git clone --recurse-submodules --shallow-submodules \
     -b releases/gcc-9 https://github.com/palian-tech/cegcc-build
 cd cegcc-build
 ./build_cf.sh /where/to/install arm-mingw32ce
 ./build_cf.sh /where/to/install sh4-mingw32ce

Each release tag ``vX`` also tags the commits it builds from in the four
forks as ``cegcc-vX`` (``.github/scripts/tag-forks.sh``, run by the
release workflow with the secret ``FORKS_TOKEN``).

Docker image (arm-mingw32ce and sh4-mingw32ce)
==============================================

The ``releases/gcc-9`` branch (the GCC 9.3 line, fixes only; formerly
``sh4-mingw32ce``) publishes an image with both Windows CE
targets in ``/opt/cegcc`` (on ``PATH``):

* ``ghcr.io/palian-tech/cegcc:v9.3.0-ce.N`` -- release tags
* ``ghcr.io/palian-tech/cegcc:gcc9`` -- the newest ``v9.3.0-ce.N``
* ``ghcr.io/palian-tech/cegcc:gcc9-edge`` -- every push to ``releases/gcc-9``

``:latest`` and ``:edge`` follow the newest line (GCC 16.2, branch
``releases/gcc-16``); ``:latest`` pointed to 9.3 up to ``v9.3.0-ce.3``.

The same tags are published on Docker Hub as ``paliantech/cegcc``.

Pull it and compile for each target::

 docker pull ghcr.io/palian-tech/cegcc:gcc9
 docker run --rm -v "$PWD":/w -w /w ghcr.io/palian-tech/cegcc:gcc9 \
     sh4-mingw32ce-gcc -O2 -D_WIN32_WCE=0x500 hello.c -o hello-sh4.exe
 docker run --rm -v "$PWD":/w -w /w ghcr.io/palian-tech/cegcc:gcc9 \
     arm-mingw32ce-gcc -O2 -D_WIN32_WCE=0x500 hello.c -o hello-arm.exe

``sh4-mingw32ce`` produces Windows CE 5.00 SH-4 images (PE machine 0x01A6,
image base 0x10000) following the calling convention of the Microsoft SH-4
compiler; floating point is software only (``-m4-nofpu``). Link extra
libraries as usual, e.g. ``-lcommctrl`` or ``-lceddk``.

To build the image yourself, in that checkout::

 docker build --network host -t cegcc .

The image build runs the tests in ``tests/sh4`` before it is published:

* ``binutils-test.sh`` -- SH-4 assembler and linker checks.
* ``abi/run-abi-test.sh`` -- GCC code linked with the Microsoft SH-4
  compiler's code (listings in ``tests/sh4/abi/ref``) and run under
  qemu-sh4, calls in both directions.
* ``fpu/run-fpu-test.sh`` -- coredll floating-point thunks against
  ``/Qsh4`` code under qemu-sh4.
* ``prog/run-hello.sh`` and ``prog/build.sh`` -- test programs for both
  targets, PE headers and coredll imports checked against
  ``tests/sh4/coredll``.

Status and open items of this line: `issue #14
<https://github.com/palian-tech/cegcc-build/issues/14>`_ (16.2 line: `issue
#15 <https://github.com/palian-tech/cegcc-build/issues/15>`_).
