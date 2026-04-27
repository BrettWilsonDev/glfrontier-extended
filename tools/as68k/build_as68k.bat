@echo off

pushd %~dp0

mkdir .\build

cd .\build

call cmake -DCMAKE_BUILD_TYPE=Release ..

cmake --build . --config Release

echo ============== DONE ==============
pause