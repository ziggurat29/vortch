@echo off
rem Usage: build.bat [preset]   (default preset: core)
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || (echo VCVARS_FAILED & exit /b 1)
rem Set AFTER vcvars: vcvars exports its own VCPKG_ROOT (VS-bundled) which we override.
set "VCPKG_ROOT=C:\Users\lemleyd\vcpkg"
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=core"
echo === configure (%PRESET%) ===
cmake --preset %PRESET% || exit /b 1
echo === build (%PRESET%) ===
cmake --build --preset %PRESET% || exit /b 1
echo === test (%PRESET%) ===
ctest --test-dir build/%PRESET% --output-on-failure
endlocal
