/*
 * Populous (Amiga 1989) - First chunk translation to C + low-level SDL2
 *
 * This is a direct structural + semantic translation of the first chunk
 * of the IRA disassembly you posted (from the equates through SUB_536
 * and the early loader tables).
 *
 * The goal is to make the 68000 logic readable while staying close to
 * the original operations (memory allocation patterns, A4-relative
 * globals, library opens, copper-ish setup, the demo.pic / load.pic
 * loader, mouse region selection, etc.).
 *
 * Graphics: We use a very low-level SDL2 layer that emulates Amiga
 * bitplanes + palette + "present".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "amiga_hardware.h"
#include "amiga_globals.h"
#include "sdl_lowlevel.h"
#include "symbols.h"      /* launcher symbols */
#include "symbols_prg.h"  /* prg (main game) symbols from this map */
#include "graphics.h"
#include "audio.h"
#include "title_music.h"
#include "intro_data.h"
#include "pop_log.h"
#ifndef POPULOUS_DEFAULT_ASSET_DIR
#define POPULOUS_DEFAULT_ASSET_DIR "binaries/populous-extracted"
#endif

/* Global state (replaces the huge A4-relative area) */
AmigaGlobals g;

/* Our low-level Amiga screen */
static AmigaScreen *screen = NULL;

/* ========================================================================= */
/* Forward declarations for ALL known functions from the IRA symbol map     */
/* This lets us call them in the right order even before we translate their  */
/* bodies. When you paste the next chunk, I'll fill in the real code.        */
/* ========================================================================= */

static void sub_714(void);
static void sub_E40(void);
static void sub_10A2(void);
static void sub_110A(void);
static void sub_1186(void);
static void sub_15FA(void);
static void sub_1696(void);
static void sub_16E4(void);
static void sub_17B2(void);
static void sub_1802(void);
static void sub_1910(void);
static void sub_19EE(void);
static void sub_1B8A(void);
static void sub_1BE8(void);
static void sub_1C12(void);
static void sub_1CBC(void);
static void sub_1D0E(void);
static void sub_1D1A(void);
static void sub_2C36(void);
static void sub_2DD8(void);
static void sub_2F9C(void);
static void sub_3046(void);
static void sub_3F92(void);
static void sub_5026(void);
static void sub_50B2(void);
static void sub_50E6(void);
static void sub_53EE(void);
static void sub_5420(void);
static void sub_5450(void);
static void sub_549A(void);
static void sub_55A2(void);
static void sub_55BA(void);
static void sub_55E0(void);
static void sub_55F8(void);
static void sub_5636(void);
static void sub_5792(void);
static void sub_58CA(void);
static void sub_58FA(void);
static void sub_59CA(void);
static void sub_59F0(void);
static void sub_65F0(void);
static void sub_65F8(void);
static void sub_6730(void);
static void sub_68FE(void);
static void sub_6922(void);
static void sub_6944(void);
static void sub_6968(void);
static void sub_6A1C(void);
static void sub_6A3A(void);
static void sub_6B56(void);
static void sub_6BA2(void);
static void sub_6BB2(void);
static void sub_6BC6(void);
static void sub_6BD2(void);
static void sub_6BDA(void);
static void sub_6BE6(void);
static void sub_6BF8(void);
static void sub_6C00(void);
static void sub_6C12(void);
static void sub_6C22(void);
static void sub_6C3A(void);
static void sub_6C4A(void);
static void sub_6C5C(void);
static void sub_6C6C(void);
static void sub_6C78(void);
static void sub_6C8C(void);
static void sub_6C98(void);
static void sub_6CAC(void);
static void sub_6CB4(void);
static void sub_6CC2(void);
static void sub_6CCE(void);
static void sub_6CDE(void);
static void sub_6CEA(void);
static void sub_6CF2(void);
static void sub_6CFA(void);
static void sub_6D08(void);
static void sub_6D14(void);
static void sub_6D20(void);

/* The real game entry point (after the launcher loads "populous.prg")
 * In the original flow, after the loader choice and setup, it loads
 * "populous.prg" into memory and jumps to its entry (see SUB_17B2 area etc.).
 * When you paste the prg disassembly or the code after the launcher,
 * replace this stub with the real translated main loop.
 */
static void main_game_entry(void);   /* will become the heart of the port */

/* ========================================================================= */
/* Low-level memory + library stubs (faithful to the original calls)         */
/* ========================================================================= */

static void *amiga_alloc_mem(uint32_t size, uint32_t attributes)
{
    /* Original used Exec AllocMem with MEMF_CHIP | MEMF_CLEAR etc. */
    void *mem = calloc(1, size);
    if (mem) {
        POP_LOGD("EXEC", "AllocMem(%u, 0x%08x) -> %p", size, attributes, mem);
    } else {
        POP_LOGE("EXEC", "AllocMem(%u) FAILED", size);
    }
    return mem;
}

static void amiga_free_mem(void *mem)
{
    free(mem);
}

static void *amiga_open_library(const char *name, uint32_t version)
{
    POP_LOGD("EXEC", "OpenLibrary(\"%s\", %u)", name, version);
    if (strstr(name, "graphics"))   return (void*)0x1000;
    if (strstr(name, "intuition"))  return (void*)0x2000;
    return NULL;
}

static void amiga_close_library(void *base)
{
    POP_LOGD("EXEC", "CloseLibrary(%p)", base);
}

/* ========================================================================= */
/* Translated assembly routines - FIRST CHUNK                              */
/* ========================================================================= */

/* From the very top of the disassembly */
static void sub_384(void)   /* originally opened libs + first big allocs */
{
    g.graphics_base = amiga_open_library("graphics.library", 0);
    if (!g.graphics_base) {
        POP_LOG_ERROR("graphics.library open failed");
        exit(1);
    }

    g.intuition_base = amiga_open_library("intuition.library", 0);
    if (!g.intuition_base) {
        POP_LOG_ERROR("intuition.library open failed");
        exit(1);
    }

    /* Exact sequence from the disassembly:
     * PEA  $00010002
     * PEA  $12C
     * JSR  SUB_6C4A   → AllocMem
     */
    g.alloc_1f400 = amiga_alloc_mem(0x1F400, 0x00010002);   /* main screen */
    g.alloc_8340  = amiga_alloc_mem(0x8340,  0x00010002);
    g.alloc_7d00  = amiga_alloc_mem(0x7D00,  0x00000002);
    g.alloc_4b0   = amiga_alloc_mem(0x4B0,   0x00010002);

    /* The original stored these at specific A4-negative offsets */
}

/* SUB_11A - clear the allocated areas + enable some interrupts */
static void sub_11a(void)
{
    if (g.alloc_8340)  memset(g.alloc_8340,  0, 0x8340);
    if (g.alloc_7d00)  memset(g.alloc_7d00,  0, 0x7D00);
    if (g.alloc_1f400) memset(g.alloc_1f400, 0, 0x1F400);
    if (g.alloc_4b0)   memset(g.alloc_4b0,   0, 0x4B0);

    /* MOVE.W #$0410,INTENA */
    POP_LOGD("HW", "INTENA = 0x0410  (enable display & copper interrupts)");
}

/* SUB_16E - sets up the loader structures for "demo.pic" and "populous.prg" */
static void sub_16e(void)
{
    POP_LOGD("LOADER", "SUB_16E - creating startup structures for demo.pic / populous.prg");

    /* The disassembly allocates a small struct and fills pointers to strings */
    g.load_pic_struct = amiga_alloc_mem(0x28, 0x00000002);

    /* In the asm it put pointers to the strings "populous.prg" and "startup"
     * and set up a jump table / function pointer area. We emulate the important bits. */
    g.startup_struct = amiga_alloc_mem(0x20, 0x00000002);
}

/* SUB_226 - more screen/copper related allocations */
static void sub_226(void)
{
    g.copper_area = amiga_alloc_mem(0x480, 0x00010002);
    POP_LOGD("INIT", "SUB_226 - allocated copper / screen control area (0x480 bytes)");

    /* The original then did pointer arithmetic into this block for copper list etc. */
}

/* SUB_29E - small display init */
static void sub_29e(void)
{
    /* Original did MOVE.W #$018f, something at 0x82ac */
    g.screen_width  = 320;
    g.screen_height = 256;
    POP_LOGD("INIT", "SUB_29E - display params set (320x256)");
}

/* SUB_2C6 - cleanup of early allocations */
static void sub_2c6(void)
{
    amiga_free_mem(g.copper_area);
    amiga_free_mem(g.load_pic_struct);
    amiga_free_mem(g.startup_struct);
    amiga_free_mem(g.alloc_1f400);
    amiga_free_mem(g.alloc_8340);
    amiga_free_mem(g.alloc_7d00);
    amiga_free_mem(g.alloc_4b0);
    if (g.intuition_base) amiga_close_library(g.intuition_base);
    if (g.graphics_base) amiga_close_library(g.graphics_base);
    memset(&g, 0, sizeof(g));
}

/* SUB_308 - select one of the allocated buffers */
static void sub_308(int16_t which)
{
    POP_LOGD("SELECT", "SUB_308(0x%04x) - choosing buffer", (uint16_t)which);
    /* In the real code this picked between the different alloc_ blocks */
}

/* SUB_2E2 - poke interrupt register */
static void sub_2e2(int16_t value)
{
    POP_LOGD("HW", "SUB_2E2 - INTENA write 0x%04x", (uint16_t)value);
}

/* ========================================================================= */
/* SUB_41C / SUB_456 / SUB_1186: load.pic choice and the demo.pic intro     */
/* ========================================================================= */

/* The game reads the choice's low word as _start_of_game (prg asm:94-96). */
extern int g_start_of_game;

enum { LAUNCH_LOAD, LAUNCH_BLOCKS, LAUNCH_TITLE, LAUNCH_RISE, LAUNCH_SCRIPT, LAUNCH_CREDITS, LAUNCH_FADE };
enum { LAUNCH_RUN, LAUNCH_START, LAUNCH_QUIT };
/* Port timing (estimate): SUB_456's LAB_4A0 pass (asm:400-423) is MOVE.L
 * d16 16 + ADDQ.L mem 20 + CMP.L # 14 + BGT.S 8 + TST.W d16 12 + BNE.S 10
 * + BRA.S 10 = 90 cycles; $55731 passes = 31500090 cycles. A PAL frame has
 * 7093790/50 = 141876 cycles, about 900 of them in the LAB_5242 VBI, so
 * ~140976 remain: 223.4 frames -> the 224th. DMA contention by the five
 * load.pic planes is not modelled (none if the code is in fast RAM). */
#define LAUNCH_LOAD_FRAMES 224
#define LAUNCH_DROPS 100
#define LAUNCH_STRIP_TOP 50                /* SUB_714 slot counter $32 */
#define LAUNCH_ACTORS 16
#define LAUNCH_LIST 20                     /* -31984..-31905: 20 (type, cell) pairs */
#define LAUNCH_OPS 64
/* $6d28 actor words (asm:5445-5477). */
enum { A_BASE, A_PC, A_SUB, A_STEP, A_CELL, A_ANIM, A_EXTRA };

/* One drawing of this frame onto the screen copy: kind 0 SUB_1910 sprite
 * (frame, x, y), kind 1 SUB_3F92 picture (right, frame = fade step). */
typedef struct { int16_t kind, frame, x, y, right; } LaunchOp;

typedef struct {
    int state, frame, choice, credit, held, script_done;
    int16_t strip_count[2];               /* -31998,A4: rows 75 / 50 (SUB_536 asm:453-454) */
    int16_t strip_y[2], strip_fade[2];    /* this frame's SUB_5026 row and fade step */
    uint8_t strip[2][5][320];             /* LAB_1FA2 / LAB_25E2: 5 planes x 8 rows x 40 bytes */
    int16_t button, x, y;                 /* -32004 latch, -32000 X, -32002 Y */
    int16_t drop[LAUNCH_DROPS];           /* LAB_7090 (-31894,A4) */
    int16_t speed[LAUNCH_DROPS];          /* LAB_715A (-31692,A4) */
    int16_t settled[LAUNCH_DROPS];        /* the blocks copied into -31904 (asm:1560-1567) */
    uint32_t seed;                        /* -32246,A4: DC.L 1 at $6f30 */
    int16_t actor[LAUNCH_ACTORS][7];      /* $6d28 (-32766,A4) */
    int16_t toggle;                       /* LAB_708E (-31896,A4) */
    int16_t counter;                      /* -31994,A4 */
    int16_t fade_a, fade_b, dirty;        /* SUB_714 locals -14, -16, -12 */
    int16_t list_type[LAUNCH_LIST], list_cell[LAUNCH_LIST];   /* -31984 / -31982 */
    int sdr_down, sdr_modifier, key_latch;  /* CIA-A SDR and LAB_57FE */
    int vblanks;                          /* vertical blanks this step waits */
    int ops;
    LaunchOp op[LAUNCH_OPS];
    uint16_t fade_pal[32];                /* SUB_58FA's working copy */
    int fade_colours, fade_steps, fade_step, fade_next;
} Launcher;

static Launcher launch;
static uint8_t launch_load_pixels[320 * 200], launch_demo_pixels[320 * 200];
/* -31900 (demo.pic + settled blocks, LAB_120E asm:1559-1566), -31904 (the
 * script's background), its copy at this frame's SUB_58CA, the last frame. */
static uint8_t launch_clean[320 * 200], launch_bg[320 * 200], launch_frame_bg[320 * 200];
static uint8_t launch_last[320 * 200];
static uint16_t launch_load_palette[32];
static uint8_t launch_font[4400];
static int launch_font_loaded;
static GraphicsAssets *launch_assets;
static TitleBank launch_music;
static int launch_audio;

/* Test seams: the 50 Hz wait and per-frame event injection. */
static void launch_vbl_wait(void) { SDL_Delay(20); }
static void (*launch_wait)(void) = launch_vbl_wait;
static void (*launch_frame_hook)(int frame);
static int launch_frames;

/* LAB_640C asm:4663-4682 after SUB_1BE8 (asm:2186-2198, heights << 3):
 * land0 frame and height of the 10x10 intro blocks, row-major. The upper
 * layer at +$64 stays empty; SUB_714 keeps its blocks in a list (SUB_1CBC). */
static const uint8_t launch_block_type[LAUNCH_DROPS] = {
    0,22,6,15,15,15,15,15,9,25, 0,22,6,15,15,15,15,15,9,25, 28,30,6,15,15,15,15,15,9,25,
    12,12,14,15,15,15,15,15,9,25, 12,8,15,11,3,3,3,3,1,25, 15,9,15,9,15,15,15,27,19,17,
    15,9,15,9,15,15,15,29,24,0, 3,1,15,9,15,15,15,15,25,0, 3,3,3,1,27,19,19,19,21,24,
    19,19,19,19,17,0,0,0,18,17};
static const uint8_t launch_block_height[LAUNCH_DROPS] = {
    0,0,1,1,1,1,1,1,1,0, 0,0,1,1,1,1,1,1,1,0, 0,0,1,1,1,1,1,1,1,0, 1,1,1,1,1,1,1,1,1,0,
    2,2,1,1,1,1,1,1,1,0, 2,2,1,1,0,0,0,0,0,0, 2,2,1,1,0,0,0,0,0,0, 2,2,1,1,0,0,0,0,0,0,
    1,1,1,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0};

/* LAB_1DCE asm:2368-2392, the SUB_1D1A line list ($ff wraps to the start). */
static const char *const launch_credits[] = {
    "POPULOUS", "(C) 1989 BULLFROG PRODUCTIONS", "PUBLISHED BY ELECTRONIC ARTS",
    "PROGRAMMING AND DESIGN BY", "PETER MOLYNEUX", "AND GLENN CORPES", "TECHNICAL HELP FROM",
    "KEVIN DONKIN", "GRAPHICS BY", "GLENN CORPES", "DAN WHEELER", "SEAN COOPER", "ANDY JONES",
    "& PETER MOLYNEUX", "GAME SOUND & MUSIC BY", "DAVE HANLON", "MUSIC IDEA BY LES EDGAR",
    "TITLE MUSIC BY ROB HUBBARD", "SPECIAL THANKS TO", "HISOFT FOR DEVPAC 2", "ADRIAN MOORE",
    "EVERYONE AT ULTIMA", "EXCEPT ROB"};
#define LAUNCH_CREDIT_LINES ((int)(sizeof launch_credits / sizeof launch_credits[0]))

/* SUB_6538 asm:4683-4698 (via SUB_2FC). */
static uint16_t sub_6538(void)
{
    launch.seed = launch.seed * 0x41c64e6du + 0x3039u;
    return (uint16_t)((launch.seed >> 16) & 0x7fff);
}

/* SUB_10A2 asm:1428-1466: n drops start 232..244 above, speed 0..8. */
static void sub_10a2(int count)
{
    for (int i = 0; i < count; ++i) {
        launch.drop[i] = (int16_t)(sub_6538() % 13 + 0xe8);
        launch.speed[i] = (int16_t)(sub_6538() % 9);
    }
}

/* SUB_110A asm:1467-1516: fall one step, bounce at 0 with half speed;
 * returns the first nonzero speed, 0 once every drop rests. */
static int16_t sub_110a(int count)
{
    int16_t first = 0;
    for (int i = 0; i < count; ++i) {
        int16_t speed = (int16_t)(launch.speed[i] + 1);
        int16_t drop = (int16_t)(launch.drop[i] - speed);
        if (drop < 0) { speed = (int16_t)-(speed / 2); drop = 0; }
        if (!first) first = speed;
        launch.drop[i] = drop; launch.speed[i] = speed;
    }
    return first;
}

/* SUB_456 asm:408-434 on the latched position (lo-res, y 0..199). */
static int launch_region(int x, int y)
{
    if (y >= 0x9c && y < 0xaa && x >= 0xe7) return 3;
    if (y >= 0xaa && y < 0xb8 && x >= 0xe7) return 2;
    if (y >= 0xb8 && x >= 0xf1) return 1;
    return 0;
}

/* VBI left button latch, asm:3767-3780: 2 waits for a release; a fresh
 * press clears it and freezes the position. */
static void launch_mouse(int pressed, int x, int y)
{
    if (!launch.button || (launch.button == 2 && pressed)) return;
    launch.button = pressed ? 0 : 0x40;
    launch.x = (int16_t)x; launch.y = (int16_t)y;
}

/* SUB_456 entry asm:388-403: _choice cleared, latch armed, counter 0. */
static void launch_load_begin(void)
{
    launch.state = LAUNCH_LOAD; launch.frame = 0; launch.choice = 0; launch.button = 2;
}

static void launch_blocks(uint8_t *pixels, const int16_t *drop);

/* ---- SUB_714 block script ---------------------------------------------- */

/* Process start: the $6d28 actor words as loaded (asm:5445-5477). They are
 * not reset between intros; SUB_536 only sets pc, step and cell. */
static void launch_actors_reset(void)
{
    memcpy(launch.actor, intro_actor_init, sizeof launch.actor);
}

/* LAB_640C+$c8 after SUB_1BE8 (heights << 3); indices 100..107 run into
 * SUB_6538's code (asm:4684-4685), which SUB_50E6 reads unshifted. */
static int launch_height(int index)
{
    if (index >= 0 && index < LAUNCH_DROPS) return launch_block_height[index] << 3;
    if (index >= LAUNCH_DROPS && index < LAUNCH_DROPS + 8) return (int8_t)intro_height_tail[index - LAUNCH_DROPS];
    return 0;   /* not reached: cells stay within 3..107 */
}

static int launch_floor8(int value) { return value >= 0 ? value / 8 : -((-value + 7) / 8); }

/* SUB_50B2/SUB_50E6 asm:3593-3709: an actor's 16x16 sprite at cell, moved
 * sub/8 of the way along step; frame + direction + LAB_708E toggle. */
static void launch_draw(int cell, int step, int frame, int sub, int drop, int extra)
{
    int row = cell / 10, column = cell % 10;
    int x = (column - row) * 16 + 0x98, y = (column + row) * 8 + 0x1c - drop;
    int height = (int8_t)launch_height(cell);
    y -= height;
    int8_t diff = (int8_t)(height - launch_height(cell + step));   /* SUB.B, EXT.W */
    y += launch_floor8((int16_t)(diff * sub));                      /* MULU, ASR.W #3 */
    int dx = 0, dy = 0, dir = 8;
    switch (step) {
    case -11: dx = 0;  dy = -2; dir = 0;  break;
    case -10: dx = 2;  dy = -1; dir = 2;  break;
    case -9:  dx = 4;  dy = 0;  dir = 4;  break;
    case 1:   dx = 2;  dy = 1;  dir = 6;  break;
    case 11:  dx = 0;  dy = 2;  dir = 8;  break;
    case 10:  dx = -2; dy = 1;  dir = 10; break;
    case 9:   dx = -4; dy = 0;  dir = 12; break;
    case -1:  dx = -2; dy = -1; dir = 14; break;
    default:  break;
    }
    x += dx * sub;
    y += dy * sub;
    if (extra) frame += launch.toggle;
    else if (frame < 0x40) frame += dir + launch.toggle;
    if (launch.ops < LAUNCH_OPS)
        launch.op[launch.ops++] = (LaunchOp){0, (int16_t)frame, (int16_t)x, (int16_t)y, 0};
}

/* SUB_1910 asm:1951-2023: y < 0 skips it; below $b8 only $c8-y+1 rows (the
 * 201st row, inside the next plane in the original, is clipped here). */
static void launch_sprite(uint8_t *pixels, int frame, int x, int y)
{
    if (!launch_assets || y < 0) return;
    int rows = 16;
    if (y > 0xb8) { rows = 0xc8 - y; if (rows <= 0) return; ++rows; }
    GraphicsSheet sheet = graphics_sprites(launch_assets);
    if (frame < 0 || (size_t)(frame + 1) * 256 > sheet.size) return;
    for (int r = 0; r < rows && r < 16 && y + r < 200; ++r)
        for (int c = 0; c < 16 && x + c < 320; ++c) {
            if (x + c < 0) continue;
            size_t src = (size_t)frame * 256 + (size_t)r * 16 + (size_t)c;
            uint8_t *pixel = &pixels[(y + r) * 320 + x + c];
            *pixel = (uint8_t)((sheet.keep[src] ? *pixel : 0) | sheet.indices[src]);
        }
}

/* SUB_3F92 asm:3091-3144: a LAB_4026 64x64 picture (right: +$800, at byte
 * $20 = x 256) dissolved in with LAB_1F62[step + 1, 2, 0 ...]; a set mask
 * bit keeps the destination. */
static void launch_corner(uint8_t *pixels, int right, int step)
{
    const uint16_t *words = intro_corner_words + (right ? 0x400 : 0);
    int left = right ? 256 : 0, d0 = 0;
    for (int row = 0; row < 64; ++row)
        for (int word = 0; word < 4; ++word) {
            d0 += 2;
            if (d0 == 6) d0 = 0;
            uint16_t keep = intro_fade_mask[step + d0 / 2];
            const uint16_t *planes = words + (row * 4 + word) * 4;
            for (int bit = 0; bit < 16; ++bit) {
                int shift = 15 - bit, colour = 0;
                if ((keep >> shift) & 1) continue;
                for (int plane = 0; plane < 4; ++plane) colour |= ((planes[plane] >> shift) & 1) << plane;
                pixels[row * 320 + left + word * 16 + bit] = (uint8_t)colour;
            }
        }
}

static void launch_corner_op(int right, int step)
{
    if (launch.ops < LAUNCH_OPS) launch.op[launch.ops++] = (LaunchOp){1, (int16_t)step, 0, 0, (int16_t)right};
}

/* SUB_1CBC asm:2258-2284: list block one level up, (c-r)*16+$90, (c+r)*8-h+$18. */
static void launch_upper_block(uint8_t *pixels, int cell, int type)
{
    if (!launch_assets) return;
    int row = cell / 10, column = cell % 10;
    int y = (column + row) * 8 - (int8_t)launch_height(cell) + 0x18;
    if (y < 0) return;   /* SUB_19EE asm:2024-2025 */
    graphics_blit(pixels, 320 * 200, 320, 200, graphics_land(launch_assets), type, (column - row) * 16 + 0x90, y);
}

static int launch_script_byte(int actor, int pc)
{
    int index = intro_script_start[actor] + pc;
    return index >= 0 && index < INTRO_SCRIPT_BYTES ? intro_script_memory[index] : 0xff;
}

static int launch_sign(int value) { return (value > 0) - (value < 0); }

/* LAB_9D6..LAB_AFC asm:845-958: (type, cell) into the cell-sorted list. */
static void launch_list_put(int cell, int type)
{
    int count = 0, at;
    while (count < LAUNCH_LIST && launch.list_cell[count]) ++count;
    if (count >= LAUNCH_LIST - 1) return;   /* not reached: at most 14 entries */
    for (at = 0; at < count; ++at) {
        if (launch.list_cell[at] == cell) { launch.list_type[at] = (int16_t)type; return; }
        if (cell < launch.list_cell[at]) {
            for (int move = count; move >= at + 1; --move) {
                launch.list_cell[move] = launch.list_cell[move - 1];
                launch.list_type[move] = launch.list_type[move - 1];
            }
            break;
        }
    }
    launch.list_cell[at] = (int16_t)cell;
    launch.list_type[at] = (int16_t)type;
}

/* One actor of SUB_714's loop LAB_8A8..LAB_E24 (asm:730-1227). */
static void launch_actor_step(int index)
{
    int16_t *a = launch.actor[index];
    int drop = launch.drop[index], b = launch_script_byte(index, a[A_PC]);
    if (b < 100) {                                      /* go to cell b */
        if (a[A_CELL] == b) b = launch_script_byte(index, ++a[A_PC]);
        int column = a[A_CELL] % 10, row = a[A_CELL] / 10;
        a[A_STEP] = (int16_t)(launch_sign(b / 10 - row) * 10 + launch_sign(b % 10 - column));
        launch_draw(a[A_CELL], a[A_STEP], a[A_BASE], a[A_SUB], drop, a[A_EXTRA]);
        a[A_SUB] = (int16_t)((a[A_SUB] + 1) & 7);
        if (!a[A_SUB]) a[A_CELL] = (int16_t)(a[A_CELL] + a[A_STEP]);
    } else if (b < 130) {                               /* put a block */
        launch_list_put(a[A_CELL], intro_actor_tables[0x4b + b - 100]);
        launch_draw(a[A_CELL], a[A_STEP], a[A_BASE], a[A_SUB], drop, 0);
        ++a[A_PC];
        launch.dirty = 1;
    } else if (b <= 220) {                              /* one frame */
        launch_draw(a[A_CELL], a[A_STEP], intro_actor_tables[b - 130], a[A_SUB], drop, a[A_EXTRA]);
        ++a[A_PC];
    } else if (b == 0xfd || b == 0xfe) {                /* dissolve a picture in */
        int right = b == 0xfd;
        int16_t *step = right ? &launch.fade_a : &launch.fade_b;
        if (*step >= 0x13) { launch_corner(launch_bg, right, *step); ++a[A_PC]; }
        launch_corner_op(right, *step);
        ++*step;
    } else if (b == 0xfc) {                             /* wait for the right picture */
        if (launch.fade_a >= 0x14) ++a[A_PC];
    } else if (b == 0xfb) {                             /* idle; garbage 7th argument taken as 0 */
        int frame = launch.toggle + 0x1b + (index >= 7 ? 2 : 0);
        if ((int16_t)sub_6538() > 0x7530) frame = (index >= 7 ? 1 : 0) * 4 + 0x0e;
        launch_draw(a[A_CELL], a[A_STEP], intro_actor_tables[frame], a[A_SUB], drop, 0);
    } else if (b == 0xfa) {                             /* four-frame animation */
        if (a[A_ANIM] == -1) launch_draw(a[A_CELL], a[A_STEP], a[A_BASE], a[A_SUB], drop, a[A_EXTRA]);
        else {
            if (++a[A_ANIM] == 4) a[A_ANIM] = 0;
            launch_draw(a[A_CELL], a[A_STEP], a[A_ANIM] + a[A_BASE], a[A_SUB], drop, 0);
        }
    } else if (b == 0xf9) {                             /* bob; actor $20 counts to $f0 */
        if (a[A_BASE] == 0x20 && ++launch.counter == 0xf0)
            for (int i = 0; i < LAUNCH_ACTORS; ++i) ++launch.actor[i][A_PC];
        a[A_SUB] = (int16_t)(a[A_SUB] + a[A_ANIM]);
        if (a[A_SUB] == -1 || a[A_SUB] == 8) { a[A_ANIM] = (int16_t)-a[A_ANIM]; a[A_SUB] = (int16_t)(a[A_SUB] + a[A_ANIM]); }
        launch_draw(a[A_CELL], a[A_STEP], (launch.counter & 3) + 0x8a, a[A_SUB], drop, a[A_EXTRA]);
    } else if (b == 0xf7) {                             /* release every waiting actor */
        for (int i = 0; i < LAUNCH_ACTORS; ++i)
            if (launch_script_byte(i, launch.actor[i][A_PC]) != 0xff) ++launch.actor[i][A_PC];
    } else if (b == 0xf6) {
        ++launch.counter;
        launch_draw(a[A_CELL], a[A_STEP], (launch.counter & 3) + 0x79, a[A_SUB], drop, 0);
    }
}

/* SUB_536 asm:449-502: list emptied, 16 actors at their cells, step 8. */
static void launch_sub536_begin(void)
{
    for (int i = 0; i < LAUNCH_LIST && launch.list_cell[i]; ++i) launch.list_cell[i] = 0;
    for (int i = 0; i < LAUNCH_ACTORS; ++i) {
        launch.actor[i][A_PC] = 0;
        launch.actor[i][A_CELL] = intro_actor_cells[i];
        launch.actor[i][A_STEP] = 8;
    }
    memcpy(launch_bg, launch_clean, sizeof launch_bg);
    launch.strip_count[0] = 0x4b; launch.strip_count[1] = LAUNCH_STRIP_TOP;
    launch.counter = 0;
    memset(launch.strip, 0, sizeof launch.strip);
    launch.script_done = 0;
    sub_10a2(LAUNCH_ACTORS);
}

/* SUB_5792 asm:4091-4122: LAB_7158 holds the SDR byte for one call after a
 * new key-down; LAB_57FE latches it until key-up, except $60..$67. */
static int launch_sub_5792(void)
{
    if (launch.key_latch) { if (!launch.sdr_down) launch.key_latch = 0; return 0; }
    if (!launch.sdr_down) return 0;
    if (!launch.sdr_modifier) launch.key_latch = 1;
    return 1;
}

/* Port seam for keyboard events: the CIA-A SDR's last byte. */
static void launch_key(int down, int modifier)
{
    launch.sdr_down = down;
    launch.sdr_modifier = modifier;
}

/* SUB_58FA asm:4176-4246: one nibble less per pass; the VHPOSR wait (lines
 * 251..255, asm:4230-4234) allows one pass per frame. */
static void launch_fade_begin(const uint16_t *palette, int colours, int next)
{
    launch.state = LAUNCH_FADE;
    memcpy(launch.fade_pal, palette, (size_t)colours * sizeof *palette);
    launch.fade_colours = colours;
    launch.fade_steps = 0;
    for (int i = 0; i < colours; ++i)
        for (int shift = 0; shift < 12; shift += 4)
            if (((palette[i] >> shift) & 15) > launch.fade_steps) launch.fade_steps = (palette[i] >> shift) & 15;
    launch.fade_step = 0;
    launch.fade_next = next;
}

static void launch_fade_pass(void)
{
    for (int i = 0; i < launch.fade_colours; ++i) {
        uint16_t colour = launch.fade_pal[i];
        for (int shift = 0; shift < 12; shift += 4)
            if ((colour >> shift) & 15) colour = (uint16_t)(colour - (1u << shift));
        launch.fade_pal[i] = colour;
    }
    ++launch.fade_step;
}

/* SUB_1186 asm:1517-1532: demo.pic, SUB_2C36 popsam music, 100 drops. */
static void launch_intro_begin(void)
{
    launch.state = LAUNCH_BLOCKS; launch.frame = 0; launch.credit = 0;
    if (launch_music.pcm) {
        launch_audio = audio_open();
        title_music_start(&launch_music, 0);
    }
    sub_10a2(LAUNCH_DROPS);
}

static void launch_music_off(void)
{
    title_music_stop();
    if (launch_audio) { audio_close(); launch_audio = 0; }
}

/* LAB_1236 asm:1572-1586: SUB_58FA fades the demo palette (-32310), then
 * SUB_2DD8 stops the music and SUB_41C loops back to load.pic. The first
 * pass lands in the frame after the last one drawn: right away when
 * SUB_714 aborts before drawing, one step later after a drawn frame. */
static void launch_intro_end(int now)
{
    launch_fade_begin(graphics_palette, 16, LAUNCH_LOAD);
    if (now && launch.fade_steps) launch_fade_pass();
}

/* SUB_1D1A asm:2288-2343: the next LAB_1F9E line, centred, into a strip:
 * plane 0 preset to $ff, then font.dat (LAB_CE0C) bytes 0..4 per glyph
 * row into planes 0..4; the pointer wraps at the $ff end marker. */
static void launch_strip_text(int slot)
{
    uint8_t (*strip)[320] = launch.strip[slot];
    const char *text = launch_credits[launch.credit];
    int length = (int)strlen(text), left = (40 - length) >> 1;
    memset(strip[0], 0xff, sizeof strip[0]);
    memset(strip[1], 0, 4 * sizeof strip[0]);
    for (int index = 0; index < length && launch_font_loaded; ++index) {
        int glyph = ((unsigned char)text[index] - 0x20) * 40;
        if (glyph < 0 || glyph + 40 > (int)sizeof launch_font || left + index >= 40) continue;
        for (int row = 0; row < 8; ++row)
            for (int plane = 0; plane < 5; ++plane)
                strip[plane][row * 40 + left + index] = launch_font[glyph + row * 5 + plane];
    }
    if (++launch.credit >= LAUNCH_CREDIT_LINES) launch.credit = 0;
}

/* One pass of SUB_41C's loops. vblanks: the waits this pass makes (SUB_358
 * flips wait once; SUB_714 adds SUB_55E0, asm:724). */
static int launch_step(void)
{
    ++launch.frame;
    launch.vblanks = 1;
    switch (launch.state) {
    case LAUNCH_LOAD:
        if (launch.frame > LAUNCH_LOAD_FRAMES) { launch.button = 2; launch_intro_begin(); break; }   /* LAB_51E, choice 0 */
        if (launch.button) break;
        launch.choice = launch_region(launch.x, launch.y);
        launch.button = 2;
        if (launch.choice) {                       /* LAB_426: fade the 32 load.pic colours, then populous.prg */
            launch_fade_begin(launch_load_palette, 32, LAUNCH_START);
            return LAUNCH_RUN;
        }
        launch_intro_begin();                      /* LAB_43E: any other click */
        break;
    case LAUNCH_FADE:
        if (launch.fade_step < launch.fade_steps) { launch_fade_pass(); return LAUNCH_RUN; }
        if (launch.fade_next == LAUNCH_START) return LAUNCH_START;
        launch_music_off();
        launch_load_begin();
        return LAUNCH_RUN;
    case LAUNCH_BLOCKS:                            /* asm:1533-1555 */
        if (sub_110a(LAUNCH_DROPS)) break;
        memcpy(launch.settled, launch.drop, sizeof launch.settled);
        /* LAB_120E: the last frame shown becomes -31904 and -31900. */
        memcpy(launch_clean, launch_demo_pixels, sizeof launch_clean);
        launch_blocks(launch_clean, launch.settled);
        launch.state = LAUNCH_TITLE; launch.frame = 0;
        sub_10a2(18);                              /* SUB_E40 asm:1234-1238, same frame */
        /* fall through */
    case LAUNCH_TITLE:                             /* LAB_E4E asm:1239-1323 */
        if (sub_110a(18)) break;
        for (int i = 0; i < 18; ++i) { launch.drop[i] = 0xa0; launch.speed[i] = (int16_t)(sub_6538() & 7); }
        launch.state = LAUNCH_RISE; launch.frame = 0;   /* LAB_F58 asm:1324-1342 */
        /* fall through */
    case LAUNCH_RISE:                              /* LAB_F9A asm:1343-1427 */
        if (sub_110a(18)) break;
        /* SUB_E40 returns 0 without a frame: SUB_714 calls SUB_536
         * (asm:449-585), whose 16-drop loop shows one frame per SUB_110A
         * including the last, with actors 0, 1, 7 and 8 (asm:521-580). */
        launch.state = LAUNCH_SCRIPT; launch.frame = 0;
        launch_sub536_begin();
        /* fall through */
    case LAUNCH_SCRIPT:
        if (!launch.script_done) {                 /* LAB_5F4: no latch or key test */
            launch.toggle ^= 1;
            launch.script_done = !sub_110a(LAUNCH_ACTORS);
            memcpy(launch_frame_bg, launch_bg, sizeof launch_frame_bg);
            launch.ops = 0;
            for (int i = 0; i < 9; i = i == 1 ? 7 : i + 1)
                launch_draw(launch.actor[i][A_CELL], launch.actor[i][A_STEP], launch.actor[i][A_BASE],
                            launch.actor[i][A_SUB], launch.drop[i], 0);
            return LAUNCH_RUN;
        }
        launch.state = LAUNCH_CREDITS; launch.frame = 0;
        launch.fade_a = launch.fade_b = launch.dirty = 0;   /* SUB_714 asm:589-592 */
        /* fall through */
    case LAUNCH_CREDITS:                           /* SUB_714 LAB_730 asm:594-1232, endless */
        launch.toggle ^= 1;
        if (launch_sub_5792() || !launch.button) {        /* asm:602-607, before drawing */
            launch_intro_end(1);
            return LAUNCH_RUN;
        }
        memcpy(launch_frame_bg, launch_bg, sizeof launch_frame_bg);
        for (int slot = 0; slot < 2; ++slot) {
            int16_t count = launch.strip_count[slot], fade = count;
            if (count == LAUNCH_STRIP_TOP) launch_strip_text(slot);
            if (fade > 25) fade = (int16_t)(LAUNCH_STRIP_TOP - fade);
            if (fade < 0) fade = 0;
            launch.strip_y[slot] = count; launch.strip_fade[slot] = fade;
            if (--count < 0) count = LAUNCH_STRIP_TOP;
            launch.strip_count[slot] = count;
        }
        if (launch.dirty) {                        /* asm:684-722: the right picture is not redrawn */
            memcpy(launch_bg, launch_clean, sizeof launch_bg);
            if (launch.fade_b >= 0x14) launch_corner(launch_bg, 0, launch.fade_b);
            for (int i = 0; i < LAUNCH_LIST && launch.list_cell[i]; ++i)
                launch_upper_block(launch_bg, launch.list_cell[i], launch.list_type[i]);
            launch.dirty = 0;
        }
        launch.ops = 0;
        for (int i = 0; i < LAUNCH_ACTORS; ++i) launch_actor_step(i);
        launch.vblanks = 2;                        /* SUB_55E0 (asm:724) and SUB_358 */
        return LAUNCH_RUN;
    }
    /* SUB_1186's and SUB_E40's loops test SUB_5792 and the latch after
     * their WaitTOF (asm:1552-1555, 1309-1315, 1414-1420). */
    if (launch.state != LAUNCH_LOAD && (launch_sub_5792() || !launch.button)) launch_intro_end(0);
    return LAUNCH_RUN;
}

/* SUB_1C12 asm:2200-2284 with LAB_7226 = $80 (scale 1, asm:1534): row r,
 * column c at x = (c-r)*16+$90, y = (c+r)*8-height+$20-drop; SUB_19EE
 * (asm:2024-2040) skips tiles above the top. */
static void launch_blocks(uint8_t *pixels, const int16_t *drop)
{
    if (!launch_assets) return;
    GraphicsSheet land = graphics_land(launch_assets);
    for (int r = 0, i = 0; r < 10; ++r)
        for (int c = 0; c < 10; ++c, ++i) {
            int y = (c + r) * 8 - launch_block_height[i] * 8 + 0x20 - drop[i];
            if (y < 0) continue;
            graphics_blit(pixels, 320 * 200, 320, 200, land, launch_block_type[i], (c - r) * 16 + 0x90, y);
        }
}

/* SUB_1802 asm:1866-1940: glyph g at (x, y), 32 rows (200 - y + 1 below
 * $a8); a set mask bit keeps the background, the planes are ORed in. */
static void launch_glyph(uint8_t *pixels, int x, int y, int g)
{
    if (y < 0) return;
    int rows = y > 0xa8 ? 0xc8 - y + 1 : 32;
    if (y > 0xa8 && 0xc8 - y <= 0) return;
    for (int row = 0; row < rows && row < 32 && y + row < 200; ++row) {
        const uint16_t *words = intro_title_glyphs[g][row];
        for (int bit = 0; bit < 16 && x + bit < 320; ++bit) {
            int shift = 15 - bit, colour = 0;
            for (int plane = 0; plane < 4; ++plane) colour |= ((words[plane + 1] >> shift) & 1) << plane;
            uint8_t *pixel = &pixels[(y + row) * 320 + x + bit];
            *pixel = (uint8_t)(((words[0] >> shift) & 1 ? *pixel : 0) | colour);
        }
    }
}

/* SUB_E40 asm:1245-1305 (falling, top = $a8 - drop) and asm:1350-1410
 * (rising, top = drop + 8): banner 5/6x8/7 at $50..$e0 over drops 0..9,
 * then the letters of $6e98 at $60 + 16i over drops 0..7. */
static void launch_title(uint8_t *pixels, int rising)
{
    for (int i = 0; i < 10; ++i)
        launch_glyph(pixels, 0x50 + 16 * i, rising ? launch.drop[i] + 8 : 0xa8 - launch.drop[i],
                     i == 0 ? 5 : i == 9 ? 7 : 6);
    for (int i = 0; i < 8; ++i)
        launch_glyph(pixels, 0x60 + 16 * i, rising ? launch.drop[i] + 8 : 0xa8 - launch.drop[i],
                     intro_title_word[i]);
}

/* SUB_5026 asm:3548-3590: a strip's 160 words onto rows y..y+7; the keep
 * mask is plane 0 ORed with LAB_1F62 words fade+1, fade+2, fade cycling. */
static void launch_strip(uint8_t *pixels, int slot)
{
    const uint8_t (*strip)[320] = launch.strip[slot];
    int y = launch.strip_y[slot], fade = launch.strip_fade[slot];
    for (int word = 0; word < 160; ++word) {
        int row = word / 20, column = word % 20;
        if (y + row >= 200) break;
        uint16_t keep = intro_fade_mask[fade + (word % 3 == 2 ? 0 : word % 3 + 1)];
        keep |= (uint16_t)(strip[0][word * 2] << 8 | strip[0][word * 2 + 1]);
        for (int bit = 0; bit < 16; ++bit) {
            if ((keep >> (15 - bit)) & 1) continue;
            int plane_byte = word * 2 + bit / 8, shift = 7 - bit % 8, colour = 0;
            for (int plane = 0; plane < 4; ++plane) colour |= ((strip[plane + 1][plane_byte] >> shift) & 1) << plane;
            pixels[(y + row) * 320 + column * 16 + bit] = (uint8_t)colour;
        }
    }
}

/* One composed frame: the SUB_58CA background copy plus this frame's drawing. */
static void launch_compose(uint8_t *pixels)
{
    if (launch.state == LAUNCH_LOAD) { memcpy(pixels, launch_load_pixels, sizeof launch_load_pixels); return; }
    if (launch.state == LAUNCH_FADE) { memcpy(pixels, launch_last, sizeof launch_last); return; }
    if (launch.state == LAUNCH_SCRIPT || launch.state == LAUNCH_CREDITS) {
        memcpy(pixels, launch_frame_bg, sizeof launch_frame_bg);
        if (launch.state == LAUNCH_CREDITS)
            for (int slot = 0; slot < 2; ++slot) launch_strip(pixels, slot);
        for (int i = 0; i < launch.ops; ++i) {
            const LaunchOp *op = &launch.op[i];
            if (op->kind) launch_corner(pixels, op->right, op->frame);
            else launch_sprite(pixels, op->frame, op->x, op->y);
        }
        return;
    }
    memcpy(pixels, launch_demo_pixels, sizeof launch_demo_pixels);
    launch_blocks(pixels, launch.state == LAUNCH_BLOCKS ? launch.drop : launch.settled);
    if (launch.state == LAUNCH_TITLE || launch.state == LAUNCH_RISE) launch_title(pixels, launch.state == LAUNCH_RISE);
    if (launch.state == LAUNCH_CREDITS)
        for (int slot = 0; slot < 2; ++slot) launch_strip(pixels, slot);
}

static void launch_render(void)
{
    static uint8_t pixels[320 * 200];
    const uint16_t *palette = graphics_palette;
    int colours = 16;
    if (launch.state == LAUNCH_LOAD) { palette = launch_load_palette; colours = 32; }
    if (launch.state == LAUNCH_FADE) { palette = launch.fade_pal; colours = launch.fade_colours; }
    launch_compose(pixels);
    memcpy(launch_last, pixels, sizeof launch_last);
    for (int i = 0; i < colours; ++i) amiga_set_color(screen, i, palette[i]);
    for (int y = 0; y < screen->height; ++y)
        for (int x = 0; x < screen->width; ++x)
            screen->chunky_buffer[y * screen->width + x] =
                y < 200 && x < 320 ? screen->palette[pixels[y * 320 + x]] : screen->palette[0];
    amiga_present(screen);
}

static int launch_read(const char *root, const char *name, uint8_t *out, size_t size)
{
    char path[4160];
    snprintf(path, sizeof path, "%s/%s", root, name);
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    int ok = fread(out, 1, size, file) == size && fgetc(file) == EOF;
    fclose(file);
    return ok;
}

/* SUB_17B2/SUB_1696/SUB_16E4 file loads, from the verified export:
 * load.pic 5 planes + palette, demo.pic, land0, font.dat, popsam. */
static int launch_load_assets(void)
{
    const char *root = getenv("POPULOUS_ASSET_DIR");
    if (!root) root = POPULOUS_DEFAULT_ASSET_DIR;
    uint8_t raw_palette[64];
    char error[160];
    static uint8_t popsam[TITLE_POPSAM_SIZE];
    char path[4160];
    snprintf(path, sizeof path, "%s/raw/load.pic", root);
    FILE *file = fopen(path, "rb");
    int palette_ok = file && !fseek(file, 40000, SEEK_SET) && fread(raw_palette, 1, 64, file) == 64;
    if (file) fclose(file);
    if (!palette_ok || !launch_read(root, "pixels/load.pic.indices.u8", launch_load_pixels, sizeof launch_load_pixels) ||
        !launch_read(root, "pixels/demo.pic.indices.u8", launch_demo_pixels, sizeof launch_demo_pixels)) {
        POP_LOGE("LOADER", "load.pic/demo.pic missing in %s", root);
        return 0;
    }
    for (int i = 0; i < 32; ++i) launch_load_palette[i] = (uint16_t)(raw_palette[2*i] << 8 | raw_palette[2*i+1]);
    for (size_t i = 0; i < sizeof launch_load_pixels; ++i)
        if (launch_load_pixels[i] > 31 || launch_demo_pixels[i] > 15) return 0;
    launch_font_loaded = launch_read(root, "raw/font.dat", launch_font, sizeof launch_font);
    launch_assets = graphics_load(root, 0, 0, error, sizeof error);
    if (!launch_assets) POP_LOGE("LOADER", "intro blocks: %s", error);
    snprintf(path, sizeof path, "%s/raw/popsam", root);
    file = fopen(path, "rb");
    size_t length = file ? fread(popsam, 1, sizeof popsam, file) : 0;
    int whole = file && fgetc(file) == EOF;
    if (file) fclose(file);
    if (!whole || !title_bank_parse(&launch_music, popsam, length, error, sizeof error))
        POP_LOGE("LOADER", "title music disabled: %s", whole ? error : "raw/popsam missing");
    return 1;
}

static void launch_free_assets(void)
{
    title_music_stop();
    if (launch_audio) { audio_close(); launch_audio = 0; }
    title_bank_free(&launch_music);
    graphics_free(launch_assets);
    launch_assets = NULL;
}

static void launch_pointer(int window_x, int window_y, int *x, int *y)
{
    int width = 0, height = 0;
    SDL_GetWindowSize(screen->window, &width, &height);
    *x = width > 0 ? window_x * screen->width / width : 0;
    *y = height > 0 ? window_y * screen->height / height : 0;
    if (*x < 0) *x = 0;
    if (*x > 319) *x = 319;
    if (*y < 0) *y = 0;
    if (*y > 199) *y = 199;     /* LAB_6FD2 = $18f, halved (asm:3757-3765) */
}

/* SUB_41C asm:330-346 around SUB_456 (asm:347-442) and SUB_1186:
 * returns the choice 1..3, or 0 to quit (Escape / window close, port). */
static int sub_456(void)
{
    POP_LOGD("LOADER", "SUB_456 - load.pic, click regions 3/2/1 (tutorial/conquest/game)");
    memset(&launch, 0, sizeof launch);
    launch.seed = 1;
    launch_actors_reset();
    launch_load_begin();
    int result = LAUNCH_RUN;
    while (result == LAUNCH_RUN) {
        if (launch_frame_hook) launch_frame_hook(launch_frames);
        ++launch_frames;
        SDL_Event ev;
        while (result == LAUNCH_RUN && SDL_PollEvent(&ev)) {
            int x, y;
            if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)) result = LAUNCH_QUIT;
            else if ((ev.type == SDL_KEYDOWN && !ev.key.repeat) || ev.type == SDL_KEYUP) {
                /* Raw codes $60..$67: shifts, Ctrl, Alts, Amigas, Caps Lock. */
                SDL_Keycode key = ev.key.keysym.sym;
                int modifier = key == SDLK_LSHIFT || key == SDLK_RSHIFT || key == SDLK_LCTRL || key == SDLK_RCTRL ||
                               key == SDLK_LALT || key == SDLK_RALT || key == SDLK_LGUI || key == SDLK_RGUI ||
                               key == SDLK_CAPSLOCK;
                launch_key(ev.type == SDL_KEYDOWN, modifier);
            } else if (ev.type == SDL_MOUSEMOTION) {
                launch_pointer(ev.motion.x, ev.motion.y, &x, &y); launch_mouse(launch.held, x, y);
            } else if ((ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) && ev.button.button == SDL_BUTTON_LEFT) {
                launch.held = ev.type == SDL_MOUSEBUTTONDOWN;
                launch_pointer(ev.button.x, ev.button.y, &x, &y); launch_mouse(launch.held, x, y);
            }
        }
        if (result != LAUNCH_RUN) break;
        result = launch_step();
        if (result != LAUNCH_RUN) break;
        launch_render();
        for (int vblank = 0; vblank < launch.vblanks; ++vblank) {
            if (title_music_playing()) title_music_vbi();   /* the VBI runs every frame */
            launch_wait();
        }
    }
    if (result == LAUNCH_QUIT) {
        launch_music_off();    /* port: Escape / window close quits at once, no fade */
        launch_load_begin();
        POP_LOGD("LOADER", "quit from the launcher");
        return 0;
    }
    POP_LOGD("LOADER", "choice = %d", launch.choice);
    g.choice = launch.choice;
    return launch.choice;
}

/* SUB_536 - sets up a ton of tables (the big data area after the code) */
static void sub_536(void)
{
    POP_LOGD("DATA", "SUB_536 - initializing the big lookup tables (palettes, etc.)");

    /* In the disassembly this clears and fills many tables at labels like
     * LAB_6F62, LAB_6F82, LAB_6FD2 etc. with the numeric data that follows.
     * We just acknowledge it here — the actual table data is in the DATA section. */
}

/* ========================================================================= */
/* Main entry - translated from SECSTRT_0 / SUB_4 / LAB_4                    */
/* ========================================================================= */

int main(int argc, char **argv)
{
    pop_log_init();

    POP_LOG_DEBUG("=== Populous Amiga - C Port (SDL2 low-level) ===");

    int status = EXIT_FAILURE;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        POP_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        SDL_Quit();
        return status;
    }

    /* Create a low-level Amiga-style screen */
    screen = amiga_create_screen(320, 256, 4);
    if (!screen) {
        POP_LOG_ERROR("Failed to create screen");
        goto cleanup;
    }

    /* === Exact order from the first chunk of the disassembly === */

    sub_384();          /* open libs + allocate the four big CHIP blocks */
    sub_11a();          /* clear them + set INTENA */

    /* Load "demo.pic" path is referenced here in the asm */
    sub_16e();          /* set up loader structures for demo.pic + populous.prg */

    sub_226();          /* copper / extra screen control area */
    if (!g.alloc_1f400 || !g.alloc_8340 || !g.alloc_7d00 || !g.alloc_4b0 ||
        !g.load_pic_struct || !g.startup_struct || !g.copper_area) {
        POP_LOG_ERROR("Launcher allocation failed");
        goto cleanup;
    }
    sub_29e();          /* display params */

    /* SUB_4 asm:83-86: with arguments the launcher skips its screens; the
     * port hands them to the PRG instead (CLI starts bypass the launcher). */
    int choice = 1;
    if (argc <= 1) {
        if (!launch_load_assets()) POP_LOGE("LOADER", "launcher screens skipped, normal game");
        else choice = sub_456();     /* SUB_41C: load.pic choice, intro on timeout */
        launch_free_assets();
    }
    if (!choice) { status = EXIT_SUCCESS; goto cleanup; }

    sub_536();          /* initialize the large data tables that follow in the binary */

    /* SUB_16E asm:120-126 stores the choice at $42; populous.prg's _main
     * reads its low word as _start_of_game (prg asm:94-96). prg_main()
     * emulates the JMP into the loaded hunk (ENTRY $0003E1E8, A4 $000593EE). */
    POP_LOG_DEBUG("[LAUNCHER] Choice %d -> populous.prg (_start_of_game)", choice);
    extern void prg_main(void);
    extern int g_argc;
    extern char **g_argv;
    g_argc = argc;
    g_argv = argv;
    g_start_of_game = choice;
    /* SDL belongs to the launcher; PRG owns its own screen and event loop. */
    amiga_destroy_screen(screen);
    screen = NULL;
    prg_main();
    extern int prg_exit_status(void);
    status = prg_exit_status();
cleanup:
    sub_2c6();
    amiga_destroy_screen(screen);
    screen = NULL;
    SDL_Quit();

    return status;
}