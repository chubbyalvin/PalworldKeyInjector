# Changelog

## 1.0 — 2026-08-15

- Introduced the standalone `PalworldKeyInjector.dll` identity and
  `palworld_*` API.
- Added 103 fixed allowed keys with optional Ctrl/Shift/Alt modifier states.
- Added exact Palworld-host and foreground-PID checks at submission and again
  immediately before delayed input.
- Added an approximately 80 ms deferred-input worker, fixed eight-entry queue,
  four-token burst/75 ms refill limit, and duplicate pending suppression.
- Blocked Alt+F4, Alt+Tab, Alt+Escape, Alt+Space, Ctrl+Escape,
  Ctrl+Shift+Escape, and Ctrl+Alt+Delete. Windows-key input is absent.
- Added UE4SS Lua wrapper/example, native policy tests, non-Palworld host
  harness, strict PE verifier, audited import and export manifests, build
  instructions, and security model.
- Preserved Caps Lock as a single compatibility press with no automatic
  toggle restoration.
