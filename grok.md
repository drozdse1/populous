# Grok Project Documentation - Populous C/SDL2 Port

## Project Overview

This repository is a fork of [tetracorp/populous](https://github.com/tetracorp/populous), the high-quality IRA disassembly of the Amiga game *Populous* (specifically the clean 1993 Hit Squad budget release).

The goal of this fork is to maintain the original disassembly while adding a modern, low-level C port using SDL2 that closely follows the original 68000 assembly logic.

### Key Principles of the Port

- **Fidelity to the disassembly**: We translate the code as directly as possible rather than reimplementing the game from a high-level design.
- **A4 BASEREG emulation**: The original heavily uses A4 as a base register for globals. The C port emulates this with a large byte array + offset macros.
- **Low-level graphics**: SDL2 is used to emulate Amiga bitplanes, copper lists, sprites, palette, and screen swapping — no high-level game engines or tilemaps.
- **Launcher + prg split preserved**: The original game has a small launcher (`populous`) that loads and jumps into the main game module (`populous.prg`). Our port mirrors this split (`populous_init.c` + `populous_prg.c`).
- **Incremental translation**: Functions are translated one logical chunk at a time as assembly is provided.

## Current Status (as of June 2026)

### Translated Components

**Launcher side** (`populous_init.c`):
- Early bootstrap (SUB_384, SUB_11A, SUB_16E, etc.)
- Library opening (graphics.library, intuition.library)
- Big CHIP RAM allocations
- Interactive loader (demo.pic / load.pic + mouse region selection)
- "populous.prg" loading path

**Main game module** (`populous_prg.c`):
- `_main` (real entry point of populous.prg)
- `_rs`, `_closeall`
- Enhanced quick-test demo (CLI N levels still primary): richer "bild/ground" loading shim with distinct per-terrain palettes, improved textured renderer (seed phase + height-edge detail), live 'p' toggle for paint_map / population-dot view (exercises transcribed BTST overlay), 'v' volcano raise + existing 'q' quake + mouse sculpt, _move_peeps now level-speed + height-biased for terrain differentiation. All still fully buildable + visible in SDL without further asm.
- Mouse / interrupt setup (`_create_mouse`, `_setup_interrupt`, "petersint" VBI server)
- Serial interrupt setup
- Library open/close
- `_setup_display` (full conquest parameter handling, seed/terrain/powers/game_mode logic, screen setup, first people placement)
- `_animate` (core game heartbeat, including numpad scrolling, debug keys A/F9/*, peeps view dots, conquest flow, etc.)

Many callees inside animate/setup_display are still stubbed (printf + TODO or simple visible placeholders for motivation).

### Visible Demo Mode

The port currently includes "motivation mode" drawing:
- Immediate test pattern on prg entry (green background + white box)
- Simple terrain grid driven by the same `seed` + `ground_in` logic from the translated `_setup_display`
- Moving demo "peeps" dots
- Numpad-controlled scrolling (real `xoff`/`yoff`)
- Basic keyboard handling (ESC to quit, A key for debug win effect)

This was added so there is something visually rewarding to see after each translation step.

## Directory Structure in This Fork

```
populous/
├── README.md                 # High-level overview
├── grok.md                   # This file - detailed development notes
├── .gitignore
├── asm/                      # Original disassembly files (from tetracorp)
│   └── README.txt
└── populous_c_port/          # C + SDL2 translation
    ├── Makefile
    ├── README.md
    ├── amiga_hardware.h
    ├── amiga_globals.h
    ├── sdl_lowlevel.h / .c
    ├── symbols.h
    ├── symbols_prg.h
    ├── populous_init.c
    └── populous_prg.c
```

## Build & Run (C Port)

```bash
cd populous_c_port
make clean && make
./populous_init_port
```

Run from the terminal (do **not** double-click the binary on macOS — it will be treated as a console app and the SDL window may not appear properly).

## Development History (with Grok)

The port was developed iteratively through direct translation sessions:

1. First launcher chunk (SUB_4 through SUB_536 + data tables)
2. Full symbol map for the prg module (ENTRY $3E1E8, BASEREG A4 at $593EE, all CODE/TEXT ranges, 100+ symbols, 1993-specific BANNER notes)
3. Actual code bodies for `_main`, interrupt/serial setup, `_openlibrary`/`_closelibrary`, `_setup_display` (including detailed conquest byte handling), and a large part of `_animate` (scrolling, debug keys, peeps overview, etc.)

All translations aim to preserve:
- Original control flow and comments from the IRA listing
- 1993-vs-1989 differences noted in the disassembly
- Exact A4-relative accesses via displacement macros

## Future Plans / Next Steps

- Continue translating function bodies as more assembly is provided (`_move_peeps`, `_draw_map`, power effects, sound, serial 2P, save/load, etc.)
- Improve the SDL low-level layer (proper bitplane handling, sprite conversion from the chunk, copper list interpretation)
- Integrate real graphics data (icon_data, font_data, ground tiles, peep sprites) once the code that consumes them is translated
- Eventually support loading real level data and running actual conquest levels

## Notes for Future Sessions

- Always run the C port from the terminal, not by double-clicking.
- The current visible drawing is intentionally simplified for motivation. Real rendering should follow the translated `_draw_it`, sprite routines, etc. once available.
- Keep A4 emulation accurate — many bugs in ports like this come from incorrect global layout.
- The conquest byte handling in `_setup_display` is particularly important and matches the level format analysis from tetracorp.github.io.

---

## Recent Progress (Quick-Test Focus)

Prioritized areas that allow fast iteration and visible/terminal feedback without needing the full real map or sprite code yet:

### Demo Conquest Level Table + Fast "Level Loading"
- Added a small static table `demo_levels[8][9]` modeled after the real `level.dat` format (enemy rating, speed, power bitfields, game mode, terrain, pop hints, seed offset).
- Command-line support: `./populous_init_port N` now loads the N%8 entry and applies the values to the emulated conquest variables (`conquest`, `conq_01_speed`, `conq_02_enemypow`, `conq_03_yourpow`, `game_mode`, `conq_05_terrain`, `conq_08_seed`, `in_conquest`).
- This directly exercises the translated `_setup_display` conquest path and lets you instantly see different "worlds".

### Game Init Status Visibility
- After the translated `_setup_display`, a clear `[INIT]` line is printed with the key values:
  ```
  [INIT] conquest=... speed=... enemy_pow=0x... your_pow=0x... mode=... terrain=... seed=...
  ```
- `_make_map` (demo version using the real `_newrand`) prints height range and current demo peeps count.

### Load Ground / Bitmap Shim Progress
- `___load_ground` is no longer a pure printf stub. It now actually calls `amiga_set_color` on the SDL screen to shift the base palette according to the terrain type.
- Different CLI numbers now produce visibly different color schemes in the window (grass vs desert vs snow vs rocky tint).

### Visible Demo Rendering Improvements (for Motivation + Testing)
- Height-based shading in `_draw_it` (the function called from the translated `_draw_map` path).
- Demo peeps are placed by `_place_first_people` using conquest-derived counts and the real RNG.
- Peeps are drawn as prominent bright blocks so population differences are obvious at a glance.
- Numpad scrolling (real `xoff`/`yoff` from the assembly) still works for exploring the generated map.

### How to Test Quickly
```bash
cd populous_c_port
make
./populous_init_port 0
./populous_init_port 2
./populous_init_port 5
./populous_init_port 7
```
Each run should show different `[QUICKTEST]` / `[INIT]` output and a different-looking landscape + different number of visible peeps.

### Next Priorities (still focused on testability)
- Expand the demo level table or wire a real small `level.dat` slice.
- Start using more real A4 map storage (map_who, map_alt, map_blk areas) instead of the isolated demo array.
- Basic population / "town" init from the starting pop bytes.
- First real ground tile / image loading shim (beyond just palette) so terrains look structurally different.

All changes are in the `populous_c_port/` subtree and documented here for the fork.

---

*This file is maintained as living project documentation for the C/SDL2 port effort.*