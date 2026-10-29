# Populous Amiga C/SDL2 port

This is an incremental study port of the 1993 Hit Squad release, based on the
[tetracorp disassembly](https://github.com/tetracorp/populous). It preserves the
launcher (`populous_init.c`), PRG (`populous_prg.c`), A4-relative state and SDL2
low-level layer. This is a bounded original-rule intermediate port, not a full
Amiga conversion. Automated matches reach an outcome; independent X11/WSLg
checks also verified a visible match through defeat, reset and clean exit using
agent-driven inputs at human rates. A human tester's winning strategy is not proven.

## Current state

All eight synthetic scenarios start on explicitly legal flat vertex terrain
with both populations. `_make_map` now derives original block masks and cell
altitudes from 65x65 vertex heights. The 22-byte A4 peep records drive movement,
settlement, eight-tick growth/mana, splitting, ordinary combat and outcomes.
There is no second population simulator. The SDL top-down drawing is simplified,
not the original isometric graphics.

A4 has storage on both sides of its signed 16-bit base. Word/long access uses
GCC-compatible packed/may-alias accessors, and peep words use `memcpy`; this
in-memory representation is host-endian, **not** a save-file/disk decoder.
Allocated graphics buffers remain native host pointers, never truncated into
32-bit Amiga fields. Reset rebuilds deterministic scenario state; quit releases
PRG resources and returns through the launcher cleanup without another event
loop. SDL software rendering is used if accelerated rendering is unavailable.

The bounded rule translation covers `_valid_move`, `_check_life`, `_set_town`,
ordinary `_where_do_i_go`/magnet navigation, settlement growth/mana/splitting,
ordinary `_do_battle`/`_battle_over`, joining, water loss and vertex sculpting.
Source labels and assembly ranges appear beside each implementation. The
opponent uses the same settlement/exploration rules, not timer-created units.
Special powers, knights, multi-party battle joining, original computer
power/auto-level logic, celebration animations and exact Amiga movement
interpolation remain unsupported. The eight scenarios are flat synthetic
starting plots, not original maps. Rule-word overflow is bounded at 32000
population and mana at -250..1999999 for host safety.

The original `_load_ground` reads gameplay tables from `landN` files, including
`population_add`, `mana_add`, `weapons_add`, `battle_add1` and `battle_add2`
(`src/populous_prg.asm:16468-16514`). Reset validates the selected file before
changing A4 and installs its actual values plus original initialized tables.
Missing/invalid files fail startup: there is no invented-rule fallback.
Original graphics rendering, audio, campaign, save/load and multiplayer remain absent.

## Local original data (required to play)

`extract_land.py` uses Python 3's standard library to extract only root `land0`
through `land3` from an uncompressed 880 KiB DOS0/OFS ADF. It validates directory
and file/data block checksums, parent/owner, sequence, block table, exact payload
length, bounds and cycles; it does not scan sectors for apparent filenames.
It accepts mixed-case original names, emits lowercase fixed names, rejects
unsafe names, output-directory symlinks and existing destination files. Keep
its output outside the repository. It does not extract any other assets.

```sh
python3 populous_c_port/extract_land.py binaries/populous.adf /tmp/populous-original-data
make -C populous_c_port test-land-local LAND_DATA_DIR=/tmp/populous-original-data
```

The supplied local `binaries/populous.adf` was checked with all four files.
Release identity is not inferred from the filename. The owner authorized keeping
this supplied ADF in a local commit only, not publishing or pushing it. Extracted
LAND files stay outside the repository; use an unused output directory if the
example directory already exists.
`test-land` uses synthetic fixtures only, including malformed OFS chains and
LAND headers; it is included in `test`. The C parser/loader also runs under
ASan/UBSan in `test-sanitize`. The explicitly opt-in `test-land-local` checks all
four external files and fails if missing. Use the toolchain overrides below if
make/SDL2 are not installed globally.

### Rule interface and source layout

`land_data.h` exposes `LandRules` (host-endian `uint16_t` words and `uint8_t`
colours), `land_rules_parse(data, size, out, error, error_size)` and
`land_rules_load(directory, land_index, out, error, error_size)`. Both return 1
on success, 0 on failure, leave the output unchanged on failure, and optionally
write a bounded diagnostic. The file loader accepts index 0..3, reads only a
regular non-symlink `landN`, and never silently substitutes default rules.
There is no global state, A4 mutation or graphics allocation in the loader.
PRG `game_reset` installs the successfully decoded words in original A4 slots.

The sequential reads in `src/populous_prg.asm:16483-16529` define this layout:

| File offset | Field | Encoding |
| --- | --- | --- |
| 0 | `walk_death` | BE16 |
| 2 | `population_add` | 11 BE16 |
| 24 | `mana_add` | 11 BE16 |
| 46 | `weapons_add` | 11 BE16 |
| 68 | `battle_add1` | 11 BE16 |
| 90 | `battle_add2` | 3 BE16 |
| 96 | `map_colour` | 16 bytes |
| 112 | `sprite_set` | BE16 |
| 114 | block graphics | 0x8340 bytes, not decoded here |

Exact total length is 33714 bytes; there is no magic/signature in a LAND header.
Validation deliberately supports positive signed-word walking loss, nonnegative
signed-word rule values, palette indices 0..15, and a one-digit sprite set
(`_read_sprites`, assembly lines 16410-16413). These are supported-domain safety
checks, not authenticity checks or a claim that all historical variants fit.
`_load_ground` also duplicates the 16 colours into a 32-byte map and sorts a
copy of weapons into ascending `weapons_order` (lines 16540-16606); the parser
performs both setup operations. It does not load sprites, decode graphics or
claim that the presence of these tables makes the preview playable.

## Build and run

Requires GCC-compatible C11, GNU make, SDL2 development files (`sdl2-config`), and
`timeout` for the test targets. A display is needed for interactive use.

```sh
make -C populous_c_port
POPULOUS_DATA_DIR=/tmp/populous-original-data ./populous_c_port/populous_init_port
POPULOUS_DATA_DIR=/tmp/populous-original-data ./populous_c_port/populous_init_port 7
make -C populous_c_port test
make -C populous_c_port test-sanitize
```

The optional argument is a decimal integer from 0 through `INT_MAX`; selection
is `N % 8`, so 8 selects slot 0. No argument also selects slot 0. Negative values,
non-digits, extra arguments and overflow return failure. The eight internal
rows are synthetic configurations, not loaded original levels. Existing
effective low-byte seed offsets are preserved explicitly.

## Controls

- Cyan is your population; red is the opponent. Hollow squares are settlements,
  yellow markers are battles. Bottom bars show population share and your mana.
  The window title reports population, mana, tool, pause and outcome.
- Left/right mouse: raise/lower the selected vertex. Neighbouring vertices follow
  the original maximum slope of one, height 0..8. Cost is 10 plus 4 per changed
  vertex; the original admission threshold is 10, so a large operation can leave
  negative mana. NO_BUILD, up-only and near-town restrictions are enforced.
- `1`: settle/explore (default); `2`: join friendly walkers; `3`: seek nearby foes.
- `0`: gather walkers at the initial magnet (32,32). After a leader reaches it,
  `m` selects magnet placement; left-click moves the target for 200 mana. `m`
  again returns to terrain. Settlements release walkers when over capacity.
- Arrows/numpad: scroll. `p`: dot overview. Space: pause. `r`: reset. Escape/window
  close: quit. Pause and outcome stop rules, not event handling or drawing.

The opponent independently settles, grows, explores, merges and fights. Flatten
plots to encourage settlement, wait for population/mana, and lower the four
corners of hostile plots to deny land. The goal is to remove the opposing living
population, including settlements. `q`, `v`, `a` and unported special powers are
inactive. No timer or population target declares a winner.

## Verification

`test-runtime` checks real SDL software rendering and partial-initialization
cleanup, injected resource failures, launcher handoff, and invalid video driver
errors. `test-game` includes the production PRG and launcher code and checks A4
boundaries/unaligned access, map separation, all eight start populations and
occupancy, deterministic reset, renderer output, SDL event handling, pause,
scroll limits, scaled mouse coordinates, inert powers, allocation-failure
cleanup, repeated Escape/quit, and CLI failure propagation. Both are included
in `test` and ASan/UBSan `test-sanitize`; each invocation has a 20-second timeout.

`test-game` additionally exercises legal start plots, source-derived capacity,
loaded increments, render resource invariance, scaled/scrolled sculpt inputs,
HUD rejection, build restrictions, leader/magnet costs, load failure and matches.
Regressions cover settlement occupancy recovery during 500 normal ticks, remote
sculpt preserving navigation history, split-leader transfer for both owners,
occupied-slot preservation, exact seeded battle casualties/rewards and near-town
sculpt acceptance/rejection. Isolated rule fixtures set up controlled state;
the bounded match tests use only reset, normal ticks and legal SDL inputs.
Normal tests generate synthetic LAND files under a temporary directory and
remove them. Original-data integration is explicit:

```sh
POPULOUS_TEST_REAL_DATA=1 POPULOUS_DATA_DIR=/tmp/populous-original-data \
  SDL_VIDEODRIVER=dummy ./populous_c_port/tests/test_game
```

The fixed scenario-0 sequence selects fight tendency, simulates settlement and
combat, then after tick 6000 sends legal right-clicks to the four corners of
visible hostile plots every eight ticks. Budget: 12000 ticks, no teleportation
or population/result edits. With the supplied original grass LAND it reached
victory at tick 8353 after 288 engagements and 287 settlement formations, with
111498 friendly population remaining. Normal and ASan/UBSan runs agreed. A
separate legal self-sculpt sequence verifies defeat and reset/quit. The synthetic
rule fixture may reach defeat sooner; it is not a balance/authenticity fixture.
At 20 simulation ticks/sec this represents minutes of gameplay, not a claim
that a human can execute the automated click rate.

These automatic checks use SDL's dummy video driver/software renderer and alone
are not proof of visible playability. The independent X11/WSLg review additionally
observed terrain edits, pause, growth, combat and a stable DEFEAT (cyan 0, red
69515), followed by reset and clean Escape/window-close exits. It used real-time
simulation and agent-driven input at approximately one click per second, with no
population patches. The three review findings (settlement occupancy, local
navigation history and split-leader transfer) were repaired and independently
rechecked. No blocking defects remained within this intermediate scope.

For a human-paced visible check (procedure, not a verified winning strategy):

1. Start scenario 0 without scrolling, press `3`, and wait 30 seconds. Confirm
   both populations change and hollow settlement squares appear.
2. Press Space. At the default 640x512 window, vertex (10,10) is near pixel
   (82,82). A click while paused must not change terrain. Unpause, left-click
   once, then right-click once to restore it; allow a second between clicks.
3. Let the match run for about five minutes. Watch for yellow battle markers;
   record the population/title rather than assuming an outcome on a timer.
4. Lower one visible hostile plot at a time: right-click its four vertex corners,
   roughly eight window pixels apart at default size, at no more than one click
   per second. Watch mana and the changed terrain. Repeat as needed, letting the
   simulation run; there is no guaranteed human-paced completion bound yet.
5. If an outcome appears, verify it remains stable, then press `r` to restore the
   initial populations. Click the window to focus it and press Escape. Also test
   closing the window on a fresh launch. Report missing outcomes or quit failures.

This acceptance covers the bounded intermediate port, not complete original-rule
fidelity or a human-tested winning strategy. The simplified graphics, synthetic
starts and unsupported features listed above remain intentional limitations.

The development environment used an unpacked toolchain under
`/tmp/populous-toolchain` rather than system packages. Its reproducible command
is (normal SDL installations do not need these overrides):

```sh
LD_LIBRARY_PATH=/tmp/populous-toolchain/usr/lib/x86_64-linux-gnu:/tmp/populous-toolchain/usr/lib/x86_64-linux-gnu/pulseaudio:/tmp/populous-toolchain/usr/lib/x86_64-linux-gnu/sdl2-classic \
/tmp/populous-toolchain/usr/bin/make -C populous_c_port all test test-sanitize \
  SDL_CONFIG='/tmp/populous-toolchain/usr/bin/sdl2-config --prefix=/tmp/populous-toolchain/usr' \
  LDLIBS=/tmp/populous-toolchain/usr/lib/x86_64-linux-gnu/sdl2-classic/libSDL2-2.0.so.0.3200.10 \
  CFLAGS='-std=c11 -O2 -I/tmp/populous-toolchain/usr/include/x86_64-linux-gnu'
```

Launch with that unpacked toolchain's runtime libraries:

```sh
POPULOUS_DATA_DIR=/tmp/populous-original-data \
LD_LIBRARY_PATH=/tmp/populous-toolchain/usr/lib/x86_64-linux-gnu:/tmp/populous-toolchain/usr/lib/x86_64-linux-gnu/pulseaudio:/tmp/populous-toolchain/usr/lib/x86_64-linux-gnu/sdl2-classic \
./populous_c_port/populous_init_port
```

Use the same `LD_LIBRARY_PATH` for the opt-in original-data integration command.

## Legal and credits

Original disassembly and analysis: tetracorp
(<https://tetracorp.github.io/populous>). This port is for study, preservation
and reverse-engineering education. Use lawfully obtained original data; do not
distribute commercial assets or original game files.
