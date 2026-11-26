#!/usr/bin/env python3
"""Write a copy of an Orbis executable with a different SDK version in its
process parameters (PT_SCE_PROCPARAM, 'ORBI' magic, u32 at +0x10).

Lets a test build launch on firmware older than the SDK it was linked with.
It does not make missing library functions available: imports that the
older firmware lacks still fail to resolve at load time.
"""
import struct
import sys

PT_SCE_PROCPARAM = 0x61000001


def main():
    if len(sys.argv) != 4:
        print("usage: patch_sdk_version.py in.elf out.elf 0xVERSION", file=sys.stderr)
        return 2
    src, dst, ver = sys.argv[1], sys.argv[2], int(sys.argv[3], 0)
    data = bytearray(open(src, "rb").read())
    if data[:4] != b"\x7fELF":
        print("not an ELF", file=sys.stderr)
        return 1
    phoff, = struct.unpack_from("<Q", data, 32)
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    for i in range(phnum):
        p_type, _, p_offset = struct.unpack_from("<IIQ", data, phoff + i * phentsize)
        if p_type != PT_SCE_PROCPARAM:
            continue
        if data[p_offset + 8:p_offset + 12] != b"ORBI":
            print("process param magic missing", file=sys.stderr)
            return 1
        old, = struct.unpack_from("<I", data, p_offset + 0x10)
        struct.pack_into("<I", data, p_offset + 0x10, ver)
        open(dst, "wb").write(data)
        print("sdk_version 0x%08x -> 0x%08x (%s)" % (old, ver, dst))
        return 0
    print("no PT_SCE_PROCPARAM segment", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
