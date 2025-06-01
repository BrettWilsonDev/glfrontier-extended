### You must provide your own `fe2.s` file

- Check the links to related projects in the main [README](../../README.md) — some of them may include `fe2.s` in their repositories.

- After building this repo using [build_as68k](./build_as68k), and once the `as68k` executable is available, run the `build-fe2-bin-c` script.  
  This script requires `fe2.s` to be in the **same directory** as both the script and the `as68k` executable.  
  It will generate two output files: `fe2.s.c` and `fe2.s.bin`.