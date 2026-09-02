#!/usr/bin/env python3
"""Strict, dependency-free verifier for the official x64 release DLL."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import struct
import sys


IMAGE_FILE_DLL = 0x2000
IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA = 0x0020
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE = 0x0040
IMAGE_DLLCHARACTERISTICS_NX_COMPAT = 0x0100


class VerificationError(Exception):
    pass


class PEImage:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        if len(self.data) < 0x40 or self.data[:2] != b"MZ":
            raise VerificationError("not a DOS/PE image")
        pe_offset = self.u32(0x3C)
        if self.data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise VerificationError("PE signature is missing")

        coff = pe_offset + 4
        self.machine = self.u16(coff)
        section_count = self.u16(coff + 2)
        self.timestamp = self.u32(coff + 4)
        optional_size = self.u16(coff + 16)
        self.characteristics = self.u16(coff + 18)
        optional = coff + 20
        if self.u16(optional) != 0x20B:
            raise VerificationError("image is not PE32+ (64-bit)")
        if optional_size < 112:
            raise VerificationError("truncated PE32+ optional header")

        self.entry_point = self.u32(optional + 16)
        self.subsystem = self.u16(optional + 68)
        self.dll_characteristics = self.u16(optional + 70)
        directory_count = self.u32(optional + 108)
        self.directories: list[tuple[int, int]] = []
        for index in range(min(directory_count, 16)):
            entry = optional + 112 + index * 8
            self.directories.append((self.u32(entry), self.u32(entry + 4)))

        section_table = optional + optional_size
        self.sections: list[tuple[int, int, int, int]] = []
        for index in range(section_count):
            section = section_table + index * 40
            if section + 40 > len(self.data):
                raise VerificationError("truncated section table")
            virtual_size = self.u32(section + 8)
            virtual_address = self.u32(section + 12)
            raw_size = self.u32(section + 16)
            raw_offset = self.u32(section + 20)
            self.sections.append(
                (virtual_address, virtual_size, raw_offset, raw_size))

    def require(self, offset: int, length: int) -> None:
        if offset < 0 or length < 0 or offset + length > len(self.data):
            raise VerificationError("truncated or invalid PE offset")

    def u16(self, offset: int) -> int:
        self.require(offset, 2)
        return struct.unpack_from("<H", self.data, offset)[0]

    def u32(self, offset: int) -> int:
        self.require(offset, 4)
        return struct.unpack_from("<I", self.data, offset)[0]

    def u64(self, offset: int) -> int:
        self.require(offset, 8)
        return struct.unpack_from("<Q", self.data, offset)[0]

    def rva_offset(self, rva: int) -> int:
        for virtual_address, virtual_size, raw_offset, raw_size in self.sections:
            span = max(virtual_size, raw_size)
            if virtual_address <= rva < virtual_address + span:
                delta = rva - virtual_address
                if delta >= raw_size:
                    raise VerificationError("RVA points into an uninitialized section")
                return raw_offset + delta
        raise VerificationError(f"RVA 0x{rva:x} is not in a file-backed section")

    def c_string_rva(self, rva: int) -> str:
        offset = self.rva_offset(rva)
        end = self.data.find(b"\0", offset)
        if end < 0:
            raise VerificationError("unterminated PE string")
        try:
            return self.data[offset:end].decode("ascii")
        except UnicodeDecodeError as error:
            raise VerificationError("non-ASCII PE name") from error

    def directory(self, index: int) -> tuple[int, int]:
        if index >= len(self.directories):
            return (0, 0)
        return self.directories[index]

    def exports(self) -> list[str]:
        export_rva, export_size = self.directory(0)
        if export_rva == 0 or export_size == 0:
            raise VerificationError("export directory is missing")
        offset = self.rva_offset(export_rva)
        self.require(offset, 40)
        name_count = self.u32(offset + 24)
        name_array = self.rva_offset(self.u32(offset + 32))
        names = []
        for index in range(name_count):
            names.append(self.c_string_rva(self.u32(name_array + index * 4)))
        return names

    def imports(self) -> list[str]:
        import_rva, import_size = self.directory(1)
        if import_rva == 0 or import_size == 0:
            return []
        descriptor = self.rva_offset(import_rva)
        result = []
        while True:
            self.require(descriptor, 20)
            original_thunk = self.u32(descriptor)
            name_rva = self.u32(descriptor + 12)
            first_thunk = self.u32(descriptor + 16)
            if original_thunk == 0 and name_rva == 0 and first_thunk == 0:
                break
            library = self.c_string_rva(name_rva)
            thunk = self.rva_offset(original_thunk or first_thunk)
            while True:
                value = self.u64(thunk)
                thunk += 8
                if value == 0:
                    break
                if value & (1 << 63):
                    function = f"ordinal:{value & 0xFFFF}"
                else:
                    hint_name = self.rva_offset(value)
                    function = self.c_string_rva(value + 2)
                    self.require(hint_name, 2)
                result.append(f"{library}!{function}")
            descriptor += 20
        return result


def expected_exports_from_source(root: Path) -> list[str]:
    result = [
        "palworld_api_v1",
        "palworld_mod_none",
        "palworld_mod_ctrl",
        "palworld_mod_shift",
        "palworld_mod_alt",
        "palworld_mod_ctrl_shift",
        "palworld_mod_ctrl_alt",
        "palworld_mod_shift_alt",
        "palworld_mod_ctrl_shift_alt",
    ]
    pattern = re.compile(r"^PAL_KEY_ENTRY\([^,]+,\s*([^,]+),")
    key_list = root / "src" / "palworld_key_list.inc"
    for line in key_list.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line.strip())
        if match:
            result.append("palworld_inject_" + match.group(1).strip())
    return result


def lines(path: Path) -> list[str]:
    return [line.strip() for line in path.read_text(encoding="ascii").splitlines()
            if line.strip()]


def check_equal(label: str, actual: list[str], expected: list[str]) -> None:
    actual_set = set(actual)
    expected_set = set(expected)
    missing = sorted(expected_set - actual_set)
    extra = sorted(actual_set - expected_set)
    duplicates = sorted({item for item in actual if actual.count(item) > 1})
    if missing or extra or duplicates or len(actual) != len(expected):
        details = []
        if missing:
            details.append("missing=" + ", ".join(missing))
        if extra:
            details.append("unexpected=" + ", ".join(extra))
        if duplicates:
            details.append("duplicates=" + ", ".join(duplicates))
        raise VerificationError(f"{label} mismatch: {'; '.join(details)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dll", nargs="?", default="dist/PalworldKeyInjector.dll")
    parser.add_argument("--root", default=None)
    arguments = parser.parse_args()

    root = Path(arguments.root).resolve() if arguments.root else Path(__file__).resolve().parents[1]
    dll = Path(arguments.dll)
    if not dll.is_absolute():
        dll = (root / dll).resolve()
    image = PEImage(dll)

    if image.machine != 0x8664:
        raise VerificationError(f"machine is 0x{image.machine:04x}, not AMD64")
    if not image.characteristics & IMAGE_FILE_DLL:
        raise VerificationError("PE image is not marked as a DLL")
    if image.entry_point == 0:
        raise VerificationError("DLL entry point is missing")
    required_mitigations = (
        IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA
        | IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE
        | IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    )
    if image.dll_characteristics & required_mitigations != required_mitigations:
        raise VerificationError("HIGH_ENTROPY_VA, DYNAMIC_BASE, or NX_COMPAT is missing")
    reloc_rva, reloc_size = image.directory(5)
    if reloc_rva == 0 or reloc_size == 0:
        raise VerificationError("base-relocation directory is missing")

    expected_file_exports = lines(root / "expected-exports.txt")
    source_exports = expected_exports_from_source(root)
    check_equal("expected-exports.txt versus source", expected_file_exports, source_exports)
    actual_exports = image.exports()
    check_equal("DLL exports", actual_exports, expected_file_exports)
    if any(not name.startswith("palworld_") for name in actual_exports):
        raise VerificationError("a public export lacks the palworld_ prefix")

    expected_imports = lines(root / "expected-imports.txt")
    actual_imports = image.imports()
    check_equal("DLL imports", actual_imports, expected_imports)

    version_match = re.search(
        r'#define\s+PALWORLD_KEYINJECTOR_VERSION_STRING\s+"([^"]+)"',
        (root / "src" / "palworld_keyinjector_internal.h").read_text(encoding="utf-8"))
    if version_match is None:
        raise VerificationError("source version macro is missing")
    version = version_match.group(1)
    banner = f"PalworldKeyInjector {version}".encode("ascii")
    if banner not in image.data:
        raise VerificationError("source version does not match the binary banner")

    expected_hosts = [
        "Palworld-Win64-Shipping.exe",
        "Palworld-WinGDK-Shipping.exe",
    ]
    for expected_host_name in expected_hosts:
        expected_host = expected_host_name.encode("utf-16le") + b"\0\0"
        if expected_host not in image.data:
            raise VerificationError(
                f"Palworld host guard string is missing: {expected_host_name}")

    lower_data = image.data.lower()
    forbidden = [
        b"palwheel", b"palwheel_", b"num_lock", b"numlock",
        b"scroll_lock", b"scrolllock", b"palworld_inject_win",
    ]
    present = [value.decode("ascii") for value in forbidden if value in lower_data]
    if present:
        raise VerificationError("forbidden binary string(s): " + ", ".join(present))

    digest = hashlib.sha256(image.data).hexdigest()
    print(f"PASS: {dll}")
    print("  machine: AMD64 / PE32+")
    print(f"  exports: {len(actual_exports)} exact named exports")
    print(f"  imports: {len(actual_imports)} exact imports from KERNEL32.dll and USER32.dll")
    print("  mitigations: HIGH_ENTROPY_VA, DYNAMIC_BASE, NX_COMPAT, relocations")
    print(f"  version banner: PalworldKeyInjector {version}")
    print("  host guards: Palworld-Win64-Shipping.exe, Palworld-WinGDK-Shipping.exe")
    print(f"  SHA-256: {digest}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, VerificationError, UnicodeError, struct.error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
