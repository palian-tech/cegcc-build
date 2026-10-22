#!/usr/bin/env python3
"""Wrap the sections of a PE image into a static ELF executable for
qemu-sh4 (Linux user mode), so that test code linked by sh4-mingw32ce-ld
can run without Windows CE.  Each section becomes a PT_LOAD segment at
ImageBase + VirtualAddress; the entry point is kept.

    pe2elf.py IN.exe OUT.elf
"""
import struct
import sys

d = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
assert d[pe:pe + 4] == b'PE\0\0'
machine, nsec, _, _, _, optsize, _ = struct.unpack_from('<HHIIIHH', d, pe + 4)
assert machine == 0x1a6, hex(machine)
opt = pe + 24
entry = struct.unpack_from('<I', d, opt + 16)[0]
base = struct.unpack_from('<I', d, opt + 28)[0]
segs = []
for i in range(nsec):
    s = opt + optsize + 40 * i
    vsize, va, rawsize, rawptr = struct.unpack_from('<IIII', d, s + 8)
    data = d[rawptr:rawptr + min(rawsize, vsize or rawsize)]
    segs.append((base + va, data, max(vsize, len(data))))

ehsize, phsize = 52, 32
off = ehsize + phsize * len(segs)
phdrs = b''
body = b''
for vaddr, data, memsz in segs:
    pad = (vaddr - off - len(body)) % 0x1000
    body += b'\0' * pad
    foff = off + len(body)
    # PT_LOAD, flags RWX, alignment 0x1000.
    phdrs += struct.pack('<IIIIIIII', 1, foff, vaddr, vaddr, len(data),
                         memsz, 7, 0x1000)
    body += data
ident = b'\x7fELF\x01\x01\x01' + b'\0' * 9
# ET_EXEC, EM_SH (42).
hdr = ident + struct.pack('<HHIIIIIHHHHHH', 2, 42, 1, base + entry, ehsize, 0,
                          0, ehsize, phsize, len(segs), 40, 0, 0)
open(sys.argv[2], 'wb').write(hdr + phdrs + body)
