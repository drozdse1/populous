# Exploring Populous

_Yet another Commodore Amiga game disassembly project_

This ongoing project reverse-engineers and analyzes _Populous_, a strategy game
released in 1989 by Bullfrog Productions. Its findings are documented here:
<https://tetracorp.github.io/populous/>

See also the
[Tetracorp homepage](https://tetracorp.github.io/)
for more Amiga game disassembly projects (including _K240_, _Dungeons of Avalon_
and _Knightmare_), as well as a list of Amiga reverse-engineering resources
elsewhere on the web.

## Additions in this fork (C/SDL2 Port)

This fork also contains an incremental low-level C + SDL2 port of the disassembly.

The port aims to stay extremely faithful to the original 68000 code:
- Preserves the launcher / `populous.prg` split
- Emulates A4 as base register for globals (BASEREG)
- Uses SDL2 only as a thin hardware emulation layer (bitplanes, copper, sprites, palette, vsync)
- No high-level game engines

**Location:** `populous_c_port/`

**Quick start for testing:**
```bash
cd populous_c_port
make
./populous_init_port          # default
./populous_init_port 5        # try different "levels" (demo conquest table)
./populous_init_port 17
```

The port includes a small demo level table so you can rapidly test different level loading / game init states and immediately see different maps + population counts in the window.

See `grok.md` for detailed development notes, current translation status, and what has been ported so far (e.g. `_main`, `_setup_display`, large parts of `_animate`, real `_newrand`, etc.).

## Repository layout (this fork)

- `src/` — Original 68000 disassembly (from upstream tetracorp)
- `populous_c_port/` — C + SDL2 translation (new in this fork)
- `grok.md` — Living documentation of the C port development
- `docs/` — Original documentation from upstream

This fork keeps the original disassembly intact while adding the C port work as a companion for study and experimentation.
