#!/usr/bin/env python3
"""A minimal gdb remote stub for tests/gcc16/gdb-check.sh.

  gdb-stub.py ARCH PC          (ARCH: arm or sh4; PC: hex address)

Speaks the remote protocol on stdin/stdout (gdb: target remote | ...),
like the gdb stub of an emulator: stopped with SIGTRAP at PC, all memory
reads as zeros, no qGetTIBAddr. For arm it sends an XML target
description with the core registers only (r0-r15, cpsr), as QEMU does,
which gdb can only use when it is built with expat; without it, gdb
expects the old FPA register layout and rejects the 'g' reply.
"""
import sys

ARCH = sys.argv[1]
PC = int(sys.argv[2], 16)

ARM_XML = """<?xml version="1.0"?>
<!DOCTYPE target SYSTEM "gdb-target.dtd">
<target>
  <architecture>arm</architecture>
  <feature name="org.gnu.gdb.arm.core">
""" + "".join(
    '    <reg name="r%d" bitsize="32"/>\n' % i for i in range(13)) + """\
    <reg name="sp" bitsize="32" type="data_ptr"/>
    <reg name="lr" bitsize="32"/>
    <reg name="pc" bitsize="32" type="code_ptr"/>
    <reg name="cpsr" bitsize="32" regnum="25"/>
  </feature>
</target>
"""


def le32(v):
    return v.to_bytes(4, "little").hex()


def regs():
    if ARCH == "arm":
        # r0-r12, sp, lr, pc, cpsr (user mode).
        return le32(0) * 13 + le32(0x00100000) + le32(0) + le32(PC) + le32(0x10)
    # sh4: r0-r15 (r15 = sp), pc, pr, gbr, vbr, mach, macl, sr; the rest
    # of gdb's SH-4 registers are left out (unavailable).
    return (le32(0) * 15 + le32(0x00100000) + le32(PC) + le32(0) * 5
            + le32(0x40000000))


def reply(pkt):
    if pkt.startswith("qSupported"):
        feats = "PacketSize=4000"
        if ARCH == "arm":
            feats += ";qXfer:features:read+"
        return feats
    if pkt.startswith("qXfer:features:read:target.xml:") and ARCH == "arm":
        off, length = (int(x, 16) for x in pkt.rsplit(":", 1)[1].split(","))
        chunk = ARM_XML[off:off + length]
        return ("l" if off + length >= len(ARM_XML) else "m") + chunk
    if pkt == "?":
        return "S05"
    if pkt == "g":
        return regs()
    if pkt.startswith("m"):
        length = int(pkt[1:].split(",")[1], 16)
        return "00" * length
    if pkt.startswith(("Hg", "Hc")) or pkt == "qSymbol::":
        return "OK"
    if pkt == "qAttached":
        return "1"
    if pkt.startswith("D"):
        return "OK"
    # qGetTIBAddr, p, Z0, vCont?, qC, qOffsets ...: not supported.
    return ""


def send(out, data):
    csum = sum(data.encode()) & 0xFF
    out.write(b"$%s#%02x" % (data.encode(), csum))
    out.flush()


def main():
    inp, out = sys.stdin.buffer, sys.stdout.buffer
    while True:
        c = inp.read(1)
        if not c:
            return
        if c == b"\x03":
            send(out, "S02")
            continue
        if c != b"$":
            continue  # acks
        data = b""
        while True:
            c = inp.read(1)
            if not c:
                return
            if c == b"#":
                break
            data += c
        inp.read(2)  # checksum
        out.write(b"+")
        pkt = data.decode()
        if pkt == "k":
            return
        send(out, reply(pkt))
        if pkt.startswith("D"):
            return


main()
