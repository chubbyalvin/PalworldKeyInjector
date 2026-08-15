# API reference

## ABI

Every public symbol is a Lua C entry point with the conceptual signature:

```c
int function_name(lua_State *state);
```

Load it with `package.loadlib(dllPath, symbolName)` and call the returned
function with no Lua arguments. Every entry point returns C integer `0`, which
means **zero Lua return values**. There is no dependency on Lua/UE4SS symbols.

This ABI follows the supplied integration: UE4SS exposes `package.loadlib`,
but a parameterized or result-bearing Lua C API would need functions from the
exact Lua runtime. The fixed no-argument surface avoids that fragile
dependency. `examples/palworld_keyinjector.lua` provides the convenient string
interface and pre-call error reporting.

## API/version probe

### `palworld_api_v1`

- Arguments: none.
- Lua return values: none.
- Behavior: confirms that the v1 export family can be loaded and called; does
  not enqueue input.
- Validation/asynchrony: none.

```lua
local probe, err = package.loadlib(path, "palworld_api_v1")
assert(type(probe) == "function", err)
probe()
```

## Modifier-arm exports

| Export | Modifier state armed for the next key call |
|---|---|
| `palworld_mod_none` | none; also clears an earlier arm |
| `palworld_mod_ctrl` | Ctrl |
| `palworld_mod_shift` | Shift |
| `palworld_mod_alt` | Alt |
| `palworld_mod_ctrl_shift` | Ctrl+Shift |
| `palworld_mod_ctrl_alt` | Ctrl+Alt |
| `palworld_mod_shift_alt` | Shift+Alt |
| `palworld_mod_ctrl_shift_alt` | Ctrl+Shift+Alt |

For every modifier export:

- Arguments: none.
- Lua return values: none.
- Behavior: stores thread-local modifier state for the next key export.
- Lifetime: consumed once, cleared by `palworld_mod_none`, or expires after
  250 ms. It is not a queued input request.
- Validation: the bitset is fixed in the export; Windows-key state cannot be
  armed.

Always call `palworld_mod_none` first. If a modified key is wanted, then call
exactly one modifier export followed immediately by one key export on the same
Lua thread:

```lua
reset()
ctrlShift()
f10()
```

## Key exports

There is one `palworld_inject_<token>` entry for each of the 103 keys below.
`expected-exports.txt` is the exhaustive public-symbol list.

| Key names | Export token(s) |
|---|---|
| `A`–`Z` | `a`–`z` |
| `0`–`9` | `0`–`9` |
| `F1`–`F24` | `f1`–`f24` |
| `GRAVE`, `MINUS`, `EQUALS` | `grave`, `minus`, `equals` |
| `LEFT_BRACKET`, `RIGHT_BRACKET`, `BACKSLASH` | `left_bracket`, `right_bracket`, `backslash` |
| `SEMICOLON`, `APOSTROPHE`, `COMMA`, `PERIOD`, `SLASH` | same lowercase token |
| `SPACE`, `TAB`, `ENTER`, `BACKSPACE`, `ESCAPE`, `CAPS_LOCK` | same lowercase token |
| `UP`, `DOWN`, `LEFT`, `RIGHT`, `INSERT`, `DELETE`, `HOME`, `END` | same lowercase token |
| `PAGE_UP`, `PAGE_DOWN` | `page_up`, `page_down` |
| `NUMPAD_0`–`NUMPAD_9` | `numpad_0`–`numpad_9` |
| `NUMPAD_ADD`, `NUMPAD_SUBTRACT`, `NUMPAD_MULTIPLY`, `NUMPAD_DIVIDE`, `NUMPAD_DECIMAL`, `NUMPAD_ENTER` | same lowercase token |

For every key export:

- Arguments: none.
- Lua return values: none.
- Behavior: consumes the current thread's armed modifier state, applies the
  fixed key allowlist and denylist, and attempts to enqueue one asynchronous
  press/release request.
- Delay: due approximately 80 ms after native acceptance.
- Submission validation: supported descriptor, dangerous combination, exact
  current-process executable name, current foreground PID, worker state,
  duplicate pending request, queue capacity, and token bucket.
- Delivery validation: host and foreground PID are checked again immediately
  before `SendInput`; failure cancels the request silently.
- Input order: Ctrl down, Shift down, Alt down, key down/up, then Alt up,
  Shift up, Ctrl up as applicable. A partial `SendInput` call triggers a
  best-effort key/modifier-up cleanup batch.

Example direct F10 call:

```lua
local reset = assert(package.loadlib(path, "palworld_mod_none"))
local f10 = assert(package.loadlib(path, "palworld_inject_f10"))
reset()
f10()
```

## Wrapper string API

`examples/palworld_keyinjector.lua` is not part of the native ABI. It exposes:

```lua
local client, error = Injector.new(optionalDllPath)
local ok, detail = client:inject("CTRL+SHIFT+F10")
local parsed, error = Injector.parse("PAGE_DOWN")
```

Canonical syntax is zero or more modifiers in the order Ctrl, Shift, Alt,
then exactly one key: `CTRL+SHIFT+ALT+F10`. Input is case-insensitive and
whitespace is removed. Accepted aliases are:

| Alias | Canonical |
|---|---|
| `RETURN` | `ENTER` |
| `ESC` | `ESCAPE` |
| `TILDE` | `GRAVE` |
| `HYPHEN` | `MINUS` |
| `EQUAL` | `EQUALS` |
| `LEFT_ARROW`, `RIGHT_ARROW`, `UP_ARROW`, `DOWN_ARROW` | `LEFT`, `RIGHT`, `UP`, `DOWN` |
| `INS`, `DEL` | `INSERT`, `DELETE` |
| `PAGEUP`, `PGUP`, `PAGEDOWN`, `PGDN` | `PAGE_UP`, `PAGE_DOWN` |
| `ADD`, `SUBTRACT`, `MULTIPLY`, `DIVIDE`, `DECIMAL` | corresponding `NUMPAD_*` key |
| `NUMPAD0`–`NUMPAD9` | `NUMPAD_0`–`NUMPAD_9` |

The wrapper returns `false, reason` for malformed text, unsupported keys,
Windows-key aliases (`WIN`, `WINDOWS`, `LWIN`, `RWIN`, `META`, `SUPER`),
blocked combinations, missing exports, load errors, or synchronous Lua call
errors. It returns `true, canonicalName` after successful native invocation.
Because the native ABI returns no Lua value, `true` does not reveal whether
native host/focus/queue/rate checks accepted the request or whether the later
foreground recheck delivered it.

## Blocked combinations

The DLL rejects a combination if it contains:

- `ALT+F4`;
- `ALT+TAB`;
- `ALT+ESCAPE`;
- `ALT+SPACE`;
- `CTRL+ESCAPE`;
- `CTRL+SHIFT+ESCAPE`;
- `CTRL+ALT+DELETE`.

Additional modifiers do not bypass a rule; for example
`CTRL+SHIFT+ALT+F4` remains blocked by the Alt+F4 rule. The Windows key has no
export and cannot be represented.
