@echo off

pushd %~dp0

mkdir ..\build-em

xcopy /E /I /Y "..\assets\" "..\build-em\assets\"

cd ..\build-em

IF "%1"=="async" (
    echo using asyncify
) ELSE IF "%1"=="debug" (
    call emcmake cmake .. -DPLATFORM=Web -DUSE_SDL3=OFF -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXECUTABLE_SUFFIX=".html"
) ELSE IF NOT "%1"=="" (
    echo Unknown argument: %1
    exit /b 1
) ELSE (
    echo using asyncify : default
    echo This will take a while...
    call emcmake cmake .. -DPLATFORM=Web -DUSE_SDL3=OFF -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXECUTABLE_SUFFIX=".html" -DASYNCIFY=1
)

call emmake make

echo ============== DONE ==============
pause