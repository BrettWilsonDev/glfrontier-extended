@echo off
rem ==========================================================================
rem build.bat - configure and build GLFrontier with whatever is installed.
rem
rem   build.bat [modded^|original^|both^|android^|all] [debug^|release] [run]
rem
rem   modded    the game code with this fork's mods (default)  -> build\
rem   original  the unmodified game code (GLF_MODDED_FE2=OFF)  -> build-original\
rem   both      build both
rem   android   both Android APKs (tools\build-android.bat)
rem             -> build-android\GLFrontier.apk, GLFrontier-original.apk
rem   all       both exes and both APKs, e.g. for a release
rem   debug / release   build type (default: release; the APKs are always release)
rem   run       start the game after building (not with "both" / "android" / "all")
rem
rem Tools, first one found wins, for a new build directory:
rem   generator: Ninja, MinGW Makefiles (mingw32-make), else CMake's default
rem              (Visual Studio)
rem   compiler:  clang, gcc, else CMake's default (MSVC)
rem An existing build directory keeps the generator / compiler it was
rem configured with (CMake cannot switch them in place).
rem ==========================================================================
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
set "VARIANT=modded"
set "CONFIG=Release"
set "RUN="

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="modded"   (set "VARIANT=modded"   & shift & goto parse)
if /i "%~1"=="original" (set "VARIANT=original" & shift & goto parse)
if /i "%~1"=="both"     (set "VARIANT=both"     & shift & goto parse)
if /i "%~1"=="android"  (set "VARIANT=android"  & shift & goto parse)
if /i "%~1"=="all"      (set "VARIANT=all"      & shift & goto parse)
if /i "%~1"=="debug"    (set "CONFIG=Debug"     & shift & goto parse)
if /i "%~1"=="release"  (set "CONFIG=Release"   & shift & goto parse)
if /i "%~1"=="run"      (set "RUN=1"            & shift & goto parse)
if /i "%~1"=="-h"     goto usage
if /i "%~1"=="/?"     goto usage
if /i "%~1"=="help"   goto usage
echo Unknown option: %~1
goto usage
:parsed

if "%VARIANT%"=="android" (
    if defined RUN echo "run" is ignored with "android": use tools\build-android.bat --run
    call :android || exit /b 1
    exit /b 0
)

where cmake >nul 2>nul || (echo cmake not found on PATH & exit /b 1)

if "%VARIANT%"=="both" (
    if defined RUN echo "run" is ignored with "both".
    set "RUN="
    call :build modded   || exit /b 1
    call :build original || exit /b 1
    exit /b 0
)
if "%VARIANT%"=="all" (
    if defined RUN echo "run" is ignored with "all".
    set "RUN="
    call :build modded   || exit /b 1
    call :build original || exit /b 1
    call :android        || exit /b 1
    echo === all built: build\, build-original\ and build-android\*.apk
    exit /b 0
)
call :build %VARIANT% || exit /b 1
if defined RUN call :run %VARIANT%
exit /b %errorlevel%

rem --------------------------------------------------------------------------
:build
if "%~1"=="original" (
    set "DIR=%ROOT%build-original"
    set "MODDED=OFF"
) else (
    set "DIR=%ROOT%build"
    set "MODDED=ON"
)

set "GEN_ARGS="
if exist "!DIR!\CMakeCache.txt" (
    echo === %~1: reusing the setup in !DIR!
) else (
    call :pick_tools
    echo === %~1: new build directory !DIR! [!TOOLS!]
)

cmake -S "%ROOT%." -B "!DIR!" !GEN_ARGS! -DCMAKE_BUILD_TYPE=%CONFIG% -DGLF_MODDED_FE2=!MODDED!
if errorlevel 1 (echo === %~1: configure FAILED & exit /b 1)

cmake --build "!DIR!" --config %CONFIG% --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 (echo === %~1: build FAILED & exit /b 1)
echo === %~1: built in !DIR!
exit /b 0

rem --------------------------------------------------------------------------
:android
echo === android: modded APK
call "%ROOT%tools\build-android.bat" --variant modded
if errorlevel 1 (echo === android modded: build FAILED & exit /b 1)
echo === android: original APK
call "%ROOT%tools\build-android.bat" --variant original
if errorlevel 1 (echo === android original: build FAILED & exit /b 1)
echo === android: built build-android\GLFrontier.apk and GLFrontier-original.apk
exit /b 0

rem --------------------------------------------------------------------------
:pick_tools
set "GEN="
set "CC="
set "CXX="
where ninja >nul 2>nul && set "GEN=Ninja"
if not defined GEN where mingw32-make >nul 2>nul && set "GEN=MinGW Makefiles"
if defined GEN (
    where clang >nul 2>nul && where clang++ >nul 2>nul && (set "CC=clang" & set "CXX=clang++")
    if not defined CC where gcc >nul 2>nul && where g++ >nul 2>nul && (set "CC=gcc" & set "CXX=g++")
    rem Ninja with MSVC needs a Developer Command Prompt (cl on PATH)
    if not defined CC (
        where cl >nul 2>nul && (set "CC=cl" & set "CXX=cl")
    )
    if not defined CC set "GEN="
)
if defined GEN (
    set GEN_ARGS=-G "!GEN!" -DCMAKE_C_COMPILER=!CC! -DCMAKE_CXX_COMPILER=!CXX!
    set "TOOLS=!GEN!, !CC!"
) else (
    set "TOOLS=CMake default generator and compiler"
)
exit /b 0

rem --------------------------------------------------------------------------
:run
if "%~1"=="original" (set "DIR=%ROOT%build-original") else (set "DIR=%ROOT%build")
set "EXE="
for %%E in ("!DIR!\GLFrontier.exe" "!DIR!\%CONFIG%\GLFrontier.exe") do (
    if not defined EXE if exist "%%~E" set "EXE=%%~E"
)
if not defined EXE (echo GLFrontier.exe not found in !DIR! & exit /b 1)
echo === starting !EXE!
pushd "!DIR!"
start "" "!EXE!"
popd
exit /b 0

rem --------------------------------------------------------------------------
:usage
echo.
echo   build.bat [modded^|original^|both^|android^|all] [debug^|release] [run]
echo.
echo   modded    game code with this fork's mods (default)  -^> build\
echo   original  unmodified game code                        -^> build-original\
echo   both      build both
echo   android   both Android APKs                           -^> build-android\
echo   all       both exes and both APKs
echo   debug / release   build type (default: release)
echo   run       start the game after building
echo.
echo   e.g.  build.bat original run
echo         build.bat both debug
echo         build.bat all
exit /b 1
