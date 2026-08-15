#ifndef PALWORLD_KEYINJECTOR_INTERNAL_H
#define PALWORLD_KEYINJECTOR_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#define PALWORLD_KEYINJECTOR_VERSION_MAJOR 1
#define PALWORLD_KEYINJECTOR_VERSION_MINOR 0
#define PALWORLD_KEYINJECTOR_VERSION_STRING "1.0"

enum {
    PAL_MOD_NONE = 0,
    PAL_MOD_CTRL = 1u << 0,
    PAL_MOD_SHIFT = 1u << 1,
    PAL_MOD_ALT = 1u << 2,
    PAL_MOD_MASK = PAL_MOD_CTRL | PAL_MOD_SHIFT | PAL_MOD_ALT,
};

enum {
    PAL_KEY_FLAG_EXTENDED = 1u << 0,
};

typedef enum pal_key_id {
#define PAL_KEY_ENTRY(enum_id, export_token, canonical_name, virtual_key, flags) \
    PAL_KEY_##enum_id,
#include "palworld_key_list.inc"
#undef PAL_KEY_ENTRY
    PAL_KEY_COUNT
} pal_key_id;

typedef struct pal_key_descriptor {
    uint16_t virtual_key;
    uint16_t flags;
} pal_key_descriptor;

typedef enum pal_block_reason {
    PAL_BLOCK_NONE = 0,
    PAL_BLOCK_ALT_F4,
    PAL_BLOCK_ALT_TAB,
    PAL_BLOCK_ALT_ESCAPE,
    PAL_BLOCK_ALT_SPACE,
    PAL_BLOCK_CTRL_ESCAPE,
    PAL_BLOCK_CTRL_SHIFT_ESCAPE,
    PAL_BLOCK_CTRL_ALT_DELETE,
} pal_block_reason;

typedef enum pal_request_result {
    PAL_REQUEST_ACCEPTED = 0,
    PAL_REQUEST_NOT_PALWORLD,
    PAL_REQUEST_NOT_FOREGROUND,
    PAL_REQUEST_UNSUPPORTED_KEY,
    PAL_REQUEST_BLOCKED_COMBINATION,
    PAL_REQUEST_RATE_LIMITED,
    PAL_REQUEST_QUEUE_FULL,
    PAL_REQUEST_WORKER_UNAVAILABLE,
} pal_request_result;

typedef struct pal_rate_limiter {
    uint64_t last_refill_ms;
    unsigned int tokens;
} pal_rate_limiter;

const pal_key_descriptor *pal_key_descriptor_for(pal_key_id key);
pal_block_reason pal_blocked_combination(pal_key_id key, unsigned int modifiers);
int palworld_expected_host_path_utf16(const uint16_t *path);

void pal_rate_limiter_initialize(pal_rate_limiter *limiter, uint64_t now_ms);
int pal_rate_limiter_try_consume(pal_rate_limiter *limiter, uint64_t now_ms);

#endif
