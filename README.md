# IMPORTANT INFORMATION

* **Compiler Requirement**: This project will build with gcc or clang.
* **Build System**: This project uses CMake.
* **Dependencies**: This project has no dependencies sdl2 physFs and minivorbis are all setup using fetchcontent.

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
* GCC Compiler or Clang Compiler
* CMake

## How to Build

1. Download this repository
2. Create a build directory: `mkdir build`
3. Change into the build directory: `cd build`
4. Run CMake: `cmake -DCMAKE_BUILD_TYPE=Release ..`
5. Build the project: `cmake --build . --config Release`

## Acknowledgments
* Contributor to the original project: https://github.com/pcercuei/glfrontier
* sdl2 for windowing and input: https://github.com/libsdl-org/SDL/tree/SDL2
* physFs for file system (loading and saving save game): https://github.com/icculus/physfs
* minivorbis for audio: https://github.com/edubart/minivorbis
