# Populous (Amiga 1989/1993) → C + SDL2 Low-Level Port

**This is a study/port companion to the excellent IRA disassembly of the Hit Squad 1993 budget re-release (the "POPULOUS 2.7" version) hosted at:**

- GitHub: https://github.com/tetracorp/populous (the .asm + .cnf files for ira/Aira Force)
- Website + analysis: https://tetracorp.github.io/populous

The disassembly was chosen (as noted in the repo README) because the 1993 Hit Squad budget release "lacks most of the obfuscation present in the original release". It splits into the launcher ("populous") and the main game module ("populous.prg") — exactly the split our port mirrors (populous_init.c + populous_prg.c).

Disassembly and analysis are believed to constitute fair use for study, analysis, and commentary. You still need a legally obtained original copy of the game to play it.

Our port goals remain:
- Make the 68000 logic readable in modern C while staying extremely close to the original.
- Low-level only (SDL2 emulating bitplanes, copper, sprites, custom chips, Exec/ graphics.library calls, A4 BASEREG globals, etc.). No high-level engines.
- Preserve the launcher/prg split, memory allocation patterns, interrupt/serial/mouse setup, conquest/level code system, 1993 additions (save/load, IFF screenshot tool, debug code, "LANDSCAPE NO", conquest scoring changes), and the exact structure so someone following the tetracorp disassembly can cross-reference.

## Credits & Sources
- All assembly chunks, symbol maps, and comments come from the tetracorp/populous disassembly (1993 Hit Squad version).
- Higher-level analysis (especially the level format, which directly explains the conq_XX bytes, seed/terrain, power bitfields, and game mode we translated in `_setup_display`) lives at the project site linked above.
- Ripped graphics and level lists are also available from the site for future asset work.

## What was translated (so far)
- Hardware register equates (`amiga_hardware.h`)
- A4-relative globals emulation (byte array + displacement macros matching the BASEREG layout at 0x593EE)
- Launcher (first chunk): SUB_384 / SUB_11A / SUB_16E / SUB_226 / SUB_456 etc. (demo.pic/load.pic loader, mouse regions, big CHIP allocs, "populous.prg" load path)
- Full "populous.prg" symbol map (ENTRY $0003E1E8, A4 at $000593EE, all CODE/TEXT ranges, 100+ symbols, all str* level code fragments, 1993 BANNER notes)
- **Major functions from the latest chunk you pasted**:
  - `_main` (real prg entry: big AllocMem for sprite_data/sprite_data_32/blk_data/back_scr/screen/copper/fb, start_of_game handling, open_screen + read_sprites + sprite_to_amiga + start_game, serial setup, call to `_animate`, closeall)
  - `_rs`, `_closeall` (full cleanup + INTENA pokes)
  - Mouse/interrupt: `_create_mouse`, `_setup_interrupt` ("petersint" / Peter Molyneux VBI server), `_set_mouse`, `_free_inter`
  - Serial: `_setup_serial_interrupt` (two servers for read/write), `_free_serial_interrupt`
  - Libs: `_openlibrary` (FindTask + priority, graphics.library + intuition.library, pal detect), `_closelibrary`, `_set_pri`, `_debug` (the intentionally empty one present in both versions)
  - `_setup_display` (the big one with the exact source comments: "seed = level mod 8 + bytes8+9", terrain type, the five conquest control bytes (speed, enemy/your powers as bitfields, game mode), in_conquest vs normal path, load_ground, clear_map, make_alt/make_map/make_woods_rocks, double-buffer Setscreen/copy_screen/draw_map, place_first_people)
  - `_animate` (the core heartbeat from the chunk: icon bar setup, start_of_game==2 conquest show_world loop, tempo/beat from population, clr_wsc + move_mana + draw_it + sprite layers (draw_s_32 vs normal), peeps overview dots when paused/paint (the BTST logic), do_funny/move_peeps unless paused, view_timer, game_turn==0x1000 funny placement, show_the_shield, swap, full numpad scroll handlers, debug keys (A=instant win by clearing enemy peeps, F9=display_debug (new 1993), *=corner + flag cheat that prints "CHEAT" + mutates stats), sculpt/interogate/zoom/button handling)
- Supporting shims (___OpenLibrary / CloseLibrary / AllocMem / FreeMem / Setscreen / copy_screen / toggle_icon / draw_sprite / draw_s_32 / a_putpixel / text / keyboard / Add/RemIntServer / etc.) plus the LCG `_newrand` (period 2^15 as documented in the BANNER).
- Launcher now branches into the real prg `_main` on the "full game" choice.
- SDL2 low-level layer (AmigaScreen with bitplanes + chunky + present-as-copper/vsync, basic blit/pixel/palette).

(The real sub-bodies for many callees inside `_animate` / `_setup_display` — `_move_peeps`, `_draw_map`, `_move_mana`, the full tile renderer, etc. — are still printf stubs. Paste the next assembly body and we'll fill them the same way.)

## Building & Running
(See previous instructions — `make && ./populous_init_port`.)

The current code follows the exact flow from the chunk you pasted. When you run with choice==1 it will now execute the real `_main` → allocs → screen/sprite init → `_animate` loop (with the numpad scrolls, debug keys, conquest path if start_of_game triggers it, etc.), all driven from the A4 emulation area.

## Current quick-test demo (no more asm paste needed yet)
For fast iteration on "level laden + init game status" without pasting more chunks:
- `./populous_init_port N` where N=0..7 loads a different row from the internal demo_levels table (different terrain, start pop_you / pop_enemy, power bitfields, speed, seed offset).
  - You immediately see different base colors + texture patterns, different # of moving peeps, different bottom status bars (player yellow vs enemy), and height range / features (rocks on rocky, flatter dunes on desert).
- In the running window:
  - Arrow keys or numpad 1-9: scroll the view (xoff/yoff) — the map + peeps + overlays scroll.
  - Mouse move: updates the emulated mousex / big_mousex / mousey (A4 state used by transcribed _animate logic for interogate/sculpt/zoom).
  - Left-click: raises terrain at the clicked map cell (visible immediately).
  - Right-click: lowers terrain (sculpt test).
  - `q`: area "quake" centered near current view (mutates demo_height using real ___newrand + redraw).
  - `v`: symmetric "volcano" raise cluster (positive power test on any loaded level).
  - `p`: toggle paint_map (population view) — hides big owner-peeps, emphasizes the dot overlay from the _animate BTST/a_putpixel logic. Excellent for inspecting init pop per level.
  - `r`: re-runs _make_map + _place_first_people + redraw for the current loaded level params (live reset without restarting the exe).
  - ___load_ground / bild shim now loads visibly distinct palettes + the _draw_it texture was upgraded with start_seed phase + height-delta "tile edges" so different N feel like different ground data loaded.
  - `a`: crude "win" wipe (from the debug key in chunk).
  - `F9`: debug stub print.
  - ESC: quit the loop cleanly.
- All the visible rendering (_draw_it) + peeps (DemoPeep struct + owner/flags from _animate BTST logic) + bars + terrain features + real 15-bit newrand LCG + height validation on place are live and react to the CLI level data + your mouse/keys.
- This directly addresses the priority "priotiisere bereiche die wir schnell testen koennen (level laden, init game status, loade bild shimr was auch immer)" — you get instant visual + printf feedback on different conquest setups.

Next real progress happens when you paste the body of a specific routine (e.g. full _move_peeps, _draw_map tiles, a _do_ power, ground tile load, or more _animate callees). The current demo keeps you motivated and the structure (A4, shims, SDL bitplane-ish present, symbols) ready.

## Legal note
This is a reverse-engineered study/port of a 1989/1993 commercial game, based on the tetracorp disassembly (fair use for analysis and commentary). Only use it for personal education, preservation, or with legally obtained copies of the original. Do not distribute commercial assets or the original data.

Next: Paste the next chunk of `populous_prg.asm` (or tell me a specific routine, e.g. "the body of _move_peeps" or "the peep struct + move logic"), and I'll translate it into the same style, wiring the SDL layer and keeping every comment/note from the disassembly. We can also pull ripped graphics or level format details from the tetracorp site when we're ready for assets or the full conquest system.

## Goals
- Make the 68000 assembly logic readable in modern C.
- Stay relatively low-level (no high-level game engines).
- Use **SDL2** as the low-level graphics abstraction (it maps surprisingly well to Amiga concepts: surfaces = bitplanes, textures = copper output, events ≈ interrupts).
- Preserve the original structure (memory allocation, "library" opens, A4-style globals, etc.) so someone studying the disassembly can follow along.

## What was translated (so far)
- Hardware register equates (`amiga_hardware.h`)
- The big A4-relative globals area (turned into a clean struct)
- SUB_384 – library opening (graphics + intuition) + first big CHIP allocations
- SUB_11A – memory clearing + interrupt enabling
- SUB_16E – the famous loader that references "demo.pic" / "populous.prg" and sets up startup structures
- SUB_226 – more screen/copper allocations
- SUB_456 – the interactive loader (load.pic + mouse region selection)
- SUB_308 / SUB_2E2 / SUB_29E / SUB_2C6 – various small init and cleanup routines
- SUB_536 + initial data table setup from the big numeric sections
- Memory allocation stubs (originally Exec AllocMem with MEMF_CHIP)
- Basic entry point flow (the JMP at SECSTRT_0 and early bootstrap)
- **Full "populous.prg" symbol map integration** (the real game module):
  - Complete ENTRY $0003E1E8 / OFFSET / BASEREG A4 / BASEADR $000593EE
  - All 40+ CODE ranges + 50+ TEXT ranges (non-contiguous because of interspersed strings/tables)
  - 100+ SYMBOL entries: _main, _animate, _setup_display, _make_map/_draw_map/_mod_map/_zoom_map, all _do_* powers (quake/volcano/swamp/knight/flood/war/funny), peeps AI (_move_peeps + magnet/explorer variants, _where_do_i_go etc.), battles, _place_first_people, UI (_requester, _keyboard, _mouse, icons, text, options), 1993 _save_game/_load_game/_checksum, _write_iff (new tester IFF screenshot tool), _code/_decode (level passwords), sound, serial 2P, _newrand (LCG), low-level gfx (_draw_sprite, _make_copper, _vbi_timer, _Setscreen, sprite conversion etc.), plus all C shims and Amiga Exec/Graphics/DOS thunks.
  - All the level code word fragments (strRING, strVERY, ... strY) and other TEXT strings (strCHEAT*, strStartGame, strGENESIS*, strGAM*, strLandscapeNo (new in 1993), strLost/Won etc.).
  - Full BANNER/COMMENT block with exact 1993-vs-1989 diffs (removed copy protection + obfuscation at $4F3C6 area and $4D092 encrypted section, new save/load/IFF/debug/"LANDSCAPE NO", conquest scoring points notes, lines cut, vasm reassemble command, A4 globals location).
- `populous_prg.c` skeleton: prg_main (the real entry after launcher "load"), A4 emulation area, comprehensive forward decls for the entire map, minimal printf stubs for core loop functions, high-level game flow comments matching the map ( _main → _setup_display/_make_copper/_make_map/_place_first_people → infinite _animate + _keyboard + _mouse loop).
- Launcher (`populous_init.c`) now branches on the SUB_456 choice: choice==1 calls prg_main() (emulates loading the prg and jumping with A4 set).
- Makefile now builds + links `populous_prg.c` together with the launcher.

## How the graphics mapping works
- Original Amiga bitplanes + copper list → `AmigaScreen` with raw bitplane pointers + a chunky 32-bit buffer + SDL_Texture.
- `amiga_present()` is basically the modern equivalent of waiting for the copper to finish and flipping.
- Real planar-to-chunky conversion and full copper emulation are still stubs (the original code does a lot of that later).

## Building & Running
Requires SDL2 development files.

```bash
cd populous_c_port
make
make run
```

Or manually:
```bash
gcc -Wall -Wextra -std=c11 -O2 `sdl2-config --cflags` \
    populous_init.c sdl_lowlevel.c -o populous_init_port `sdl2-config --libs`
./populous_init_port
```

## Important clarification (from your question)
**The launcher chunk + full prg symbol map = the complete program structure.**

What you first pasted was the **early bootstrap / launcher / loader** (the code that displays demo.pic + load.pic, does mouse choice, allocates CHIP, then loads "populous.prg").

The second (much larger) paste you just gave is the **complete IRA symbol map for the real game module "populous.prg"** itself:

- Entry at $0003E1E8, A4 base register at $000593EE (globals for map/peeps/mana/UI/power state all accessed via A4 offsets in the original).
- Every function, every string (including the giant level-code word list used by the conquest password system), every data table, all the 1993 additions (save/load, the IFF screenshot writer `_write_iff` which is a dead tester tool, the new debug code, "LANDSCAPE NO" in options, conquest scoring changes, removed copy protection).
- Non-contiguous CODE + TEXT ranges because strings and tables are mixed in.
- BANNER notes document exactly what was added/cut vs the 1989 original and give the vasm command to reassemble.

The repo split (launcher vs prg) mirrors the original binary layout for a reason: small loader that brings in the big game hunk.

In the C port right now:
- The launcher runs its early SUBs exactly as in the first paste.
- On the "full game" choice it calls into `prg_main()` (the emulated entry to the prg).
- `populous_prg.c` has the structural skeleton + A4 emulation + every symbol from your latest map as forward decl + high-level flow comments.
- The actual bodies are still printf stubs. **Next step is to paste real assembly bodies** (one function or logical group at a time) and we translate them faithfully into C while driving the existing SDL low-level layer (bitplanes, palette, present-as-copper, later sprites/copper lists).

Yes — paste the next chunk of actual 68k code (e.g. the body of `_main`, `_animate`, `_draw_map`, `_move_peeps`, any `_do_quake` etc.) whenever you want and say what to focus on. We will keep extending the same tree (`populous_c_port/`) and stay buildable at every step. Graphics assets (tiles, peep sprites, icons, font, sounds) will come from the ADF or can be extracted into arrays once we have the access code.

## Next steps (if you want to continue)
- Paste the **next chunk of actual assembly** (the body of the next SUB_ or range of code you want translated). The symbol map you just gave is now fully integrated (see `symbols.h` and forward declarations).
- The repo split makes sense: this module is the launcher. If you have the `populous_prg.asm` parts (or the later CODE ranges after the loader), paste those — that's where the real game lives.
- Good next targets: SUB_714, SUB_17B2 area (the prg load), the main rendering loop, copper usage, or any of the big data sections for graphics/tiles.
- I'll keep extending the same SDL2 low-level port, adding proper Amiga-style memory, blitting, and display emulation as we go.

The original disassembly this was taken from is the excellent ongoing work at:
https://github.com/tetracorp/populous
https://tetracorp.github.io/populous/

This map + the previous launcher map = the complete code for the 1993 Hit Squad budget release of Populous (launcher + prg). The repo splits them for the same reason the original game did: small bootstrap that loads the main game module + data. Graphics assets (tiles, sprites, sounds) live in the ADF/disk files.

Feel free to paste the next chunk (or multiple files/sections from the repo) whenever you're ready. I'll extend the C port one logical piece at a time while keeping it buildable and using the low-level SDL layer.

This C port is meant as a companion for study and experimentation, not a full game reimplementation.

Current status after integrating the full prg map you pasted:
- `symbols_prg.h` now contains the complete map (all ranges, symbols, strings, BANNERS, 1993 notes).
- `populous_prg.c` + `populous_init.c` + Makefile wired so choice 1 enters the prg (A4 emulation area is allocated, prg_main loop runs the documented high-level flow).
- Build with `make`; the prg side currently emits lots of "[PRG] ..." printf lines and loops forever (no real graphics takeover yet — we will add shared AmigaScreen + real drawing when bodies are translated).
- Next: paste assembly for any specific routine and we fill in the logic + SDL calls (planar blits, peep sprites as `insert_sprite`, copper waits as vsync/present, A4-relative map/peep accesses, 1993 save/load paths, level code decode, etc.).

Legal note
----------
This is a reverse-engineered study/port of a 1989/1993 commercial game. Only use it for personal education, preservation, or with legally obtained copies of the original. Do not distribute commercial assets.

## Legal note
This is a reverse-engineered study/port of a 1989 commercial game. Only use it for personal education, preservation, or with legally obtained copies of the original. Do not distribute commercial assets.
