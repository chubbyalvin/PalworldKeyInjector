# Building from source

The short commands are in `README.md`. This document records the details
needed to reproduce or audit the v1.1 build.

## Generated API files

`src/palworld_key_list.inc` is the single key catalog. Run:

```text
py -3 tools\generate_exports.py
```

It regenerates `PalworldKeyInjector.def` and `expected-exports.txt`. The strict
verifier independently derives the expected names from the same catalog and
fails if source, manifest, or DLL differ.

## MSVC x64

Install Visual Studio 2022/Build Tools 2022 with Desktop development with C++,
an x64 MSVC toolset, Windows SDK, CMake tools, and Python 3. From an x64 Native
Tools Command Prompt, use `build-msvc-x64.bat` or:

```bat
py -3 tools\generate_exports.py
cmake -S . -B build\msvc-x64 -A x64 -DBUILD_TESTING=ON
cmake --build build\msvc-x64 --config Release
ctest --test-dir build\msvc-x64 -C Release --output-on-failure
powershell -ExecutionPolicy Bypass -File .\verify-exports.ps1
```

The DLL uses `/NODEFAULTLIB /ENTRY:DllMain` and links only the required Windows
libraries. It has no Lua or C-runtime import. `/GS-` is required by the
freestanding, no-CRT entry; the implementation uses no unbounded string/memory
operation, and PE ASLR/high-entropy/NX mitigations remain enabled.

## MinGW-w64 x64

Use a 64-bit MinGW-w64 distribution, CMake, MinGW Make, and Python 3 on PATH.
Run `build-mingw-x64.bat` or:

```bat
py -3 tools\generate_exports.py
cmake -S . -B build\mingw-x64 -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build\mingw-x64
ctest --test-dir build\mingw-x64 --output-on-failure
powershell -ExecutionPolicy Bypass -File .\verify-exports.ps1
```

The MinGW DLL target uses `-nostdlib`, a direct `DllMain` entry, and only
KERNEL32/USER32. The hosted test executables continue to use the normal
runtime.

## Official v1.1 release build

The official v1.1 release binary packaged with this source update was built in
a Linux x86-64 environment with:

- GCC 14.2.0;
- GNU ld/objcopy 2.44 with the `i386pep` backend;
- Python 3.13.5.

The environment did not contain MSVC, MinGW-w64, Wine, or CMake, and did not
permit installing them. The included `tools/build-gcc-bfd-pe-x64.sh` therefore
uses the ordinary production C sources with a minimal Windows declaration
header, the Microsoft x64 calling convention, generated KERNEL32/USER32 import
libraries, and GNU ld's PE-x64 backend:

```bash
./tools/build-gcc-bfd-pe-x64.sh
python3 tools/verify_pe.py
```

`tools/fix_bfd_coff_relocations.py` preserves named x64 PC-relative
relocations while converting the compiler's ELF objects to PE-COFF.
`tools/normalize_pe.py` zeroes only the COFF timestamp and optional checksum
after stripping, making repeat builds with these exact tool versions stable.
This route is included for full source-to-binary traceability; MSVC is the
preferred normal Windows build route. The compiled release DLL is distributed
separately through GitHub Releases.

## Output and verification

The release DLL is always `dist/PalworldKeyInjector.dll`. Run:

```powershell
.\verify-exports.ps1
Get-FileHash .\dist\PalworldKeyInjector.dll -Algorithm SHA256
```

The verifier is strict for the official security surface. A binary built with
a modified toolchain or flags fails if it adds even a legitimate import. If
that occurs, do not weaken the manifest blindly: inspect with
`dumpbin /imports`, explain every addition, and consciously update
`expected-imports.txt` only for a new reviewed release.

Useful manual checks:

```bat
dumpbin /headers dist\PalworldKeyInjector.dll
dumpbin /exports dist\PalworldKeyInjector.dll
dumpbin /imports dist\PalworldKeyInjector.dll
```

An independent compiler can produce a functionally equivalent DLL but will
usually produce different bytes. No cross-toolchain reproducibility claim is
made. For the official release file, compare against the checksum published with
the GitHub Release.

## Non-Palworld host harness

With `BUILD_TESTING=ON`, CMake also builds
`PalworldKeyInjectorNonPalworldHost.exe`. From Windows:

```bat
build\msvc-x64\Release\PalworldKeyInjectorNonPalworldHost.exe dist\PalworldKeyInjector.dll
```

It calls the API and F10 export, waits 150 ms, and exits. Run it in a safe text
field or with an input monitor: no F10 may appear. This is a manual security
test because absence of globally delivered input cannot be asserted by the
process itself.
