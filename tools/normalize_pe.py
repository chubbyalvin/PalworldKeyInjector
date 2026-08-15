#!/usr/bin/env python3
"""Normalize non-semantic PE metadata changed by GNU objcopy."""

from pathlib import Path
import struct
import sys


def normalize(path: Path) -> None:
    data = bytearray(path.read_bytes())
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("not a PE file")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError("PE signature missing")
    coff = pe_offset + 4
    optional = coff + 20
    if struct.unpack_from("<H", data, optional)[0] != 0x20B:
        raise ValueError("not PE32+")
    struct.pack_into("<I", data, coff + 4, 0)       # COFF TimeDateStamp
    struct.pack_into("<I", data, optional + 64, 0) # OptionalHeader.CheckSum
    path.write_bytes(data)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: normalize_pe.py <dll>")
    normalize(Path(sys.argv[1]))
