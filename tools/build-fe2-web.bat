@echo off

pushd %~dp0

mkdir ..\build-em

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
    echo Game code is split into small functions ^(tools/flatten_fe2.py^): a few minutes at most
    call emcmake cmake .. -DPLATFORM=Web -DUSE_SDL3=OFF -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXECUTABLE_SUFFIX=".html" -DASYNCIFY=1
)

call emmake make -j%NUMBER_OF_PROCESSORS%

echo ============== DONE ==============
pause