#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
KEY_LIST = ROOT / "src" / "palworld_key_list.inc"
TOKEN = re.compile(r"^PAL_KEY_ENTRY\([^,]+,\s*([^,]+),")

exports = [
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

for line in KEY_LIST.read_text(encoding="utf-8").splitlines():
    match = TOKEN.match(line.strip())
    if match:
        exports.append(f"palworld_inject_{match.group(1).strip()}")

if len(exports) != len(set(exports)):
    raise SystemExit("Duplicate export generated")

definition = "LIBRARY PalworldKeyInjector\nEXPORTS\n"
definition += "".join(f"    {name}\n" for name in exports)
(ROOT / "PalworldKeyInjector.def").write_text(definition, encoding="ascii")
(ROOT / "expected-exports.txt").write_text(
    "".join(f"{name}\n" for name in exports), encoding="ascii")
print(f"Generated {len(exports)} exports")
