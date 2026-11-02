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
Original computer power/auto-level logic and exact Amiga movement
interpolation remain unsupported. The eight positional scenarios remain flat
synthetic plots; `--level` instead uses the assembly-derived original generator.
Merge helpers cap population at 32000; general mana helpers clamp to -250..1999999.
Raw terrain commands, battle casualties and single-winner completion, ordinary
town economy and already-wet non-combat water loss instead preserve their source
word/32-bit arithmetic, using unsigned operations rather than C signed overflow.

The exact `0x80` peep remnant now consumes its original unsigned 16-bit timer
(`src/populous_prg.asm:4283-4315`): it claims only empty occupancy and is removed
when the **old** counter is zero, so 40 lasts 41 updates. It does not walk, lose
walking population, draw RNG or change animation frames. Ordinary encounters
ignore bit-7 residents without deleting them; the immediate-movement occupancy
model and hidden-town safeguards remain in force. Removal uses normal leader/view
cleanup and population accounting. Heading-bearing winners now create this remnant
when they defeat a town, preserving population 1 and timer 40 while burning its
footprint. City burning follows the original 25 offsets, including in-range row
crossings; out-of-array accesses are safely skipped. Ordinary town burning uses
17 movement-validated offsets. Existing water precedence and mixed
state dispatch remain bounded host behavior, not original drowning/phase parity.
`--remnant` and the default suite cover both resident iteration orders, word
boundaries, occupancy, cleanup and stopped ticks. Normal/ASan/UBSan suites, all
495 original starts and original-data match regressions passed for this milestone.

The exact walking-victory state `6` consumes the source-derived celebration
frames (`src/populous_prg.asm:3584-3599,5833-5845`): 85 -> 86 -> 87, then the
completion update clears flag `0x04` and advances the ordinary walker frame to zero.
No navigation, walking loss or RNG occurs during these three updates. Frame
increments wrap as 16-bit words and completion compares the signed result; the
second frame call is retained rather than replaced with an unconditional reset.
Water handling still precedes these bounded consumers. Town-victory state `5`
uses the same celebration frames, but completion clears `0x04` and recalculates
its town frame from current capacity instead of switching to a walker frame.
It does not repaint the footprint or run growth, mana, splitting or evacuation
in that update; ordinary town processing resumes on the following update.
Single-winner victories produce these celebration states. The renderer uses
the original celebration and remnant sprites, plus headed walker/wait/water
variants for their canonical states. Heading presence reads all four host bytes,
not the original pointer's high halfword. Sprite selection does not mutate A4
or add interpolation; mixed-state and full movement parity remain separate
work. `--special-sprites` compares literal frame/anchor cases
against an independent pixel-mask oracle, including clipping, hidden residents,
non-flat terrain and an exact 143-frame sheet. Full normal/sanitized suites,
original-data matches and all 495 starts passed with unchanged test time limits.
The focused `--celebration` tests cover signed frame boundaries, full-A4 state
preservation, later-resident progress, pause/result/quit gates and water precedence.
Normal and sanitized full suites, original-data matches and all 495 starts passed.

An already-set war state resets both destinations and displayed magnets to
cell `0x0820` on each resident update. Exact peaceful towns evacuate when their
land has no capacity, their four-byte heading metadata is nonzero, or war is set
(`src/populous_prg.asm:3425-3432,3600-3633`). Evacuation clears the town flag,
frame and movement delta, removes its existing town footprint and skips growth,
splitting and movement for that update. Population, occupancy, metadata and
leader/view references are retained. Water, battle and waiting dispatch retain
their existing precedence. Gameplay Armageddon connects these ordinary war
navigation and deferred water-assistance paths; special-figure parity is separate.

Ordinary town economy retains its eight-turn cadence but uses the original
arithmetic widths (`src/populous_prg.asm:3863-3882,4007-4014`): sign-extended table
mana wraps at 32 bits, and population growth wraps at 16 bits after the existing
split attempt. The ordinary completion checks the signed population and removes
nonpositive towns, including on non-economy turns. Evacuation remains an immediate
return that bypasses this check. Thus an eligible direct town update can revive
a nonpositive population through growth; the resident-loop entry still skips
initially nonpositive records. Dry `0x11` recovery now reaches this same consumer
after water loss, including its growth-revival and evacuation-bypass behavior.
AI selection and terrain work remain separate work.
This is not full town-consumer or original cleanup parity. `--town-economy`
checks literal word/longword boundaries, cadence, low-byte weapons, direct revival
and evacuation bypass, split/full-slot behavior, and resident-pass death bookkeeping.
Focused/full normal and sanitized suites, original-data matches and all 495
original starts passed; existing match outcomes remain unchanged.

Town footprint maintenance follows economy and any split attempt, before ordinary
death cleanup (`src/populous_prg.asm:3634,4037-4055`). The existing footprint helper
runs only when the town frame changed, a flat block needs conversion, or a city
frame has fewer than nine city-overlay cells. Otherwise terrain and overlays stay
untouched. The incoming frame is captured before ordinary reframing; evacuation
still removes its footprint and returns separately. This does not add missing
AI terrain work or change the footprint helper's existing map-boundary behavior.
`--town-footprint` uses full-A4 coordinate-based expectations for skipped refreshes,
changed/flat/incomplete-city updates, downgrade, edges, death/revival, splitting
and later-resident visibility. Focused/full normal and sanitized suites,
original-data matches and all 495 starts passed; existing outcomes remain unchanged.

Successful town splits initialize only the original child-record fields
(`src/populous_prg.asm:3901-3989`). The reused slot's counter bytes 6/7 and navigation
history byte 21 survive; direction, frame, all four heading bytes and marker byte
20 are cleared. The child inherits the parent's radius before any increment and
its newly assigned weapon. Parent view and owner-leader references transfer
independently when they point to the parent; unrelated views/leaders and third-party
occupancy stay unchanged. Earlier child slots start processing next pass, while
later dry children can advance their initial walker frame in the current pass.
Full-slot special-figure creation and original phased movement remain deferred.
`--town-split` checks poisoned reusable records, allocation/count limits, independent
view/leader transfer, radius boundaries, occupancy and uninterrupted birth-to-walk
progression with inherited navigation history. Focused/full normal and sanitized
suites, original-data matches and all 495 starts passed; match outcomes are unchanged.

Eligible town-economy turns apply the original local capacity overrides before
splitting (`src/populous_prg.asm:3818-3862`). A reframed city with population above
305, controller word exactly 1 and a zero controller latch uses capacity 305 and
sets the latch. The existing internal cheat word can then select capacity 50 for
its owner; no new cheat control is exposed. Neither override changes the real
terrain-derived frame or economy table index. The latch does not queue terrain
work, and remains set even if no normal peep slot is available. Shared-owner
ordering and the existing post-pass latch cadence remain in effect. AI terrain
work, candidate selection and full-pool special figures are still deferred.
`--town-capacity` covers literal predicates, ordering, table-index preservation,
reused/full slots, shared latches and mailbox cadence. Focused/full normal and
sanitized suites, original-data matches and all 495 starts passed. Earlier enemy
splits change match timing; original winner assertions and test budgets remain intact.

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
production remain disabled. Mixed-water recovery and combat-water differences
remain; canonical entry, submerged loss and dry recovery are described below.
A controlled two-island fixture raises a route solely through movement-produced
commands, enters combat at tick 41 and reaches mutual extinction at tick 45 in
normal and sanitized builds. This is a prerequisite integration test, not an
original campaign trace or a human-playability claim.

Newly submerged non-combat residents follow the original entry transition
(`src/populous_prg.asm:3562-3580`): remove any town footprint, set flags to `0x12`,
and clear the stored frame only if its signed word is at least 8. The entry update
preserves population, timers and occupancy; it does not move, queue rescue, apply
fatal-water removal or deduct drowning loss. Those water effects begin on the
next update carrying bit `0x10`. Mixed-water recovery, combat exemption and broader
mixed-state dispatch remain separate work.
This is not full drowning or original movement-phase parity. `--water-entry`
checks full-A4 transitions, town/city footprints at map boundaries, opaque heading
preservation, all 65,536 frame words, subsequent cleanup, deferred rescue and
pause/result/quit gates without changing the match input or winner assertions.
Focused and full normal/ASan/UBSan suites, original-data matches and all 495
original starts passed after this transition.

Already-wet non-combat residents that remain on a zero block use the original
word-sized doubled loss and subtraction (`src/populous_prg.asm:3544-3559`). Death
is decided from the signed wrapped result, not an unbounded C intermediate; for
example population 100 with walk loss 30000 becomes 5636. Word wrapping can leave
up to 32767 population; this path does not apply the ordinary 32000 helper cap.
Survivors preserve their flags, frames, timers, metadata and town footprints.
Fatal mode removes them without producing a rescue command; nonfatal war rescue
is queued before loss, even for a dying producer. Cleanup retains the host's
existing leader/view and occupancy policy. No drowning frame, waiting timer or
movement advances while still submerged. `--water-loss`
checks signed loss boundaries, all 65,536 loss words per owner, state/footprint
preservation, death bookkeeping and deferred rescue. Full normal/sanitized suites,
original-data matches and all 495 starts passed; match outcomes remain unchanged
from the delayed-entry milestone.

Exact water state `0x12` returning to nonzero terrain also pays the wrapped doubled
water loss before recovering (`src/populous_prg.asm:3481-3561`). Fatal mode removes
it even though the terrain is now dry. Otherwise it becomes walker state `2`,
receives one frame call, and only a positive remaining population proceeds to
normal walker processing and its second frame call. Zero population skips that
first frame update; a negative result still advances it before cleanup. Thus a
survivor at frame 6 can move in this update, whereas frame 7 or 93 ends at frame 1
without moving. Rescue terrain does not guarantee survival of the following
recovery update. Other mixed-water states retain the previous host recovery
behavior, except for the bounded town, victory, waiting and remnant states described
below; this change does not establish full drowning, movement or cleanup parity.
`--water-recovery` covers literal full-A4 frame/loss outcomes, negative-population
encounter prevention, swamp ordering, stopped ticks and rescue-to-death sequences.
Full normal/sanitized suites, original-data matches and all 495 starts passed;
the original-data match and war-fixture outcomes remain unchanged.

Dry water-victory states `0x15` and `0x16` retain town/walker celebration after
clearing water. They pay the same wrapped loss, receive the ignored first frame
call and then the celebration consumer's second call. Only that second return
can end celebration and trigger a third town/walker reframe; no movement or town
maintenance follows in the same update. Unlike ordinary walker recovery, negative
remaining population still runs the celebration/reframe sequence before cleanup;
zero population skips framing. Town capacity scratch values are computed before
removing a dying town's existing footprint. Fatal mode removes the original state
without any of these frame calls. Other mixed flags and water-combat timing remain
outside this bounded migration. `--water-celebration` checks signed frame/loss
boundaries, zero versus negative cleanup, capacity scratch values, clipped town
footprints, unrelated resident progress and deferred rescue without state injection.
Focused/full normal and sanitized suites, original-data matches and all 495
original starts passed; original-data outcomes remain unchanged.

Dry waiting-water states `0x32/0x52/0x72` recover to `0x22/0x42/0x62`, retaining
the original waiting consumer (`src/populous_prg.asm:4213-4268`). Wrapped doubled
water loss precedes two waiting-frame calls and the separate word-sized waiting
loss. The old unsigned timer decides retry after increment: values above 14 clear
waiting bits, reframe as a walker and attempt movement even when population is
zero or negative. Cleanup follows the retry, preserving the host's reciprocal
battle-release safeguards. A negative resident cannot depart through the later
movement guard, but can encounter a current-cell opponent first, as in the source.
No extra ordinary walking loss or ordinary swamp-completion trap is inserted.
Fatal mode still removes before recovery; unrelated mixed states remain deferred.
`--water-wait` checks staged word losses, zero/negative retries, unsigned timers,
open/blocked routes, current-cell battle cleanup, dead destination isolation,
mailboxes, stopped ticks and deferred rescue with independent full-A4 expectations.
Focused/full normal and sanitized suites, original-data matches and all 495
original starts passed; original-data match outcomes are unchanged.

Dry remnant-water state `0x90` recovers to `0x80` instead of becoming a walker.
After wrapped water loss it claims only empty occupancy and decrements its old
unsigned timer, even when that loss leaves zero or negative population. Old timer
zero forces expiry; signed nonpositive population also triggers cleanup, but only
after the timer update. Fatal mode removes the original state before loss or timer
work. Frames and terrain remain unchanged; no walking, swamp trap or town cleanup
is introduced. Existing host leader/view and protected-occupancy cleanup still
applies, rather than claiming full original death parity. `--water-remnant` checks
full-A4 dry/wet boundaries, word losses, timer expiry, fatal ordering, map edges,
foreign occupancy, stopped ticks and deferred rescue/death sequences. Focused/full
normal and sanitized suites, original-data matches and all 495 starts passed;
original-data match outcomes remain unchanged.

Dry water-town state `0x11` recovers to exact town state `1` after the wrapped
water loss (`src/populous_prg.asm:3544-3557,3600-4055`). Fatal mode instead removes
the original town state before loss or framing. Nonfatal recovery computes the
first town frame for any nonzero population, including negative words; exactly
zero skips that call. Both then reach the ordinary town consumer without an early
death check. Eligible growth can revive a nonpositive population, while ordinary
completion removes one that remains nonpositive. Evacuation for unsuitable land,
heading metadata or war deliberately bypasses that cleanup: it leaves a walker
record with the post-loss population, which the next pass skips if nonpositive.
The first frame call also affects whether subsequent footprint maintenance sees
a frame change. Existing capacity overrides, split initialization and protected
occupancy apply; no extra movement occurs in the recovery update. Town AI terrain
work, candidate selection, full-pool special figures and original cleanup parity
remain outside this bounded change. `--water-town` covers full-A4 state/loss
matrices, selective splits and growth revival, clipped footprints, deferred rescue,
and 200 frozen pause/result/quit cases. Focused/full normal and sanitized suites,
original-data matches and all 495 starts passed with unchanged limits. The two
victory sequences still finish at ticks 11938 and 11953; defeat remains tick 695.

Battle casualties now use the original `SUB.W` result for survival decisions
(`src/populous_prg.asm:6082-6200`). Both losses use the lesser pre-casualty rolled
strength, divide before multiplying by the unsigned opposing weapon byte, then
subtract modulo 65536. The signed wrapped result decides survival and whether
combat/town framing runs. Thus `32000 - 81610` survives at 15926, while
`10974 - 76510` wraps to exactly zero and skips framing. Negative wrapped results
still frame before completion. The host continues storing nonpositive casualties
as zero while carrying their signed residual locally; it does not reproduce the
original transient negative record storage or simultaneous-death cleanup order.
RNG order, battle scheduling, water exemptions and completion remain unchanged.
`--battle-loss` exercises these word boundaries, independent full-A4 completion
and simultaneous-death expectations, both argument orders, and same-pass resident
continuation. Focused/full normal and sanitized suites, original-data matches and
all 495 starts passed. Victory ticks remain 11938/11953 and defeat 695, with
unchanged winner assertions, inputs and time limits.

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
Gameplay knight creation connects these paths through `K` (see below).
Every valid single-winner fight now uses one original-derived completion path:
walker/town celebration, headed-loser reward precedence,
owner bookkeeping, conditional occupancy and loser-view transfer. Raw heading
presence selects special rewards and burning, even when the reference is opaque;
it is not dereferenced. Ordinary victories also enter the three-update celebration.
Rewards sign-extend their original word and wrap 32-bit mana, flooring only the
loser's result against the active minimum. Completion does not use the general
mana clamp. Mutual death keeps its separate cleanup path.
For town losers, an exactly zero post-loss population retains the entry frame
for the reward; a negative pre-clamp population recomputes the town frame before
terrain changes.
A headed winner burns the town and leaves a timed remnant. An ordinary winner
may occupy the defeated town, setting the original conquest timer and
protecting the newly drawn town from loser cleanup. Higher-slot new states can
already advance later in the same resident pass; no global deferral was added.
Host scheduling and `core_remove` semantics remain unchanged; this is not full
combat parity. Original previous-cell cleanup remains separate work.
`--battle-over` covers the unified completion
path, including full-A4 ordinary-town cases with signed rewards and successful
or failed conquest. Full normal/sanitized suites and all 495 starts passed;
ordinary match inputs and winner assertions are unchanged.
The focused `--heading-town` suite covers town
reward boundaries, independent burn-footprint expectations, conquest, same-pass
state consumption and remnant expiry. Normal/sanitized full suites, original-data
matches and all 495 starts passed for this extension.
`--heading-battle` verifies full-A4 outcomes, raw presence, reward precedence,
word counters, signed mana boundaries, third-party occupancy and ordered
producer/consumer ticks. Full normal/sanitized suites, original-data matches and
all 495 starts passed without changing the existing winner assertions.
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
Battle updates now advance the original three-frame cycles: 70-72 for ordinary
combat, 130-132 or 134-136 for one headed participant (by its owner), and 138-140
for two. Both frame calls follow casualties and precede completion; exact-zero
population skips framing, while a negative pre-clamp result still advances.
The renderer uses those stored frames. Both current-cell and destination
encounters initialize attacker and defender frames after writing combat links;
no casualty or RNG draw occurs at entry. A render-only representative fallback
remains for malformed/legacy frames. Exact-zero town losses after normal entry
retain their combat frame and use the original fallback reward of 100; direct
completion fixtures without entry still exercise retained town-frame rewards.
Full combat timing parity remains separate work. No
copper, large-sprite specials, original cursor/font,
audio or functional power toolbar is claimed. A separate compact HUD explains
the implemented keyboard controls. `--battle-frames` checks all 65,536 stored
word values for each base, death/zero distinctions and stored-frame pixels;
the full suite also checks once-per-pair resident updates. Full normal/sanitized
suites, original-data matches and all
495 starts passed with unchanged winner assertions. `--battle-entry` additionally
checks both encounter paths, raw heading bytes, signed frames, retry cleanup,
pre-casualty pixels and entry-to-town-reward behavior; the same full verification
passed after enabling entry framing.

`test-graphics` checks synthetic AND/OR conflicts, clipped/invalid blits, malformed
asset lengths/indices/masks, and projection/picking. `test-game` checks literal
terrain/overlay/occupant pixels, transactional load failure, scaled isometric
sculpt/magnet input, minimap navigation, pause, and reset. Its original top-down
match regression remains available through an explicit diagnostic toggle. A
second end-to-end match uses actual isometric clicks and minimap navigation only.
With original capacity overrides and supplied real data the isometric sequence
reaches victory at tick 11938 (125 battles, 392 settlement formations, population
192496/0), and legal self-sculpt reaches defeat at tick 695. Normal and sanitizer
suites cover these paths; no outcome, population or camera patches are used in
the match sequence.

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
change during casting. The first subsequent resident update marks newly submerged
non-combat residents wet and removes their town footprint without population loss;
water loss or fatal removal follows on the next wet update. These rules still use
the port's immediate movement model and battle-water exemption, not full original
drowning/tick parity. Sound, editor bypass,
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

## Knight power

`K` immediately converts the current living friendly leader using active
permission `0x20` and the original 7500-mana cost. Pause, war, a finished match,
quit or an invalid leader reject the action without spending mana. Diagnostics
are not an editor bypass; keyboard repeats are ignored.

The cast sets the leader's byte-20 marker to 1, selects an enemy through the
original heading algorithm, leaves the magnet at that resident's position,
releases the leader and records effect 69. A player cast earns 150 points.
It does not increase weapons, spend population, force a walking state, move
people or alter terrain. Existing town, waiting and battle states retain their
normal dispatch order. A second cast needs a newly acquired leader.

Pursuit, swamp avoidance, combat completion, town burning, timed remnants and
canonical original sprites are connected. Terrain/tool use remains available;
Armageddon later clears headings through its existing path. Original toolbar,
audio, strategic AI, phased movement and complete combat parity remain open.

`--knight` checks atomic casts, all 256 leader flags, costs, permissions, HUD and
post-cast ordinary actions. Its controlled SDL-K scenario reaches pursuit,
combat, burnt town, celebration and remnant expiry at tick 68 for both owners,
without post-cast state injection. A separate funded original-level smoke runs
100 ticks for each side. Full normal/sanitized suites and all 495 original starts
passed; these fixtures do not establish a manual winning strategy.

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
audio, editor/AI activation, other special figures, phased water behavior and original end-game
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
- `k`: convert the current leader to a knight, if allowed, for 7500 mana; unavailable during war.
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
victory at tick 11953 after 125 engagements and 402 settlement formations, with
212387 friendly population remaining in the diagnostic-view sequence. Normal and
ASan/UBSan runs agreed. Original capacity overrides change the earlier
7516/263/164/27250 result without changing the input script, 12000-tick budget or
winning-state assertion. The separate isometric sequence finishes at tick 11938. A
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
