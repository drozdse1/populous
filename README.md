# Populous (Amiga) - Disassembly + C/SDL2 Port

This is a fork of the excellent [tetracorp/populous](https://github.com/tetracorp/populous) disassembly project.

The original work provides a clean IRA disassembly of the 1993 Hit Squad budget release of *Populous*.

## Additions in This Fork

- `populous_c_port/` — An incremental, low-level C + SDL2 translation of the disassembly.
  - Preserves the original launcher + `populous.prg` split.
  - Emulates A4 as base register for globals.
  - Uses SDL2 to emulate Amiga bitplanes, copper, sprites, and custom chip behavior.
  - Currently includes translated `_main`, interrupt/serial setup, `_setup_display` (with full conquest parameter logic), and large parts of `_animate`.

See [grok.md](./grok.md) for detailed development history, current status, quick-test instructions, and notes.

The `populous_c_port/` directory now includes a demo level table and CLI support (`./populous_init_port N`) so you can rapidly test different "level loading" and "game init status" scenarios and immediately see different maps + population counts in the window.

## Directory Structure

```
.
├── asm/                      # Original 68000 disassembly (from tetracorp)
├── populous_c_port/          # C/SDL2 port (our translation work)
├── grok.md                   # Detailed project + development documentation
└── README.md
```

## Building the C Port

```bash
cd populous_c_port
make
./populous_init_port
```

Run from the terminal (especially important on macOS — double-clicking the binary often fails to show the SDL window properly).

## Credits

- Original disassembly & analysis: tetracorp (https://tetracorp.github.io/populous)
- C/SDL2 port work: developed incrementally with Grok

This is for study, preservation, and reverse-engineering education. You will still need a legally obtained copy of the original game.
