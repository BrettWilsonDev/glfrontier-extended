# vendor

Third-party code the build uses as is: nothing is downloaded at build time.

| Directory    | Library | Version | Built by |
|--------------|---------|---------|----------|
| `SDL2`       | [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2) | `cee83f2d9a91` (SDL2 branch, 2.33) | its own CMakeLists.txt, via `cmake/GlfDeps.cmake` |
| `physfs`     | [PhysicsFS](https://github.com/icculus/physfs) | `c30aa9086d4c` | `cmake/GlfDeps.cmake` |
| `glad`       | [glad](https://github.com/Dav1dde/glad) 0.1.36 output: OpenGL 3.3 core, no extensions | v0.1.36 | `cmake/GlfDeps.cmake` |
| `imgui`      | [Dear ImGui](https://github.com/ocornut/imgui) | v1.91.9 | `cmake/GlfDeps.cmake` |
| `glutess`    | [glutess](https://github.com/mlabbe/glutess) | | `CMakeLists.txt` |
| `minivorbis` | [minivorbis](https://github.com/edubart/minivorbis) | | header only |
| `minshell.html` | web page template for the emscripten build | | |

SDL2, PhysFS, glad and imgui are stripped down to what the game's builds use
(Windows, Linux, Android; the web build only takes SDL's headers, emscripten
brings its own SDL2). To update one, copy the new version's files over the
ones here (the same files, plus any new file the kept ones need) and build
every target.

* SDL2: no macOS / iOS / consoles / WinRT / Haiku / OS/2 sources, no tests, docs,
  IDE projects or autotools build. The drivers without sources are forced off in
  `cmake/GlfDeps.cmake` (Vulkan, Direct3D renderer, disk audio, offscreen,
  KMS/DRM, JACK, OSS, ...), and HIDAPI is Android only.
* PhysFS: directories only, every archiver (zip, 7z, ...) is left out.
* glad: the generated loader only, not the generator: no Python at build time.
  (To change the GL version, run glad 0.1.36 once with
  `--profile core --api gl=3.3 --extensions= --generator c` and replace these files.)
* imgui: the core and the SDL2 + OpenGL3 backends.
