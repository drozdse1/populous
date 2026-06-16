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
static void sub_6538(void);
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
        printf("[EXEC] AllocMem(%u, 0x%08x) -> %p\n", size, attributes, mem);
    } else {
        fprintf(stderr, "[EXEC] AllocMem(%u) FAILED\n", size);
    }
    return mem;
}

static void amiga_free_mem(void *mem)
{
    free(mem);
}

static void *amiga_open_library(const char *name, uint32_t version)
{
    printf("[EXEC] OpenLibrary(\"%s\", %u)\n", name, version);
    if (strstr(name, "graphics"))   return (void*)0x1000;
    if (strstr(name, "intuition"))  return (void*)0x2000;
    return NULL;
}

static void amiga_close_library(void *base)
{
    printf("[EXEC] CloseLibrary(%p)\n", base);
}

/* ========================================================================= */
/* Translated assembly routines - FIRST CHUNK                              */
/* ========================================================================= */

/* From the very top of the disassembly */
static void sub_384(void)   /* originally opened libs + first big allocs */
{
    g.graphics_base = amiga_open_library("graphics.library", 0);
    if (!g.graphics_base) {
        fprintf(stderr, "graphics.library open failed\n");
        exit(1);
    }

    g.intuition_base = amiga_open_library("intuition.library", 0);
    if (!g.intuition_base) {
        fprintf(stderr, "intuition.library open failed\n");
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
    printf("[HW] INTENA = 0x0410  (enable display & copper interrupts)\n");
}

/* SUB_16E - sets up the loader structures for "demo.pic" and "populous.prg" */
static void sub_16e(void)
{
    printf("[LOADER] SUB_16E - creating startup structures for demo.pic / populous.prg\n");

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
    printf("[INIT] SUB_226 - allocated copper / screen control area (0x480 bytes)\n");

    /* The original then did pointer arithmetic into this block for copper list etc. */
}

/* SUB_29E - small display init */
static void sub_29e(void)
{
    /* Original did MOVE.W #$018f, something at 0x82ac */
    g.screen_width  = 320;
    g.screen_height = 256;
    printf("[INIT] SUB_29E - display params set (320x256)\n");
}

/* SUB_2C6 - cleanup of early allocations */
static void sub_2c6(void)
{
    if (g.copper_area)   amiga_free_mem(g.copper_area);
    if (g.load_pic_struct) amiga_free_mem(g.load_pic_struct);
    /* The big screen allocs are usually kept until later */
}

/* SUB_308 - select one of the allocated buffers */
static void sub_308(int16_t which)
{
    printf("[SELECT] SUB_308(0x%04x) - choosing buffer\n", (uint16_t)which);
    /* In the real code this picked between the different alloc_ blocks */
}

/* SUB_2E2 - poke interrupt register */
static void sub_2e2(int16_t value)
{
    printf("[HW] SUB_2E2 - INTENA write 0x%04x\n", (uint16_t)value);
}

/* SUB_456 - the interactive load.pic screen + mouse button choice */
static int sub_456(void)
{
    printf("[LOADER] SUB_456 - showing load.pic, waiting for mouse click regions\n");

    /* The disassembly polls mouse and checks regions:
     *   0x009c-0x00aa X with certain Y → choice 3
     *   0x00aa-0x00b8 X → choice 2
     *   0x00b8+       → choice 1
     */
    /* For the port we just simulate a choice (user can change this) */
    int choice = 1;   /* 1 = full game, 2 = ?, 3 = demo */

    printf("[LOADER] Simulated mouse choice = %d\n", choice);
    g.choice = choice;
    return choice;
}

/* SUB_536 - sets up a ton of tables (the big data area after the code) */
static void sub_536(void)
{
    printf("[DATA] SUB_536 - initializing the big lookup tables (palettes, etc.)\n");

    /* In the disassembly this clears and fills many tables at labels like
     * LAB_6F62, LAB_6F82, LAB_6FD2 etc. with the numeric data that follows.
     * We just acknowledge it here — the actual table data is in the DATA section. */
}

/* ========================================================================= */
/* Main entry - translated from SECSTRT_0 / SUB_4 / LAB_4                    */
/* ========================================================================= */

int main(int argc, char **argv)
{
    printf("=== Populous Amiga - C Port (SDL2 low-level) ===\n\n");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed\n");
        return 1;
    }

    /* Create a low-level Amiga-style screen */
    screen = amiga_create_screen(320, 256, 4);
    if (!screen) {
        fprintf(stderr, "Failed to create screen\n");
        return 1;
    }

    /* === Exact order from the first chunk of the disassembly === */

    sub_384();          /* open libs + allocate the four big CHIP blocks */
    sub_11a();          /* clear them + set INTENA */

    /* Load "demo.pic" path is referenced here in the asm */
    sub_16e();          /* set up loader structures for demo.pic + populous.prg */

    sub_226();          /* copper / extra screen control area */
    sub_29e();          /* display params */

    /* The original then called several more inits, then the loader */
    int choice = sub_456();     /* the interactive "load.pic" + mouse regions */

    sub_536();          /* initialize the large data tables that follow in the binary */

    /* =====================================================================
     * Now we would load "populous.prg" (the module whose full symbol map
     * you just pasted: ENTRY $0003E1E8, BASEREG A4 at $000593EE).
     * In the C port we emulate that by calling prg_main() (defined in
     * populous_prg.c, which has the complete forward decls + stubs from
     * the map + high-level game loop _main/_animate/_setup_display etc.).
     *
     * prg_main emulates the JMP after the launcher has set up A4 globals area.
     * 1993 features (save/load, IFF writer, debug, removed protection, new
     * "LANDSCAPE NO" option string, conquest scoring) are noted in symbols_prg.h.
     * ===================================================================== */
    if (choice == 1) {
        printf("\n[LAUNCHER] Choice == 1 (full game) -> jumping into prg_main (populous.prg)\n");
        extern void prg_main(void);
        extern int g_argc;
        extern char **g_argv;
        g_argc = argc;
        g_argv = argv;
        prg_main();   /* does not return in current implementation (owns the loop) */
        /* If it ever returns we would fall through to cleanup */
    }

    /* Very crude "we loaded the picture" (first-chunk demo path for other choices) */
    printf("\n[GFX] Would now decode demo.pic / load.pic into bitplanes...\n");
    printf("[GFX] For demo purposes we'll just put some colors on screen.\n");

    /* Fake some palette and a simple pattern so the window isn't black */
    for (int i = 0; i < 16; i++) {
        amiga_set_color(screen, i, (i << 8) | (i << 4) | i);
    }

    /* Draw a very simple pattern using the chunky buffer */
    for (int y = 0; y < screen->height; y++) {
        for (int x = 0; x < screen->width; x++) {
            int color = ((x ^ y) >> 4) & 0x0F;
            screen->chunky_buffer[y * screen->width + x] = screen->palette[color];
        }
    }

    printf("\nPress ESC or close the window to exit the first-chunk demo.\n");

    /* Simple event loop so you can see the result */
    int running = 1;
    SDL_Event ev;
    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }
        amiga_present(screen);
        SDL_Delay(16);
    }

    /* Cleanup */
    sub_2c6();
    amiga_destroy_screen(screen);
    SDL_Quit();

    printf("\nFirst chunk port finished.\n");
    return 0;
}