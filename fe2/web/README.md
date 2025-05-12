### Compilation Note
These libraries, `libfe2_part_one.a` and `libfe2_part_two.a`, are used for Emscripten builds to build for the web.

Although it is possible to compile `fe2.s.c` as objects, it takes a considerable amount of time. To expedite the process for Emscripten web builds, pre-compiled static libraries are provided.