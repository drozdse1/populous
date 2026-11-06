/*
 * populous_prg.c
 *
 * Main game module ("populous.prg") — unity build of modular sources.
 *
 * Layout (include order == original translation order):
 *   a4_state.h              Speaking A4 globals / accessors
 *   modules/01_preamble.c   Forwards, peep accessors, constants, prg_main
 *   modules/02_rules.c      Map, peeps, battles, AI, powers, generate, animate/reset
 *   modules/03_ui_save.c    Pointer/HUD, requesters, save/load, world/lord
 *   modules/04_render_input.c Draw path, move_peeps tick, Amiga/SDL shims, input
 *   modules/05_serial.c     Two-player serial link (over serial_link: TCP/LAN)
 *
 * Prefer named macros from a4_state.h (war_flag, map_vertex_word, …) over raw
 * A4_WORD(0x…)/A4_DISP offsets when touching gameplay code.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#include "land_data.h"
#include "level_data.h"
#include "graphics.h"
#include "audio.h"
#ifndef POPULOUS_DEFAULT_ASSET_DIR
#define POPULOUS_DEFAULT_ASSET_DIR "binaries/populous-extracted"
#endif
#include "symbols_prg.h"
#include "sdl_lowlevel.h"
#include "a4_state.h"
#include "pop_log.h"
#include "serial_link.h"

/* Host-side demo / window state (not part of the A4 image). */
static AmigaScreen *prg_screen = NULL;
static int prg_quit = 0;
static int prg_status = EXIT_SUCCESS;
static void *host_buffers[8];
static GraphicsAssets *game_assets;
static uint8_t game_pixels[GFX_WIDTH * GFX_HEIGHT];
static uint8_t game_font[4400];
static int game_font_loaded;
static int diagnostic_view;

static void ensure_prg_screen(void);

#include "modules/01_preamble.c"
#include "modules/02_rules.c"
#include "modules/03_ui_save.c"
#include "modules/04_render_input.c"
#include "modules/05_serial.c"

#ifdef PRG_STANDALONE_TEST
int main(void)
{
    prg_main();
    return 0;
}
#endif
