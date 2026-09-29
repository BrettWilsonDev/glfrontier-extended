@echo off
rem Build FE2ShipBuilder.exe (FE2 Ship Builder) into tools\fe2ShipBuilder\build.
rem Uses the same tools as build.bat: Ninja or MinGW Makefiles with clang or
rem gcc when found, else CMake's defaults. The game code headers it compiles in
rem come from fe2\ (run tools\build_fe2.py first if fe2_modded.s changed).
rem
rem Every clang/gcc on PATH is checked, not just the first: cross toolchains
rem (Arm Toolchain for Embedded, LLVM Embedded Toolchain for Arm,
rem arm-none-eabi, ...) also install a clang.exe but cannot build Windows
rem programs. A compiler is only used if it targets MinGW (the GNU Windows
rem ABI the non-MSVC flags in CMakeLists.txt are written for) and can compile,
rem link and run a small C and C++ test program. Plain LLVM clang targeting
rem windows-msvc is skipped too; run from a VS prompt without Ninja for MSVC.
setlocal EnableDelayedExpansion
pushd %~dp0

where cmake >nul 2>nul || (echo cmake not found on PATH & popd & pause & exit /b 1)

set "TEST_DIR=%TEMP%\fe2shipbuilder_cc_test"

rem A cache left by a failed configure with a cross compiler would be reused
rem forever, so drop it when its compiler fails the same check.
if exist build\CMakeCache.txt call :check_cache

set "GEN_ARGS="
if not exist build\CMakeCache.txt (
    set "GEN="
    set "CC="
    where ninja >nul 2>nul && set "GEN=Ninja"
    if not defined GEN where mingw32-make >nul 2>nul && set "GEN=MinGW Makefiles"
    if defined GEN (
        call :find_compilers
        if not defined CC set "GEN="
    )
    if defined GEN (
        echo Using !CC! and !CXX!
        set GEN_ARGS=-G "!GEN!" "-DCMAKE_C_COMPILER=!CC!" "-DCMAKE_CXX_COMPILER=!CXX!"
    )
)

cmake -S . -B build !GEN_ARGS! -DCMAKE_BUILD_TYPE=Release || goto failed
cmake --build build --config Release --parallel %NUMBER_OF_PROCESSORS% || goto failed

if exist "%TEST_DIR%" rmdir /s /q "%TEST_DIR%"
echo ============== DONE: tools\fe2ShipBuilder\build\FE2ShipBuilder.exe ==============
popd
pause
exit /b 0

:failed
if exist "%TEST_DIR%" rmdir /s /q "%TEST_DIR%"
echo ============== BUILD FAILED ==============
popd
pause
exit /b 1

rem ---------------------------------------------------------------------------
rem Sets CC/CXX to the full paths of the first clang/clang++ (else gcc/g++)
rem pair on PATH that passes :try_pair. Full paths are passed to CMake so it
rem cannot resolve the bare name back to a cross compiler earlier on PATH.
:find_compilers
set "CC="
set "CXX="
for /f "delims=" %%i in ('where clang 2^>nul') do if not defined CC call :try_pair "%%~i" "%%~dpiclang++.exe"
for /f "delims=" %%i in ('where gcc 2^>nul') do if not defined CC call :try_pair "%%~i" "%%~dpig++.exe"
exit /b 0

rem :try_pair <c compiler> <c++ compiler>
:try_pair
if not exist "%~2" exit /b 1
call :test_compiler "%~1" c || exit /b 1
call :test_compiler "%~2" c++ || exit /b 1
set "CC=%~1"
set "CXX=%~2"
set "CC=!CC:\=/!"
set "CXX=!CXX:\=/!"
exit /b 0

rem :test_compiler <compiler> <c|c++>
rem Fails unless the compiler targets MinGW and its test program runs.
:test_compiler
set "MACHINE="
for /f "delims=" %%m in ('"%~1" -dumpmachine 2^>nul') do set "MACHINE=%%m"
if not defined MACHINE (
    echo Skipping %1: not a working compiler
    exit /b 1
)
set "HOST_OK="
echo !MACHINE! | findstr /i /l "mingw windows-gnu" >nul && set "HOST_OK=1"
if not defined HOST_OK (
    echo Skipping %1: targets !MACHINE!, not MinGW
    exit /b 1
)
if not exist "%TEST_DIR%" mkdir "%TEST_DIR%"
del /q "%TEST_DIR%\*" >nul 2>nul
if /i "%~2"=="c++" (
    set "SRC=%TEST_DIR%\t.cpp"
    > "!SRC!" echo #include ^<string^>
    >> "!SRC!" echo int main^(^) { std::string s^("x"^); return static_cast^<int^>^(s.size^(^)^) - 1; }
) else (
    set "SRC=%TEST_DIR%\t.c"
    > "!SRC!" echo #include ^<stdio.h^>
    >> "!SRC!" echo int main^(void^) { return 0; }
)
"%~1" "!SRC!" -o "%TEST_DIR%\t.exe" >nul 2>nul
if errorlevel 1 (
    echo Skipping %1: cannot compile and link a Windows program
    exit /b 1
)
"%TEST_DIR%\t.exe" >nul 2>nul
if errorlevel 1 (
    echo Skipping %1: test program does not run
    exit /b 1
)
exit /b 0

rem Deletes build\ when the cached C compiler (clang/gcc only; MSVC's cl.exe
rem needs its own environment) fails :test_compiler. Only removing the cache
rem is not enough: the dependency sub-builds keep the old compiler too.
:check_cache
set "OLD_CC="
for /f "tokens=1,* delims==" %%a in ('findstr /b /c:"CMAKE_C_COMPILER:" build\CMakeCache.txt') do set "OLD_CC=%%b"
if not defined OLD_CC exit /b 0
for %%f in ("!OLD_CC!") do if /i "%%~nxf"=="cl.exe" exit /b 0
call :test_compiler "!OLD_CC!" c && exit /b 0
echo Cached compiler is not a working MinGW compiler; reconfiguring.
rmdir /s /q build
exit /b 0
