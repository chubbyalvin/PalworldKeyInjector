# Security and threat model

## Intended trust boundary

PalworldKeyInjector narrows an ordinary UE4SS mod helper's accidental and
reusable capability. It is useful only when loaded into a process named
`Palworld-Win64-Shipping.exe` and only while that process owns the foreground
window. It is not a sandbox and cannot make an untrusted native mod safe.

A malicious in-process mod can call Windows APIs directly or ship another
DLL. Likewise, an unrelated hostile program can rename its executable to the
expected basename. The executable-name guard is deliberate capability
restriction, not process authentication or an anti-malware boundary.

## Assets protected

- Other applications should not receive a delayed Palworld hotkey after focus
  changes.
- A caller should not turn this DLL into a general key, text, macro, mouse, or
  cross-process automation primitive.
- System/window-management shortcuts should not be synthesized.
- Request floods should not create unbounded threads, memory, or pending work.
- Reviewers should be able to audit the compiled capability from source,
  exports, imports, and strings.

## Controls

### Host confinement

At process attach, `GetModuleFileNameW(NULL, ...)` obtains the current process
path. Only a case-insensitive basename equal to
`Palworld-Win64-Shipping.exe` is accepted. There is no caller-supplied target,
PID, HWND, path, or override.

### Foreground confinement

`GetForegroundWindow` plus `GetWindowThreadProcessId` must identify the
current PID at submission. The same check occurs again immediately before
`SendInput`, after the ~80 ms delay. Switching away in that interval cancels
delivery. An unavoidable scheduler-sized interval remains between the final
check and the kernel/user input call; there is no atomic Windows API combining
the two operations.

### Fixed input vocabulary

The native interface exposes 103 fixed key functions and eight fixed
Ctrl/Shift/Alt modifier states. No raw virtual-key, scan code, string, text,
array, length, sequence, Windows-key bit, target, or callback is accepted from
the caller. The internal table in `src/palworld_key_list.inc` is the only key
mapping.

Num Lock, Scroll Lock, Print Screen, media/system keys, Windows keys, mouse
buttons, and arbitrary VK values are absent. Each accepted request is exactly
one key down/up with optional modifier down/up events.

### Denylist

At minimum the requested high-risk combinations are blocked:

- Alt+F4 (close foreground application);
- Alt+Tab (task switcher);
- Ctrl+Escape (Start menu);
- Ctrl+Shift+Escape (Task Manager).

The implementation additionally blocks Alt+Escape (window cycling), Alt+Space
(window/system menu), and Ctrl+Alt+Delete (secure attention; normally not
synthesizable by `SendInput` anyway). Rules match even when extra modifiers
are present. There is no Windows-key representation.

### Bounded concurrency

The first otherwise-valid request lazily starts one worker thread. Before
starting it, the DLL pins its module for process lifetime so the worker cannot
execute unloaded code. All requests share a fixed eight-entry array queue; no
request allocation occurs. Identical pending key/modifier pairs are
suppressed. A token bucket starts with four tokens and refills one every 75 ms.
The design never creates a thread per request.

Modifier selection is thread-local, consumed by one key call, and expires
after 250 ms. This prevents stale modifier state from leaking into unrelated
later calls on the same thread.

### Partial-input cleanup

The DLL submits a key/modifier batch through `SendInput`. If Windows reports a
partial batch, the DLL sends a best-effort cleanup containing key-up and
modifier-up events. Windows can still reject injection (for example because
of integrity/UIPI rules); the no-result Lua ABI cannot report that
asynchronously.

### Minimal native dependencies

The official DLL imports only the exact functions in `expected-imports.txt`
from KERNEL32 and USER32. There are no Lua/CRT, file, registry, clipboard,
shell/process-launch, network, IPC, mouse, or window-message imports. The
strict PE verifier rejects any import-manifest drift.

The binary is AMD64 PE32+ with high-entropy ASLR, dynamic-base, NX-compatible,
and a base-relocation directory. The freestanding direct entry avoids a
runtime initialization surface. The code uses fixed-size buffers and explicit
bounds, with no unbounded memory/string routine.

## Known limitations

- The no-Lua-dependency ABI returns zero Lua values. The wrapper can report
  parsing, policy, symbol, and Lua call failures, but native host/focus/rate/
  queue rejection and final delivery are intentionally silent.
- `SendInput` marks input as injected; Palworld, UE4SS, another mod, Windows,
  or security software may ignore it.
- Foreground ownership is a process-level check, not a particular Palworld
  HWND check. This is intentional because Palworld can own more than one
  window.
- The executable basename can be imitated and is not a digital-signature or
  path-identity check.
- Caps Lock changes global toggle state. The DLL does not automatically
  restore it, avoiding a second synthetic hotkey event.
- If a worker has started, the module is pinned until process exit. This is a
  safety/lifetime tradeoff; UE4SS should not attempt a live unload/reload.
- Native policy tests and structural verification cover the source-level and
  PE-level security surface. Manual in-game integration can be exercised using
  the Lua wrapper and example under `examples/`.

## Review checklist

1. Run `verify-exports.ps1` or `python3 tools/verify_pe.py`.
2. Compare the SHA-256 with the checksum published for the GitHub Release.
3. Review `src/palworld_key_list.inc` for the complete supported vocabulary.
4. Review `pal_blocked_combination` for the denylist.
5. Review `palworld_submit_request` and `palworld_send_request` for the two
   host/focus gates, queue/rate rules, delay, and sole `SendInput` path.
6. Confirm the packaged DLL contains no legacy identity string or unsupported
   lock/Windows-key export names.
