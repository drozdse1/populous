# Populous Amiga C/SDL2 port

## C source layout

The playable PRG lives under a unity build:

- `a4_state.h` — A4 globals with speaking names (`war_flag`, `map_vertex_word`, `core_mana`, …)
- `populous_prg.c` — includes `modules/01_preamble.c` … `04_render_input.c` in order
- existing units — `populous_init.c` (launcher), `graphics.c`, `audio.c`, `land_data.c`, `level_data.c`, `sdl_lowlevel.c`, `pop_log.c`

See `modules/README.md`.

## Logging

Leveled logger in `pop_log.h` / `pop_log.c` (TRACE/DEBUG/INFO/WARN/ERROR).

- Default level: **INFO** (quiet). Old `[PRG]`/`[SDL]`/`[EXEC]` traces are DEBUG.
- `POPULOUS_LOG_LEVEL=debug` (or `trace`/`warn`/`error`/`off`) to change.
- `POPULOUS_LOG_TIMESTAMPS=1` prefixes `HH:MM:SS`.
- WARN/ERROR go to stderr; lower levels to stdout.

Keep behaviour identical when splitting further; run `make test`.


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
It does not activate campaign progression.
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
including 10 mana for a saturated vertex. Editor mode (`_paint_map`) skips the
mana admission and the payment like war (see "Editor mode").
Nonwar submerged rescue uses this consumer too; other command opcodes and
remaining AI producers are separate work.
Starts restore controllers 0/1, divisors 1/selected speed and latches 1/0.
Runtime initialization rejects zero speed transactionally; the disk parser still
retains that byte. For controller 1, a corrupt zero divisor discards that owner's
command and logs an error without dividing or clearing its latch. Controller 0
does not use that divisor. No replacement cadence is guessed.

Primary-water magnet navigation queues deferred raises during war, or outside
war for controller word exactly 1 when `GAME_MODE_NO_BUILD` is clear
(`src/populous_prg.asm:5277-5327`). Ordinary and admitted heading navigation queue
the walker's current cell, not the blocked destination, then continue fallback
navigation. Opaque invalid headings do not produce. Fatal-water mode does not
suppress this still-dry walker's command; only the old-water caller has that gate.
Residents already carrying bit `0x10` also queue outside war when their owner's
controller word is exactly 1 and `GAME_MODE_NO_BUILD` is clear
(`src/populous_prg.asm:3491-3543`). Fatal-water mode and the existing battle
exemption suppress this entry producer. It runs before water loss or land recovery,
including on already-dry terrain, without checking funds, cadence or the old latch.
Newly assigned water flags do not produce in that same pass. Later residents
overwrite earlier same-owner commands; rejection leaves a pending command intact.
No terrain changes until the consumer runs, and normal raises still cost mana.
Both callers share the same validated admission and mailbox writer; their
state-specific fatal/heading guards remain at the callsites. `--magnet-rescue`
checks word-controller and mode boundaries, current-cell coordinates, opaque
references, fallback history, mixed producer ordering and paid retry-to-crossing.
Focused/full normal and sanitized suites, real-data victories, historical cadence
regressions and all 495 starts passed with unchanged limits after enabling this
walking producer; measured match outcomes remain those listed below.
A rejected primary swamp step can also queue a raise
(`src/populous_prg.asm:5328-5383`): outside war, controller word exactly 1, with
both no-build and build-only-up clear. It targets the adjacent swamp cell, unlike
water assistance's current-cell command, then continues ordinary fallback.
Heading-absent walkers still take their earlier direct-swamp return; fallback-only
swamps do not produce. Fatal-water mode is not an admission restriction here.
The port rejects geometrically invalid primary steps and opaque invalid heading
references before production; these are explicit host bounds, not literal Amiga
behavior. `--swamp-rescue` checks 28,512 full-A4 admission cases, navigation,
shared writes and paid swamp clearance followed by crossing. Focused/full normal
and sanitized suites, original-data victories, historical cadence regressions and
all 495 starts passed with unchanged inputs, outcomes and limits. Other AI remains
disabled.
Mixed-water recovery and combat-water differences remain; canonical entry,
submerged loss and dry recovery are described below.
A controlled two-island fixture raises a route solely through movement-produced
commands, enters combat at tick 41 and reaches mutual extinction at tick 45 in
normal and sanitized builds. This is a prerequisite integration test, not an
original campaign trace or a human-playability claim.

The every-eight-ticks sinking strategy used before nonwar rescue is no longer a
verified victory strategy with this producer enabled: at tick 12000 the isometric
run still has populations 65387/9594 (225 battles), and the diagnostic run has
28842/142403 (325 battles; changed from 26818/177062 by one
collision-wait mark at turn 10625). Those earlier victory timings are historical evidence,
not current rescue-enabled results. `--rescue-legacy-match` preserves both old
scripts as explicit ongoing-at-12000 regressions. The default victory scripts now
use the same legal lowering inputs every tick after tick 6000, rather than every
eighth tick. Both win at tick 11928: isometric 125 battles, 394 settlement formations,
population 205816/0; diagnostic 125 battles, 401 formations, population 219245/0.
Defeat remains tick 695. Start, game rules, strict victory assertions and test
limits are unchanged. Focused/full normal and sanitized suites, original-data
victories, both historical-script regressions and all 495 starts passed.
`--water-rescue` covers admission, word-controller boundaries, deferred payment,
recovery, same-owner overwrite, shared town latches and stopped ticks.

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
Fatal mode removes them without producing a rescue command; admitted rescue
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
occupancy apply; no extra movement occurs in the recovery update. Candidate selection, full-pool special figures and original cleanup parity
remain outside this bounded change. `--water-town` covers full-A4 state/loss
matrices, selective splits and growth revival, clipped footprints, deferred rescue,
and 200 frozen pause/result/quit cases. Focused/full normal and sanitized suites,
original-data matches and all 495 starts passed with unchanged limits. The two
victory sequences at this milestone finished at ticks 11938 and 11953; defeat remained tick 695.

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
all 495 starts passed. At this milestone victory ticks remained 11938/11953 and defeat 695, with
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
Original sound effects, the heartbeat and the in-game `gmusic1` music play
(see "Sound"); SAVE A GAME / LOAD A GAME use the original `.GAM` format (see
"Save and load"); multiplayer remains absent; the conquest campaign is
partially ported (see "Game end, score and conquest"). The original toolbar
is clickable (see Controls); its options and game-options icons open the
original requesters (see "Requesters and options"); the message requester is
not ported. Selected tendency, mode, music, FX
and pause icons are highlighted with the original `_toggle_icon` diamond XOR;
the short press flashes on the power and view icons are not reproduced.

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
`values.json` with stored LAND tables, palettes, neutral level records and the
`_load_sound` tables of the sound banks `gmusic1`/`gwords` and the eight
launcher title samples of `popsam` (PCM stays raw), and
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
`mouths.pic` (5040 bytes) is six 48x35 mouth frames as `_read_mouth`/
`_draw_mouth` use them (asm:16668-16690, 19894-19922): 0x348 bytes per frame,
each row 6 bytes of plane 0, 1, 2, 3, MSB left; exported as frame-major indices
and a 288x35 sheet in the `lord.pic` palette (the screen it is drawn on). Frame
0 matches `lord.pic` pixel for pixel at byte 0x12, y 0x87, where the game draws
it. `font.dat` is preserved raw (the port reads it as `_font_data`). Level entries retain eight bytes and a BE16
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
frames 77/76. Mouse picking uses the corresponding 9x9 vertex lattice,
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
copper, large-sprite specials or audio is claimed; the toolbar requesters are
described under "Requesters and options".

The mouse pointer is the original hardware sprite: `_pointer` (A4 0x84b6) is
set as in the original (query 0, terrain player*3+1, magnet player+2, swamp 5;
asm:615-620, 1054-1056, 2042, 2110-2131, 7480-7487) and indexes the
`_sprite_to_amiga` frames 0x44,0x4e,0x50,0x51,0x4f,0x53,0x52,0x81
(asm:16783-16830). Every non-zero plane value is opaque in _palette (colours
16-31 repeat 0-15); the mask is ignored as in `_convert_sprite`; the hotspot
is the top-left pixel (`_move_pointer`, asm:19928-19972). It is drawn last over
the full 320x256 frame; the former substitute vertex cross is gone. After a
swamp target the port keeps its magnet tool and pointer, where the original
falls back to mode-1. `_move_mana` (asm:3164-3244) draws sprite 69 on the
panel mana scale: x 160+16*(step-1)+2*eighth, y 8+8*(step-1)+eighth from the
`_mana_values` thresholds, at (311,87) above 160000.
The lower HUD (y 200..255) is a port addition, not original: seven 40-column
lines in the original 8x8 font (`amiga_text`, raw/font.dat == `_font_data`)
explain status and the implemented keyboard controls. `--battle-frames` checks all 65,536 stored
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
With nonwar rescue, the every-tick input sequence and supplied real data, the
isometric sequence reaches victory at tick 11928 (125 battles, 394 settlement
formations, population 205816/0), and legal self-sculpt reaches defeat at tick 695. Normal and sanitizer
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

Requires a GCC- or Clang-compatible C11 compiler (`cc` by default; override with
`make CC=...`), GNU make, and SDL2 development files (found via `sdl2-config`, or
`pkg-config sdl2` as fallback). The test targets use `timeout` or `gtimeout` when
available and otherwise run without a time limit. A display is needed for
interactive use.

```sh
make -C populous_c_port
./populous_c_port/populous_init_port          # launcher: load.pic, intro, choice
./populous_c_port/populous_init_port 7        # any argument skips the launcher
./populous_c_port/populous_init_port --tutorial
make -C populous_c_port test
make -C populous_c_port test-sanitize
```

### macOS (Apple Silicon and Intel)

Untested by the maintainers so far; the Makefile detects Darwin and Homebrew SDL2.

```sh
xcode-select --install                 # Apple clang + make
brew install sdl2 pkg-config coreutils # coreutils is optional (gtimeout for tests)
make -C populous_c_port
POPULOUS_ASSET_DIR="$PWD/binaries/populous-extracted" ./populous_c_port/populous_init_port
make -C populous_c_port test
```

The asset path is baked in at build time as `../binaries/populous-extracted`
relative to `populous_c_port/`; `POPULOUS_ASSET_DIR` overrides it at run time.
macOS ships GNU make 3.81, which is sufficient; `brew install make` provides
`gmake` if needed. ASan/UBSan (`make test-sanitize`) work with Apple clang.

Without arguments the original launcher screens run first (see "Launcher and
intro"); its choice selects the start. Every argument bypasses the launcher and
goes straight to the game. The optional positional argument is a decimal integer
from 0 through `INT_MAX`; selection is `N % 8`, so 8 selects slot 0. These
eight configurations are port test fixtures: they retain their synthetic
terrain and low-byte seed offsets and are not the original custom path. The
launcher's "normal game" choice starts the original custom landscape 0 (see
"Custom games"). `--tutorial` (port flag) starts the tutorial like the
launcher's tutorial button.

### Custom games

The original boots with `_in_conquest` -1 (asm:24313) and `_seed` 0, so
`_animate`'s first `_setup_display(0,1)` (asm:503-512) builds a custom
landscape; only `_start_of_game` 2 (world screen) and 3 (tutorial) differ.
`_setup_display` with `_in_conquest` -1 (asm:430-457) sets `_start_seed =
_seed`; a nonzero `_seed` draws one `___newrand`, and when the argument is -1
and the draw is odd `_ground_in` steps 0,1,2,3,0 (no `conq_05`). No `conq_`
byte is read: powers, rating and speed stay in `_stats` (`_options`) and the
rules in `_game_mode` (`_set_options`). Then `_clear_map` (asm:1023-1188,
without the level mana), `_make_alt`, `_make_map(0,0,63,63)`,
`_make_woods_rocks` and `_place_first_people`, whose custom counts are 1, or 3
when `_serial_off` and that side's `_stats+$c` rating is above 4
(asm:7664-7678, 7766-7780); `_seed` + 1. The port runs the shared conquest
generator for this (`custom_generate`). Entry points:

| entry | original | `_seed` / argument |
| --- | --- | --- |
| launcher "normal game" (choice 1) | asm:503-512 | 0 / 1 (boot `_stats`, `_game_mode` $10) |
| MOVE TO NEXT MAP | `_new_map` 1, asm:18917-18932 | the live `_seed` / -1 |
| RESTART THIS MAP | `_new_map` `_ground_in`+2 | `_start_seed` / `_ground_in` (same land) |
| CONQUEST, a number typed | asm:12262-12272, 13236-13239 | that number (`___atoi` low word) / -1 |
| end of a custom game | asm:14410-14432 | the live `_seed` / -1 |

`r` restarts the same landscape. Port deviations: the original never reloads
A4, the port carries `_stats` (both records), `_game_mode` and `_player` into a
custom start (boot values after a fresh `prg_main` or before any non-synthetic
start; a synthetic fixture does not count); `_paint_map` and two-player state
are not carried. The tutorial's end now also continues with the next custom
landscape, as the original. `prg_main` without the launcher
(`_start_of_game` 0, the tests) keeps synthetic slot 0.

### Original world starts

```sh
./populous_c_port/populous_init_port --level 0
./populous_c_port/populous_init_port --level 494
```

`--level N` accepts decimal 0..494 without wrapping. Negative numbers, non-digits,
overflow, unknown options and extra arguments fail. `r` repeats the selected
original level or synthetic scenario. Original starts load `level.dat` from the
same raw-data directory as LAND; the synthetic path does not need that file.

```sh
./populous_c_port/populous_init_port --code GENESIS
./populous_c_port/populous_init_port --code shisoding
```

`--code NAME` enters a world name like `_show_world` (asm:12278-12300): `GENESIS`
is world 0, any other name is decoded by `_decode` (first matching start, middle
and end syllable, then the world `n = 0,5,..,5000` whose `_newrand` seed yields
the code). The name is uppercased; unknown syllables, extra characters, unused
codes and worlds above 2470 (no `level.dat` record) fail like other bad arguments.
World `n` starts `--level n/5`.

```sh
./populous_c_port/populous_init_port --conquest
```

`--conquest` opens the world selection screen first, as the original's CLI
argument `2` (`_start_of_game == 2`, asm:85-91, 536-551); GENESIS runs paused
behind it. The launcher's conquest button does the same. See "World selection".

## Launcher and intro

`populous_main` (src/populous_main.asm) is the launcher; `populous_init.c` ports
its flow. `SUB_41C` (asm:330-346) loops over `SUB_456` (asm:347-442): `load.pic`
(5 planes, own 32-colour palette, `SUB_17B2` asm:1842-1865) waits for a fresh
left click, latched by the vertical blank handler (asm:3743-3800: `-32004`
button, `-32000` X, `-32002` Y in lo-res pixels, Y 0..199). The click position
picks the choice that `SUB_16E` stores at `$42` and `populous.prg` reads as
`_start_of_game` (prg asm:94-96):

| click (lo-res) | choice | port path |
| --- | --- | --- |
| X >= 231, 156 <= Y < 170 | 3 tutorial | `_start_game` asm:13975-13999: `_tutorial`/`LAB_5172E` stats, seed `0x69bc`, `_tutorial_game_mode` 1, `_pause`, 10000 mana (asm:1134), 15 vs 3 people, ground 0, custom game (`in_conquest` -1) |
| X >= 231, 170 <= Y < 184 | 2 conquest | world selection first, as `--conquest` |
| X >= 241, Y >= 184 | 1 game | the default start: custom landscape 0 (see "Custom games") |
| elsewhere, or no click for ~5 s | 0 | the intro, then `load.pic` again |

The labels of the three bottom-right buttons were not read from the picture; the
mapping follows the code. The original's timeout is a busy loop of 0x55730
passes (asm:404-407); the port uses 250 frames (5 s, about 94 cycles per pass on
a 7.09 MHz 68000). Escape or closing the window quits (port addition).

The intro `SUB_1186` (asm:1517-1566) shows `demo.pic` (gameplay palette), starts
the title music and runs, one step per 50 Hz frame:

1. `SUB_10A2`/`SUB_110A` (asm:1428-1516, launcher RNG `SUB_6538` seeded 1): 100
   land0 blocks of the 10x10 `LAB_640C` landscape (heights `<< 3`, `SUB_1BE8`)
   fall from 232..244 lines above and bounce into place (`SUB_1C12`
   asm:2200-2284 isometric positions, `SUB_19EE` clipping).
2. `SUB_E40` (asm:1234-1427): 18 title letters fall, then rise to the top.
3. `SUB_714`: the credits list `LAB_1DCE` (23 lines, `SUB_1D1A` 8x8 font.dat
   rendering).

After the credits `load.pic` returns (`SUB_41C` loops). A fresh left click in
any phase ends the intro at once (asm:1553-1555 and the other loops); no key
advances it in the original. With the default timer the phases take 59, 58+48
and 23 x 50 frames.

Deviations: the title letters (`SUB_1802`) come from `LAB_5A0C` inside the
launcher executable, which is not extracted, so phase 2 keeps its timing but
draws nothing; `SUB_714`'s script (byte code `LAB_1272`..`LAB_15F6`, the upper
block layer, copper strip and scrolling) is not ported: each credit line is
shown for 50 frames at the bottom of the screen and the intro ends after one
pass instead of following the script; `SUB_1D1A`'s fifth plane is dropped on
the 16-colour demo palette. Palette fades are not reproduced.

Title music: `SUB_2C36` (asm:2418-2471) reads `popsam` (one 0x1c01e-byte Read; the file has that size),
`SUB_2F9C` (asm:2645-2698) starts song 0 and `LAB_3060` (asm:2706-2896) runs per
vertical blank; `title_music.c` ports this Rob Hubbard replayer. `popsam` holds
only eight samples (BE32 length, BE16 rate in Hz, signed 8-bit PCM; period base
`0x369e99 / rate`). The score, 16 instruments (loop points, volume, vibrato,
pulse-width limits), three pulse waves and the note table are launcher code
(`LAB_3532`..`LAB_3F91`, asm:2899-3089), transcribed byte-exact from the
listing into `title_music.c`. Paula DMA restart/latch semantics (play LC/LEN,
then repeat the pair written later) are emulated in `audio.c`. The music plays
only during the intro and is stopped before the game starts. A missing or
invalid `raw/popsam` leaves the intro silent.

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
from LAND; the original effect code 66 plays its drumkit sound (see "Sound").
In editor mode (`_paint_map`, see "Editor mode") the cast skips the mana,
war, pause and permission gates and costs nothing (asm:8297); the 50 score is
still awarded. The toolbar icon and `s` arm targeting in editor mode
regardless of mana and permission (asm:2031). Computer swamp selection remains
a TODO.

## Editor mode (`_paint_map`)

The original free/paint mode is entered with PAINT MAP and left with PLAY GAME
in the Game Setup requester (`_game_options` asm:13044-13056; OK keeps it,
CANCEL restores the opened value, asm:13326). START GAME on the world screen
clears it (asm:12769); it is part of saved games. There is no host key for it
(the former diagnostic `p` toggle is gone). While it is set:

- `_animate` (asm:676-738) runs neither `_do_funny` nor `_move_peeps`: people
  stand still, `_game_turn` does not advance and no end of game is detected.
  Mailboxes are still executed.
- Every power prologue skips the mana, war, pause and permission tests and the
  payment: `_do_raise_point`/`_do_lower_point` (asm:7104-7126, 7189-7219),
  `_do_flood`, `_do_quake`, `_do_volcano`, `_do_swamp`, `_do_knight` (after its
  leader test) and `_do_war` (asm:7569, 7865, 8048, 8297, 8404, 8484). Scores
  are still added, except Armageddon's. `_do_magnet` has no bypass.
- Map clicks sculpt without `_ok_to_build` and the game-mode build limits
  (`_sculpt` asm:7307-7312, 7417-7438): left raises, right lowers, free.
- `_paint_the_map` (asm:15123-15274) posts at `_cur_x`/`_cur_y`, the vertex
  under the pointer: F1/F2 place a friendly/enemy walker (Shift: leader,
  replacing the current one), F3 cycles a tree, F4 a rock (below height 7, from
  `_last_tree`/`_last_rock`), F5 deletes rock, object and occupant, F6/F7 double
  plus 500 below 100000 (Shift halves) the good/evil mana, F8 mirrors the
  landscape (`_rotate_all_map`), Delete clears all land and people
  (`_clear_all_map`), 1..9 select landscape 0..8 (`_do_action` y=13). These are
  executed by `_get_message` opcodes 7..13 and `_do_action` y=9..13
  (asm:18017-18183, 18943-18999). Outside editor mode the keys keep their
  normal bindings (digits 1..3 still set the tendency).
- Port limits: only landscapes 0..3 exist, so keys 5..9 keep the current
  landscape (the original sets `_ground_in` and tries to load a custom one);
  byte coordinates outside the 64x64 map are dropped instead of writing past
  `_map_who`; paused games do not consume the posted commands until unpaused,
  as for all mailbox commands in the port.

## Earthquake power

`Q` is a port-specific immediate binding for the original viewport-origin command,
not a clicked target. It uses the current camera origin (0..56), requires active
permission `0x08` and at least 2500 mana, and is rejected during pause, war or a
finished match. Keyboard repeats do not cast again. An accepted cast charges 2500
and awards the player-caster 25 score; in editor mode the gates and the charge
are skipped, the score is not (asm:7865).

Two sequential 9x9 vertex passes run column first, row second. Zero heights consume
no randomness; nonzero heights draw `%5` (0 unchanged, 1 raise, 2..4 lower).
Recursive slope propagation happens immediately. Only the affected cell region
is rebuilt afterwards; outside movement history and all occupancy remain intact.
The existing build counter accumulates changes rather than being reset by casting.
No direct population damage is invented; later simulation handles changed terrain.

After admission, charge, effect code 76 and score, and before any terrain change,
the original 20-frame shake runs (`_do_quake` asm:7895-7925) for every caster:
frames alternate -4/-1 scanlines, only terrain tiles, overlays and front faces
move (panel, HUD and shield stay), peeps are not drawn (asm:15842-15849) and the
frames show the pre-quake terrain. Ticks are frozen during the synchronous loop;
each frame is presented and waits about one vblank (20 ms). Without a screen or
assets (headless rule tests) the shake is skipped; rules are identical either way.
Effect 76 rumbles during the shake and is cut afterwards (`_kill_effect(2,4)`,
asm:8037-8040). The toolbar icon posts opcode 3.

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
drowning/tick parity. Effect 74 plays (see "Sound"); AI power selection remains
deferred; the toolbar icon posts opcode 14/4. In editor mode it is free of gates
and cost, still scoring 250 (asm:7569).

## Volcano power

`V` casts immediately at the viewport origin, not the mouse. Gameplay admission
requires active permission `0x40`, at least 10000 mana and no pause/war/finished
match. It charges 10000, awards the player-caster 100 score and records effect 75.
Editor mode skips the gates and the charge, not the score (asm:8048). There is
no clicked-target mode or automatic crater.

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
The toolbar icon posts opcode 6. Effect 75 plays; exact queued-command timing remains open.

## Knight power

`K` immediately converts the current living friendly leader using active
permission `0x20` and the original 7500-mana cost. Pause, war, a finished match,
quit or an invalid leader reject the action without spending mana; keyboard
repeats are ignored. In editor mode a leader is still required, but the mana,
war, pause and permission gates and the charge are skipped (asm:8404).

The cast sets the leader's byte-20 marker to 1, selects an enemy through the
original heading algorithm, leaves the magnet at that resident's position,
releases the leader and records effect 69. A player cast earns 150 points.
It does not increase weapons, spend population, force a walking state, move
people or alter terrain. Existing town, waiting and battle states retain their
normal dispatch order. A second cast needs a newly acquired leader.

Pursuit, swamp avoidance, combat completion, town burning, timed remnants and
canonical original sprites are connected. Terrain/tool use remains available;
Armageddon later clears headings through its existing path. The toolbar icon
posts opcode 14/5. Effect 69 plays; strategic AI, phased movement and complete combat parity remain open.

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
available. Armed swamp/tool selection is retained. In editor mode Armageddon
skips gates, charge and the 5000 score (asm:8484-8516).

This is gameplay integration, not complete original-game parity (the toolbar
icon posts opcode 14/3 outside editor mode):
AI activation (effect 73 plays), other special figures, phased water behavior and original end-game
accounting beyond `good_pop` remain open. In the controlled cast-through-SDL
island test, either caster activates war, a route is raised, combat starts at
tick 41 and the stronger blue party wins at tick 43 (2963/0 population), identically
in normal and sanitized runs. Funding and unequal armies are test fixtures;
there is no terrain/population injection after the cast.

## Sound

`audio.c` emulates the four Paula channels behind the original `audio.device`
requests: signed 8-bit samples, rate = 3546895 / period Hz (PAL), volume 0..64
per request, zero-order hold, channels 0/3 left and 1/2 right, request queues
with cycle counts (0 repeats until aborted). SDL is optional at runtime: without
an audio device (`SDL_AUDIODRIVER=dummy` or none) the mixer still runs and the
game is silent. `_start_game` opens the channels and loads `raw/gmusic1` from
`POPULOUS_ASSET_DIR` (asm:13966-13972); a missing or invalid bank is reported
and the game continues silently.

`gmusic1` (90534 bytes) follows `_load_sound` (asm:20712-20872, big-endian):
64 measure lengths, sequence (8 entries), measure block (16384 bytes, 0x1000
read, rest skipped), 104 drumkit entries (period, length, volume = priority,
sample, repeat word), 12 sample lengths and 73046 bytes of PCM, plus a final
word. The loader rejects any file not consumed exactly or with indices outside
the original buffers. `gwords` (79076 bytes, 104 drumkit entries, 9 speech
samples; entries 0..9 are the words) is the `_won_conquest` lord speech: loaded
in place of `gmusic1` while the lord screen is shown, played on channel 2 (see
Game end), then `gmusic1` is loaded again (asm:14745-14756).

What plays (all faithful to the 50 Hz vbi, asm:19620-19672):

- `_effect` on channels 2 and 3 (then music waits 40 frames): 0x42 walker
  drowns in swamp, 0x45 knight, 0x49 Armageddon, 0x4a flood, 0x4b volcano,
  0x4c quake, 0x4d magnet; 0x43/0x44 (by `_toggle`) for a visible battle and
  0x46 for a visible flag-0x80 remnant (`_move_sprite` flags read by
  `_move_peeps`, asm:3434-3454, re-arming after 3 passes).
- Heartbeat: 0x47 every `_tempo` frames and 0x48 at `_tempo-_beat_two` on
  channel 0; `_tempo = 50 - 2*(enemy/own population)`, at least 7
  (asm:564-605). Only while effects are on.
- Music: `_PlayMeas` plays one beat on all four tracks every 10 frames
  (note `'A'`+n = sound n, below `'A'` is silent) and follows the sequence.
- `_PlaySound` keeps a busy channel's sound against a lower priority; a drumkit
  repeat word queues a second request.

Deviations: the second `_PlaySound` request's cycle count lies past the pushed
arguments in the original (an undefined stack word); the port uses the repeat
word itself (5 for the quake rumble, 6..14 for music pads). `_PlayMeas` is
skipped for a bank without a sequence. The vbi is paced from `SDL_GetTicks`.

## Controls

- Blue is your population; red is the opponent. Original houses/castles show
  settlements. The lower HUD and window title report population, mana, tool,
  pause and outcome.
- Toolbar power icons (lower-left 3x3 diamond grid, `_zoom_map` asm:1942-2042):
  flood, Armageddon, volcano, earthquake, knight and swamp. Either button posts
  the original mailbox command, executed on the next tick; volcano and quake use
  the current viewport. Swamp arms map targeting like `s`. Costs, permissions
  and war are checked by the power routines; paused or finished games ignore
  the icons. In editor mode the Armageddon icon posts nothing.
- Other lower-left toolbar icons (`_zoom_map` asm:1838-2395), left or right button:
  - Arrow pad: scroll one cell; its centre shield recentres on the viewed figure.
  - Tendency icons: settle, join, fight and go to magnet, posted as mailbox
    commands like the powers (ignored while paused or finished).
  - Mode icons: query, terrain and papal magnet select the tool and disarm the
    swamp target. In query mode (`_interogate` asm:2719-2765) world clicks never
    sculpt or move the magnet: hovering or left-clicking a drawn figure gives a
    10-turn temporary view, right-click views it permanently.
  - Shield (top right, `_show_the_shield` asm:2769-3147): for the viewed figure
    the walker/town/battle sprite with its population, capacity or strength bars,
    plus the good/bad population bars. A dead figure clears the view. The owner
    flag and weapon icons are not drawn: the asset export has no `_icon_data`.
  - Ankh: left views the leader (else the magnet), right the magnet.
  - Crossed swords: cycle through battles. Walker icon: cycle own walkers;
    right picks ordinary walkers, left walkers with a heading. The original
    battle search never ends when no battle exists after the last viewed slot;
    the port bounds that scan. Viewing marks a 10-turn temporary view.
- Lower-right system icons: `zzZ` toggles pause (also while paused or after the
  end); the music and FX icons toggle `_music_off`/`_effect_off` (nonzero = on;
  switching on stops all channels, asm:1744-1781). Options, game
  options and the message icon post their original `_do_action` codes 7/8/2;
  the consumer opens `_options` (7) and `_game_options` (8) at the next tick,
  the message requester (2) stays inert. While a requester is open the game
  is paused, keys other than Escape are ignored and left clicks go to it.
- Left/right mouse: raise/lower the selected vertex. Neighbouring vertices follow
  the original maximum slope of one, height 0..8. Cost is 10 plus 4 per changed
  vertex; the original admission threshold is 10, so a large operation can leave
  negative mana. NO_BUILD, up-only and near-town restrictions are enforced.
- `1`: settle/explore (default); `2`: join friendly walkers; `3`: seek nearby foes.
- `0`: gather walkers at the initial magnet (32,32). After a leader reaches it,
  `m` selects magnet placement; left-click moves the target for 200 mana. `m`
  again returns to terrain (from query mode `m` selects the magnet). Settlements release walkers when over capacity.
- `k`: convert the current leader to a knight, if allowed, for 7500 mana; unavailable during war.
- `a`: Armageddon, if allowed, for 80000 mana; terrain/magnet clicks lock during war.
- `v`: volcano on the current viewport, if allowed, for 10000 mana.
- `f`: global flood, if allowed, for 40000 mana.
- `q`: earthquake on the current viewport, if allowed, for 2500 mana.
- `s`: select one-shot swamp targeting when allowed and mana exceeds 5000;
  left-click targets, then the previous terrain/magnet tool returns.
- Arrows/numpad: scroll by world cells. Click the upper-left diamond minimap to
  recenter anywhere (also while paused); its white outline marks the viewport.
  `d`: explicit top-down diagnostic view (in editor mode it shows people as dots).
  Space: pause. `r`: reset the selected start and original graphics; synthetic
  starts return to view (6,6), original starts center near the friendly leader.
  Escape/window close: quit. Pause/outcome stop rules, not navigation/drawing.
- After the game ends the end screen appears; a left click on its requester
  button (as `_check_cancel(button,0)`) or, as a host convenience, any key
  except Escape continues (key repeats are ignored), Escape still quits.
- After a conquest win the lord screen follows; any left click continues to
  the world selection, Escape still quits.

The opponent independently settles, grows, explores, merges and fights. Flatten
plots to encourage settlement, wait for population/mana, and lower the four
corners of hostile plots to deny land. The goal is to remove the opposing living
population, including settlements.
No timer or population target declares a winner.

## Game end, score and conquest

`game_tick` detects the result like `_move_peeps` (asm:4323-4347): your own empty
population or surrender loses first, then the opponent's wins. There is no draw.
Right after that tick the main loop runs the `_end_game` statistics once
(asm:14019-14316; deferred from inside the tick so its A4 effects stay as before):
the seven `_end_text` rows (A4 0x8e7e, 46 bytes each) receive `GAME WON/LOST`,
battles won, knights (peeps with a heading and population), towns (flag 1,
frame other than 0x2a) and castles (frame 0x2a), "you" padded to five characters.

The final `_score` (A4 0xae72) adds to the accumulated power/placement score:
+5000 if you won more battles; per power bit 8..256 of the permission words
+1000 if yours is clear and +1000 if the opponent's is set; with `_serial_off`
(set by every single-player start, asm:12768) `(10 - opponent speed) * 15`
(asm:14239-14247); x10 on a win; at least 500; above 555555 it becomes 515090.

The end screen draws these rows with the original 8x8 font (`raw/font.dat`,
byte-identical to `_font_data`; without the file the box has no text) at the
`_end_text` positions +16, plus `TRY IT AGAIN` (conquest loss) or `NEW GAME`.
Continuing (see Controls):

- conquest win (an original `--level`/`--code` start): `_won_conquest`
  (asm:14460-14491) advances `_level_number` by `score/5000 + 1`, rounds up to
  a multiple of 5 and caps at 2470; winning 2470 wraps to world 0. After the
  click the lord screen (below) is shown; after its click the world selection
  screen opens with `_message` set to the next world's code (asm:14492-14501,
  14766-14772), and START GAME starts it through the normal reset;
- conquest loss: the same world again;
- a custom game (`_in_conquest` -1, also the tutorial): the next custom
  landscape from the live `_seed` (asm:14410-14432);
- synthetic scenarios are no conquest: the selected scenario restarts.

Each start re-derives `_level_number`/`_in_conquest` from the selected world and
clears `_score`. The box is the original `_requester(d_screen,16,16,0x120,
0xa6,button)` frame with its single button (asm:14340-14385), with only the
original rows. Not ported: the end screen in the diagnostic view.

### Lord screen

`_won_conquest` (asm:14503-14765, `core_lord_*` in `populous_prg.c`) after a
conquest win, the game frozen until a left click (Escape still quits; other
keys and buttons are ignored, as the loop reads only `_left_button`):

- `_music_off`/`_effect_off` saved and cleared (no music, heartbeat or effects),
  `_kill_effect(0,4)`;
- `_read_lord`: `lord.pic` from the data directory (`POPULOUS_DATA_DIR`, else
  `<assets>/raw`): 0x7d00 bytes of four plane-major planes into the screen, 16
  BE16 RGB12 words into `_tcmap[0..15]`. If it fails, `_level_number` returns to
  the won world, `_message` is emptied (asm:14762-14765) and world selection
  opens directly (showing GENESIS);
- `_read_mouth` (`mouths.pic`), `gwords` loaded, `_seed` = old world (then
  `___newrand` for its name);
- at x 8, y 0xb2/0xba with `___text`: `WELL DONE <rank> YOU CONQUERED ` and
  `<old world or GENESIS> NOW BATTLE AT <next>` (`_con_text` ranks by
  next world/250, LAB_52224..5222C), or after 2470 `WELL DONE YOU HAVE
  CONQUERED EVIL` / `THE BATTLE IS OVER BUT TRY SHISODING` (LAB_52230/52234);
- the loop, one pass per 4 vertical blanks: pass counter 10 plays word 0 on
  channel 2, then when channel 2 is idle `_words[rank*2]` and
  `_words[rank*2+1]` (-1 none); after 250 passes it starts again. After
  2470: `_end_words` 0,1,3,5,6 at passes 10..14 (each when channel 2 is idle),
  mouth frame 5 at byte 0x10, y 0x4b at pass 16, restart after 200. Each pass
  draws the mouth frame at byte 0x12, y 0x87 (35 rows); while channel 2 plays
  the frame bounces 0,1,2,3,4,3,..; idle passes reset it to 0;
- the click: `_kill_effect(0,4)`, `gmusic1` again, `_music_off`/`_effect_off`
  restored, world selection.

Port deviations: the picture uses the `lord.pic` palette and the pointer the
gameplay `_palette` (the original's `_tcmap[16..31]`); the HUD rows below y 200
are black but for `CLICK TO CONTINUE, ESC QUIT`. A short `lord.pic` also counts
as failed (the original checks only `Open`); without `mouths.pic` the mouth is
not drawn (the original would copy stale `_map_steps`), and the frames are kept
in their own buffer instead of the A4 `_map_steps`. The 4-`_waittof` pass is
4 of the port's 50 Hz vbi calls.

## World selection

`_show_world` (asm:12190-12773) is ported as the requester `CORE_REQ_WORLD`
(`core_world_*` in `populous_prg.c`); the simulation is paused while it is open.
`_requester(d_screen,16,0x18,0x120,0xa0,"START GAME","NEW GAME")`, `WORLD TO
CONQUER`, the 16-box name field at 0x20,0x2c with `_message` (A4 0x3058, empty
-> `GENESIS`). A name is a world: `GENESIS` is 0, others go through `_decode`;
an unknown name prints `NO SUCH WORLD` (asm:12296-12309) and returns to the
entry. For a valid world, record `level/25` of `level.dat` is read into
`_conquest` 0x84bc..0x84c5 (asm:12315-12368, the seed word big-endian) and
the lines 1..14 of `_c_text` (asm:24621-24725) are shown with `_con_text`
values (asm:12380-12743): battle number, landscape, reactions, rating, land,
swamps, water, population and YES/NO per power for YOU/HIM.

- START GAME: `_player` 0, `_serial_off` 1, `_paint_map`/`_war` 0, the
  `_stats` flags of asm:12762-12770 and `_level_number`; the result goes to
  `_in_conquest` and the world starts.
- NEW GAME: empties `_message` and starts the `_get_name` entry
  (asm:10307-10488): keys 0x20..0x7a (upper-cased, at most 15), Backspace /
  Delete, Return; START GAME ends the entry, NEW GAME empties it. After a
  rejected name the entry continues after it (strlen), as the original.
- A number (`atoi` != 0) sets `_start_seed`/`_seed` and returns -1: the start
  and conquest-win loops show the screen again; from the game options it makes
  a custom game (`_in_conquest` -1) on that landscape (see "Custom games").

Opened from `--conquest`, game options CONQUEST (asm:13221-13239, also sets
`_new_map`) and after a conquest win. Keys are SDL key-down symbols (no text
input needed); Escape has no meaning in `_get_name` and keeps the port's quit.
Port deviations: the cursor (0x83) blinks every 10 frame ticks instead of 36
loop passes, and Backspace clears the deleted cell at once instead of at the
next blink; a missing `level.dat` stays in the entry instead of the "insert
disk" message; worlds above 2470 (no record) cannot be started; START/conquest
screens clear the screen, the game-options one draws over the setup screen.

## Requesters and options

`_requester` (asm:9901-10270) is ported as `core_requester`: a frame of 16 px
`_icon_data` tiles 12..23 (asm:21763-21927; title 12/13/14, sides 18/20/19,
bottom 15/16/17, button 21/22../23) drawn with `_draw_icon` (asm:19184-19210),
OK/CANCEL (`_end_ok`/`_end_cancel`, A4 0x84c6/0x84f4) placed and sized as in
the original and labelled with the original font; `_check_cancel`
(asm:12106-12153) hit-tests x..x2, y..y+16 inclusive. The original runs every
requester as a blocking loop inside `_do_action`; the port keeps the requester
state, stops `game_tick` while one is open, routes left presses to it at the
pointer (`_mousex`/`_mousey`) and draws it over every frame (original view
only; in the diagnostic view it is open but not drawn). `_tempo` is cleared
when a requester opens rather than on every draw.

- `_options` (`_do_action` y=7, asm:13617-13873; left button: your side, right:
  the opponent; skipped while the other side's mailbox also asks for options):
  `_option_text` (A4 0x8522). In a custom game (`_in_conquest` -1) entries
  1..9 toggle the power bits of `_stats[side]+0xe`, the aggression and rate
  sliders set `+0xc`/`+0x10` (shown as 10-value). OK keeps, CANCEL restores
  the copy (the mailbox bytes excepted, which the original clears afterwards).
  In a conquest game the settings are shown only.
- `_game_options` (y=8, asm:12801-13350, `_game_text` A4 0x87ee): PLAY GAME /
  PAINT MAP (`_paint_map`), GOOD / EVIL (`_player`, `_not_player`, the swapped
  `_stats+6` human flags, `_pointer`), HUMAN VS AMIGA / AMIGA VS AMIGA
  (`_stats+6`), ONE PLAYER (tick only, `_serial_off` is always 1), CUSTOM GAME
  (`_in_conquest` -1), GAME OPTIONS, MOVE TO NEXT MAP / RESTART THIS MAP
  (`_new_map`; a conquest start restarts the current world, which is what
  `_setup_display` builds for both; a custom game builds the next landscape
  from the live `_seed` or restarts `_start_seed`, see "Custom games"), SURRENDER (`_surender` = `_player`, the
  next tick loses). Entries 0..8 are ignored in a conquest game. CANCEL
  restores `_serial_off`, `_paint_map`, `_player`, its `_stats+6` and
  `_in_conquest` as the original.
- `_set_options` (GAME OPTIONS, asm:13362-13509, `_set_text` A4 0x8fc0) over
  the setup screen: custom games toggle the `_game_mode` bits (water fatal,
  swamps bottomless, cannot build, only build up, build near towns); CANCEL
  restores `_game_mode`; it shows `LANDSCAPE NO <_start_seed>`.
- CONQUEST opens the world selection (see "World selection").
- SAVE A GAME / LOAD A GAME open the `_save_load` file requester (see "Save
  and load"); LOAD A GAME only while `_serial_off` (asm:13259-13276).
- Shown but inert: TWO PLAYERS (`_two_players`, serial),
  the two-player serial exchange after both dialogs, and the message
  requester (y=2).

## Save and load

Sources: `_save_load` (requester, `src/populous_prg.asm:10493-10972`),
`_get_name` (`:10307-10488`), `_sort` (`:11016-11079`), `_get_a_dir`
(`:16841-16915`), `_save_game` (`:17123-17385`), `_checksum` (`:17389-17407`),
`_load_game` (`:17412-17703`), the `_game_options` entries (`:13259-13276`).

**Requester.** `_requester(d_screen,0x48,0x10,0xb0,0xb0)` with SAVE/LOAD and
CANCEL over the game setup screen: title SAVE GAME / LOAD GAME, 12 rows of the
drawer listing at x 0x58 (click a row to take its name), arrows 0x88/0x89 at
x 0xd8 (up while the top row > 0, down while top+5 < count, as the original),
DIRECTORY (`_drawer`, y 0x8d) and FILE NAME (`_filename`, y 0xa0). Clicking a
field starts `_get_name` (16 boxes, keys 0x20..0x7a upper-cased, backspace,
Return; OK ends the entry and acts, CANCEL empties the field and closes).
`_drawer`/`_filename` keep their A4 places (0x302c/0x3040) between dialogs. The
listing holds regular files whose upper-cased name (first 29 characters) ends
in `.GAM`, shown without it, sorted. OK with an empty name or CANCEL returns to
the game setup. The name is `_drawer` + `/` (unless it ends in `:`/`/` or has
one character) + `_filename`; `.GAM` is appended unless present. A successful
save sets `_saved` and, like a load, closes the game setup; a failed save shows
ERROR IN SAVING until the next click.

**Where files go.** Relative drawers and names are resolved under
`$POPULOUS_SAVE_DIR`, else `$XDG_DATA_HOME/populous/saves`, else
`~/.local/share/populous/saves`, else `./saves`; an absolute drawer (`/...`) is
used as it is. Saving creates the base directory, not sub-drawers.

**Format** (29646 bytes, all big-endian as the 68000 wrote them, in `_Write`
order):

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | signature `$FFFFB615` |
| 4 | 0x2102 | `_alt`, 65x65 words |
| 0x2106 | 4x0x1000 | `_map_alt`, `_map_blk`, `_map_bk2`, `_map_who` bytes |
| 0x6106 | 0x20 | `_magnet[2]`: 4 words, `good_pop`/`bad_pop` long, mana long |
| 0x6126 | 0x1222 | `_peeps`, 211 records: bytes 0-3, words 4-12, pointer 14, word 18, bytes 20-21 |
| 0x7348 | 2x0x2e | `_stats`, strABC: bytes 0-3, words 4-0x20, pointers 0x22/0x26/0x2a |
| 0x73a4 | 7x2 | `_mode`, `_seed`, `_start_seed`, `_player`, `_ground_in`, `_war`, `_game_turn` |
| 0x73b2 | 10 | `_conquest` (8 bytes, seed word) |
| 0x73bc | 2+2+4+2+2 | `_in_conquest`, `_paint_map`, `_battle_won[2]`, `_game_mode`, `_level_number` |
| 0x73c8 | 4 | `_score` |
| 0x73cc | 2 | checksum |

The checksum is the 16-bit sum of the big-endian words of `_alt` (0x1081) and
the four maps (0x800 each) plus the low word of `good_pop + bad_pop`; it is
also stored in `_land_checksum`. Peep pointers are Amiga addresses of `_peeps`
records (`$53014 + 22*slot`, the IRA image's `_peeps`), 0 for none; the port's
host values (slot+1) are converted both ways.

**Load** (after the original): the file data, then `___load_ground(_ground_in)`
(LAND rules and graphics), `_not_player`, `_god_magnet`/`_devil_magnet` from
`_magnet`, `no_peeps` = last peep with a non-zero population + 1, the first
three (mailbox) bytes of both `_stats` cleared, `_get_heading` for every live
peep with a pointer, and the terrain pointer.

**Deviations.** The original checks only the signature and reads field by
field; the port reads the whole file and rejects it before any change unless
it has exactly 29646 bytes, the signature, a matching checksum, `_player` 0..1,
`_ground_in` 0..3 and loadable LAND/graphics data, then publishes the staged
state at once (a failed load still closes the setup, as `_save_load` returns 1).
Saves go to a temporary file renamed into place, so a failed save leaves no
partial file. A pointer that is not a `_peeps` record address (for example from
another load address) is read as the peep itself, so `_get_heading` recomputes
it, or 0 in `_stats`. The `level.dat` disk check after a save is dropped. The
port's own tool state is reset to the terrain tool; a synthetic demo start
keeps its restart target. The listing holds at most 256 names.

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
errors; with an injected frame hook and no-op wait (no real sleeps) it clicks
each `load.pic` region through SDL events, steps the intro state machine on the
VBI clock (timeout, three phases, credits, return, click abort in every phase,
deterministic drops), runs the timeout into the intro with music, and checks
that every CLI argument bypasses the launcher. `test-game` includes the production PRG and launcher code and checks A4
boundaries/unaligned access, map separation, all eight start populations and
occupancy, deterministic reset, renderer output, SDL event handling, pause,
scroll limits, scaled mouse coordinates, inert powers, allocation-failure
cleanup, repeated Escape/quit, CLI failure propagation and the sound triggers
(effect ids to drumkit samples, priorities, heartbeat, music beats, toggles)
and the lord screen (picture/palette, text, mouth frames per pass, `gwords`
words on channel 2, click to world selection, `_read_lord` failure).
`test-audio` covers the mixer (period to rate, volume, panning, one-shot,
cycles, loops, queues), the `_load_sound` parser and the no-device path.
These and `test-graphics` are included
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
remove them. Collision wait (`--collision-wait`, asm:4058-4067/4145-4184): an exact-2 walker
still alive after framing (and any move/loss) marks the `map_who` occupant of its
own cell with 0x20, frame 101 and counter 0, unless it is itself, slot >= 208,
already framed or bit 0x10. Under phased movement it reads the occupant named at the walker's new cell.

Phased movement (`--phased-move`, `--walker-extras`; `_move_explorer` asm 4425-4617):
a departing walker releases its previous claim (pos-(+10)), meets the peep named at
its own cell, claims that cell only if empty and moves without writing the
destination. `_set_battle` pulls a walker defender back, `_zero_population` clears
both claims. Also ported: the computer walker's own lower request on block 0x42,
the first-walker (0x82dc) and richest-town (0x82d8) pointers, the oldest-town
pointer, town count 0x99fc, `_one_block_flat` and mailbox opcode 2 (`_do_lower_point`,
charge build_count*4+10). The AI routines reading these pointers, opcodes 3-14 and
the `_toggle` mana increment (toggle is never set in the port) remain open.
With phased timing the former scripts, which only acted after tick 6000, lose at
turn 4801 (128 battles, 182 towns, 0/199237); `--rescue-legacy-every-tick` and
`--rescue-legacy-match` pin that defeat. The default scripts use the same legal
inputs every tick from tick 3000 and win at turn 4185: diagnostic 102 battles,
159 towns, 6713/0; isometric 102 battles, 162 towns, 6987/0. Defeat stays at 695.
These 4185/4801/695 values are historical (before the computer AI).

Computer AI (`--set-devil-magnet`, `--devil-effect`, `--computer-effect`,
`--make-level`, `--opcode-magnet`, `--opcode-action`, `--opcode-effects`): each
unlatched owner runs `_set_devil_magnet` then `_devil_effect` (asm 3254-3270);
towns run `_make_level`. Opcodes 3 quake, 4 swamp, 5 magnet, 6 volcano and 14
tendency/war/flood/knight are consumed. Assumption: the early `_make_level` exit
stores the low byte of the owner's `_stats` address in peep+20 (0xa4/0xd2 at the
disassembly base); this depends on the load address and was not measured, yet it
feeds battle countdown, a signed table index and an exact compare. With the AI
and unchanged inputs: both views win at turn 4697 (32 battles, 78 towns,
1434/0); the legacy scripts lose at 3104 (33/79/0/30639); the defeat phase ends
at 831 (0/6323). `good_pop` (`--good-pop`, signed long at 0x99fe+owner*16) is
reset per pass (asm 3296-3301), summed from pre-move words at the top of the peep
loop (3459-3474) and corrected by slot-order joins (5605-5620, 6725-6740). The
end check (4323-4347) tests the player first, honours surrender and has no draw,
so outcomes appear one pass after the last death. Like `_clear_map` (asm 1023-1188), a start or
`r` reset keeps 0x99fc, good_pop/bad_pop and `_stats`+0x14/+0x16/+0x22/+0x26/+0x2a;
only the next pass clears them, after `_devil_effect` (`--start-reset`). Open: opcodes 7-13, `_toggle` mana,
edge-of-map targets (host rejects x/y>56) and null pointer reads (address 8).
Level parameters in AI decisions (`--ai-level-parameters`): rating `_stats`+$c
keeps the computer on its passive tendency path while towns < 2*rating+15 or
`game_turn`%90 < rating+10 (`_set_devil_magnet` asm 9612-9627); reaction speed
`_stats`+$10 is the `_get_message` cadence (asm 17932-17950): a controller-1 side's
latch clears only when `game_turn` % speed == 0, so it re-decides once per `speed`
turns; allowed powers `_stats`+$e are BTSTed before each `_devil_effect` choice
(war $100, flood $80, knight $20, volcano $40, swamp $10, quake $08; magnet $02/$04
in `_set_devil_magnet`, levelling $01) and again by each power consumer. Level
records install them as asm 406-427 (rating/speed to the enemy record, powers as
`pow<<3|7`). Fixed: `_do_computer_effect` aims at the own magnet cell only when the
computer has NO leader (`TST.W _magnet; BNE`, asm 9530-9536); the port had it inverted.
The 11928 results below are historical (immediate-commit movement).

Original-data integration is explicit:

```sh
POPULOUS_TEST_REAL_DATA=1 \
  POPULOUS_DATA_DIR="$PWD/binaries/populous-extracted/raw" \
  POPULOUS_ASSET_DIR="$PWD/binaries/populous-extracted" \
  SDL_VIDEODRIVER=dummy ./populous_c_port/tests/test_game
```

The fixed scenario-0 sequence selects fight tendency, simulates settlement and
combat, then after tick 6000 sends legal right-clicks to the four corners of
visible hostile plots every tick. Budget: 12000 ticks, no teleportation
or population/result edits. With nonwar rescue and the supplied original grass
LAND it reaches victory at tick 11928 after 125 engagements and 401 settlement
formations, with 219245 friendly population remaining in the diagnostic view.
Normal and ASan/UBSan runs agreed. The original every-eight-ticks script no longer
wins within that budget; `--rescue-legacy-match` retains its new ongoing outcome
as a separate regression. The default's stricter input cadence is explicit, not
a relaxed victory assertion or changed game rule. Historical capacity-override
results were 11953/125/402/212387 (diagnostic) and 11938 (isometric), following the
earlier 7516/263/164/27250 result. The current isometric sequence also wins at
11928. A separate legal self-sculpt sequence verifies defeat and reset/quit. The synthetic
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
