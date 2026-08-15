# PalworldKeyInjector v1.0

PalworldKeyInjector is a small, open-source x64 Windows keyboard-input helper
for Palworld UE4SS Lua mods. While Palworld owns the foreground window, a mod
can request one explicitly supported key with optional Ctrl, Shift, and Alt.
The DLL waits approximately 80 ms—enough for the caller's UI to close—then
uses `SendInput`.

It exists for cases where one Palworld mod needs to activate an ordinary
Palworld or other-mod key binding. For example, a radial-menu action can emit
F10 and another mod's `RegisterKeyBind(Key.F10, ...)` can receive it.

## Narrow by design

This is not a general macro engine, global keyboard injector, text automation
tool, mouse injector, process launcher, or remote-control component. It has no
API for raw virtual-key values, text, sequences, target windows/processes,
Windows-key input, mouse input, files, registry, clipboard, shell/process
launch, IPC, or networking.

Every injectable key has a fixed no-argument export compiled into the DLL.
This slightly wider export table is intentional: inspection of the supplied
UE4SS integration showed that `package.loadlib` conveniently calls Lua C
functions but does not provide a safe parameter ABI without linking to the
exact embedded Lua runtime. Fixed exports preserve zero-dependency loading
and make arbitrary key values structurally impossible.

## Runtime safeguards

- The current process executable basename must be exactly
  `Palworld-Win64-Shipping.exe` (case-insensitive). The caller cannot override
  it.
- Palworld's process must own the foreground window both when a request is
  submitted and immediately before `SendInput` after the delay.
- Only the keys below and Ctrl/Shift/Alt modifiers exist in the native API.
- `ALT+F4`, `ALT+TAB`, `CTRL+ESCAPE`, and `CTRL+SHIFT+ESCAPE` are blocked.
  The small additional denylist blocks `ALT+ESCAPE`, `ALT+SPACE`, and
  `CTRL+ALT+DELETE` because they are Windows task/window-management or secure
  attention combinations.
- The Windows key is not implemented.
- One lazily created worker serves a fixed eight-entry queue. There is no
  thread per request.
- A token bucket permits a burst of four requests, then refills one token per
  75 ms. An identical key/modifier request already pending is suppressed.
- Each request is one key press/release, with optional modifier down/up events;
  sequences are impossible.

See [SECURITY.md](SECURITY.md) for the complete threat model and limitations.

## Supported keys

| Group | Keys |
|---|---|
| Letters | `A`–`Z` |
| Number row | `0`–`9` |
| Function | `F1`–`F24` |
| Punctuation | `GRAVE`, `MINUS`, `EQUALS`, `LEFT_BRACKET`, `RIGHT_BRACKET`, `BACKSLASH`, `SEMICOLON`, `APOSTROPHE`, `COMMA`, `PERIOD`, `SLASH` |
| Controls | `SPACE`, `TAB`, `ENTER`, `BACKSPACE`, `ESCAPE`, `CAPS_LOCK` |
| Navigation | `UP`, `DOWN`, `LEFT`, `RIGHT`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGE_UP`, `PAGE_DOWN` |
| Numpad | `NUMPAD_0`–`NUMPAD_9`, `NUMPAD_ADD`, `NUMPAD_SUBTRACT`, `NUMPAD_MULTIPLY`, `NUMPAD_DIVIDE`, `NUMPAD_DECIMAL`, `NUMPAD_ENTER` |

Any allowed key may use any subset of `CTRL`, `SHIFT`, and `ALT`, except for
the exact blocked combinations above. Num Lock, Scroll Lock, Print Screen,
media/system keys, the Windows key, and mouse buttons are absent.

### Caps Lock

Caps Lock is present for compatibility with configurations such as a Steam
Input rear button mapped to Caps Lock. Injecting it toggles the operating
system's Caps Lock state. The DLL sends exactly one press/release and does not
try to toggle it back, because a second synthetic press could trigger the
binding twice. Mods should request it only when that state change is intended.

## UE4SS Lua usage

Put these three files in the calling mod's `Scripts` directory:

- `PalworldKeyInjector.dll` from a GitHub Release, or a locally built `dist/PalworldKeyInjector.dll`;
- `examples/palworld_keyinjector.lua`;
- the mod's `main.lua`.

Then call the wrapper:

```lua
local PalworldKeyInjector = require("palworld_keyinjector")
local injector, loadError = PalworldKeyInjector.new()
if injector == nil then
    print("DLL load failed: " .. tostring(loadError) .. "\n")
    return
end

RegisterKeyBind(Key.F9, function()
    local ok, detail = injector:inject("F10")
    print("request: " .. tostring(ok) .. ", " .. tostring(detail) .. "\n")
end)
```

The wrapper accepts one canonical key with optional `CTRL+SHIFT+ALT`
modifiers, such as `F10`, `PAGE_DOWN`, `NUMPAD_5`, `CTRL+F10`, or
`CTRL+SHIFT+F10`. It normalizes case/whitespace and a short documented alias
set. It rejects unsupported and blocked specifications before calling native
code.

`ok == true` means that wrapper parsing, symbol loading, and the no-argument
native call completed. The `package.loadlib` ABI has no safe zero-dependency
way to return the later asynchronous result; host, focus, queue, rate, and
pre-send cancellation can therefore remain silent. Never treat `true` as
proof that input was delivered.

See [API.md](API.md) and [examples/ue4ss_lua_example.lua](examples/ue4ss_lua_example.lua).

## Building

### Requirements

For the primary MSVC path, install Visual Studio 2022 or Visual Studio 2022
Build Tools with the **Desktop development with C++** workload, the MSVC x64
compiler/toolset, Windows SDK, and CMake tools. Run commands from an x64 Native
Tools/Developer Command Prompt. Python 3 is used to generate and verify the
export manifest.

For MinGW, install a current 64-bit MinGW-w64 GCC toolchain, CMake, a MinGW
make implementation, and Python 3. Ensure `gcc`, `cmake`, the make tool, and
`py` or `python` are on `PATH`; do not use a 32-bit-only toolchain.

### MSVC: provided script

```bat
build-msvc-x64.bat
powershell -ExecutionPolicy Bypass -File .\verify-exports.ps1
```

### MSVC: manual CMake commands

```bat
py -3 tools\generate_exports.py
cmake -S . -B build\msvc-x64 -A x64 -DBUILD_TESTING=ON
cmake --build build\msvc-x64 --config Release
ctest --test-dir build\msvc-x64 -C Release --output-on-failure
powershell -ExecutionPolicy Bypass -File .\verify-exports.ps1
```

### MinGW-w64: provided script

```bat
build-mingw-x64.bat
powershell -ExecutionPolicy Bypass -File .\verify-exports.ps1
```

### MinGW-w64: manual CMake commands

```bat
py -3 tools\generate_exports.py
cmake -S . -B build\mingw-x64 -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build\mingw-x64
ctest --test-dir build\mingw-x64 --output-on-failure
powershell -ExecutionPolicy Bypass -File .\verify-exports.ps1
```

All paths produce `dist\PalworldKeyInjector.dll`. The DLL target is
freestanding and deliberately omits a C runtime; tests are normal hosted
executables. See [BUILDING.md](BUILDING.md) for the exact release toolchain,
manual inspection, and troubleshooting notes.

## Verifying

`verify-exports.ps1` invokes a dependency-free PE parser. Success reports:

- AMD64/PE32+ DLL architecture;
- exact agreement among source, `expected-exports.txt`, and the DLL;
- the exact audited KERNEL32/USER32 import manifest;
- ASLR/high-entropy/NX flags and a base-relocation directory;
- source/binary version agreement and the Palworld host-guard string;
- absence of legacy branding and unsupported lock/Windows-key export names;
- SHA-256.

Optional Visual Studio inspection:

```bat
dumpbin /headers dist\PalworldKeyInjector.dll
dumpbin /exports dist\PalworldKeyInjector.dll
dumpbin /imports dist\PalworldKeyInjector.dll
```

Calculate the hash independently:

```powershell
Get-FileHash .\dist\PalworldKeyInjector.dll -Algorithm SHA256
```

Official v1.0 DLL SHA-256:

```text
229feab17545d5ac9f0639b997047f2e53f3e75b9fdefdd80c2e23a63284fbb4
```

The official v1.0 release binary was compiled directly from this source using
the documented GCC 13.3.0/GNU Binutils 2.42 freestanding PE-x64 path. The
compiled DLL is distributed separately through GitHub Releases; this repository
archive is source-only. MSVC remains the primary supported rebuild path. No
byte-for-byte equivalence across compilers is claimed.

## Tests

- `tests/native/policy_tests.c` covers all 103 key descriptors, host-name
  matching, the denylist, and rate limiting.
- `tests/native/non_palworld_host.c` is a Windows harness that loads the DLL
  from an unrelated executable and requests F10; no input may be emitted.

The native policy tests are included in normal CMake test builds. For manual
in-game integration checks, use the Lua wrapper and example under `examples/`.

## License

PalworldKeyInjector is licensed separately under the MIT License. Copyright
(c) 2026 CHUBBYALVIN. See [LICENSE](LICENSE).
