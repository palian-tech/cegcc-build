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

Docker image (arm-mingw32ce and sh4-mingw32ce)
==============================================

The ``releases/gcc-9`` branch (the GCC 9.3 line, fixes only; formerly
``sh4-mingw32ce``) publishes an image with both Windows CE
targets in ``/opt/cegcc`` (on ``PATH``):

* ``ghcr.io/palian-tech/cegcc:v9.3.0-ce.N`` -- release tags
* ``ghcr.io/palian-tech/cegcc:gcc9`` -- the newest ``v9.3.0-ce.N``
* ``ghcr.io/palian-tech/cegcc:gcc9-edge`` -- every push to ``releases/gcc-9``

``:latest`` and ``:edge`` follow the newest line (GCC 16.2, branch
``master``); ``:latest`` pointed to 9.3 up to ``v9.3.0-ce.3``.

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

To build the image yourself::

 git submodule update --init
 docker build --network host -t cegcc .

See ``docs/sh4/STATUS.md`` for the build steps, tests and current status.
