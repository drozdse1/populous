# Populous (Amiga) - Disassembly + C/SDL2 Port

This is a fork of the excellent [tetracorp/populous](https://github.com/tetracorp/populous) disassembly project.

The original work provides a clean IRA disassembly of the 1993 Hit Squad budget release of *Populous*.

## Additions in This Fork

- `populous_c_port/` — An incremental, low-level C + SDL2 translation of the disassembly.
  - Preserves the original launcher + `populous.prg` split.
  - Emulates A4 as base register for globals.
  - Uses SDL2 for original isometric LAND graphics, small sprites, the 32x32 sprites of the wandering "funny" objects and the original game background; full presentation remains incomplete.
  - Repairs signed A4 storage, native-pointer ownership, initial map/population setup, reset and clean shutdown.
  - Launcher intro: falling land0 blocks, the POPULOUS title letters (`SUB_1802`, glyphs `LAB_5A0C`) and the endlessly repeating fading credit strips of `SUB_714` (`SUB_1D1A`/`SUB_5026`), with Rob Hubbard's title music, the 16-actor block-building script of `SUB_714` (byte code `LAB_1272`, `SUB_50B2` sprites, `SUB_1CBC` blocks, `SUB_3F92` pictures), keyboard abort and the `SUB_58FA` palette fades.

The port now has a **bounded original-rule game loop**: movement, settlement, growth, mana, ordinary combat, legal sculpting and population-based outcomes use external original `landN` rules. Automated SDL-input matches reach victory/defeat. Independent X11/WSLg checks also observed growth, combat, a stable defeat, reset and clean exit through agent-driven inputs at human rates. This is a playable intermediate port, not a complete Amiga conversion: remaining gaps are listed in `TODO.md`, and a human-tested winning strategy has not been demonstrated. Original world initialization is available separately through `--level N`; see the port guide for bounded overlapping-start support and remaining campaign/state-machine limitations.

Gameplay powers include swamp (`S`), earthquake (`Q`), flood (`F`), volcano (`V`), knight (`K`) and Armageddon (`A`), each subject to the original active permissions and mana costs. Knight converts the current leader for 7500 mana and connects enemy pursuit, combat, town burning and original sprites. Armageddon connects ordinary war movement and deferred water assistance, not instant victory; flood, Armageddon and special-figure paths (drowning, follow-up ticks, end-of-game accounting) have been audited against the original; power audio is wired as in the original (each cast queues its `_effect` sound for channels 2 and 3, swamp is silent, the quake rumble stops when the quake ends; `--power-audio`); every power's whole chain (selection, permission, mana/score, world change, inhabitants, frame, audio) is exercised end to end, accepted and refused, for both sides (`--power-chains`); the toolbar action path (icons, tendency/mode posts, pause via the mailbox) follows `_zoom_map`; full original tick-parity verification against reference traces remains open.

See [the current port guide](./populous_c_port/README.md) for tested commands, controls and limitations. [grok.md](./grok.md) is historical development context, not the current feature contract.

`./populous_c_port/populous_init_port N` selects one of eight synthetic scenarios using `N % 8`; no argument selects slot 0. `./populous_c_port/populous_init_port --level N` selects an original world numbered 0..494 without wrapping. Invalid or negative arguments fail. Both initial populations and terrain are generated before the first game frame.

## Screenshot

![Populous C/SDL2 port with original isometric graphics and settlements](docs/images/Screenshot%202026-11-02%20131410.png)

User-captured gameplay screenshot, November 2, 2026, showing the defeat screen of the intermediate port.

## Directory Structure

```
.
├── src/                      # Original 68000 disassembly (from tetracorp)
├── populous_c_port/          # C/SDL2 port (our translation work)
├── grok.md                   # Detailed project + development documentation
└── README.md
```

## Building the C Port

This branch builds with the Makefile in `populous_c_port`. Run all commands from there:

```bash
cd populous_c_port
```

Build:

```bash
make
```

Test:

```bash
make test            # all test suites
make test-sanitize   # the same suites under ASan/UBSan
make test-net        # only the two-player network tests
```

Run:

```bash
./populous_init_port                  # launcher with intro
./populous_init_port --conquest       # straight to world selection
./populous_init_port --tutorial       # tutorial
./populous_init_port --level 5        # original world by number (0..494)
./populous_init_port --code NAME      # original world by level code
```

Two players over a LAN (TCP port 2611):

```bash
./populous_init_port --host                # machine A
./populous_init_port --join 192.168.1.20   # machine B, IP of machine A
```

Pick different gods on both machines and click CONNECT ME. Machine A must allow
incoming TCP port 2611 in its firewall. See `populous_c_port/README.md` for details.

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
