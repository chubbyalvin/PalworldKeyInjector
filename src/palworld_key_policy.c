#include "palworld_keyinjector_internal.h"

#if defined(PAL_BFD_CROSS)
#define PAL_INTERNAL_DATA
#else
#define PAL_INTERNAL_DATA static
#endif

PAL_INTERNAL_DATA const pal_key_descriptor PAL_KEYS[PAL_KEY_COUNT] = {
#define PAL_KEY_ENTRY(enum_id, export_token, canonical_name, virtual_key, flags) \
    { (uint16_t)(virtual_key), (uint16_t)(flags) },
#include "palworld_key_list.inc"
#undef PAL_KEY_ENTRY
};

PAL_INTERNAL_DATA const uint16_t PALWORLD_EXPECTED_EXECUTABLE_WIN64[] = {
    'P','a','l','w','o','r','l','d','-','W','i','n','6','4','-',
    'S','h','i','p','p','i','n','g','.','e','x','e',0
};

PAL_INTERNAL_DATA const uint16_t PALWORLD_EXPECTED_EXECUTABLE_WINGDK[] = {
    'P','a','l','w','o','r','l','d','-','W','i','n','G','D','K','-',
    'S','h','i','p','p','i','n','g','.','e','x','e',0
};

const pal_key_descriptor *pal_key_descriptor_for(pal_key_id key)
{
    if ((unsigned int)key >= (unsigned int)PAL_KEY_COUNT) {
        return NULL;
    }
    return &PAL_KEYS[(unsigned int)key];
}

pal_block_reason pal_blocked_combination(pal_key_id key, unsigned int modifiers)
{
    const unsigned int mods = modifiers & PAL_MOD_MASK;
    const int has_ctrl = (mods & PAL_MOD_CTRL) != 0;
    const int has_shift = (mods & PAL_MOD_SHIFT) != 0;
    const int has_alt = (mods & PAL_MOD_ALT) != 0;

    if (key == PAL_KEY_F4 && has_alt) {
        return PAL_BLOCK_ALT_F4;
    }
    if (key == PAL_KEY_TAB && has_alt) {
        return PAL_BLOCK_ALT_TAB;
    }
    if (key == PAL_KEY_ESCAPE && has_alt) {
        return PAL_BLOCK_ALT_ESCAPE;
    }
    if (key == PAL_KEY_SPACE && has_alt) {
        return PAL_BLOCK_ALT_SPACE;
    }
    if (key == PAL_KEY_ESCAPE && has_ctrl && has_shift) {
        return PAL_BLOCK_CTRL_SHIFT_ESCAPE;
    }
    if (key == PAL_KEY_ESCAPE && has_ctrl) {
        return PAL_BLOCK_CTRL_ESCAPE;
    }
    if (key == PAL_KEY_DELETE && has_ctrl && has_alt) {
        return PAL_BLOCK_CTRL_ALT_DELETE;
    }
    return PAL_BLOCK_NONE;
}

static uint16_t ascii_lower_utf16(uint16_t value)
{
    if (value >= (uint16_t)'A' && value <= (uint16_t)'Z') {
        return (uint16_t)(value + ((uint16_t)'a' - (uint16_t)'A'));
    }
    return value;
}

static int palworld_executable_name_matches(
    const uint16_t *base,
    const uint16_t *expected)
{
    size_t i;

    for (i = 0; expected[i] != 0 || base[i] != 0; ++i) {
        if (ascii_lower_utf16(expected[i]) != ascii_lower_utf16(base[i])) {
            return 0;
        }
    }
    return 1;
}

int palworld_expected_host_path_utf16(const uint16_t *path)
{
    const uint16_t *base;
    size_t i;

    if (path == NULL || path[0] == 0) {
        return 0;
    }

    base = path;
    for (i = 0; path[i] != 0; ++i) {
        if (path[i] == (uint16_t)'\\' || path[i] == (uint16_t)'/') {
            base = &path[i + 1];
        }
    }

    return palworld_executable_name_matches(
               base, PALWORLD_EXPECTED_EXECUTABLE_WIN64)
        || palworld_executable_name_matches(
               base, PALWORLD_EXPECTED_EXECUTABLE_WINGDK);
}

#undef PAL_INTERNAL_DATA
