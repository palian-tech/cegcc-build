This is a fork of https://github.com/MaxKellermann/cegcc-build to build a CeGCC 9.3.0 toolchain.
Unlike Max Kellermann's repo, this one also targets x86 Windows CE devices.

Clone this repository using::

 git clone git://github.com/enlyze/cegcc-build

To build::

 cd cegcc-build
 git submodule update --init
 ./build_cf.sh /where/to/install i386-mingw32ce

Alternatively, you can also build an arm-mingw32ce toolchain.
This version builds up on Max Kellermann's work and enables some more modern C++ features and Windows CE API definitions.

Docker image (arm-mingw32ce and sh4-mingw32ce)
==============================================

The ``sh4-mingw32ce`` branch publishes an image with both Windows CE
targets in ``/opt/cegcc`` (on ``PATH``):

* ``ghcr.io/palian-tech/cegcc:edge`` -- every push to ``sh4-mingw32ce``
* ``ghcr.io/palian-tech/cegcc:v9.3.0-ce.N`` and ``:latest`` -- release tags

Pull it and compile for each target::

 docker pull ghcr.io/palian-tech/cegcc:edge
 docker run --rm -v "$PWD":/w -w /w ghcr.io/palian-tech/cegcc:edge \
     sh4-mingw32ce-gcc -O2 -D_WIN32_WCE=0x500 hello.c -o hello-sh4.exe
 docker run --rm -v "$PWD":/w -w /w ghcr.io/palian-tech/cegcc:edge \
     arm-mingw32ce-gcc -O2 -D_WIN32_WCE=0x500 hello.c -o hello-arm.exe

``sh4-mingw32ce`` produces Windows CE 5.00 SH-4 images (PE machine 0x01A6,
image base 0x10000) following the calling convention of the Microsoft SH-4
compiler; floating point is software only (``-m4-nofpu``). Link extra
libraries as usual, e.g. ``-lcommctrl`` or ``-lceddk``.

To build the image yourself::

 git submodule update --init
 docker build --network host -t cegcc .

See ``docs/sh4/STATUS.md`` for the build steps, tests and current status.
