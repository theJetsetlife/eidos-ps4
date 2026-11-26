#!/usr/bin/env python3
"""Check a fake-signed SELF against the ELF it was made from.

Confirms the SELF magic, the embedded ELF header and program headers, the
ELF digest in the extended info, and that every data entry carries the bytes
of its source segment. It reads only the two files and prints a summary.
"""
import argparse
import hashlib
import struct
import sys

SELF_MAGIC = b"\x4F\x15\x3D\x1D"
COMMON = "<4s4B"
EXT = "<I2HQ2H4x"
ENTRY = "<4Q"
EX_INFO = "<4Q32s"
PT_NAMES = {1: "LOAD", 0x61000010: "SCE_RELRO", 0x61000000: "SCE_DYNLIBDATA",
            0x6FFFFF00: "SCE_COMMENT"}


def align_up(x, a):
    return (x + a - 1) & ~(a - 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("self_file")
    ap.add_argument("--expect-paid", type=lambda v: int(v, 0))
    a = ap.parse_args()

    elf = open(a.elf, "rb").read()
    sf = open(a.self_file, "rb").read()
    errors = []

    magic, *_ = struct.unpack_from(COMMON, sf, 0)
    if magic != SELF_MAGIC:
        print("FAIL: not a SELF (bad magic)")
        return 1
    off = struct.calcsize(COMMON)
    key_type, hdr_size, meta_size, file_size, n_entries, flags = struct.unpack_from(EXT, sf, off)
    off += struct.calcsize(EXT)
    entries = [struct.unpack_from(ENTRY, sf, off + i * 32) for i in range(n_entries)]
    off += n_entries * 32

    if elf[:4] != b"\x7fELF":
        print("FAIL: input is not an ELF")
        return 1
    e_type, = struct.unpack_from("<H", elf, 16)
    phoff, = struct.unpack_from("<Q", elf, 32)
    phentsize, phnum = struct.unpack_from("<HH", elf, 54)
    ehsize, = struct.unpack_from("<H", elf, 52)
    hdr_len = max(ehsize, phoff + phentsize * phnum)
    if sf[off:off + hdr_len] != elf[:hdr_len]:
        # the writer drops section headers, so compare ELF header minus shoff/shnum fields
        if sf[off:off + 40] != elf[:40] or sf[off + 64:off + hdr_len] != elf[64:hdr_len]:
            errors.append("embedded ELF/program headers differ from input")
    ex_off = off + align_up(hdr_len, 16)
    paid, ptype, app_ver, fw_ver, digest = struct.unpack_from(EX_INFO, sf, ex_off)
    if digest != hashlib.sha256(elf).digest():
        errors.append("extended-info digest is not SHA-256 of the input ELF")
    if a.expect_paid is not None and paid != a.expect_paid:
        errors.append("paid 0x%x != expected 0x%x" % (paid, a.expect_paid))
    if file_size > len(sf):
        errors.append("header file_size %d exceeds file length %d" % (file_size, len(sf)))

    phdrs = [struct.unpack_from("<IIQQQQQQ", elf, phoff + i * phentsize) for i in range(phnum)]
    checked = 0
    for props, eoff, filesz, memsz in entries:
        seg_idx = (props >> 20) & 0xFFFF
        if (props >> 16) & 1:               # has_digests: meta entry for the next data entry
            continue
        if (props >> 1) & 1 or (props >> 3) & 1:
            errors.append("entry for segment %d is encrypted or compressed" % seg_idx)
            continue
        if seg_idx >= phnum:
            errors.append("entry points at segment %d of %d" % (seg_idx, phnum))
            continue
        p_type, _, p_offset, _, _, p_filesz, _, _ = phdrs[seg_idx]
        if filesz != p_filesz or sf[eoff:eoff + filesz] != elf[p_offset:p_offset + p_filesz]:
            errors.append("segment #%d (%s) data mismatch" % (seg_idx, PT_NAMES.get(p_type, hex(p_type))))
        checked += 1

    print("SELF  %s" % a.self_file)
    print("  key_type=0x%x entries=%d header=%d meta=%d file_size=%d" % (key_type, n_entries, hdr_size, meta_size, file_size))
    print("  elf e_type=0x%x paid=0x%x ptype=%d app_ver=0x%x fw_ver=0x%x" % (e_type, paid, ptype, app_ver, fw_ver))
    print("  data segments verified: %d" % checked)
    if checked == 0:
        errors.append("no data segment entries found")
    for e in errors:
        print("FAIL: " + e)
    if not errors:
        print("OK")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
