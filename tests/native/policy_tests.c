#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "palworld_keyinjector_internal.h"

static int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

static void ascii_to_utf16(const char *source, uint16_t *destination, size_t capacity)
{
    size_t index = 0;
    while (source[index] != '\0' && index + 1 < capacity) {
        destination[index] = (uint16_t)(unsigned char)source[index];
        ++index;
    }
    destination[index] = 0;
}

static void test_key_catalog(void)
{
    unsigned int index;
    CHECK(PAL_KEY_COUNT == 103);
    for (index = 0; index < (unsigned int)PAL_KEY_COUNT; ++index) {
        const pal_key_descriptor *key = pal_key_descriptor_for((pal_key_id)index);
        CHECK(key != NULL);
        CHECK(key != NULL && key->virtual_key != 0);
        CHECK(key != NULL && (key->flags & ~PAL_KEY_FLAG_EXTENDED) == 0);
    }
    CHECK(pal_key_descriptor_for((pal_key_id)-1) == NULL);
    CHECK(pal_key_descriptor_for(PAL_KEY_COUNT) == NULL);
    CHECK(pal_key_descriptor_for(PAL_KEY_F10)->virtual_key == 0x79);
    CHECK(pal_key_descriptor_for(PAL_KEY_F24)->virtual_key == 0x87);
    CHECK(pal_key_descriptor_for(PAL_KEY_CAPS_LOCK)->virtual_key == 0x14);
    CHECK((pal_key_descriptor_for(PAL_KEY_NUMPAD_ENTER)->flags
        & PAL_KEY_FLAG_EXTENDED) != 0);
}

static void test_denylist(void)
{
    CHECK(pal_blocked_combination(PAL_KEY_F4, PAL_MOD_ALT) == PAL_BLOCK_ALT_F4);
    CHECK(pal_blocked_combination(PAL_KEY_TAB, PAL_MOD_ALT) == PAL_BLOCK_ALT_TAB);
    CHECK(pal_blocked_combination(PAL_KEY_ESCAPE, PAL_MOD_ALT)
        == PAL_BLOCK_ALT_ESCAPE);
    CHECK(pal_blocked_combination(PAL_KEY_SPACE, PAL_MOD_ALT)
        == PAL_BLOCK_ALT_SPACE);
    CHECK(pal_blocked_combination(PAL_KEY_ESCAPE, PAL_MOD_CTRL)
        == PAL_BLOCK_CTRL_ESCAPE);
    CHECK(pal_blocked_combination(PAL_KEY_ESCAPE, PAL_MOD_CTRL | PAL_MOD_SHIFT)
        == PAL_BLOCK_CTRL_SHIFT_ESCAPE);
    CHECK(pal_blocked_combination(PAL_KEY_DELETE, PAL_MOD_CTRL | PAL_MOD_ALT)
        == PAL_BLOCK_CTRL_ALT_DELETE);

    CHECK(pal_blocked_combination(PAL_KEY_F10, PAL_MOD_NONE) == PAL_BLOCK_NONE);
    CHECK(pal_blocked_combination(PAL_KEY_F10,
        PAL_MOD_CTRL | PAL_MOD_SHIFT | PAL_MOD_ALT) == PAL_BLOCK_NONE);
    CHECK(pal_blocked_combination(PAL_KEY_ENTER, PAL_MOD_ALT) == PAL_BLOCK_NONE);
    CHECK(pal_blocked_combination(PAL_KEY_CAPS_LOCK, PAL_MOD_NONE) == PAL_BLOCK_NONE);
}

static void test_host_guard(void)
{
    uint16_t path[256];

    ascii_to_utf16("C:\\Steam\\Palworld-Win64-Shipping.exe", path, 256);
    CHECK(palworld_expected_host_path_utf16(path));
    ascii_to_utf16("c:\\steam\\PALWORLD-win64-shipping.EXE", path, 256);
    CHECK(palworld_expected_host_path_utf16(path));
    ascii_to_utf16("Palworld-Win64-Shipping.exe", path, 256);
    CHECK(palworld_expected_host_path_utf16(path));
    ascii_to_utf16("C:\\Games\\NotPalworld.exe", path, 256);
    CHECK(!palworld_expected_host_path_utf16(path));
    ascii_to_utf16("C:\\Games\\Palworld-Win64-Shipping.exe.bak", path, 256);
    CHECK(!palworld_expected_host_path_utf16(path));
    CHECK(!palworld_expected_host_path_utf16(NULL));
}

static void test_rate_limiter(void)
{
    pal_rate_limiter limiter;
    unsigned int index;

    pal_rate_limiter_initialize(&limiter, 1000);
    for (index = 0; index < 4; ++index) {
        CHECK(pal_rate_limiter_try_consume(&limiter, 1000));
    }
    CHECK(!pal_rate_limiter_try_consume(&limiter, 1000));
    CHECK(!pal_rate_limiter_try_consume(&limiter, 1074));
    CHECK(pal_rate_limiter_try_consume(&limiter, 1075));
    CHECK(!pal_rate_limiter_try_consume(&limiter, 1075));
    CHECK(pal_rate_limiter_try_consume(&limiter, 2000));
    CHECK(pal_rate_limiter_try_consume(&limiter, 2000));
    CHECK(pal_rate_limiter_try_consume(&limiter, 2000));
    CHECK(pal_rate_limiter_try_consume(&limiter, 2000));
    CHECK(!pal_rate_limiter_try_consume(&limiter, 2000));

    pal_rate_limiter_initialize(&limiter, 5000);
    CHECK(pal_rate_limiter_try_consume(&limiter, 4000));
    CHECK(limiter.last_refill_ms == 4000);
}

int main(void)
{
    test_key_catalog();
    test_denylist();
    test_host_guard();
    test_rate_limiter();

    if (failures != 0) {
        fprintf(stderr, "%d policy test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("All PalworldKeyInjector policy tests passed.");
    return EXIT_SUCCESS;
}
