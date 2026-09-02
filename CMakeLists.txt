cmake_minimum_required(VERSION 3.21)

project(PalworldKeyInjector VERSION 1.1 LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "PalworldKeyInjector.dll must be built for Windows")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "PalworldKeyInjector requires a 64-bit build")
endif()

add_library(PalworldKeyInjector SHARED
    src/palworld_keyinjector.c
    src/palworld_key_policy.c
    src/palworld_rate_limiter.c
    PalworldKeyInjector.def
)

target_compile_features(PalworldKeyInjector PRIVATE c_std_11)
target_include_directories(PalworldKeyInjector PRIVATE src)
target_compile_definitions(PalworldKeyInjector PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set_target_properties(PalworldKeyInjector PROPERTIES
    PREFIX ""
    OUTPUT_NAME "PalworldKeyInjector"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/dist"
)

target_link_libraries(PalworldKeyInjector PRIVATE kernel32 user32)

if(MSVC)
    target_compile_options(PalworldKeyInjector PRIVATE /W4 /WX /GS- /Zl)
    target_link_options(PalworldKeyInjector PRIVATE
        /NODEFAULTLIB /ENTRY:DllMain /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
elseif(MINGW)
    target_compile_options(PalworldKeyInjector PRIVATE
        -Wall -Wextra -Wpedantic -Werror -ffreestanding -fno-builtin
        -fno-stack-protector)
    target_link_options(PalworldKeyInjector PRIVATE
        -nostdlib -Wl,--entry,DllMain,--dynamicbase,--nxcompat,--high-entropy-va)
else()
    target_compile_options(PalworldKeyInjector PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_link_options(PalworldKeyInjector PRIVATE
        -Wl,--dynamicbase,--nxcompat,--high-entropy-va)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(PalworldKeyInjectorPolicyTests
        tests/native/policy_tests.c
        src/palworld_key_policy.c
        src/palworld_rate_limiter.c
    )
    target_compile_features(PalworldKeyInjectorPolicyTests PRIVATE c_std_11)
    target_include_directories(PalworldKeyInjectorPolicyTests PRIVATE src)
    if(MSVC)
        target_compile_options(PalworldKeyInjectorPolicyTests PRIVATE /W4 /WX)
    else()
        target_compile_options(PalworldKeyInjectorPolicyTests PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME PalworldKeyInjectorPolicyTests
        COMMAND PalworldKeyInjectorPolicyTests)

    add_executable(PalworldKeyInjectorNonPalworldHost
        tests/native/non_palworld_host.c)
    if(MSVC)
        target_compile_options(PalworldKeyInjectorNonPalworldHost PRIVATE /W4)
    else()
        target_compile_options(PalworldKeyInjectorNonPalworldHost PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()
