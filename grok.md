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

## Analysis of Game Data Structures (sourced from https://tetracorp.github.io/populous/)

The site https://tetracorp.github.io/populous/ (especially the [Level format](https://tetracorp.github.io/populous/analysis/level-format.html) page, mirrored locally in `docs/_posts/`) provides the definitive reverse-engineered explanation of the conquest/level system. We have used it to validate and document the constants and data flow in our C port.

### Level.dat and Conquest Traits
- Real data lives in a 990-byte `level.dat` (99 entries × 10 bytes).
- Each entry controls **five consecutive levels** (traits only change every 5 "battle numbers").
- Our `demo_levels[8][9]` table in `populous_prg.c` is a hand-picked set of representative entries (one or two per terrain) that exercise all the important variations.

Exact byte mapping (matches our array columns and the A4 globals set in the translated `_setup_display`):

| Byte | Meaning                          | Our variable / demo column      | Notes from analysis |
|------|----------------------------------|---------------------------------|---------------------|
| 0    | Enemy rating (1=strongest, 10=weakest) | `conquest` / demo[0]           | Displayed to player as Very Good / Good / Average / Poor / Very Poor |
| 1    | Enemy reaction speed / aggression | `conq_01_speed` / demo[1]      | 1=Very Fast ... 10=Very Slow |
| 2    | Powers bitfield (enemy)          | `conq_02_enemypow` / demo[2]   | See bitfield below |
| 3    | Powers bitfield (you)            | `conq_03_yourpow` / demo[3]    | See bitfield below |
| 4    | Game mode bitfield               | `conq_04_mode` / `game_mode` / demo[4] | Build rules, swamp, water |
| 5    | Terrain type                     | `conq_05_terrain` / `ground_in` / demo[5] | 0-3 (see below) |
| 6    | Your starting population         | demo[6] → `current_start_pop_you` + placed in `_place_first_people` | Max ~30 |
| 7    | Enemy starting population        | demo[7]                        | Used for enemy pop in our demo |
| 8-9  | 16-bit seed offset               | `conq_08_seed` (low byte used) / demo[8] | Added to (battle number mod 8) |

In `_setup_display` (directly from the assembly comments + our translation):
```c
seed = (in_conquest & 7) + conq_08_seed;
start_seed = seed;
ground_in = conq_05_terrain;
...
```

### Power Bitfields (bytes 2/3)
```c
#define POWER_EARTHQUAKE (1u << 0)
#define POWER_SWAMP      (1u << 1)
#define POWER_KNIGHT     (1u << 2)
#define POWER_VOLCANO    (1u << 3)
#define POWER_FLOOD      (1u << 4)
#define POWER_ARMAGEDDON (1u << 5)
```
Armageddon is always available in the bitfield even if not shown on the menu due to space. Our demo values (0x07, 0x0f, 0x1f, 0x3f, 0x01, 0x15, 0x2a ...) are realistic subsets of the 25 distinct combinations that appear in real data.

### Game Mode (byte 4) – detailed bits
```c
#define GAME_MODE_WATER_FATAL       (1u << 0)  // 0=harmful, 1=fatal
#define GAME_MODE_SWAMP_BOTTOMLESS  (1u << 1)  // 0=shallow, 1=bottomless
#define GAME_MODE_NO_BUILD          (1u << 2)
#define GAME_MODE_BUILD_ONLY_UP     (1u << 3)
#define GAME_MODE_BUILD_NEAR_TOWNS  (1u << 4)
```
In real Conquest data the three build bits are mutually exclusive (only one or none set). Custom mode can combine them freely.

### Terrain Types (byte 5)
```c
#define TERRAIN_GRASS   0   // Grass Planes
#define TERRAIN_DESERT  1
#define TERRAIN_SNOW    2   // Snow and Ice
#define TERRAIN_ROCKY   3
```
These directly drive `___load_ground`, the base color in `_draw_it`, feature generation in `_make_map` (rocks on rocky, dunes on desert), and texture modulation. Our current visible renderer already produces clearly different looks for each.

### Starting Population & Other
- Bytes 6/7 are the number of initial "walkers" (peeps).
- The 16-bit seed offset (bytes 8-9) is the main source of layout variety once combined with the battle number.

### Level Codes & RNG
The full 32-segment word table (RING/OUT/HILL, VERY/QAZ/TORY, ... SHI/Y/T) lives in the TEXT sections of the binary and is present in our `symbols_prg.h`.

The exact LCG used for everything (including level code hashing and map generation) is implemented verbatim in our `___newrand`:
```c
/* MULU #$24a1 ; ADDI.W #$24df ; BCLR #$F */
```
This matches the disassembly listing on the tetracorp site exactly (we fixed it after the initial 1103515245 placeholder).

Battle number (`in_conquest`) → level code is `(battle * 5)` fed through the LCG, then split into three 5-bit indices (15 bits total, top bit ignored).

Level 0 has the special code GENESIS (and its calculated alias SHISODING). The game caps at 494 (WEAVUSPERT) for normal conquest.

### How This Helps the Port
- Our quick-test CLI `N` and the `demo_levels` table now have **accurate documentation** tied to real data.
- `_setup_display`, `_make_map`, `_place_first_people`, `___load_ground`, and the visible `_draw_it` / bars are all exercising the exact parameters described on the site.
- Future work (real level.dat loader, proper power availability checks, scoring, conquest progression) can directly use these byte definitions and the LCG.

The local copy of the analysis is in `docs/_posts/2025-06-14-level-format.md` (and the rendered site is in `docs/`).

This analysis was performed on 2026-06-16 at the request of the user to ground the constants in the C code against the best available public reverse-engineering.

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
- The conquest byte handling in `_setup_display` is particularly important and now has extensive documentation (including full bitfield and terrain tables) thanks to the https://tetracorp.github.io/populous/ level format analysis.

---

## Recent Progress (Quick-Test Focus)

Prioritized areas that allow fast iteration and visible/terminal feedback without needing the full real map or sprite code yet:

### Demo Conquest Level Table + Fast "Level Loading"
- Added a small static table `demo_levels[8][9]` modeled after the real `level.dat` format (enemy rating, speed, power bitfields, game mode, terrain, pop hints, seed offset). See the new detailed "Analysis of Game Data Structures" section above (sourced from tetracorp.github.io/populous level-format analysis) for the full byte-by-byte mapping, power bits, game mode rules, terrain types, and LCG.
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
- (Autonomous) More of the _animate callees made visible (_move_mana now drives a mana bar; map state migrated into A4 map_who etc. for fidelity; pop growth + shield flashes added in latest session).

## Autonomous Agent Work (ongoing, no user input between updates)
Per user request to continue in agent mode:
- Periodically make incremental improvements focused on quick visual/CLI feedback for "level laden, init game status, ground/bild shim, animation heartbeat".
- After batches of changes: update this grok.md, sync trees, commit + push to the fork (https://github.com/drozdse1/populous).
- Current autonomous phase (post tetracorp analysis): fidelity improvements (A4 globals for map data) + more loop coverage (visible mana from _move_mana) + polish.

Recent autonomous additions (this cycle):
- Added A4_DISP defines for the core map structures (_map_who at +0x1f88, _map_blk etc.) using exact offsets from the asm LEA instructions and binary labels.
- Migrated the demo height storage behind the A4 map_who area (demo_height_at macro + accessors). Generation, drawing, placement, movement, sculpt, powers, and held-mouse edits now all operate on (or mirror to) the emulated globals. This is a big step toward the real game data layout so translated _make_map etc. will see consistent state.
- Implemented visible _move_mana (called every frame in the _animate loop): it computes a target from current pop + peeps and smoothly updates demo_mana.
- Added a mana bar (distinct color) drawn in _draw_it near the pop status bars. This makes the core transcribed heartbeat "feel alive" and shows resource state changing with init pop and simulation.
- Minor robustness (DEMO_MAP_* constants) and continued use of the POWER_* / TERRAIN_* defines from the previous analysis pass.
- All changes keep full backward compatibility for `./populous_init_port N` testing and interactive keys (p/q/v/r/mouse).

Builds and smokes cleanly for multiple N. The window now shows evolving status (mana bar reacts to pop) on top of the A4-backed map.

Next autonomous steps will target more _animate callees (_clr_wsc, _show_the_shield with visible effect), simple population growth simulation so bars change over time without user input, and possibly basic icon or shield drawing shims.

(Updates to repo/grok.md/push happen automatically at milestones.)
- Start using more real A4 map storage (map_who, map_alt, map_blk areas) instead of the isolated demo array.
- Basic population / "town" init from the starting pop bytes.
- First real ground tile / image loading shim (beyond just palette) so terrains look structurally different.

All changes are in the `populous_c_port/` subtree and documented here for the fork.

---

*This file is maintained as living project documentation for the C/SDL2 port effort.*