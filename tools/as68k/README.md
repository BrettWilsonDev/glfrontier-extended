### Assembling the game

The game's 68k source is `fe2/fe2_modded.s` (Tom Morton's disassembly of
Frontier: Elite 2, annotated and modded).

- Easiest: from the repository root run `python tools/build_fe2.py`. It
  finds `as68k` (build it with [build_as68k](./build_as68k.bat) first) and
  updates `fe2/fe2_modded.s.c`, `fe2/fe2_bin.h` and `fe2/fe2_labels.h`.

- By hand: after building as68k, copy `fe2_modded.s` next to the `as68k`
  executable and run the generated `build-fe2-bin-c` script there. It
  writes `fe2_modded.s.c`, `fe2_modded.s.bin` and `fe2_bin.h`.
