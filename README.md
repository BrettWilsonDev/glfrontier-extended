# GLFrontier Extended

## IMPORTANT INFORMATION

* **Build System**: This project uses CMake.
* **Dependencies**: This project dependencies are automatically downloaded and managed by CMake or are in the `vendor` folder.

## The Game Itself
Some special keys to note:

* Ctrl-F11	- Toggle fullscreen.
* Ctrl-E	- Toggle hardware GL / original software renderers.
* Ctrl-M	- Toggle mouse grabbing.
* Ctrl-Q	- Quit.
* Ctrl-F    - Toggles m68k menu (cheat menu).
* F	        - Toggles fps readout.

## What's New in This Fork
* This fork has replaced the old build system with CMake.
* Dedicated software renderer that makes use of SDL internal renderer
* Wasm through Emscripten that allows the game to be played int he browser at full speed (software rendering only)
* Touch screen support with keyboard keys having dedicated touch buttons (software rendering only)
* gcc, clang, msvc compilers supported

## Dependencies
Required: 
* GCC or Clang or MSVC compilers
* CMake
* emscripten sdk (only for wasm builds)

## How to Build

1. Download this repository
2. Create a build directory: `mkdir build`
3. Change into the build directory: `cd build`
4. Run CMake: `cmake -DCMAKE_BUILD_TYPE=Release ..`
5. Build the project: `cmake --build . --config Release`

* you may have to redo step 4 and 5 after it throws an unknown header error as sometimes glad just builds late.

## TODO

* Move the legacy opengl (fixed function pipline) code to opengl version 3.0.
* Web build support for the opengl renderer. (fixed function pipline wont run)
* Add more cheat options.
* Android build?

## Acknowledgments
* Tom Morton original author of GLFrontier Wayback machine archive [Tom Morton - GLFrontier](https://web.archive.org/web/20171014043201/http://tom.noflag.org.uk/glfrontier.html)
* This project is forked from: [Pcercuei's Copy of GLFrontier](https://github.com/pcercuei/glfrontier)
* Incorporates additional code from: [GLFrontier-win32](https://github.com/Kochise/GLFrontier-win32.git)
* sdl2 for windowing and input: [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2)
* physFs for file system (loading and saving save game): [physFs](https://github.com/icculus/physfs)
* minivorbis for audio: [MiniVorbis](https://github.com/edubart/minivorbis)
* nuklear for gui: [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear)
* glad for loading opengl functions: [glad](https://github.com/Dav1dde/glad)
* Rip of the GLU tesselator into a standalone static library: [glutess](https://github.com/mlabbe/glutess)