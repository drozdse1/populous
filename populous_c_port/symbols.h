/*
 * symbols.h
 * Complete symbol map from the IRA disassembly of the Populous Amiga main executable.
 * Generated from the user's paste of the full symbol table.
 *
 * Addresses are as reported by IRA V2.11.
 * Use these for comments, forward declarations, and to match the original binary layout.
 *
 * Note: This is the bootstrap/launcher module. The real game logic lives in
 * the loaded "populous.prg" (see strings) and later CODE ranges / other modules
 * in the full repo disassembly.
 */

#ifndef POPULOUS_SYMBOLS_H
#define POPULOUS_SYMBOLS_H

#include <stdint.h>

/* Entry and basic layout */
#define ENTRY_POINT         0x00000000
#define ORIGINAL_OFFSET     0x00000000

/* CODE ranges (actual 68000 instructions) */
#define CODE_RANGE_0_START  0x00000000
#define CODE_RANGE_0_END    0x000000FA

#define CODE_RANGE_1_START  0x00000104
#define CODE_RANGE_1_END    0x00000204

#define CODE_RANGE_2_START  0x00000226
#define CODE_RANGE_2_END    0x000003F8

#define CODE_RANGE_3_START  0x0000041C
#define CODE_RANGE_3_END    0x00000454

#define CODE_RANGE_4_START  0x00000456
#define CODE_RANGE_4_END    0x0000052C

#define CODE_RANGE_5_START  0x00000536
#define CODE_RANGE_5_END    0x00001268

/* ... many more ranges follow in the full binary ... */

/* TEXT / string ranges (data, not code) */
#define TEXT_demo_pic       0x000000FA  /* "demo.pic" */
#define TEXT_populous_prg1  0x00000204  /* "populous.prg" */
#define TEXT_populous_prg2  0x00000211
#define TEXT_startup        0x0000021E
#define TEXT_load_pic       0x0000052C
#define TEXT_graphics_lib   0x000003F8  /* "graphics.library" */
#define TEXT_intuition_lib  0x00000409  /* "intuition.library" */
#define TEXT_dos_lib        0x00002CD4  /* "dos.library" */
#define TEXT_icon_lib       0x00006A06  /* "icon.library" */

/* === All known SUB_ functions with original addresses === */
#define SUB_4               0x00000004
#define SUB_104             0x00000104
#define SUB_11A             0x0000011A
#define SUB_16E             0x0000016E
#define SUB_226             0x00000226
#define SUB_29E             0x0000029E
#define SUB_2B0             0x000002B0
#define SUB_2C6             0x000002C6
#define SUB_2E2             0x000002E2
#define SUB_2FC             0x000002FC
#define SUB_308             0x00000308
#define SUB_358             0x00000358
#define SUB_384             0x00000384
#define SUB_41C             0x0000041C
#define SUB_456             0x00000456
#define SUB_536             0x00000536
#define SUB_714             0x00000714
#define SUB_E40             0x00000E40
#define SUB_10A2            0x000010A2
#define SUB_110A            0x0000110A
#define SUB_1186            0x00001186
#define SUB_15FA            0x000015FA
#define SUB_1696            0x00001696
#define SUB_16E4            0x000016E4
#define SUB_17B2            0x000017B2
#define SUB_1802            0x00001802
#define SUB_1910            0x00001910
#define SUB_19EE            0x000019EE
#define SUB_1B8A            0x00001B8A
#define SUB_1BE8            0x00001BE8
#define SUB_1C12            0x00001C12
#define SUB_1CBC            0x00001CBC
#define SUB_1D0E            0x00001D0E
#define SUB_1D1A            0x00001D1A
#define SUB_2C36            0x00002C36
#define SUB_2DD8            0x00002DD8
#define SUB_2F9C            0x00002F9C
#define SUB_3046            0x00003046
#define SUB_3F92            0x00003F92
#define SUB_5026            0x00005026
#define SUB_50B2            0x000050B2
#define SUB_50E6            0x000050E6
#define SUB_53EE            0x000053EE
#define SUB_5420            0x00005420
#define SUB_5450            0x00005450
#define SUB_549A            0x0000549A
#define SUB_55A2            0x000055A2
#define SUB_55BA            0x000055BA
#define SUB_55E0            0x000055E0
#define SUB_55F8            0x000055F8
#define SUB_5636            0x00005636
#define SUB_5792            0x00005792
#define SUB_58CA            0x000058CA
#define SUB_58FA            0x000058FA
#define SUB_59CA            0x000059CA
#define SUB_59F0            0x000059F0
#define SUB_6538            0x00006538
#define SUB_65F0            0x000065F0
#define SUB_65F8            0x000065F8
#define SUB_6730            0x00006730
#define SUB_68FE            0x000068FE
#define SUB_6922            0x00006922
#define SUB_6944            0x00006944
#define SUB_6968            0x00006968
#define SUB_6A1C            0x00006A1C
#define SUB_6A3A            0x00006A3A
#define SUB_6B56            0x00006B56
#define SUB_6BA2            0x00006BA2
#define SUB_6BB2            0x00006BB2
#define SUB_6BC6            0x00006BC6
#define SUB_6BD2            0x00006BD2
#define SUB_6BDA            0x00006BDA
#define SUB_6BE6            0x00006BE6
#define SUB_6BF8            0x00006BF8
#define SUB_6C00            0x00006C00
#define SUB_6C12            0x00006C12
#define SUB_6C22            0x00006C22
#define SUB_6C3A            0x00006C3A
#define SUB_6C4A            0x00006C4A
#define SUB_6C5C            0x00006C5C
#define SUB_6C6C            0x00006C6C
#define SUB_6C78            0x00006C78
#define SUB_6C8C            0x00006C8C
#define SUB_6C98            0x00006C98
#define SUB_6CAC            0x00006CAC
#define SUB_6CB4            0x00006CB4
#define SUB_6CC2            0x00006CC2
#define SUB_6CCE            0x00006CCE
#define SUB_6CDE            0x00006CDE
#define SUB_6CEA            0x00006CEA
#define SUB_6CF2            0x00006CF2
#define SUB_6CFA            0x00006CFA
#define SUB_6D08            0x00006D08
#define SUB_6D14            0x00006D14
#define SUB_6D20            0x00006D20

/* Library name labels (for reference when we emulate OpenLibrary) */
#define GraphicsLibName     0x000003F8
#define IntuitionLibName    0x00000409
#define DosLibName          0x00002CD4
#define IconLibName         0x00006A06

/* Useful labels */
#define LAB_2EEC            0x00002EEC

/* Complete symbol map from your latest paste (IRA V2.11 full run) */
#define IRA_ENTRY           0x00000000

/* CODE ranges (actual 68000 instructions) */
#define CODE_00000000_000000FA
#define CODE_00000104_00000204
#define CODE_00000226_000003F8
#define CODE_0000041C_00000454
#define CODE_00000456_0000052C
#define CODE_00000536_00001268
#define CODE_000015FA_00001796
#define CODE_000017B2_00001DCE
#define CODE_00002C36_00002CD4
#define CODE_00002CE0_00002D34
#define CODE_00002D5E_00002EDE
#define CODE_00002EEC_00002F94
#define CODE_00002F9C_00003296
#define CODE_00003F92_00004026
#define CODE_00005026_0000548C
#define CODE_0000549A_000057FE
#define CODE_000058CA_000059C8
#define CODE_000059CA_00005A0C
#define CODE_00006538_000065E4
#define CODE_000065F0_0000672E
#define CODE_00006730_000068F6
#define CODE_000068FE_00006A06
#define CODE_00006A1C_00006AFC
#define CODE_00006B02_00006C16
#define CODE_00006C22_00006D28

/* TEXT ranges (strings / data) */
#define TEXT_demo_pic       0x000000FA  /* "demo.pic" */
#define TEXT_populous_prg   0x00000204  /* "populous.prg" (the real game is loaded from this file) */
#define TEXT_startup        0x0000021E
#define TEXT_load_pic       0x0000052C
#define TEXT_graphics_lib   0x000003F8
#define TEXT_intuition_lib  0x00000409
#define TEXT_dos_lib        0x00002CD4
#define TEXT_icon_lib       0x00006A06

/* The big data/BSS after the last CODE contains the real game assets:
   tile bitplanes, copper lists, palettes, level data, object tables, etc.
   We will map those to SDL surfaces/textures in the low-level gfx layer. */

#endif /* POPULOUS_SYMBOLS_H */