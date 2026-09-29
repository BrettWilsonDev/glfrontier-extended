@echo off
rem ==========================================================================
rem build-android.bat - build GLFrontier for Android (build-android\GLFrontier.apk)
rem
rem   tools\build-android.bat [options]
rem
rem Passes everything to tools\androidBuild\build_android.py, e.g.
rem   --run                       also install and start it (adb)
rem   --variant original          the unmodded game
rem   --abi arm64-v8a x86_64      CPUs to include (x86_64 for the emulator)
rem   --clean                     delete build-android first
rem
rem Needs Python 3 (no packages, see requirements.txt). The JDK, Android SDK
rem and NDK are downloaded if missing, into %LOCALAPPDATA%\glfrontier-android.
rem ==========================================================================
setlocal

set "PY="
where python >nul 2>nul && set "PY=python"
if not defined PY where py >nul 2>nul && set "PY=py -3"
if not defined PY (echo Python 3 not found on PATH & exit /b 1)

%PY% "%~dp0androidBuild\build_android.py" %*
exit /b %errorlevel%
