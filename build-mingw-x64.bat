@echo off
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake was not found. Install it or add it to PATH.
    exit /b 1
)

where gcc >nul 2>nul
if errorlevel 1 (
    echo MinGW-w64 GCC was not found. Install it or add it to PATH.
    exit /b 1
)

py -3 tools\generate_exports.py
if errorlevel 1 exit /b 1

cmake -S . -B build\mingw-x64 -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1

cmake --build build\mingw-x64
if errorlevel 1 exit /b 1

ctest --test-dir build\mingw-x64 --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo Built: dist\PalworldKeyInjector.dll
