# Populous (Amiga) - Disassembly + C/SDL2 Port

This is a fork of the excellent [tetracorp/populous](https://github.com/tetracorp/populous) disassembly project.

The original work provides a clean IRA disassembly of the 1993 Hit Squad budget release of *Populous*.

## Additions in This Fork

- `populous_c_port/` — An incremental, low-level C + SDL2 translation of the disassembly.
  - Preserves the original launcher + `populous.prg` split.
  - Emulates A4 as base register for globals.
  - Uses SDL2 for original isometric LAND graphics, small sprites and the original game background; full presentation remains incomplete.
  - Repairs signed A4 storage, native-pointer ownership, initial map/population setup, reset and clean shutdown.

The port now has a **bounded original-rule game loop**: movement, settlement, growth, mana, ordinary combat, legal sculpting and population-based outcomes use external original `landN` rules. Automated SDL-input matches reach victory/defeat. Independent X11/WSLg checks also observed growth, combat, a stable defeat, reset and clean exit through agent-driven inputs at human rates. This is a playable intermediate port, not a complete Amiga conversion: several rule branches, powers and strategic AI are unported, and a human-tested winning strategy has not been demonstrated. Original world initialization is available separately through `--level N`; see the port guide for bounded overlapping-start support and remaining campaign/state-machine limitations.

See [the current port guide](./populous_c_port/README.md) for tested commands, controls and limitations. [grok.md](./grok.md) is historical development context, not the current feature contract.

`./populous_c_port/populous_init_port N` selects one of eight synthetic scenarios using `N % 8`; no argument selects slot 0. `./populous_c_port/populous_init_port --level N` selects an original world numbered 0..494 without wrapping. Invalid or negative arguments fail. Both initial populations and terrain are generated before the first game frame.

## Directory Structure

```
.
├── src/                      # Original 68000 disassembly (from tetracorp)
├── populous_c_port/          # C/SDL2 port (our translation work)
├── grok.md                   # Detailed project + development documentation
└── README.md
```

## Building the C Port

```bash
make -C populous_c_port
./populous_c_port/populous_init_port --level 0
```

Requires GCC-compatible C11, GNU Make and SDL2 development files. The commands use
the supplied export under `binaries/populous-extracted`, including `raw/level.dat`.
For this machine's persistent WSL toolchain, source
`$HOME/.local/share/populous-toolchain/env.sh` first; see the port guide.
Run from a terminal with a working graphical display. Other platforms have not
been verified by this migration step.

## Credits

- Original disassembly & analysis: tetracorp (https://tetracorp.github.io/populous)
- C/SDL2 port work: developed incrementally with Grok

This is for study, preservation, and reverse-engineering education. You will still need a legally obtained copy of the original game.
