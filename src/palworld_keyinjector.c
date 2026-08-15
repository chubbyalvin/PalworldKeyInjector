#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdint.h>

#include "palworld_keyinjector_internal.h"

typedef struct lua_State lua_State;

#if defined(_MSC_VER)
#define PAL_PUBLIC __declspec(dllexport)
#elif defined(__MINGW32__)
#define PAL_PUBLIC __attribute__((dllexport))
#else
#define PAL_PUBLIC
#endif

#if defined(PAL_BFD_CROSS)
#define PAL_INTERNAL_DATA
#else
#define PAL_INTERNAL_DATA static
#endif

enum {
    PAL_INJECTION_DELAY_MS = 80,
    PAL_MODIFIER_TTL_MS = 250,
    PAL_QUEUE_CAPACITY = 8,
};

typedef struct pal_injection_request {
    pal_key_id key;
    unsigned int modifiers;
    ULONGLONG due_ms;
} pal_injection_request;

PAL_INTERNAL_DATA const volatile char PAL_VERSION_BANNER[] =
    "PalworldKeyInjector " PALWORLD_KEYINJECTOR_VERSION_STRING;
/* Keep one absolute image relocation so ASLR remains effective even though
 * normal code/data references are RIP-relative on x64. */
PAL_INTERNAL_DATA const void * volatile PAL_RELOCATION_ANCHOR =
    (const void *)PAL_VERSION_BANNER;

PAL_INTERNAL_DATA CRITICAL_SECTION g_queue_lock;
PAL_INTERNAL_DATA CONDITION_VARIABLE g_queue_changed;
PAL_INTERNAL_DATA pal_injection_request g_queue[PAL_QUEUE_CAPACITY];
PAL_INTERNAL_DATA unsigned int g_queue_head = 0;
PAL_INTERNAL_DATA unsigned int g_queue_count = 0;
PAL_INTERNAL_DATA pal_rate_limiter g_rate_limiter;
PAL_INTERNAL_DATA int g_rate_limiter_ready = 0;
PAL_INTERNAL_DATA int g_sync_ready = 0;
PAL_INTERNAL_DATA int g_worker_attempted = 0;
PAL_INTERNAL_DATA int g_worker_available = 0;
PAL_INTERNAL_DATA int g_worker_started = 0;
PAL_INTERNAL_DATA volatile LONG g_shutdown = 0;
PAL_INTERNAL_DATA DWORD g_modifier_tls = TLS_OUT_OF_INDEXES;
PAL_INTERNAL_DATA int g_host_checked = 0;
PAL_INTERNAL_DATA int g_host_valid = 0;
/* A process path can be 32,767 UTF-16 code units. Keeping this outside the
 * stack avoids compiler-specific stack-probe runtime dependencies. */
PAL_INTERNAL_DATA WCHAR g_executable_path[32768];

static int palworld_check_current_host(void)
{
    DWORD length = GetModuleFileNameW(NULL, g_executable_path,
        (DWORD)(sizeof(g_executable_path) / sizeof(g_executable_path[0])));

    if (length == 0 || length >=
        (DWORD)(sizeof(g_executable_path) / sizeof(g_executable_path[0]))) {
        return 0;
    }
    g_executable_path[length] = 0;
    return palworld_expected_host_path_utf16(
        (const uint16_t *)g_executable_path);
}

static int palworld_current_host_is_valid(void)
{
    if (!g_host_checked) {
        g_host_valid = palworld_check_current_host();
        g_host_checked = 1;
    }
    return g_host_valid;
}

static int palworld_is_foreground_process(void)
{
    HWND foreground = GetForegroundWindow();
    DWORD foreground_pid = 0;

    if (foreground == NULL) {
        return 0;
    }
    (void)GetWindowThreadProcessId(foreground, &foreground_pid);
    return foreground_pid != 0 && foreground_pid == GetCurrentProcessId();
}

static void palworld_clear_input(INPUT *input)
{
    volatile unsigned char *bytes = (volatile unsigned char *)input;
    size_t index;
    for (index = 0; index < sizeof(*input); ++index) {
        bytes[index] = 0;
    }
}

static void palworld_append_key_input(
    INPUT *inputs,
    unsigned int *count,
    WORD virtual_key,
    DWORD extra_flags,
    int key_up)
{
    INPUT *input = &inputs[*count];
    palworld_clear_input(input);
    input->type = INPUT_KEYBOARD;
    input->ki.wVk = virtual_key;
    input->ki.dwFlags = extra_flags | (key_up ? KEYEVENTF_KEYUP : 0);
    ++(*count);
}

static void palworld_send_request(const pal_injection_request *request)
{
    const pal_key_descriptor *key = pal_key_descriptor_for(request->key);
    INPUT inputs[8];
    INPUT cleanup[4];
    unsigned int input_count = 0;
    unsigned int cleanup_count = 0;
    DWORD key_flags;
    UINT inserted;

    if (key == NULL || !palworld_current_host_is_valid()
        || !palworld_is_foreground_process()) {
        return;
    }

    key_flags = (key->flags & PAL_KEY_FLAG_EXTENDED)
        ? KEYEVENTF_EXTENDEDKEY : 0;

    if ((request->modifiers & PAL_MOD_CTRL) != 0) {
        palworld_append_key_input(inputs, &input_count, VK_CONTROL, 0, 0);
    }
    if ((request->modifiers & PAL_MOD_SHIFT) != 0) {
        palworld_append_key_input(inputs, &input_count, VK_SHIFT, 0, 0);
    }
    if ((request->modifiers & PAL_MOD_ALT) != 0) {
        palworld_append_key_input(inputs, &input_count, VK_MENU, 0, 0);
    }

    palworld_append_key_input(inputs, &input_count,
        (WORD)key->virtual_key, key_flags, 0);
    palworld_append_key_input(inputs, &input_count,
        (WORD)key->virtual_key, key_flags, 1);

    if ((request->modifiers & PAL_MOD_ALT) != 0) {
        palworld_append_key_input(inputs, &input_count, VK_MENU, 0, 1);
    }
    if ((request->modifiers & PAL_MOD_SHIFT) != 0) {
        palworld_append_key_input(inputs, &input_count, VK_SHIFT, 0, 1);
    }
    if ((request->modifiers & PAL_MOD_CTRL) != 0) {
        palworld_append_key_input(inputs, &input_count, VK_CONTROL, 0, 1);
    }

    inserted = SendInput((UINT)input_count, inputs, (int)sizeof(INPUT));
    if (inserted == (UINT)input_count) {
        return;
    }

    palworld_append_key_input(cleanup, &cleanup_count,
        (WORD)key->virtual_key, key_flags, 1);
    if ((request->modifiers & PAL_MOD_ALT) != 0) {
        palworld_append_key_input(cleanup, &cleanup_count, VK_MENU, 0, 1);
    }
    if ((request->modifiers & PAL_MOD_SHIFT) != 0) {
        palworld_append_key_input(cleanup, &cleanup_count, VK_SHIFT, 0, 1);
    }
    if ((request->modifiers & PAL_MOD_CTRL) != 0) {
        palworld_append_key_input(cleanup, &cleanup_count, VK_CONTROL, 0, 1);
    }
    (void)SendInput((UINT)cleanup_count, cleanup, (int)sizeof(INPUT));
}

static DWORD WINAPI palworld_injection_worker(LPVOID parameter)
{
    (void)parameter;

    for (;;) {
        pal_injection_request request;
        ULONGLONG now;
        DWORD wait_ms;

        EnterCriticalSection(&g_queue_lock);
        while (!g_shutdown && g_queue_count == 0) {
            (void)SleepConditionVariableCS(
                &g_queue_changed, &g_queue_lock, INFINITE);
        }

        if (g_shutdown) {
            LeaveCriticalSection(&g_queue_lock);
            return 0;
        }

        now = GetTickCount64();
        request = g_queue[g_queue_head];
        if (request.due_ms > now) {
            ULONGLONG remaining = request.due_ms - now;
            wait_ms = remaining > 0xFFFFFFFEULL
                ? 0xFFFFFFFEUL : (DWORD)remaining;
            (void)SleepConditionVariableCS(
                &g_queue_changed, &g_queue_lock, wait_ms);
            LeaveCriticalSection(&g_queue_lock);
            continue;
        }

        g_queue_head = (g_queue_head + 1u) % PAL_QUEUE_CAPACITY;
        --g_queue_count;
        LeaveCriticalSection(&g_queue_lock);

        palworld_send_request(&request);
    }
}

static int palworld_start_worker_locked(void)
{
    HMODULE pinned_module = NULL;
    HANDLE thread;

    if (g_worker_attempted) {
        return g_worker_available;
    }
    g_worker_attempted = 1;

    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)(uintptr_t)&palworld_injection_worker,
            &pinned_module)) {
        return 0;
    }

    thread = CreateThread(NULL, 0, palworld_injection_worker, NULL, 0, NULL);
    if (thread == NULL) {
        return 0;
    }
    (void)CloseHandle(thread);
    g_worker_started = 1;
    g_worker_available = 1;
    return 1;
}

static unsigned int palworld_consume_modifiers(void)
{
    uintptr_t encoded;
    ULONGLONG expiry;
    unsigned int modifiers;

    if (g_modifier_tls == TLS_OUT_OF_INDEXES) {
        return PAL_MOD_NONE;
    }

    encoded = (uintptr_t)TlsGetValue(g_modifier_tls);
    (void)TlsSetValue(g_modifier_tls, NULL);
    if (encoded == 0) {
        return PAL_MOD_NONE;
    }

    modifiers = (unsigned int)(encoded & PAL_MOD_MASK);
    expiry = (ULONGLONG)(encoded >> 3);
    if (GetTickCount64() > expiry) {
        return PAL_MOD_NONE;
    }
    return modifiers;
}

static void palworld_arm_modifiers(unsigned int modifiers)
{
    uintptr_t encoded;

    if (g_modifier_tls == TLS_OUT_OF_INDEXES) {
        return;
    }

    modifiers &= PAL_MOD_MASK;
    if (modifiers == PAL_MOD_NONE) {
        (void)TlsSetValue(g_modifier_tls, NULL);
        return;
    }

    encoded = (uintptr_t)((GetTickCount64() + PAL_MODIFIER_TTL_MS) << 3);
    encoded |= (uintptr_t)modifiers;
    (void)TlsSetValue(g_modifier_tls, (LPVOID)encoded);
}

static int palworld_queue_contains_locked(
    pal_key_id key,
    unsigned int modifiers)
{
    unsigned int index;
    for (index = 0; index < g_queue_count; ++index) {
        const unsigned int queue_index =
            (g_queue_head + index) % PAL_QUEUE_CAPACITY;
        if (g_queue[queue_index].key == key
            && g_queue[queue_index].modifiers == modifiers) {
            return 1;
        }
    }
    return 0;
}

static pal_request_result palworld_submit_request(
    pal_key_id key,
    unsigned int modifiers)
{
    ULONGLONG now;
    unsigned int tail;

    modifiers &= PAL_MOD_MASK;
    if (pal_key_descriptor_for(key) == NULL) {
        return PAL_REQUEST_UNSUPPORTED_KEY;
    }
    if (pal_blocked_combination(key, modifiers) != PAL_BLOCK_NONE) {
        return PAL_REQUEST_BLOCKED_COMBINATION;
    }
    if (!palworld_current_host_is_valid()) {
        return PAL_REQUEST_NOT_PALWORLD;
    }
    if (!palworld_is_foreground_process()) {
        return PAL_REQUEST_NOT_FOREGROUND;
    }
    if (!g_sync_ready || g_modifier_tls == TLS_OUT_OF_INDEXES) {
        return PAL_REQUEST_WORKER_UNAVAILABLE;
    }

    EnterCriticalSection(&g_queue_lock);
    if (!palworld_start_worker_locked()) {
        LeaveCriticalSection(&g_queue_lock);
        return PAL_REQUEST_WORKER_UNAVAILABLE;
    }

    now = GetTickCount64();
    if (!g_rate_limiter_ready) {
        pal_rate_limiter_initialize(&g_rate_limiter, now);
        g_rate_limiter_ready = 1;
    }
    if (palworld_queue_contains_locked(key, modifiers)) {
        LeaveCriticalSection(&g_queue_lock);
        return PAL_REQUEST_RATE_LIMITED;
    }
    if (g_queue_count >= PAL_QUEUE_CAPACITY) {
        LeaveCriticalSection(&g_queue_lock);
        return PAL_REQUEST_QUEUE_FULL;
    }
    if (!pal_rate_limiter_try_consume(&g_rate_limiter, now)) {
        LeaveCriticalSection(&g_queue_lock);
        return PAL_REQUEST_RATE_LIMITED;
    }

    tail = (g_queue_head + g_queue_count) % PAL_QUEUE_CAPACITY;
    g_queue[tail].key = key;
    g_queue[tail].modifiers = modifiers;
    g_queue[tail].due_ms = now + PAL_INJECTION_DELAY_MS;
    ++g_queue_count;
    WakeConditionVariable(&g_queue_changed);
    LeaveCriticalSection(&g_queue_lock);
    return PAL_REQUEST_ACCEPTED;
}

PAL_PUBLIC int palworld_api_v1(lua_State *state)
{
    (void)state;
    (void)PAL_VERSION_BANNER[0];
    (void)PAL_RELOCATION_ANCHOR;
    return 0;
}

#define PAL_MOD_EXPORT(name, value) \
    PAL_PUBLIC int palworld_mod_##name(lua_State *state) \
    { \
        (void)state; \
        palworld_arm_modifiers(value); \
        return 0; \
    }

PAL_MOD_EXPORT(none, PAL_MOD_NONE)
PAL_MOD_EXPORT(ctrl, PAL_MOD_CTRL)
PAL_MOD_EXPORT(shift, PAL_MOD_SHIFT)
PAL_MOD_EXPORT(alt, PAL_MOD_ALT)
PAL_MOD_EXPORT(ctrl_shift, PAL_MOD_CTRL | PAL_MOD_SHIFT)
PAL_MOD_EXPORT(ctrl_alt, PAL_MOD_CTRL | PAL_MOD_ALT)
PAL_MOD_EXPORT(shift_alt, PAL_MOD_SHIFT | PAL_MOD_ALT)
PAL_MOD_EXPORT(ctrl_shift_alt, PAL_MOD_CTRL | PAL_MOD_SHIFT | PAL_MOD_ALT)

#undef PAL_MOD_EXPORT

#define PAL_KEY_ENTRY(enum_id, export_token, canonical_name, virtual_key, flags) \
    PAL_PUBLIC int palworld_inject_##export_token(lua_State *state) \
    { \
        const unsigned int modifiers = palworld_consume_modifiers(); \
        (void)state; \
        (void)palworld_submit_request(PAL_KEY_##enum_id, modifiers); \
        return 0; \
    }
#include "palworld_key_list.inc"
#undef PAL_KEY_ENTRY

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        (void)DisableThreadLibraryCalls(instance);
        g_modifier_tls = TlsAlloc();
        g_sync_ready = InitializeCriticalSectionAndSpinCount(
            &g_queue_lock, 4000) != FALSE;
        if (g_sync_ready) {
            InitializeConditionVariable(&g_queue_changed);
        }
        g_host_valid = palworld_check_current_host();
        g_host_checked = 1;
    } else if (reason == DLL_PROCESS_DETACH) {
        g_shutdown = 1;
        if (g_worker_started) {
            WakeAllConditionVariable(&g_queue_changed);
        } else {
            if (g_modifier_tls != TLS_OUT_OF_INDEXES) {
                (void)TlsFree(g_modifier_tls);
                g_modifier_tls = TLS_OUT_OF_INDEXES;
            }
            if (g_sync_ready) {
                DeleteCriticalSection(&g_queue_lock);
                g_sync_ready = 0;
            }
        }
    }
    return TRUE;
}
