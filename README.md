# GLFrontier Extended

A fork of GLFrontier (OpenGL Frontier Elite 2) that modernizes the build system and adds cross-platform support, including WebAssembly and Android.

## Game Controls

* Ctrl-F11	- Toggle fullscreen.
* Ctrl-E	- Toggle hardware GL / original software renderers.
* Ctrl-M	- Toggle mouse grabbing.
* Ctrl-Q	- Quit.
* Ctrl-F    - Toggles m68k menu (cheat menu).
* F	        - Toggles fps readout.

## What's New in This Fork Of GLFrontier
* Added native Android build (APK; no external emulator required)
* Added WebAssembly (Wasm) build via Emscripten for browser-based play at full speed
* Implemented touchscreen support with on-screen controls
* Added a dedicated software renderer using SDL's internal renderer
* Migrated build system to CMake
* Supports GCC, Clang, and MSVC

## Dependencies
Automatically managed by CMake or included in the `vendor` directory.

Required: 
* GCC or Clang or MSVC compilers
* CMake

Optional:
* emscripten sdk (only for wasm builds)
* gradle and android sdk (only for android builds)

## How to Build

1. Download this repository
2. Create a build directory: `mkdir build`
3. Change into the build directory: `cd build`
4. Run CMake: `cmake -DCMAKE_BUILD_TYPE=Release ..`
5. Build the project: `cmake --build . --config Release`

## TODO

* Complete set of cheat options
* better rendering (eg: missing planet textures, stop texture jumping)

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

![GLFrontier img](https://brettwilsondev.github.io/glfrontier-extended/imgs/glfe2screenshot1.png "GLFrontier")
![GLFrontier menu img](https://brettwilsondev.github.io/glfrontier-extended/imgs/glfrontier_menu.png "GLFrontier Emulator Menu")
![GLFrontier cheat img](https://brettwilsondev.github.io/glfrontier-extended/imgs/glfrontier_cheats.png "GLFrontier Cheat Menu")
![GLFrontier software renderer img](https://brettwilsondev.github.io/glfrontier-extended/imgs/glfrontier_software_renderer.png "GLFrontier Software Renderer")
![GLFrontier touch controls img](https://brettwilsondev.github.io/glfrontier-extended/imgs/glfrontier_touch.png "GLFrontier Touch Controls")