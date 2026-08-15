#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$project_dir/build/gcc-bfd-pe-x64"
compat_dir="$project_dir/tools/bfd-pe-compat"

python3 "$project_dir/tools/generate_exports.py"
mkdir -p "$build_dir" "$project_dir/dist"

common_flags=(
    -std=c11 -O2 -mabi=ms -mno-red-zone -ffreestanding -fno-builtin
    -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables
    -fno-ident -Wall -Wextra -Wpedantic -Werror
    -DPAL_BFD_CROSS
    -I"$compat_dir" -I"$project_dir/src"
)

for source in \
    palworld_keyinjector.c \
    palworld_key_policy.c \
    palworld_rate_limiter.c
do
    base="${source%.c}"
    gcc "${common_flags[@]}" -c "$project_dir/src/$source" \
        -o "$build_dir/$base.elf.o"
    objcopy -O pe-x86-64 "$build_dir/$base.elf.o" "$build_dir/$base.obj"
    python3 "$project_dir/tools/fix_bfd_coff_relocations.py" \
        "$build_dir/$base.elf.o" \
        "$build_dir/$base.obj"
done

make_import_library() {
    local definition="$1"
    local output="$2"
    local ignored="$3"
    ld -mi386pep --dll "$definition" --out-implib "$output" \
        --no-insert-timestamp -o "$ignored" >/dev/null 2>&1 || true
    test -s "$output"
}

make_import_library "$compat_dir/kernel32-imports.def" \
    "$build_dir/libkernel32.a" "$build_dir/kernel32-stub.dll"
make_import_library "$compat_dir/user32-imports.def" \
    "$build_dir/libuser32.a" "$build_dir/user32-stub.dll"

ld -mi386pep --dll -e DllMain --subsystem windows \
    --dynamicbase --high-entropy-va --nxcompat --enable-reloc-section \
    --no-insert-timestamp \
    "$build_dir/palworld_keyinjector.obj" \
    "$build_dir/palworld_key_policy.obj" \
    "$build_dir/palworld_rate_limiter.obj" \
    "$build_dir/libkernel32.a" "$build_dir/libuser32.a" \
    "$project_dir/PalworldKeyInjector.def" \
    -o "$project_dir/dist/PalworldKeyInjector.dll"

cp "$project_dir/dist/PalworldKeyInjector.dll" \
    "$build_dir/PalworldKeyInjector.unstripped.dll"
objcopy --strip-unneeded "$project_dir/dist/PalworldKeyInjector.dll"
python3 "$project_dir/tools/normalize_pe.py" \
    "$project_dir/dist/PalworldKeyInjector.dll"

echo "Built: $project_dir/dist/PalworldKeyInjector.dll"
