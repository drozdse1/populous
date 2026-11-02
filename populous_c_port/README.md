# Populous Amiga C/SDL2 port

This is an incremental study port of the 1993 Hit Squad release, based on the
[tetracorp disassembly](https://github.com/tetracorp/populous). It preserves the
launcher (`populous_init.c`), PRG (`populous_prg.c`), A4-relative state and SDL2
low-level layer. This is a bounded original-rule intermediate port, not a full
Amiga conversion. Automated matches reach an outcome; independent X11/WSLg
checks also verified a visible match through defeat, reset and clean exit using
agent-driven inputs at human rates. A human tester's winning strategy is not proven.

## Current state

All eight synthetic scenarios still start on legal flat vertex terrain with both
populations. An explicit `--level N` selects original world initialization from
`level.dat`: level parameters, height generation, woods/rocks and initial inhabitants.
It does not activate campaign progression or the still-unported powers/AI.
`_make_map` derives original block masks and cell altitudes from 65x65 vertex heights.
The 22-byte A4 peep records drive movement,
settlement, eight-tick growth/mana, splitting, ordinary combat and outcomes.
There is no second population simulator. The default SDL view now uses the
original `qaz.pic` background, 8x8 isometric LAND terrain and settlement overlays,
and small sprites. `d` explicitly toggles the old top-down diagnostic view.

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
Knight creation, original computer
power/auto-level logic, celebration animations and exact Amiga movement
interpolation remain unsupported. The eight positional scenarios remain flat
synthetic plots; `--level` instead uses the assembly-derived original generator.
Population is bounded at 32000; ordinary mana helpers clamp to -250..1999999.
Raw terrain commands instead preserve source word-cost/32-bit subtraction rules,
using defined unsigned arithmetic rather than a C signed-overflow operation.

The exact `0x80` peep remnant now consumes its original unsigned 16-bit timer
(`src/populous_prg.asm:4283-4315`): it claims only empty occupancy and is removed
when the **old** counter is zero, so 40 lasts 41 updates. It does not walk, lose
walking population, draw RNG or change animation frames. Ordinary encounters
ignore bit-7 residents without deleting them; the immediate-movement occupancy
model and hidden-town safeguards remain in force. Removal uses normal leader/view
cleanup and population accounting. This is a tested consumer, not knight creation
or the heading-bearing battle-winner producer. Existing water precedence and mixed
state dispatch remain bounded host behavior, not original drowning/phase parity.
`--remnant` and the default suite cover both resident iteration orders, word
boundaries, occupancy, cleanup and stopped ticks. Normal/ASan/UBSan suites, all
495 original starts and original-data match regressions passed for this milestone.

An already-set war state resets both destinations and displayed magnets to
cell `0x0820` on each resident update. Exact peaceful towns evacuate when their
land has no capacity, their four-byte heading metadata is nonzero, or war is set
(`src/populous_prg.asm:3425-3432,3600-3633`). Evacuation clears the town flag,
frame and movement delta, removes its existing town footprint and skips growth,
splitting and movement for that update. Population, occupancy, metadata and
leader/view references are retained. Water, battle and waiting dispatch retain
their existing precedence. Gameplay Armageddon connects these ordinary war
navigation and deferred water-assistance paths; special-figure parity is separate.

Ordinary magnet navigation uses the original nine-entry direction table: arriving
at the target attempts north rather than switching to exploration. A new leader
claims the view only if it is empty; followers use their leader's current cell.
The clockwise fallback records the opposite direction, and complete failure
updates avoidance history before the existing blocked-navigation wait. Direct
block-47 steps are allowed during war, never as fallback steps. Invalid leader
references safely fall back to the magnet without repairing unrelated records.
War overrides ordinary tendency selection. Valid heading references override
magnet/leader targets; special battle behavior still remains incomplete.
An independent assembly-table harness matched 10,616,832 direction/terrain/history
cases. Focused/default tests, ASan/UBSan, all 495 original starts and original-data
matches passed. This comparison is not a trace of execution on an Amiga.

The terrain-command path uses the original three-byte command/X/Y mailbox per
owner, not a FIFO. Pending raises execute in owner order after the population
pass and its terminal check; finished matches discard old commands. The separate
controller/divisor latch does not gate command execution. Raw raises use original
recursive bounds and local map rebuilding, not the UI sculpt helper; war makes
them free, while normal execution charges the original word-sized build cost,
including 10 mana for a saturated vertex. Diagnostic `P` is not editor permission.
Nonwar producers and other command opcodes remain separate work.
Starts restore controllers 0/1, divisors 1/selected speed and latches 1/0.
Runtime initialization rejects zero speed transactionally; the disk parser still
retains that byte. For controller 1, a corrupt zero divisor discards that owner's
command and logs an error without dividing or clearing its latch. Controller 0
does not use that divisor. No replacement cadence is guessed.

War-only water assistance produces those deferred raises. Ordinary and admitted
heading navigation queue the walker's current cell when the primary step is
water, then continue fallback navigation; opaque invalid headings do not produce.
Residents already carrying
bit `0x10` queue before the port's water loss or land recovery, unless fatal-water
mode or the existing battle exemption applies. Newly assigned water flags do not
produce in that same pass. Later residents overwrite earlier same-owner commands;
no terrain changes until the consumer runs. Nonwar AI and swamp-target command
production remain disabled. Existing immediate drowning/recovery differences are unchanged.
A controlled two-island fixture raises a route solely through movement-produced
commands, enters combat at tick 41 and reaches mutual extinction at tick 45 in
normal and sanitized builds. This is a prerequisite integration test, not an
original campaign trace or a human-playability claim.

Heading target selection (`_get_heading`) now has a bounded implementation:
self when no enemy qualifies, otherwise the first nearest living different-owner
record without bit 7, using Manhattan distance and no path/occupancy filter.
The host stores a one-based slot in the existing four-byte field (zero is absent),
read and written via `memcpy`. Checked resolution uses the current A4 backing;
these values are neither native pointers nor original save-file bytes. Transfers
preserve the referenced slot, including a donor's self-reference. Live heading
navigation follows validated targets, refreshes invalid gameplay targets once,
and avoids swamp on both primary and fallback routes. Heading presence overrides
tendency, prevents settlement and releases an existing leader before war recentering.
Opaque out-of-range metadata stays intact and uses bounded magnet fallback without
leader acquisition; unsafe map coordinates never become map accesses. The exact
selector remains position-word based rather than silently filtering its candidates.
Knight creation and heading-specific battle completion remain deferred.
All slot roundtrips, signed positions, ties, relocation and transfer cases pass
normally and under sanitizers. An independent selector matched 10,000 generated
record sets; full suites, original-data matches and all 495 starts also passed.

The original `_load_ground` reads gameplay tables from `landN` files, including
`population_add`, `mana_add`, `weapons_add`, `battle_add1` and `battle_add2`
(`src/populous_prg.asm:16468-16514`). Reset validates the selected file before
changing A4 and installs its actual values plus original initialized tables.
Missing/invalid files fail startup: there is no invented-rule fallback.
Audio, campaign, save/load and multiplayer remain absent. The original toolbar
is decorative; use the keyboard controls shown below the game view.

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

### Original graphics and values export

`extract_assets.py` exports an explicit allowlist of original root assets with
Python's standard library only (including its PNG writer). The source ADF is
read-only; the output parent must exist and the output directory must not exist,
including symlinks. It never exports executable files. The original four-file
`extract_land.py` CLI remains compatible; its reusable OFS reader now also
validates variable lengths and chained extension block tables.

```sh
python3 populous_c_port/extract_assets.py binaries/populous.adf /path/to/new/populous-extracted
make -C populous_c_port test-assets
```

The export contains `raw/` byte-exact original files, `graphics/` PNG sheets and
full pictures, `pixels/` frame-major/row-major byte indices and keep masks,
`values.json` with stored LAND tables, palettes and neutral level records, and
`manifest.json` with source/artifact SHA-256 hashes, frame offsets/coordinates,
present/missing assets and verified/inferred/unsupported format status. The
manifest itself is not included in its own artifact hash table. This checkout
contains the supplied ADF and export under `binaries/`. For the port, point
`POPULOUS_DATA_DIR` at the export's `raw/` directory and `POPULOUS_ASSET_DIR` at
the export root.

LAND sheets use 70 32x24 frames after the 114-byte rule header. Small sprites use
16x16 frames; large sprites use 32x32 frames. All complete file frames are kept:
the supplied small-sprite file has 147 frames even though the gameplay loader
reads 143 (the intro reads 147). PNG alpha is used only if every keep-mask pixel
has zero colour bits. Otherwise the entire sheet is an opaque preview on index
0, with conflicts reported; the original `(background AND mask) OR bits`
compositing remains recoverable from raw data and separate masks. Mask PNG white
means keep background; black means replace. Palette indices are never discarded.

The source-documented full pictures are `qaz.pic`, `demo.pic`, `lord.pic` (four
planes) and `load.pic` (five planes), 320x200, plane-major. `lord.pic` and
`load.pic` carry RGB12 palette words at the end. Unknown picture variants retain
raw bytes and, when the plane layout is known, indices without inventing colours.
`font.dat` and `mouths.pic` are preserved raw because this exporter does not
verify their rendering formats. Level entries retain eight bytes and a BE16
word, not guessed semantic labels or campaign maps. These are static extractions,
not complete animation reconstruction, copper effects or a complete Amiga renderer.
The playable renderer consumes the exported indices and keep masks directly.

`test-assets` and the old extractor suite use only synthetic data: bitplane and
endianness sentinels, mask conflicts, PNG CRC/scanlines, raw/hash integrity,
unknown formats, output refusal, variable OFS lengths and extension-chain
corruption. Both run as part of `make test`.

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

## Playable original graphics

`graphics.c` loads `pixels/qaz.pic.indices.u8`, selected `landN` indices/keep masks,
and the LAND-selected `spritesN.dat` indices/keep masks. It validates regular
non-symlink files, exact lengths (143..147 complete small-sprite frames), indices
0..15 and masks 0..1 before replacing the active assets. Startup/reset fails
with a diagnostic rather than silently falling back to placeholder art. LAND
rules continue to come from `POPULOUS_DATA_DIR` through the existing parser.

`POPULOUS_ASSET_DIR` names the export root, not `pixels/` or `graphics/`. If unset,
make embeds the absolute local `binaries/populous-extracted` path; the executable
can therefore start from the repo root or port directory. If `POPULOUS_DATA_DIR`
is unset, it defaults to `<asset-root>/raw`. Explicit invalid settings never fall
back. Rebuild after moving the repository, or set both paths explicitly:

```sh
POPULOUS_ASSET_DIR=/path/to/populous-extracted \
POPULOUS_DATA_DIR=/path/to/populous-extracted/raw \
./populous_c_port/populous_init_port
```

The framebuffer stays indexed until all `(destination AND keep) OR colour`
operations finish; only then does it expand RGB12 to ARGB. This matters for
LAND2's nonzero colour bits under keep masks. PNG alpha previews are not used.
The source projection is `left=176+16*(u-v)`, `top=64+8*(u+v)-8*map_alt`,
row-major 8x8 cells; overlays are eight pixels higher. Front faces use small
frames 77/76. The height-aware cursor uses the corresponding 9x9 vertex lattice,
`x=192+16*(u-v)`, `y=72+8*(u+v)-8*vertex_height`, choosing a nearby projected
vertex (frontmost on ties). This is bounded geometric picking, not an exact
translation of every original `_sculpt` screen-region heuristic.

Walkers use direction frames 0,2,..14 plus owner and animation phase; settlements
use 64+phase+owner*2. Frames animate from simulation ticks and stop when paused.
The canonical core commits movement destinations immediately, so sprites use
that authoritative cell without the original sub-cell movement interpolation.
Battle animation is representative, not original battle timing. No copper,
large-sprite specials, original cursor/font, audio or functional power toolbar
is claimed. A separate compact HUD explains the implemented keyboard controls.

`test-graphics` checks synthetic AND/OR conflicts, clipped/invalid blits, malformed
asset lengths/indices/masks, and projection/picking. `test-game` checks literal
terrain/overlay/occupant pixels, transactional load failure, scaled isometric
sculpt/magnet input, minimap navigation, pause, and reset. Its original top-down
match regression remains available through an explicit diagnostic toggle. A
second end-to-end match uses actual isometric clicks and minimap navigation only.
With battle reinforcement, friendly state transfer and supplied real data the
isometric sequence reaches victory at tick 9058 (271 battles, 296 settlement
formations, population 93085/0), and legal
self-sculpt reaches defeat at tick 694. Normal and sanitizer suites cover these
paths; no outcome, population or camera patches are used in the match sequence.

```sh
make -C populous_c_port test-graphics-local ASSET_DIR="$PWD/binaries/populous-extracted"
```

The implementation's real X11 check captured original graphics, settlements,
pause and a visible raised vertex in `/tmp/populous-original-graphics.png` and
`/tmp/populous-original-sculpt.png`. These are local screenshots, not distributable
assets or proof of a human-played victory. The older independent review below
used the diagnostic renderer; a fresh independent graphics review is separate.

## Build and run

Requires GCC-compatible C11, GNU make, SDL2 development files (`sdl2-config`), and
`timeout` for the test targets. A display is needed for interactive use.

```sh
make -C populous_c_port
./populous_c_port/populous_init_port
./populous_c_port/populous_init_port 7
make -C populous_c_port test
make -C populous_c_port test-sanitize
```

The optional positional argument is a decimal integer from 0 through `INT_MAX`;
selection is `N % 8`, so 8 selects slot 0. No argument also selects slot 0. These
eight configurations retain their synthetic terrain and low-byte seed offsets.

### Original world starts

```sh
./populous_c_port/populous_init_port --level 0
./populous_c_port/populous_init_port --level 494
```

`--level N` accepts decimal 0..494 without wrapping. Negative numbers, non-digits,
overflow, unknown options and extra arguments fail. `r` repeats the selected
original level or synthetic scenario. Original starts load `level.dat` from the
same raw-data directory as LAND; the synthetic path does not need that file.

`level_data.c` decodes exactly 990 bytes into 99 ten-byte records. Selection uses
record `N/5`, internal level `5*N`, and seed `(seed_offset + ((5*N) & 7)) & 0xffff`.
The parser rejects unsupported terrain outside 0..3, nonregular/symlink files,
and wrong lengths, and preserves its output on failure. Other byte fields are
retained rather than asserted authentic; startup rejects zero group counts,
over-capacity populations and insufficient placement terrain. Installed
power masks and enemy parameters do not implement the missing powers or AI.

World creation consumes the four initial random draws, three original raise/walk
passes, conditional decoration draws, and original first-inhabitant scans. It
runs against staged A4 storage; a failed reset preserves live state and assets.
The map update preserves original block 47 until submerged. Host safety deliberately
skips the original enemy-fallback probe at logical cell 4096. Preferred placement
preserves co-located inhabitants and uses the original last-writer `map_who`
representative; fallback scans still require an empty cell. The enemy fallback's
original leader-order quirk is preserved.

The previously rejected starts `70, 74, 93, 94, 160, 201, 204, 472, 477, 478, 479`
now retain their original placement rather than being relocated. Hostile current-cell
encounters are handled before settlement/departure at the existing C movement gate;
ongoing battles absorb reinforcements into the newcomer's existing party without
replacing either combatant's partner link.
Settlement guards prevent hidden/duplicate towns while retaining town/split-walker
sharing. Blocked navigation now sets `0x40` and counter 7. Waiting consumes one
LAND walking loss per tick, advances frames 101/102, and retries when the previous
unsigned counter exceeds 14 (nine waiting updates after blocked entry). A retry
does not incur a second walking loss. Seeded `0x20`/`0x60` wait states share this
consumer, but the original collision-handshake producer (setting another peep's
`0x20`, frame 101 and counter 0) and delayed movement occupancy remain unported.
This is bounded state support, not complete original movement timing. The C loop
retains battle precedence over unsupported mixed battle/wait flags so a partner
index cannot become a retry counter. Waiting artwork is not yet original-faithful.
Ordinary `_join_battle` adds the newcomer's population (capped at 32000), stronger
weapon byte, leader/view references and nonzero four-byte metadata to its party,
then consumes the newcomer. Battle frames and partner links are preserved; joining
itself rolls no casualties and pays no reward. Invalid battle pairs are rejected.
Metadata remains opaque original bytes, not a dereferenceable host pointer.
This does not restore original phased encounter timing or special-knight behavior.
Friendly destination merging separately restores `_join_forces` state transfer:
nonzero opaque metadata, leader/view references and stronger weapons follow the
survivor; its waiting bits clear and frame resets. A metadata-bearing donor cannot
merge into a target whose flags are exactly town-state `1`. Battle reinforcement
does not perform that waiting/frame reset. Same-cell friendly encounter timing
and full special-figure metadata semantics remain deferred.

This is world initialization, not 495 completed campaign levels. Independent
assembly-derived comparisons are cross-implementation evidence, **not** an
independent Amiga execution trace or proof of complete game parity.

## Swamp power

`S` is a port-specific one-shot swamp target binding, not an original keyboard
mapping. Selection requires the active swamp permission (`0x10`) and **more than
5000 mana**, following the original selector; execution accepts **exactly 5000**.
A valid left-click consumes targeting even if execution is then rejected and
restores the previous terrain/magnet tool. Right-click does not cast or sculpt
while targeting; minimap navigation is still available. Pause, war and finished
matches prevent gameplay casts and retain targeting. Other valid rejected casts
(for example, insufficient mana after selection) consume the one-shot target.

Accepted casts cost 5000, award the player-caster 50 score and consume exactly
60 random draws: 30 attempts within a 7x7 area. Empty cells of block 15, 31, 32 or
66 become block 53. Duplicates, occupied cells and invalid terrain are skipped,
not retried. Even an accepted cast placing nothing pays and advances the RNG.
The original target is `(selected_vertex - 1) & 255` per axis: vertex zero wraps
to 255 and therefore yields an empty cast, rather than a clamped edge cast.
C bounds checks precede map reads, unlike the unsafe assembly read ordering.

An ordinary walker dies on swamp at its next completed eight-frame walking cycle,
before navigation and walking loss. Shallow swamp returns to block 15; bottomless
swamp (game-mode bit 1) remains. This is not water/drowning. Frame 53 already comes
from LAND; the original effect code is recorded, but audio is not implemented.
Editor free casting is deliberately absent: `P` remains diagnostic display state
and never bypasses costs or permissions. Original toolbar activation, editor mode
and computer swamp selection remain separate TODOs.

## Earthquake power

`Q` is a port-specific immediate binding for the original viewport-origin command,
not a clicked target. It uses the current camera origin (0..56), requires active
permission `0x08` and at least 2500 mana, and is rejected during pause, war or a
finished match. Keyboard repeats do not cast again; diagnostic `P` is not a free
editor mode. An accepted cast charges 2500 and awards the player-caster 25 score.

Two sequential 9x9 vertex passes run column first, row second. Zero heights consume
no randomness; nonzero heights draw `%5` (0 unchanged, 1 raise, 2..4 lower).
Recursive slope propagation happens immediately. Only the affected cell region
is rebuilt afterwards; outside movement history and all occupancy remain intact.
The existing build counter accumulates changes rather than being reset by casting.
No direct population damage is invented; later simulation handles changed terrain.

This milestone implements terrain rules and effect code 76, **not** the original
20-step terrain-only screen shake or sound. The original shake offsets the terrain
by -4/-1 scanlines, not the entire framebuffer or a horizontal 120 pixels. Full
presentation, editor bypass and original toolbar wiring remain TODOs.

## Flood power

`F` is a port-specific immediate global flood binding. It requires active permission
`0x80`, at least 40000 mana, and no pause/war/finished match. It needs no selected
vertex and does not consume an armed swamp target; keyboard repeats are ignored.
Accepted casts charge 40000, award the player-caster 250 score, and set effect 74.

Every positive height in the entire 65x65 vertex map decreases by one, without
recursive propagation or RNG. The whole 64x64 cell map is regenerated, clearing
all movement-history words; dirty bounds and build counter stay unchanged. Even
an all-water cast pays and rebuilds the map. Population and occupancy do not
change during casting; subsequent ticks apply existing water/settlement rules.
Those rules still use the port's immediate movement model, including its battle
water exemption, not full original drowning/tick parity. Sound, editor bypass,
original toolbar and AI power selection remain deferred. `P` never makes it free.

## Volcano power

`V` casts immediately at the viewport origin, not the mouse. Gameplay admission
requires active permission `0x40`, at least 10000 mana and no pause/war/finished
match. It charges 10000, awards the player-caster 100 score and records effect 75.
There is no diagnostic/editor bypass, clicked-target mode or automatic crater.

Five nested vertex-square passes consume 165 draws, raising on residues 1, 2 or
4 modulo 5. The following 8x8 lava scan consumes 64 further draws for a valid
viewport: residue zero writes block 47 and clears its overlay unless protected.
Literal original behavior protects owner 0's leader twice, not both leaders,
and both magnet targets. With no owner-0 leader, cell zero is protected. Lava
selection consumes its draw even when protection prevents writing. Invalid or
stale owner-0 leader references are rejected before payment as a host-safety guard.

Only actual height changes expand dirty bounds; lava writes do not. The original
clamped bounds rectangle is rebuilt rather than unconditionally rebuilding the
whole footprint. No residents or occupancy are changed directly by casting.
Original toolbar, audio, editor mode and exact queued-command timing remain open.

## Armageddon power

`A` is the port's immediate Armageddon binding, not the original debug-A instant
win. It requires active permission `0x0100`, at least 80000 mana and an active,
unpaused match. It charges the casting owner, adds 5000 score only for the player
caster, clears exactly the four heading bytes of every allocated normal peep
record (including inactive records), sets effect 73 and activates war. Recasting
while war is active is allowed and pays again. Keyboard repeats are ignored.

Casting does not instantly move people, alter terrain, evacuate towns or recenter
magnets. Subsequent ticks perform the migrated war targets, town evacuation,
ordinary navigation, deferred water assistance and combat. Manual terrain and
magnet mouse actions are locked during war; camera, pause, reset and quit remain
available. Armed swamp/tool selection is retained. There is no editor bypass.

This is gameplay integration, not complete original-game parity: original toolbar,
audio, editor/AI activation, knights, phased water behavior and original end-game
accounting remain open. C still recomputes surviving population immediately and
reports simultaneous extinction as a draw. In the controlled cast-through-SDL
island test, either caster activates war, a route is raised, combat starts at
tick 41 and the stronger blue party wins at tick 43 (2963/0 population), identically
in normal and sanitized runs. Funding and unequal armies are test fixtures;
there is no terrain/population injection after the cast.

## Controls

- Blue is your population; red is the opponent. Original houses/castles show
  settlements. The lower HUD and window title report population, mana, tool,
  pause and outcome. The historic toolbar artwork does not activate powers.
- Left/right mouse: raise/lower the selected vertex. Neighbouring vertices follow
  the original maximum slope of one, height 0..8. Cost is 10 plus 4 per changed
  vertex; the original admission threshold is 10, so a large operation can leave
  negative mana. NO_BUILD, up-only and near-town restrictions are enforced.
- `1`: settle/explore (default); `2`: join friendly walkers; `3`: seek nearby foes.
- `0`: gather walkers at the initial magnet (32,32). After a leader reaches it,
  `m` selects magnet placement; left-click moves the target for 200 mana. `m`
  again returns to terrain. Settlements release walkers when over capacity.
- `a`: Armageddon, if allowed, for 80000 mana; terrain/magnet clicks lock during war.
- `v`: volcano on the current viewport, if allowed, for 10000 mana.
- `f`: global flood, if allowed, for 40000 mana.
- `q`: earthquake on the current viewport, if allowed, for 2500 mana.
- `s`: select one-shot swamp targeting when allowed and mana exceeds 5000;
  left-click targets, then the previous terrain/magnet tool returns.
- Arrows/numpad: scroll by world cells. Click the upper-left diamond minimap to
  recenter anywhere (also while paused); its white outline marks the viewport.
  `d`: explicit top-down diagnostic view; `p`: dots in that diagnostic view only.
  Space: pause. `r`: reset the selected start and original graphics; synthetic
  starts return to view (6,6), original starts center near the friendly leader.
  Escape/window close: quit. Pause/outcome stop rules, not navigation/drawing.

The opponent independently settles, grows, explores, merges and fights. Flatten
plots to encourage settlement, wait for population/mana, and lower the four
corners of hostile plots to deny land. The goal is to remove the opposing living
population, including settlements. Unported special powers remain inactive.
No timer or population target declares a winner.

## Verification

`test-level` checks the standalone decoder, numbering, seed wrap and secure file
loading. Synthetic `test-game` also covers generator RNG checkpoints, placement,
block 47, camera selection and transactional original reset. Fixture writes are
disabled in `POPULOUS_TEST_REAL_DATA` mode to keep original files read-only.

The explicit supplied-export regression checks all 495 starts, including exact
initial inhabitants and last-writer occupancy, asset loading, rendering, 100 ticks
and reset:

```sh
make -C populous_c_port test-level-local \
  LEVEL_DATA_DIR="$PWD/binaries/populous-extracted/raw"
SDL_VIDEODRIVER=dummy timeout 120s ./populous_c_port/tests/test_game_sanitize \
  --levels "$PWD/binaries/populous-extracted/raw"
```

Build `test-sanitize` first for the second command. This exhaustive integration
uses `LEVEL_TEST_TIMEOUT` (default 120s), separately from the 20s focused tests.
The export layout must have `raw/` alongside `pixels/`. Successful initialization
does not imply every modified level table is valid or every campaign feature exists.

`test-runtime` checks real SDL software rendering and partial-initialization
cleanup, injected resource failures, launcher handoff, and invalid video driver
errors. `test-game` includes the production PRG and launcher code and checks A4
boundaries/unaligned access, map separation, all eight start populations and
occupancy, deterministic reset, renderer output, SDL event handling, pause,
scroll limits, scaled mouse coordinates, inert powers, allocation-failure
cleanup, repeated Escape/quit, and CLI failure propagation. These and `test-graphics` are included
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
POPULOUS_TEST_REAL_DATA=1 \
  POPULOUS_DATA_DIR="$PWD/binaries/populous-extracted/raw" \
  POPULOUS_ASSET_DIR="$PWD/binaries/populous-extracted" \
  SDL_VIDEODRIVER=dummy ./populous_c_port/tests/test_game
```

The fixed scenario-0 sequence selects fight tendency, simulates settlement and
combat, then after tick 6000 sends legal right-clicks to the four corners of
visible hostile plots every eight ticks. Budget: 12000 ticks, no teleportation
or population/result edits. With the supplied original grass LAND it reached
victory at tick 7515 after 266 engagements and 136 settlement formations, with
17045 friendly population remaining. Normal and ASan/UBSan runs agreed. Clearing
the movement delta on town evacuation changes the previous 9058-tick result;
no test script or winning-state assertion was weakened to recover old timing. A
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
   both populations change and original settlement buildings appear.
2. Press Space. At the default 640x512 window, vertex (10,10) initially projects
   to (384,256). A click while paused must not change terrain. Unpause and
   left-click once; its raised position is now (384,240). Right-click there
   to restore it; allow a second between clicks.
3. Let the match run for about five minutes. Watch for battle sprites;
   record the population/title rather than assuming an outcome on a timer.
4. Lower one visible hostile plot at a time: right-click its four vertex corners,
   using the white hover marker at each projected vertex, at no more than one click
   per second. Watch mana and the changed terrain. Repeat as needed, letting the
   simulation run; there is no guaranteed human-paced completion bound yet.
5. If an outcome appears, verify it remains stable, then press `r` to restore the
   initial populations. Click the window to focus it and press Escape. Also test
   closing the window on a fresh launch. Report missing outcomes or quit failures.

This acceptance covers the bounded intermediate port, not complete original-rule
fidelity or a human-tested winning strategy. Synthetic starts and unsupported
features listed above remain intentional limitations.

### Existing local WSL toolchain

The historical `/tmp/populous-toolchain` did not survive a reboot. This development
machine now has checksum-verified Ubuntu GNU Make 4.4.1 and SDL2 2.32.10 development
files under `$HOME/.local/share/populous-toolchain`, with matching libraries under
`$HOME/.local/share/populous-runtime`. In a new Bash shell, from the repo root:

```bash
source "$HOME/.local/share/populous-toolchain/env.sh"
make -C populous_c_port all test test-sanitize
SDL_VIDEODRIVER=x11 SDL_RENDER_DRIVER=software ./populous_c_port/populous_init_port
```

The environment script supplies tool, multiarch-header, linker, runtime-library
and ALSA paths. Use it also before the opt-in original-data integration command.
These user-local directories are not repository contents or a bootstrap installer:
a fresh machine still needs the development dependencies listed above. A normal
SDL2 installation does not need this environment script. Successful dummy-driver
tests do not prove that WSLg forwards a visible window to the Windows desktop.

## Legal and credits

Original disassembly and analysis: tetracorp
(<https://tetracorp.github.io/populous>). This port is for study, preservation
and reverse-engineering education. Use lawfully obtained original data; do not
distribute commercial assets or original game files.
