# IMPORTANT INFORMATION

* **Compiler Requirement**: This project will not build without a GCC compiler.
* **Port Status**: The port is not 100% working, but it is very close. Note that:
	+ GL rendering is not implemented, so the game will be in software rendering mode.
	+ Parts of input are not working, but the game can be played.
    + music and some sfx are not working
	+ web build is not working despite the bat file in tools

## What's New in This Fork
This fork has replaced the old build system with CMake.

## Dependencies
Required: 
* GCC Compiler
* CMake

## How to Build

1. Download this repository
2. Create a build directory: `mkdir build`
3. Change into the build directory: `cd build`
4. Run CMake: `cmake -DCMAKE_BUILD_TYPE=Release ..`
5. Build the project: `cmake --build . --config Release`

## TODO

* Fix shortcuts
* Fix OpenGL rendering
* Fix music and sound
* Make other compilers work and get rid of OS-specific headers
* Fix Web build (if possible)