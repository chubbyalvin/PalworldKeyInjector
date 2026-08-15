@echo off
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake was not found. Install it or add it to PATH.
    exit /b 1
)

py -3 tools\generate_exports.py
if errorlevel 1 exit /b 1

cmake -S . -B build\msvc-x64 -A x64 -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1

cmake --build build\msvc-x64 --config Release
if errorlevel 1 exit /b 1

ctest --test-dir build\msvc-x64 -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo Built: dist\PalworldKeyInjector.dll
