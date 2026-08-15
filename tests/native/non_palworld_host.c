#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

typedef int (*lua_entry_point)(void *state);

int main(int argc, char **argv)
{
    HMODULE library;
    lua_entry_point api;
    lua_entry_point inject_f10;

    if (argc != 2) {
        fprintf(stderr, "Usage: non_palworld_host.exe <PalworldKeyInjector.dll>\n");
        return 2;
    }

    library = LoadLibraryA(argv[1]);
    if (library == NULL) {
        fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }

    api = (lua_entry_point)GetProcAddress(library, "palworld_api_v1");
    inject_f10 = (lua_entry_point)GetProcAddress(library, "palworld_inject_f10");
    if (api == NULL || inject_f10 == NULL) {
        fputs("Required exports are missing.\n", stderr);
        FreeLibrary(library);
        return 1;
    }

    (void)api(NULL);
    (void)inject_f10(NULL);
    Sleep(150);
    puts("Request made from a non-Palworld host. No input should have been emitted.");
    FreeLibrary(library);
    return 0;
}
