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
* Ctrl-F    - Toggles m68k menu.
* F	        - Toggles fps readout.

## What's New in This Fork
* This fork has replaced the old build system with CMake.
* Dedicated software renderer that makes use of SDL internal renderer
* Wasm through Emscripten that allows the game to be played int he browser at full speed
* Touch screen support with keyboard keys having dedicated touch buttons


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
* Tom Morton original author of GLFrontier Wayback machine archive [Tom Morton - GLFrontier](https://web.archive.org/web/20171014043201/http://tom.noflag.org.uk/glfrontier.html)
* This project builds upon the work of: [Pcercuei's Copy of GLFrontier](https://github.com/pcercuei/glfrontier)
* Incorporates additional code from: [GLFrontier-win32](https://github.com/Kochise/GLFrontier-win32.git)
* sdl2 for windowing and input: [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2)
* physFs for file system (loading and saving save game): [physFs](https://github.com/icculus/physfs)
* minivorbis for audio: [MiniVorbis](https://github.com/edubart/minivorbis)
* nuklear for gui: [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear)