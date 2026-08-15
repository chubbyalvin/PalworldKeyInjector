#ifndef PALWORLD_BFD_PE_WINDOWS_H
#define PALWORLD_BFD_PE_WINDOWS_H

#include <stddef.h>
#include <stdint.h>

#define WINAPI
#define CALLBACK

typedef int BOOL;
typedef int32_t LONG;
typedef uint16_t WORD;
typedef uint16_t WCHAR;
typedef uint32_t DWORD;
typedef uint32_t UINT;
typedef uint32_t ULONG;
typedef uint64_t ULONGLONG;
typedef uintptr_t ULONG_PTR;
typedef void *PVOID;
typedef void *LPVOID;
typedef const WCHAR *LPCWSTR;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HINSTANCE;
typedef void *HWND;
typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);

#ifndef NULL
#define NULL ((void *)0)
#endif

#define FALSE 0
#define TRUE 1
#define INFINITE 0xFFFFFFFFUL
#define TLS_OUT_OF_INDEXES 0xFFFFFFFFUL

#define DLL_PROCESS_DETACH 0
#define DLL_PROCESS_ATTACH 1

#define INPUT_KEYBOARD 1
#define KEYEVENTF_EXTENDEDKEY 0x0001
#define KEYEVENTF_KEYUP 0x0002

#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20

#define GET_MODULE_HANDLE_EX_FLAG_PIN 0x00000001
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x00000004

typedef struct _RTL_CRITICAL_SECTION_DEBUG *PRTL_CRITICAL_SECTION_DEBUG;
typedef struct _RTL_CRITICAL_SECTION {
    PRTL_CRITICAL_SECTION_DEBUG DebugInfo;
    LONG LockCount;
    LONG RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    ULONG_PTR SpinCount;
} CRITICAL_SECTION;

typedef struct _RTL_CONDITION_VARIABLE {
    PVOID Ptr;
} CONDITION_VARIABLE;

typedef struct tagKEYBDINPUT {
    WORD wVk;
    WORD wScan;
    DWORD dwFlags;
    DWORD time;
    ULONG_PTR dwExtraInfo;
} KEYBDINPUT;

typedef struct tagINPUT {
    DWORD type;
    union {
        KEYBDINPUT ki;
        unsigned char reserved[32];
    };
} INPUT;

DWORD GetModuleFileNameW(HMODULE module, WCHAR *filename, DWORD size);
BOOL GetModuleHandleExW(DWORD flags, LPCWSTR module_name, HMODULE *module);
DWORD GetCurrentProcessId(void);
ULONGLONG GetTickCount64(void);
BOOL DisableThreadLibraryCalls(HMODULE module);

BOOL InitializeCriticalSectionAndSpinCount(
    CRITICAL_SECTION *critical_section, DWORD spin_count);
void DeleteCriticalSection(CRITICAL_SECTION *critical_section);
void EnterCriticalSection(CRITICAL_SECTION *critical_section);
void LeaveCriticalSection(CRITICAL_SECTION *critical_section);
void InitializeConditionVariable(CONDITION_VARIABLE *condition_variable);
BOOL SleepConditionVariableCS(CONDITION_VARIABLE *condition_variable,
    CRITICAL_SECTION *critical_section, DWORD milliseconds);
void WakeConditionVariable(CONDITION_VARIABLE *condition_variable);
void WakeAllConditionVariable(CONDITION_VARIABLE *condition_variable);

HANDLE CreateThread(LPVOID attributes, size_t stack_size,
    LPTHREAD_START_ROUTINE start_address, LPVOID parameter,
    DWORD creation_flags, DWORD *thread_id);
BOOL CloseHandle(HANDLE object);

DWORD TlsAlloc(void);
BOOL TlsFree(DWORD index);
LPVOID TlsGetValue(DWORD index);
BOOL TlsSetValue(DWORD index, LPVOID value);

HWND GetForegroundWindow(void);
DWORD GetWindowThreadProcessId(HWND window, DWORD *process_id);
UINT SendInput(UINT input_count, INPUT *inputs, int input_size);

#endif
