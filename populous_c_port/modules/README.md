# Game PRG modules

`populous_prg.c` is a thin unity build: it sets up includes / host state and
`#include`s these fragments in original translation order so `static` helpers
and `tests/test_game.c` (which `#include`s `populous_prg.c`) keep working.

| File | Responsibility |
|------|----------------|
| `../a4_state.h` | Emulated A4 image + speaking field macros (`war_flag`, `map_vertex_word`, …) |
| `01_preamble.c` | Forward decls, peep accessors, conquest constants, `prg_main` entry |
| `02_rules.c` | Map vertices, population/battles/AI, powers, generate, animate/reset |
| `03_ui_save.c` | Pointer/HUD, options requesters, `.GAM` save/load, world/lord screens |
| `04_render_input.c` | Frame draw, `_move_peeps` tick, Amiga/SDL shims, input |

When editing gameplay state, prefer names from `a4_state.h` over raw
`A4_WORD(0x…)` / `A4_DISP(0x…)`. Named coverage now includes per-owner stats
fields, mana power costs, LAND tables, sprites, end-scoreboard rows, and
save/UI scratch. Dynamic `stats+N` / AI-offset arguments still use raw accessors.

Logging uses `../pop_log.h` (DEBUG for old `[PRG]`/`[SDL]` traces; default INFO).
