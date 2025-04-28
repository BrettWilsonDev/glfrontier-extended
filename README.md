# IMPORTANT INFORMATION

* **Compiler Requirement**: This project will not build without a GCC compiler.
* **Port Status**: The port is not 100% working, but it is very close. Note that:
	+ GL rendering is not implemented, so the game will be in software rendering mode.
	+ web build is not working despite the bat file in tools

## The Game Itself
Some special keys to note:

* Ctrl-F11	- Toggle fullscreen.
* Ctrl-E	- Toggle hardware GL / original software renderers.
* Ctrl-M	- Toggle mouse grabbing.
* Ctrl-Q	- Quit.
* F	- Toggles some debug info & fps readout.

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

* Fix OpenGL rendering
* Make other compilers work and get rid of OS-specific headers
* Fix Web build (if possible)

## Acknowledgments
Contributor to the original project: https://github.com/pcercuei/glfrontier

