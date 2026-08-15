#!/usr/bin/env python3
"""Repair objcopy's x86-64 ELF-to-COFF PC-relative relocations.

The exact release cross-build compiles with the Windows x64 ABI but uses an
ELF-host GCC. GNU objcopy 2.42 converts R_X86_64_PC32 relocations to ADDR32 and
can lose their named-symbol association. This tool copies the symbol and
addend semantics from the original ELF relocation table into the converted
COFF object. Normal MSVC and MinGW builds do not use this tool.
"""

from pathlib import Path
import re
import struct
import subprocess
import sys

IMAGE_SCN_MEM_EXECUTE = 0x20000000
IMAGE_REL_AMD64_REL32 = 0x0004

RELOCATION = re.compile(
    r"^\s*([0-9a-fA-F]+)\s+\S+\s+"
    r"R_X86_64_(PC32|PLT32)\s+\S+\s+(\S+)"
    r"(?:\s+([+-])\s+([0-9a-fA-F]+))?\s*$"
)


def elf_text_relocations(path: Path):
    output = subprocess.check_output(
        ["readelf", "-rW", str(path)], text=True, encoding="utf-8")
    in_text = False
    result = {}
    for line in output.splitlines():
        if line.startswith("Relocation section"):
            in_text = "'.rela.text'" in line
            continue
        if not in_text:
            continue
        match = RELOCATION.match(line)
        if not match:
            continue
        offset = int(match.group(1), 16)
        kind = match.group(2)
        symbol = match.group(3)
        addend = 0
        if match.group(4):
            value = int(match.group(5), 16)
            addend = value if match.group(4) == "+" else -value
        result[offset] = (kind, symbol, addend)
    return result


def coff_name(data: bytearray, offset: int, string_table: int) -> str:
    raw = bytes(data[offset : offset + 8])
    zeroes, string_offset = struct.unpack_from("<II", raw)
    if zeroes == 0 and string_offset != 0:
        start = string_table + string_offset
        end = data.find(0, start)
        if end < 0:
            raise ValueError("unterminated COFF string-table name")
        return bytes(data[start:end]).decode("ascii")
    return raw.split(b"\0", 1)[0].decode("ascii")


def patch(elf_path: Path, coff_path: Path) -> int:
    source_relocations = elf_text_relocations(elf_path)
    data = bytearray(coff_path.read_bytes())
    if len(data) < 20:
        raise ValueError("COFF object is too small")

    machine, section_count = struct.unpack_from("<HH", data, 0)
    symbol_table = struct.unpack_from("<I", data, 8)[0]
    symbol_count = struct.unpack_from("<I", data, 12)[0]
    optional_size = struct.unpack_from("<H", data, 16)[0]
    if machine != 0x8664:
        raise ValueError(f"expected AMD64 COFF machine, got 0x{machine:04x}")

    string_table = symbol_table + symbol_count * 18
    symbols = {}
    symbol_index = 0
    while symbol_index < symbol_count:
        offset = symbol_table + symbol_index * 18
        if offset + 18 > len(data):
            raise ValueError("truncated COFF symbol table")
        name = coff_name(data, offset, string_table)
        section_number = struct.unpack_from("<h", data, offset + 12)[0]
        auxiliary_count = data[offset + 17]
        if name and section_number > 0:
            symbols.setdefault(name, symbol_index)
        symbol_index += 1 + auxiliary_count

    section_offset = 20 + optional_size
    changed = 0
    for index in range(section_count):
        offset = section_offset + index * 40
        if offset + 40 > len(data):
            raise ValueError("truncated COFF section table")
        raw_name = bytes(data[offset : offset + 8]).split(b"\0", 1)[0]
        relocation_offset = struct.unpack_from("<I", data, offset + 24)[0]
        relocation_count = struct.unpack_from("<H", data, offset + 32)[0]
        characteristics = struct.unpack_from("<I", data, offset + 36)[0]
        if not raw_name.startswith(b".text") \
                or not (characteristics & IMAGE_SCN_MEM_EXECUTE):
            continue

        for relocation_index in range(relocation_count):
            relocation = relocation_offset + relocation_index * 10
            if relocation + 10 > len(data):
                raise ValueError("truncated COFF relocation table")
            virtual_address = struct.unpack_from("<I", data, relocation)[0]
            source = source_relocations.get(virtual_address)
            if source is None or source[0] != "PC32":
                continue

            _, symbol_name, addend = source
            if symbol_name.startswith("."):
                raise ValueError(
                    f"unnamed ELF section relocation remains at 0x{virtual_address:x}: "
                    f"{symbol_name} {addend:+d}")
            target_index = symbols.get(symbol_name)
            if target_index is None:
                raise ValueError(f"COFF symbol not found: {symbol_name}")

            trailing_bytes = -addend - 4
            if trailing_bytes < 0 or trailing_bytes > 5:
                raise ValueError(
                    f"unsupported PC32 addend {addend} for {symbol_name}")
            struct.pack_into("<I", data, relocation + 4, target_index)
            struct.pack_into("<H", data, relocation + 8,
                IMAGE_REL_AMD64_REL32 + trailing_bytes)
            changed += 1

    coff_path.write_bytes(data)
    return changed


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: fix_bfd_coff_relocations.py <original.elf.o> <converted.obj>")
    elf_object = Path(sys.argv[1])
    coff_object = Path(sys.argv[2])
    count = patch(elf_object, coff_object)
    print(f"Repaired {count} PC-relative relocation(s) in {coff_object.name}")
