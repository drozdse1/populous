/*
 * populous_prg.c
 *
 * Skeleton / high-level structure for the main game module ("populous.prg")
 * from the 1993 Hit Squad budget release.
 *
 * This is the code loaded by the launcher (previous symbols.h + populous_init.c).
 * Entry at $0003E1E8 relative, BASEREG A4 at $000593EE (globals base).
 *
 * The launcher conceptually loads this file into memory at PRG_LOAD_BASE,
 * sets A4 = PRG_A4_GLOBALS, then jumps to _main (0x0003E1EE).
 *
 * This module contains the full game:
 * - _main / _animate (core loop)
 * - Map generation & drawing (_make_map, _draw_map, _mod_map, _zoom_map, _paint_the_map)
 * - Peeps / population simulation (_move_peeps, _move_explorer, _move_magnet_peeps, _place_people, _zero_population, etc.)
 * - Powers & effects (_do_quake, _do_volcano, _do_swamp, _do_knight, _do_flood, _do_war, _do_magnet, _sculpt, _do_funny, _devil_effect, etc.)
 * - Battles & conquest (_do_battle, _battle_over, _set_battle, _join_battle, _won_conquest, scoring)
 * - UI & input (_requester, _keyboard, _mouse, _toggle_button, _draw_icon, _draw_bar, _text, _move_pointer, _options, _game_options, _get_name)
 * - Save / load (new in 1993: _save_game, _load_game, _checksum)
 * - Serial / 2-player (_two_players, _try_serial, _serial_message, _set_serial, _read_serial, _write_serial, etc.)
 * - Sound (_PlaySound, _setsound, _PlayMeas, _load_sound, _open_channels, etc.)
 * - Display / graphics (_setup_display, _make_copper, _draw_map, _draw_sprite, _draw_s_32, _copy_screen, _open_screen, _Setscreen, _swap_screens, _change_palette, _convert_sprite, _insert_sprite, _draw_mouth, _load_ground, _read_sprites, _read_lord, _read_mouth, _sprite_to_amiga, _free_amiga_sprites, etc.)
 * - Misc: _code / _decode (level codes), _sort, _get_message, _show_world, _rotate_all_map, _clear_all_map, _set_frame, _set_town, _set_tend_icons, _set_mode_icons, _show_the_shield, _set_temp_view, _move_mana, _where_do_i_go, _get_heading, _join_forces, _make_level, _one_block_flat, _place_first_people, _set_magnet_to, _make_woods_rocks, _do_place_funny, _set_devil_magnet, _get_a_message, _check_cancel, _put_in_string, _waitfor, _write_iff (new IFF screenshot tool), _save_picture_body, _pack_picture_bits, _drw_blk, _draw_it, _move_sprite, _clr_wsc, _read_back_scr, _get_a_dir, _set_baud_rate, _check_serial, _display_debug (new), _clear_send, _write_full, _read_full, _do_action, _newrand (LCG), _pixel, _draw_icon, _draw_bar, _draw_sprite, _draw_s_32, _toggle_icon, _vbi_timer, _make_copper, _reset_screen, _show_screen, _waittof, _change_palette, _convert_sprite, _insert_sprite, _draw_mouth, _move_pointer, _text, _set_serial, _clear_serial, _r_ser_buff, _w_ser_buff, _read_serial, _write_serial, _PlaySound, _setsound, _PlayMeas, _free_all_sounds, _open_channels, _close_channels, _load_sound, _kill_effect, _check_effect, _valid_move, _check_life, _long_asc, _word_asc, _uword_asc, _compress, _decompress, etc.
 * - C shims and Amiga exec/graphics/dos/icon/math calls (many _ prefixed).
 *
 * Graphics model (low-level, matching our SDL layer):
 * - Custom Amiga screens / bitplanes (multiple planes for the map view + UI).
 * - Copper lists for display setup, scrolling?, palette effects, dual-playfield or sprites.
 * - Sprites for peeps (small animated figures on the landscape), lords/heroes, the "mouth" (god head?).
 * - Tile-based landscape with height (raise/lower points, sculpt).
 * - Icons for powers, population bars, mana, etc.
 * - Font for text, messages, level codes.
 *
 * In the C port:
 * - Emulate A4 as a pointer to a large globals struct or byte array (prg_a4).
 * - Use our SDL low-level layer: AmigaScreen with bitplanes, palette, present (vsync/copper), sprite blitting, etc.
 * - Extend the layer as needed for the prg's custom screens, sprite DMA, copper waits, etc.
 * - For map: a 2D array or bitplanes for terrain/height/population.
 * - Peeps: list or array of structs with position, type, state; drawn as sprites on the map tiles.
 * - Powers modify the map and spawn effects/peeps.
 * - 1993 additions: save/load (binary with checksum), IFF writer (screenshot tool).
 *
 * This is the structural skeleton based on the full symbol map.
 * To fill in the actual logic, paste the assembly body for any function
 * (e.g. _main, _animate, _draw_map, _move_peeps, _do_quake, etc.)
 * and I'll translate it to C, wiring it into the SDL emulation.
 *
 * The prg also has its own startup: opens libraries (graphics, dos, icon, math),
 * sets up interrupts (VBI for game tick), display, etc.
 * Many functions are Amiga exec/graphics calls or shims (_AllocMem, _OpenLibrary, etc.).
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L  /* opendir/mkdir for the save drawer */
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
#include "symbols_prg.h"  /* all the _function addresses and data from the map */
#include "sdl_lowlevel.h" /* our low-level Amiga gfx emulation (bitplanes, sprites, copper/vsync, palette) */

/* Emulated A4 base for this prg's globals.
 * The original uses BASEREG A4 at $000593EE for almost all game state
 * (map data, peeps, UI, etc.). We allocate a block and use it as the
 * A4 "register".
 */
/* Signed d16 displacements straddle A4, including the 65x65 word vertex map.
 * GCC packed/may_alias accessors support the original two-byte long alignment
 * without violating C aliasing. Values are host-endian, not disk images. */
_Alignas(4) static uint8_t prg_a4_area[65536];
static uint8_t *prg_a4 = prg_a4_area + 32768; /* temporarily redirected for staged starts */
typedef struct __attribute__((packed, may_alias)) { int16_t value; } A4Word;
typedef struct __attribute__((packed, may_alias)) { uint32_t value; } A4Long;
#define A4_WORD(offset) (((A4Word *)A4_DISP(offset))->value)
#define A4_LONG(offset) (((A4Long *)A4_DISP(offset))->value)

/* Visible demo support for motivation */
static AmigaScreen *prg_screen = NULL;
static int prg_quit = 0;
static int prg_status = EXIT_SUCCESS;
static void *host_buffers[8];
static GraphicsAssets *game_assets;
static uint8_t game_pixels[GFX_WIDTH * GFX_HEIGHT];
static uint8_t game_font[4400];
static int game_font_loaded;
static int diagnostic_view;

/* Forward so we can use it in prg_main for the immediate test pattern */
static void ensure_prg_screen(void);

/* A4-relative displacements from this assembly chunk (BASEREG _A4,A4).
 * The numbers come from the (d16,A4) modes in the opcodes (e.g. 29409868 = MOVE.L D0,(A4,$9868)).
 * These land at the absolute locations listed in the symbol map / labels.
 * Use these for direct faithful emulation of the original data layout.
 */
#define A4_DISP(d16)   (prg_a4 + (int16_t)(d16))

/* Common ones from _main + init + _setup_display + _animate in this chunk */
#define sprite_data host_buffers[0]
#define sprite_data_32 host_buffers[1]
#define blk_data host_buffers[2]
#define back_scr host_buffers[3]
#define screen_pointer host_buffers[4]
#define copper_list host_buffers[5]
#define spare_cl host_buffers[6]
#define fb host_buffers[7]
#define start_of_game    A4_WORD(0x9898)
#define sprites_in       A4_WORD(0x989a)
#define in_conquest      A4_WORD(0x84ba)
#define conq_08_seed     A4_WORD(0x84c4)
#define conq_05_terrain  (*(uint8_t *)A4_DISP(0x84c1))
#define seed             A4_WORD(0x99e2)
#define start_seed       A4_WORD(0x99e4)
#define ground_in        A4_WORD(0x99e6)
#define conquest         (*(uint8_t *)A4_DISP(0x84bc))
#define conq_01_speed    (*(uint8_t *)A4_DISP(0x84bd))
#define conq_02_enemypow (*(uint8_t *)A4_DISP(0x84be))
#define conq_03_yourpow  (*(uint8_t *)A4_DISP(0x84bf))
#define conq_04_mode     (*(uint8_t *)A4_DISP(0x84c0))
#define game_mode        A4_WORD(0x84b8)
#define w_screen         A4_LONG(0x99e8)
#define d_screen         A4_LONG(0x99ec)
#define pal              A4_WORD(0x99e0)
#define mouse_limit      A4_WORD(0x99a8)
#define mousex           A4_WORD(0x99aa)
#define mousey           A4_WORD(0xae6e)  /* from the assembly chunk */
#define big_mousex       A4_WORD(0x99b0)
#define big_mousey       A4_WORD(0x99b2)
#define dint             (A4_DISP(0x984a))          /* interrupt server struct */
#define xoff             A4_WORD(0x99f0)
#define yoff             A4_WORD(0x99f2)
#define player           A4_WORD(0x99f4)
#define good_pop         (A4_DISP(0x99fe))          /* array base */
#define tempo            A4_WORD(0x824e)
#define tempo_now        A4_WORD(0x9a18)
#define beat_two         A4_WORD(0x9a16)
#define toggle           A4_WORD(0x9a1a)
#define no_sprites       A4_WORD(0x9a1c)
#define ok_to_build      A4_WORD(0x9a1e)
#define sound_effect     (*(uint8_t *)A4_DISP(0x9a20))
/* Sound state; absolute vbi labels rebased on A4 $593ee. */
#define effect           A4_WORD(0x2fac)
#define last_fire        A4_WORD(0x2fae)
#define music_now        A4_WORD(0x3152)
#define music            A4_WORD(0x3154)
#define sound_in         A4_WORD(0x3156)
#define seqlen           A4_WORD(0x3378)
#define no_sounds        A4_WORD(0x3542)
#define curseq           A4_WORD(0x3544)
#define curbeat          A4_WORD(0x3546)
#define curtime          A4_WORD(0x3548)
#define Soundpri(ch)     A4_WORD(0x354e + 2*(ch))
#define music_off        A4_WORD(0x8250)  /* nonzero = music plays (asm:19621) */
#define effect_off       A4_WORD(0x8252)  /* nonzero = effects play (asm:19633) */
#define mode             A4_WORD(0x8256)
#define pointer          A4_WORD(0x84b6)
#define sprite           (A4_DISP(0x9a22))          /* sprite list base */
#define peeps            (A4_DISP(0x9c26))          /* peeps array base */
#define no_peeps         A4_WORD(0xae5e)

/* Map data structures (from asm LEA (_map_xxx,A4) and absolute labels in the binary).
 * These are the core world state (height/terrain/owner/alt etc.).
 * Offsets are relative to A4 (0x593EE). We now store our demo height map here
 * for higher fidelity to the real init/game data layout (see tetracorp level-format analysis).
 * map_who at +0x1f88 is heavily used for "who" / terrain decisions in _make_map, sculpt, peeps, etc.
 */
#define map_who          (A4_DISP(0x1f88))
#define map_blk          (A4_DISP(0xcf88))
#define map_alt          (A4_DISP(0xdf88))
#define map_bk2          (A4_DISP(0xef88))
#define map_steps        (A4_DISP(0xff88))
/* map_colour at 0x8370 relative (used for palette/ground in some paths) */
#define map_colour_data  (A4_DISP(0x8370))
#define view_timer       A4_WORD(0xae60)
#define left_button      A4_WORD(0xae62)
#define right_button     A4_WORD(0xae64)
#define old_view_who     A4_WORD(0xae66)
#define view_who         A4_WORD(0xae68)
#define game_turn        A4_WORD(0xae6a)
#define pause            A4_WORD(0x9c22)
#define paint_map        A4_WORD(0x9c24)
#define serial_off       A4_WORD(0x8254)
#define not_player       A4_WORD(0x825a)
#define stats            (A4_DISP(0x82b6))          /* stats array */
#define inkey            (*(uint8_t *)A4_DISP(0xae6c))
#define bitfield_51645   (*(uint8_t *)A4_DISP(0x8257))
#define GfxBase_prg      A4_LONG(0x99a4) /* from _rs etc */
#define IntuitionBase_prg A4_LONG(0x99ac)

/* Small interrupt data areas used in setup (for server structs) */
#define LAB_52C40        (*(uint8_t *)A4_DISP(0x9852))
#define LAB_52C41        (*(uint8_t *)A4_DISP(0x9853))
#define LAB_52C42        (A4_DISP(0x9854))
#define LAB_52C46        (A4_DISP(0x9858))
#define LAB_52C4A        (A4_DISP(0x985c))
#define LAB_52DAA        (*(uint8_t *)A4_DISP(0x99bc))
#define LAB_52DAB        (*(uint8_t *)A4_DISP(0x99bd))
#define LAB_52DAC        (A4_DISP(0x99be))
#define LAB_52DB0        (A4_DISP(0x99c2))
#define LAB_52DB4        (A4_DISP(0x99c6))
#define LAB_52DC0        (*(uint8_t *)A4_DISP(0x99d2))
#define LAB_52DC1        (*(uint8_t *)A4_DISP(0x99d3))
#define LAB_52DC2        (A4_DISP(0x99d4))
#define LAB_52DC6        (A4_DISP(0x99d8))
#define LAB_52DCA        (A4_DISP(0x99dc))
#define serial           (A4_DISP(0x989c))          /* serial struct base */

/* Additional from _animate / debug paths */
#define _my_task         A4_LONG(0x9860) /* used in openlibrary */

/* Conquest / other small vars referenced */
#define LAB_516DE        A4_WORD(0x82f0)
#define LAB_516E2        A4_WORD(0x82f4)
#define LAB_516E0        A4_WORD(0x82f2)
#define LAB_516B2        A4_WORD(0x82c4)
#define LAB_516C4        (A4_DISP(0x82d6))
#define LAB_516A6        (A4_DISP(0x82b8))
#define LAB_516A5        (A4_DISP(0x82b7))
#define LAB_53015        (A4_DISP(0x9c27))  /* peeps related for win cheat */
#define LAB_53018        (A4_DISP(0x9c2a))

/* Prototypes for shims referenced by the translated chunk (defined at bottom of the inserted block) */
static void *amiga_alloc_mem(uint32_t, uint32_t);
static void  amiga_free_mem(void*);
static void  ___open_screen(void);
static void  ___read_sprites(int16_t);
static void  ___sprite_to_amiga(void);
static void  ___start_game(void);
static void  ___set_serial(void);
static void  ___load_ground(int16_t,int16_t);
static void  ___Setscreen(int16_t,void*,void*);
static void  ___copy_screen(void*,void*);
static void  ___toggle_icon(int16_t,int16_t,int16_t,void*);
static void  ___draw_it(int16_t,int16_t);
static void  ___draw_s_32(int16_t,int16_t,void*);
static void  ___draw_sprite(int16_t,int16_t,int16_t,void*);
static void  ___clr_wsc(void);
static void  ___swap_screens(void);
static int   ___show_world(void);
static void  ___return_to_game(void);
static void  ___display_debug(void);
static void  ___paint_the_map(void*);
static void  ___text(const char*,int16_t,int16_t,void*);
static void  ___a_putpixel(int16_t,int16_t);
static void  ___keyboard(void);
static void  ___setup_mouse(void);
static void  ___AddIntServer(int16_t,void*);
static void  ___RemIntServer(int16_t,void*);
static void  ___Delay(int16_t);
static void  ___reset_screen(void*);
static void  ___free_amiga_sprites(void);
static void  ___free_all_sounds(void);
static void  ___close_channels(void);
static void *___FindTask(void*);
static void  ___exit(int16_t);
static int32_t ___mulu(int32_t,int32_t);
static int32_t ___divs(int32_t,int32_t);
static uint16_t ___newrand(void);
static void ___SetIntVector(int16_t vec, void *handler) { printf("[PRG] SetIntVector %d\n", vec); }
static void *___OpenLibrary(const char *name, uint32_t ver) { printf("[PRG] OpenLibrary %s\n", name); return (void*)0x1000; }
static void  ___CloseLibrary(void *base) { printf("[PRG] CloseLibrary\n"); }

/* Note: many more offsets exist in the full binary (map at ~0x5b376 area etc.).
   Add them as we translate more bodies. The prg_a4_area simulates the entire
   A4-relative + absolute data space for this prg. */

/* === Demo data for fast visual testing ===
 * State is now stored in the emulated A4 globals (map_who etc.) for higher fidelity
 * to the real game (see tetracorp analysis and asm LEA for _map_who at +0x1f88 etc.).
 * demo_height is now an accessor macro into the A4 map storage.
 * This prepares the demo for when real _make_map/_draw_map bodies are translated.
 */
#define DEMO_MAP_W 64
#define DEMO_MAP_H 64
/* Synthetic signed cell heights are separate from map_who occupancy. */
#define demo_height_at(my, mx)  (((int8_t *)map_alt)[(my)*DEMO_MAP_W + (mx)])

/* Legacy name for minimal diff in existing code; expands to the A4-backed accessor */
#define demo_height(y,x) demo_height_at((y),(x))

/* Canonical 22-byte records: _place_people, asm 431ca-4329a.
 * The unresolved fields remain bytes, not a guessed native struct. */
#define MAX_DEMO_PEEPS 208
#define num_demo_peeps no_peeps
static int demo_mana = 50; /* Display placeholder, never earned/spent by demo rules. */
static int demo_shield_timer;
static int growth_tick;
static int16_t peep_word(int index, int offset)
{
    int16_t value;
    memcpy(&value, peeps + index * 22 + offset, sizeof value);
    return value;
}
static void set_peep_word(int index, int offset, int16_t value)
{
    memcpy(peeps + index * 22 + offset, &value, sizeof value);
}
/* Bytes 14..17: host-endian uint32_t, zero absent, slot+1 otherwise.
 * Original values were Amiga pointers (0x53014 + 22*slot), not this encoding;
 * this is not a save format. Opaque fixtures/presence checks stay opaque.
 * Decode returns -1 for absent, -2 for invalid, or a current peeps slot.
 * Slot reuse intentionally retains identity: no pointer or generation cache. */
static int peep_reference(int subject)
{
    if (no_peeps < 0 || no_peeps > 208 || subject < 0 || subject >= no_peeps) return -2;
    uint32_t raw;
    memcpy(&raw,peeps+subject*22+14,sizeof raw);
    if (raw > (uint32_t)no_peeps || raw > 208) return -2;
    return (int)raw-1;
}
static int set_peep_reference(int subject, int target)
{
    if (no_peeps < 0 || no_peeps > 208 || subject < 0 || subject >= no_peeps ||
        target < -1 || target >= no_peeps) return 0;
    uint32_t raw=(uint32_t)(target+1);
    memcpy(peeps+subject*22+14,&raw,sizeof raw);
    return 1;
}
static int peep_owner(int index) { return peeps[index * 22 + 1]; }
/* _get_heading asm:5469-5536. Used by knight creation and validated heading
 * navigation. The original ASR.W #6 floors negative rows;
 * use division explicitly so signed right shift is not implementation-defined. */
static int core_get_heading(int subject)
{
    if (!set_peep_reference(subject,subject)) return 0;
    int position=peep_word(subject,8), column=(uint16_t)position&63;
    int row=position >= 0 ? position/64 : -((-position+63)/64);
    int nearest=9999;
    for (int candidate=0;candidate<no_peeps;++candidate) {
        if (peep_owner(candidate)==peep_owner(subject) || peep_word(candidate,4)<=0 ||
            (peeps[candidate*22]&0x80)) continue;
        int other=peep_word(candidate,8), other_column=(uint16_t)other&63;
        int other_row=other >= 0 ? other/64 : -((-other+63)/64);
        int distance=abs(other_column-column)+abs(other_row-row);
        if (distance < nearest) {
            nearest=distance;
            set_peep_reference(subject,candidate);
        }
    }
    return 1;
}
static int peep_screen_x(int index) { return (peep_word(index, 8) % 64) * 4 + 2; }
static int peep_screen_y(int index) { return (peep_word(index, 8) / 64) * 4 + 2; }

/* Forward declarations for all the major functions from the map.
 * Complete list taken from the full IRA symbol map (prg entry $3E1E8, A4 at $593EE).
 * The complete set of #defines (with original addresses) lives in symbols_prg.h.
 * Core flow from map + memory: _main -> _setup_display / _make_map / _place_first_people
 * then the main loop revolves around _animate (peeps, mana, effects, computer, scoring)
 * + input (_keyboard / _mouse), drawing (_draw_map / _draw_sprite / _draw_icon etc.)
 * + _vbi_timer driven parts in real hardware.
 */
/* Real translated entry points and functions from the new assembly chunk
 * (populous.prg _main at 3e1ee etc.). Preserved 1993 comments and structure.
 * prg_main (called from launcher) emulates the load + JMP into the prg _main.
 */
static void _main(void);
static void _rs(void);
static void _closeall(void);
static void _create_mouse(void);
static void _setup_interrupt(void);
static void _set_mouse(void);
static void _free_inter(void);
static void _setup_serial_interrupt(void);
static void _free_serial_interrupt(void);
static void _closelibrary(void);
static void _set_pri(void);
static void _debug(void);
static void _openlibrary(void);

static void _setup_display(void);
static void _animate(void);

/* The rest are still stubs or will be filled when their bodies are provided in future chunks */
static void _clear_map(void);
static void _make_map(void);
static void _make_alt(void);   /* from the chunk translation */
static void _draw_it(int16_t y, int16_t x);   /* forward for the call below */

static void _draw_map(void)
{
    /* simple visible world draw for motivation - real one when body pasted */
    _draw_it(0, 0);
}
static void _mod_map(void);
static void _zoom_map(int16_t d);
static void _set_tend_icons(void);
static void _set_mode_icons(void);
static void _raise_point(void);
static void _lower_point(void);
static void _sculpt(void);
static void _do_raise_point(void);
static void _do_lower_point(void);
static void _paint_the_map(void);
static void _waitfor(void);          /* vertical blank wait */

static void _move_peeps(void);
static void _move_explorer(void);
static void _move_magnet_peeps(void);
static void _move_mana(void);
static void core_set_pointer(void);
/* Open requester. The original runs each one as a blocking loop inside
 * _do_action, so nothing else moves; the port keeps the state here, stops
 * game_tick while kind is set and routes left presses to it. _set_options
 * runs inside _game_options and returns to it. */
enum { CORE_REQ_NONE, CORE_REQ_OPTIONS, CORE_REQ_GAME, CORE_REQ_SET, CORE_REQ_WORLD, CORE_REQ_FILE };
static struct {
    int kind, side;
    uint8_t saved_stats[46];                /* _options -56(A5) */
    int16_t saved_serial, saved_paint, saved_player, saved_human, saved_conquest; /* _game_options -6..-14 */
    int16_t saved_mode;                     /* _set_options -46 */
} core_req;
static void core_options_open(int side);
static void core_game_options_open(void);
static void core_requester_click(void);
static void core_file_open(int save);
static void core_file_layout(int draw);
static void core_draw_requester(void);
/* _show_world state, see core_world_open. */
enum { CORE_WORLD_START, CORE_WORLD_OPTIONS, CORE_WORLD_WON };
static struct { int origin, shown, entry, error, level, length, blink; } core_world;
static uint8_t core_world_ok[46], core_world_cancel[46];
/* _save_load's file requester (CORE_REQ_FILE), see core_file_open. The
 * _get_a_dir listing (32-byte names) was at _w_screen; 256 here. */
#define CORE_FILE_MAX 256
static struct {
    int save, count, top, field, length, blink, error;
    char names[CORE_FILE_MAX][32];
} core_file;
static uint8_t core_file_ok[46];   /* _save_load -130(A5) */
#define core_message ((char *)A4_DISP(0x3058))
static void core_world_open(int origin);
static void core_world_key(int key);
static void core_world_layout(int draw);
static void core_world_click(void);
static void core_draw_world(void);
static void core_end_button_layout(int draw);
static int core_lord_open(int old_level, int level, int finished);
static void core_lord_leave(void);
static void core_lord_vblank(void);
static void core_draw_lord(void);
static int core_check_cancel(const uint8_t *ok, const uint8_t *cancel);
static void _where_do_i_go(void);
static void _get_heading(void);
static void _join_forces(void);
static void _zero_population(void);
static void _set_frame(void);
static void _set_town(void);
static void _place_people(void);
static void _place_first_people(void);

static void _do_battle(void);
static void _battle_over(void);
static void _set_battle(void);
static void _join_battle(void);
static void _do_magnet(void);

static void _do_quake(void);
static void _do_volcano(void);
static void _do_swamp(void);
static void _do_knight(void);
static void _do_flood(void);
static void _do_war(void);
static void _do_funny(void);
static void _do_place_funny(int16_t a);
static void _make_woods_rocks(void);
static void _devil_effect(void);
static void _do_computer_effect(void);
static void _set_devil_magnet(void);

static void _rotate_all_map(void);
static void _clear_all_map(void);
static void _make_level(void);
static void _one_block_flat(void);

static void _requester(void);
static void _toggle_button(void);
static void _get_name(void);
static void _save_load(void);
static void _game_options(void);
static void _set_options(void);
static void _options(void);
static void _return_to_game(void);
static void _start_game(void);
static void _end_game(void);
static void _won_conquest(void);

static void _code(void);             /* level code entry / conquest passwords */
static void _decode(void);

static void _save_game(void);        /* 1993 addition */
static void _load_game(void);        /* 1993 addition */
static void _checksum(void);
static void _write_iff(void);        /* 1993 tester-only IFF screenshot writer (new, not called from game) */
static void _save_picture_body(void);
static void _pack_picture_bits(void);
static void _drw_blk(void);
static void _draw_it(int16_t y, int16_t x);
static void _move_sprite(void);

static void _keyboard(void);
static void _mouse(void);
static void _move_pointer(void);
static void _draw_icon(void);
static void _draw_bar(void);
static void _draw_sprite(void);
static void _draw_s_32(void);
static void _toggle_icon(void);
static void _text(void);

static void _do_message(void);
static void _get_a_message(void);
static void _serial_message(void);
static void _two_players(void);
static void _try_serial(void);
static void _check_cancel(void);
static void _put_in_string(void);
static void _show_world(void);

static void _set_serial(void);
static void _clear_serial(void);
static void _read_serial(void);
static void _write_serial(void);
static void _check_serial(void);
static void _set_baud_rate(void);


static void _valid_move(void);
static void _check_life(void);
static void _newrand(void);          /* LCG, period 2^15 */

static void _vbi_timer(void);
static void _make_copper(void);
static void _reset_screen(void);
static void _show_screen(void);
static void _waittof(void);
static void _change_palette(void);
static void _convert_sprite(void);
static void _insert_sprite(void);
static void _draw_mouth(void);
static void _read_sprites(void);
static void _read_lord(void);
static void _read_mouth(void);
static void _load_ground(void);
static void _sprite_to_amiga(void);
static void _free_amiga_sprites(void);
static void _copy_screen(void);
static void _open_screen(void);
static void _Setscreen(void);
static void _swap_screens(void);
static void _clr_wsc(void);
static void _read_back_scr(void);
static void _a_putpixel(void);
static void _pixel(void);

static void _get_message(void);
static void _clear_send(void);
static void _write_full(void);
static void _read_full(void);
static void _do_action(void);
static void _display_debug(void);    /* new in 1993 */

static void _sort(void);
static void _interogate(void);
static void _show_the_shield(void);
static void _set_temp_view(void);

static void _long_asc(void);
static void _word_asc(void);
static void _uword_asc(void);
static void _compress(void);
static void _decompress(void);

/* Data tables (referenced via the defines in symbols_prg.h) */
extern const unsigned char _icon_data[];
extern const unsigned char _font_data[];
/* The many str* fragments for level codes live in the TEXT sections of the original binary. */

/* The many tiny printf stubs for the remaining symbols are defined later in this file
 * (they were kept from the previous skeleton so all called functions have bodies).
 * When real assembly is pasted we will replace the matching stub with a full translation.
 */

/* The prg also contains its own C runtime shims, math (dot_mulu etc.), and Amiga library thunks. */

/* Real high-level prg entry (called from the launcher after it has "loaded"
 * populous.prg and set up A4 emulation area).
 * Original: launcher does the load of the file at PRG_ENTRY, sets A4=BASEADR,
 * then JMPs to _main (which is 6 bytes after the hunk start).
 *
 * In our unified C port we call prg_main() which emulates that jump.
 * prg_main does the real game init (libs, display/copper, first map + people)
 * then the main simulation + render loop.
 */
/* Simple globals so launcher / main can pass args for quick level testing */
int g_argc = 0;
char **g_argv = NULL;
/* The launcher's choice long at $42 (SUB_16E asm:120-126); _main reads its
 * low word into _start_of_game without arguments (asm:94-96):
 * 1 normal game, 2 conquest, 3 tutorial. */
int g_start_of_game = 0;

/* Minimal sample conquest level table for quick testing of "level loading" and init.
 * Based on the exact level.dat format documented at
 * https://tetracorp.github.io/populous/analysis/level-format.html
 *
 * level.dat stores 99 entries of 10 bytes each. Each entry controls traits for
 * 5 consecutive levels (traits change every 5 numbered levels / "battle numbers").
 *
 * Byte layout (our array columns):
 *  [0] Enemy rating (1=strongest .. 10=weakest; shown as Very Good/Good/Avg/Poor/Very Poor)
 *  [1] Enemy reaction speed / aggression (1=Very Fast .. 10=Very Slow)
 *  [2] Powers bitfield for enemy
 *  [3] Powers bitfield for you (player)
 *  [4] Game mode bitfield (build rules, swamp depth, water lethality)
 *  [5] Terrain type
 *  [6] Your starting population (walkers, max ~30)
 *  [7] Enemy starting population
 *  [8] Low byte of 16-bit seed offset (added to (battle_number mod 8))
 *
 * Power bitfield (bytes 2/3):
 *   0x08 Earthquake, 0x10 Swamp, 0x20 Knight, 0x40 Volcano, 0x80 Flood, 0x100 Armageddon
 *
 * Game mode bits (byte 4):
 *   bit0 Water (0=harmful, 1=fatal)
 *   bit1 Swamp (0=shallow, 1=bottomless)
 *   bit2-4 Build rules (mutually exclusive in real data): cannot build / only up / near towns etc.
 *
 * Terrain (byte 5):
 *   0 Grass Planes, 1 Desert, 2 Snow and Ice, 3 Rocky
 *
 * Seed logic (in _setup_display): seed = (in_conquest & 7) + conq_08_seed;
 * The real LCG (___newrand) is the exact one described on the site.
 */
static const uint8_t demo_levels[8][9] = {
    /* lvl 0 */ { 5, 5, 0x07, 0x07, 0x00, 0, 12, 10, 0x0000 },  /* classic grass */
    /* lvl 5 */ { 4, 6, 0x0f, 0x07, 0x01, 1,  8, 14, 0x34 },  /* desert */
    /* lvl 10*/ { 3, 7, 0x1f, 0x0f, 0x02, 2, 15,  9, 0xcd },  /* snow */
    /* lvl 15*/ { 6, 4, 0x07, 0x1f, 0x04, 3, 10, 12, 0x78 },  /* rocky, you strong */
    /* lvl 20*/ { 2, 8, 0x3f, 0x07, 0x08, 0,  6, 18, 0xbc },  /* hard enemy */
    /* lvl 25*/ { 7, 3, 0x01, 0x3f, 0x10, 1, 20,  5, 0xf0 },  /* you very strong */
    /* lvl 30*/ { 4, 5, 0x15, 0x15, 0x00, 2, 11, 11, 0x11 },  /* balanced snow */
    /* lvl 35*/ { 5, 6, 0x2a, 0x15, 0x03, 3,  9, 13, 0x22 },  /* mixed rocky */
};

/* Constants derived from tetracorp.github.io/populous level format analysis.
 * These make the demo_levels and conquest handling clearer and match the real game data.
 */
#define TERRAIN_GRASS   0
#define TERRAIN_DESERT  1
#define TERRAIN_SNOW    2
#define TERRAIN_ROCKY   3

/* Power bitfield (used in conq_02_enemypow / conq_03_yourpow) */
#define POWER_EARTHQUAKE (1u << 0)
#define POWER_SWAMP      (1u << 1)
#define POWER_KNIGHT     (1u << 2)
#define POWER_VOLCANO    (1u << 3)
#define POWER_FLOOD      (1u << 4)
#define POWER_ARMAGEDDON (1u << 5)

/* Game mode bits (conq_04_mode / game_mode) */
#define GAME_MODE_WATER_FATAL     (1u << 0)  /* 0 = harmful, 1 = fatal */
#define GAME_MODE_SWAMP_BOTTOMLESS (1u << 1) /* 0 = shallow, 1 = bottomless */
#define GAME_MODE_NO_BUILD        (1u << 2)
#define GAME_MODE_BUILD_ONLY_UP   (1u << 3)
#define GAME_MODE_BUILD_NEAR_TOWNS (1u << 4)

static int current_start_pop_you = 12;
static int current_start_pop_enemy = 10;
typedef enum { START_SYNTHETIC, START_ORIGINAL, START_TUTORIAL, START_CUSTOM } StartKind;
typedef struct { StartKind kind; int number; } StartSelection;
/* START_CUSTOM: _setup_display(0, arg) with in_conquest -1 entered with
 * _seed and _ground_in (asm:430-457); the last one started (or asked). */
typedef struct { int16_t from, arg, ground; } CustomLand;
static CustomLand core_land;
static StartSelection current_start, requested_start;
static int requested_world;   /* --conquest: _start_of_game == 2 */
static int core_custom_live;  /* the live A4 holds _stats/_game_mode of a started game */
static int current_scenario;
static int game_reset(int scenario);
static int core_custom_start(int16_t from, int16_t arg);
static int game_reset_start(StartSelection selection);
static void game_tick(void);
static void game_render(void);
static void game_handle_event(const SDL_Event *event);

static int core_decode(const char *name);
static int parse_scenario(int argc, char **argv, int *scenario)
{
    *scenario = 0;
    if (argc <= 1) return 1;
    if (argc != 2 || !argv || !argv[1] || !argv[1][0]) return 0;
    for (const char *digit = argv[1]; *digit; ++digit)
        if (*digit < '0' || *digit > '9') return 0;
    errno = 0;
    char *end;
    unsigned long value = strtoul(argv[1], &end, 10);
    if (errno == ERANGE || *end || value > INT_MAX) return 0;
    *scenario = (int)(value % 8);
    return 1;
}

static int parse_start(int argc, char **argv, StartSelection *out)
{
    StartSelection selected = {START_SYNTHETIC, 0};
    requested_world = 0;
    if (argc == 2 && argv && argv[1] && !strcmp(argv[1], "--tutorial")) {
        selected = (StartSelection){START_TUTORIAL, 0};   /* port flag for _start_of_game == 3 */
    } else if (argc == 2 && argv && argv[1] && !strcmp(argv[1], "--conquest")) {
        /* The CLI argument 2 of _main (asm:85-91); GENESIS runs behind it. */
        selected = (StartSelection){START_ORIGINAL, 0};
        requested_world = 1;
    } else if (argc == 3 && argv && argv[1] && !strcmp(argv[1], "--level")) {
        if (!argv[2] || !argv[2][0]) return 0;
        for (const char *digit = argv[2]; *digit; ++digit)
            if (*digit < '0' || *digit > '9') return 0;
        errno = 0;
        char *end;
        unsigned long number = strtoul(argv[2], &end, 10);
        if (errno == ERANGE || *end || number >= LEVEL_COUNT) return 0;
        selected = (StartSelection){START_ORIGINAL, (int)number};
    } else if (argc == 3 && argv && argv[1] && !strcmp(argv[1], "--code")) {
        /* _show_world world-name entry (asm:12278-12300); uppercased for the
         * host shell. Worlds above 2470 have no level.dat record. */
        char name[16];
        size_t length = argv[2] ? strlen(argv[2]) : 0;
        if (!length || length >= sizeof name) return 0;
        for (size_t index = 0; index <= length; ++index) name[index] = (char)toupper((unsigned char)argv[2][index]);
        int world = core_decode(name);
        if (world < 0 || world / 5 >= LEVEL_COUNT) return 0;
        selected = (StartSelection){START_ORIGINAL, world / 5};
    } else if (!parse_scenario(argc, argv, &selected.number)) return 0;
    *out = selected;
    return 1;
}

void prg_main(void)
{
    prg_status = EXIT_SUCCESS;
    prg_quit = 0;
    if (!parse_start(g_argc, g_argv, &requested_start)) {
        fprintf(stderr, "Usage: populous_init_port [nonnegative integer <= INT_MAX | --level 0..494 | --code WORLDNAME | --conquest | --tutorial]\n");
        prg_status = EXIT_FAILURE;
        return;
    }
    /* Without arguments _start_of_game comes from the launcher (asm:94-96):
     * 2 opens the world screen (asm:536-551), 3 the tutorial (asm:13975). */
    if (g_argc <= 1 && g_start_of_game == 2) { requested_start = (StartSelection){START_ORIGINAL, 0}; requested_world = 1; }
    else if (g_argc <= 1 && g_start_of_game == 3) requested_start = (StartSelection){START_TUTORIAL, 0};
    /* 1: _animate's _setup_display(0,1) (asm:503-512) on the boot data:
     * in_conquest -1, _seed 0, _ground_in 0, i.e. custom landscape 0. */
    else if (g_argc <= 1 && g_start_of_game == 1) {
        requested_start = (StartSelection){START_CUSTOM, 0}; core_land = (CustomLand){0, 1, 0};
    }
    core_custom_live = 0;   /* a fresh boot: the _stats/_game_mode data values */
    _main();
}

/* Retain the launcher's void PRG entry; propagate failure without exiting SDL. */
int prg_exit_status(void) { return prg_status; }


/* --- Minimal stub implementations for the functions called from prg_main
 * and the core loop. These match the symbols in the map exactly.
 * Real bodies will come from translating pasted assembly chunks.
 */
#if 0
/* old skeleton duplicate - real translated version from the chunk is present later */
static void _setup_display(void)
{
    printf("[PRG] _setup_display (old)\n");
}
#endif

static void _make_copper(void)
{
    printf("[PRG] _make_copper\n");
}

/* _make_map, asm:1420-1539. Cells derive from the separate 65x65 vertices. */
static int core_vertex(int column, int row)
{
    return A4_WORD(0xae86 + 2 * (row * 65 + column));
}
static void core_set_vertex(int column, int row, int height)
{
    A4_WORD(0xae86 + 2 * (row * 65 + column)) = height;
}
static void core_make_map_region(int left, int top, int right, int bottom)
{
    for (int row = top; row <= bottom; ++row) {
        for (int column = left; column <= right; ++column) {
            int corners[] = {core_vertex(column,row), core_vertex(column+1,row),
                             core_vertex(column+1,row+1), core_vertex(column,row+1)};
            int altitude = (corners[0]+corners[1]+corners[2]+corners[3]) / 4;
            int block = 0, cell = row * 64 + column;
            for (int corner = 0; corner < 4; ++corner)
                if (corners[corner] > altitude) block |= 1 << corner;
            /* asm:1475-1517: preserve rock only while any land remains;
             * protected flat rocks keep the unadjusted average altitude. */
            int protected_rock = map_blk[cell] == 47 && (block || altitude);
            if (protected_rock) block = 47;
            if (altitude && !block) { --altitude; block = 15; }
            if (!altitude && block && block != 15) block += 16;
            map_alt[cell] = altitude;
            if (protected_rock) block = 47;
            map_blk[cell] = block;
            if (!block) map_bk2[cell] = 0;
            A4_WORD(0xff88 + cell * 2) = 0;
        }
    }
}

static void _make_map(void)
{
    core_make_map_region(0,0,63,63);
}

#define core_mana(owner) ((int32_t)A4_LONG(0x9a02 + (owner) * 16))
#define core_tendency(owner) A4_WORD(0x99fa + (owner) * 16)
#define core_leader(owner) A4_WORD(0x99f6 + (owner) * 16)
#define core_target(owner) A4_WORD(0x99f8 + (owner) * 16)
static int core_result, core_battles, core_settlements, core_tool;
static int core_swamp_selected;
static int core_shake_dy; /* terrain-only vertical offset, quake shake */
static const int16_t core_offsets[25] = {
    0,-64,1,64,-1,-63,65,63,-65,-128,2,128,-2,-126,130,126,-130,
    -127,-62,66,129,127,62,-66,-129
}; /* asm:24274 */
static const uint16_t core_city[9] = {42,44,43,44,43,41,41,41,41};
static const int32_t core_mana_values[11] = {-250,10,200,2500,5000,7500,10000,40000,80000,160000,1999999};
static int core_population(int owner)
{
    int total = 0;
    for (int person = 0; person < no_peeps; ++person)
        if (peep_owner(person) == owner && peep_word(person,4) > 0) total += peep_word(person,4);
    return total;
}
/* good_pop/bad_pop: signed longs at A4+$99fe, stride 16 (asm:25331). The
 * index is the raw owner byte or word, scaled without an owner range check. */
#define core_good_pop(owner) A4_LONG(0x99fe + (owner)*16)
static void core_add_mana(int owner, int amount)
{
    int64_t value = (int64_t)core_mana(owner) + amount;
    if (value < -250) value = -250;
    if (value > 1999999) value = 1999999;
    A4_LONG(0x9a02 + owner * 16) = (uint32_t)(int32_t)value;
}
/* _valid_move asm:20912-20954. */
static int core_valid_move(int position, int delta)
{
    if (position < 0 || position >= 4096) return 1;
    if (!delta) return 0;
    int destination = position + delta, horizontal = delta & 63;
    if (horizontal > 3) horizontal -= 64;
    int column = (position & 63) + horizontal;
    if (destination < 0 || destination >= 4096 || column < 0 || column > 63) return 1;
    if (map_blk[destination] == 0) return 3;
    return map_blk[destination] == 47 ? 2 : 0;
}
/* _check_life asm:20958-21042, including neighbouring settlement exclusion. */
static int core_check_life(int owner, int position)
{
    int life = 0;
    A4_WORD(0x2ff8) = A4_WORD(0x2fb2) = 0;
    for (int index = 0; index < 17; ++index) {
        int valid = core_valid_move(position,core_offsets[index]);
        if (valid) { if (valid == 2) life -= 15; continue; }
        int cell = position+core_offsets[index];
        if (map_blk[cell] == 31+owner || map_blk[cell] == 15) {
            if (map_blk[cell] == 15) A4_WORD(0x2fb2) = 1;
            if (!life) life = 50;
            life += 15;
        } else if (!index) return 0;
        int building = map_bk2[cell];
        if (index < 9 && map_bk2[position] == 42 && building >= 41 && building <= 44) ++A4_WORD(0x2ff8);
        else if (index && building > 32 && building <= 44) return 0;
    }
    if (life < 35) return 0;
    return life == 305 ? 3050 : life;
}
/* _set_town asm:5865-6062. Original block IDs, simplified drawing only. */
static void core_set_town(int person, int remove)
{
    int position = peep_word(person,8), owner = peep_owner(person);
    int old_city = map_bk2[position] == 42, city = !remove && peep_word(person,12) == 42;
    if (remove || (old_city && !city)) {
        for (int index = 0; index < (old_city ? 25 : 17); ++index) {
            if (core_valid_move(position,core_offsets[index]) == 1) continue;
            int cell = position+core_offsets[index];
            if (old_city || index < 9) map_bk2[cell] = 0;
            if (map_blk[cell] == 31+owner) map_blk[cell] = 15;
        }
        map_bk2[position] = 0;
        if (remove) return;
    }
    for (int index = 0; index < (city ? 25 : 17); ++index) {
        if (core_valid_move(position,core_offsets[index]) == 1) continue;
        int cell = position+core_offsets[index];
        if (index < 9) map_bk2[cell] = city ? core_city[index] : 0;
        if (map_blk[cell] == 15) map_blk[cell] = 31+owner;
    }
    if (!city && map_blk[position] == 31+owner) map_bk2[position] = peep_word(person,12);
}
static void core_remove(int person)
{
    int owner = peep_owner(person), position = peep_word(person,8);
    if (peeps[person*22]&1) core_set_town(person,1);
    if (map_who[position] == person+1) map_who[position] = 0;
    /* _zero_population asm:5671-5680: also the previous departure's claim. */
    int previous = (int16_t)(position-peep_word(person,10));
    if (previous >= 0 && previous < 4096 && map_who[previous] == person+1) map_who[previous] = 0;
    if (peeps[person*22]&8) {
        int partner = peep_word(person,6);
        /* Walking attrition can remove an attacker immediately after entry,
         * outside core_battle's winner path. Release only its reciprocal link. */
        if (partner >= 0 && partner < no_peeps && partner != person &&
            peep_word(partner,4) > 0 && (peeps[partner*22]&8) &&
            peep_word(partner,6) == person) {
            peeps[partner*22] = (peeps[partner*22]&1) ? 1 : 2;
            set_peep_word(partner,6,0); set_peep_word(partner,10,0);
            if (!(peeps[partner*22]&1)) set_peep_word(partner,12,0);
            int cell = peep_word(partner,8);
            if (!map_who[cell]) map_who[cell] = partner+1;
        }
        set_peep_word(person,6,0);
    }
    if (core_leader(owner) == person+1) {
        core_leader(owner) = 0;
        /* _zero_population / _set_magnet_to asm:5691-5716,8271-8291. */
        if (!pause) {
            core_target(owner) = position;
            A4_WORD(owner ? 0xae76 : 0xae78) = position;
        }
    }
    if (view_who == person+1) view_who = 0;
    set_peep_word(person,4,0); peeps[person*22] = 0;
}
static void core_frame_town(int person, int capacity)
{
    set_peep_word(person,12,capacity >= 3050 ? 42 : 32+capacity*10/305);
}
/* _where_do_i_go asm:4622-4901: rank five choices over nine directional rays. */
static int core_explore(int person)
{
    int distance[5] = {5,5,5,5,5}, choices[5] = {999,999,999,999,999};
    int least_steps = 9999, direction = -1, owner = peep_owner(person);
    for (int ray = 0; ray < 9; ++ray) {
        if (direction == 0 && ray == 1) direction = (___newrand()&7)+1;
        else if (++direction == 9) direction = 0;
        int position = peep_word(person,8), delta = core_offsets[direction];
        for (int depth = 0; depth < peeps[person*22+2]; ++depth) {
            if (core_valid_move(position,delta)) break;
            position += delta;
            if (map_blk[position] == 15 && depth < distance[0]) {
                int build = direction == 0 ? core_check_life(owner,position)>0 : 1;
                if (direction) for (int index = 9; index < 17; ++index) {
                    if (core_valid_move(position,core_offsets[index])) continue;
                    int block = map_bk2[position+core_offsets[index]];
                    if (block > 32 && block <= 44) build = 0;
                }
                if (build) { distance[0] = depth; choices[0] = delta; continue; }
            }
            if (!direction) continue;
            int other = map_who[position]-1, choice = -1;
            if (other >= 0 && other < no_peeps && other != person) {
                if (peeps[other*22]&8) choice = 1;
                else if (peep_owner(other) != owner) choice = 2;
                else if (peeps[other*22]&2) choice = 3;
            }
            if (choice >= 0 && depth < distance[choice]) { distance[choice] = depth; choices[choice] = delta; continue; }
            int steps = (uint16_t)A4_WORD(0xff88+position*2);
            if (delta != peep_word(person,10) && (steps < least_steps || (steps == least_steps && depth < distance[4]))) {
                least_steps = steps; distance[4] = depth; choices[4] = delta;
            }
        }
    }
    if (core_tendency(owner) == 3 && distance[2] != 5) return choices[2];
    if (core_tendency(owner) == 2 && distance[3] != 5) return choices[3];
    for (int index = 0; index < 5; ++index) if (distance[index] != 5) return choices[index];
    return 999;
}
/* _move_magnet_peeps asm:5277-5327 and _move_peeps asm:3491-3543.
 * Owner mailboxes overwrite unconditionally; the tick consumer owns execution.
 * Only the old-water caller gates fatal mode, not the blocked walker. */
static void core_queue_terrain_raise(int owner, int position)
{
    if (owner < 0 || owner > 1 || position < 0 || position >= 4096) return;
    int displacement = owner * 46;
    if (!A4_WORD(0xae80) &&
        (A4_WORD(0x82bc + displacement) != 1 || (game_mode&GAME_MODE_NO_BUILD))) return;
    A4_DISP(0x82b6 + displacement)[0] = 1;
    A4_DISP(0x82b7 + displacement)[0] = position & 63;
    A4_DISP(0x82b8 + displacement)[0] = position / 64;
    A4_WORD(0x82be + displacement) = 1;
}
/* _move_magnet_peeps asm:4912-5465. Existing host headings override magnets;
 * invalid opaque references retain their bytes and use only a bounded magnet. */
static int core_magnet_direction(int person)
{
    if (no_peeps < 0 || no_peeps > 208 || person < 0 || person >= no_peeps) return 999;
    int owner = peep_owner(person), position = peep_word(person,8);
    if (owner > 1 || position < 0 || position >= 4096) return 999;
    int reference = peep_reference(person), present = reference != -1;
    int target;
    if (reference >= 0) {
        target = peep_word(reference,8);
        if (target == position || peep_word(reference,4) <= 0 ||
            peep_owner(reference) == owner || (peeps[reference*22]&0x80) ||
            target < 0 || target >= 4096) {
            core_get_heading(person);
            reference = peep_reference(person);
            target = peep_word(reference,8);
        }
        /* The exact selector intentionally admits all signed word positions.
         * Validate once here, without retrying or changing selector semantics. */
        if (target < 0 || target >= 4096) return 999;
    } else {
        target = core_target(owner);
        if (!present) {
            int leader = core_leader(owner)-1;
            if (!core_leader(owner) && position == target) {
                core_leader(owner) = person+1;
                if (!view_who) view_who = person+1;
            }
            /* Invalid/dead/foreign leaders use the magnet without repair. */
            if (leader >= 0 && leader < no_peeps && leader < 208 && leader != person &&
                peep_word(leader,4) > 0 && peep_owner(leader) == owner &&
                peep_word(leader,8) >= 0 && peep_word(leader,8) < 4096)
                target = peep_word(leader,8);
        } else if (target < 0 || target >= 4096) return 999;
    }
    int dx = (target%64 > position%64)-(target%64 < position%64);
    int dy = (target/64 > position/64)-(target/64 < position/64);
    static const int to_delta[9] = {7,6,5,0,0,4,1,2,3};
    static const int directions[8] = {-64,-63,1,65,64,63,-1,-65};
    static const int opposite[8] = {4,5,6,7,0,1,2,3};
    int primary = to_delta[(dx+1)*3+dy+1], delta = directions[primary];
    int valid = core_valid_move(position,delta);
    if ((!valid && (!present || map_blk[position+delta] != 53)) ||
        (valid == 2 && A4_WORD(0xae80))) {
        peeps[person*22+21] = (uint8_t)delta;
        return delta;
    }
    if (valid == 3 && reference != -2)
        core_queue_terrain_raise(owner,position);
    /* asm:5328-5383 queues the rejected primary swamp, then tries fallback.
     * Retain host geometric safety and the bounded opaque-reference exclusion. */
    if (valid == 0 && reference != -2 && map_blk[position+delta] == 53 &&
        !A4_WORD(0xae80) && !(game_mode&GAME_MODE_BUILD_ONLY_UP))
        core_queue_terrain_raise(owner,position+delta);
    int direction = primary-1;
    for (int attempt = 0; attempt < 8; ++attempt, ++direction) {
        if (direction < 0) direction = 7;
        if (direction > 7) direction = 0;
        int candidate = directions[direction];
        if (!core_valid_move(position,candidate) && candidate != (int8_t)peeps[person*22+21] &&
            (!present || map_blk[position+candidate] != 53)) {
            peeps[person*22+21] = (uint8_t)directions[opposite[direction]];
            return candidate;
        }
    }
    /* asm:5445-5461 increments AFTER rejection, without a final wrap. Index 8
     * reads adjacent _palette[0] == 0; reproduce that north choice safely. */
    peeps[person*22+21] = (uint8_t)directions[direction == 8 ? 0 : opposite[direction]];
    return 999;
}
/* _battle_over asm:6207-6564. Casualties and winner selection stay in core_battle. */
static int core_battle_over(int winner, int loser, bool raw_loser_nonzero)
{
    if (no_peeps < 0 || no_peeps > 208 || winner < 0 || winner >= no_peeps ||
        loser < 0 || loser >= no_peeps || winner == loser ||
        peep_word(winner,4) <= 0 || peep_word(loser,4) > 0) return 0;
    int owner=peep_owner(winner), enemy=peep_owner(loser);
    int position=peep_word(winner,8), loser_position=peep_word(loser,8);
    if (owner > 1 || enemy > 1 || owner == enemy || position < 0 || position >= 4096 ||
        loser_position < 0 || loser_position >= 4096) return 0;
    uint32_t winner_heading, loser_heading;
    memcpy(&winner_heading,peeps+winner*22+14,sizeof winner_heading);
    memcpy(&loser_heading,peeps+loser*22+14,sizeof loser_heading);
    int32_t reward=A4_WORD(loser_heading ? 0x3012 : core_leader(enemy)==loser+1 ? 0x3014 : 0x3010);
    A4_WORD(0xae7a+owner*2)=(uint16_t)((uint16_t)A4_WORD(0xae7a+owner*2)+1u);
    if (peeps[loser*22]&1) {
        peeps[loser*22]=1;
        /* _do_battle's preliminary combat frames are overwritten here and by
         * winner frame 85. Exact-zero losers skip both frames and life scratch;
         * negative host casualties must still reframe before any terrain edit. */
        if (raw_loser_nonzero) core_frame_town(loser,core_check_life(enemy,loser_position));
        int level=(int16_t)(peep_word(loser,12)-32);
        reward=level>=0 && level<=10 ? A4_WORD(0x2ffa+level*2) : 100;
        if (winner_heading) {
            peeps[loser*22]=0x80; set_peep_word(loser,4,1); set_peep_word(loser,6,40);
            peeps[winner*22]=2;
            int city=map_bk2[loser_position]==42;
            for (int index=0; index<(city ? 25 : 17); ++index) {
                int cell=loser_position+core_offsets[index];
                /* Source city loop has no valid_move gate: retain row wrap,
                 * guarding only accesses outside the host's flat map array. */
                if (city ? (cell<0 || cell>=4096) : core_valid_move(loser_position,core_offsets[index])!=0) continue;
                if (city && index>0 && index<9 && map_bk2[cell]>=41 && map_bk2[cell]<=44)
                    map_bk2[cell]+=21;
                if (map_blk[cell]==31+enemy) map_blk[cell]=66;
            }
            if (map_bk2[loser_position]>=32 && map_bk2[loser_position]<=42) map_bk2[loser_position]+=21;
            if (map_who[loser_position]==loser+1) map_who[loser_position]=0;
        } else {
            core_set_town(loser,1);
            int capacity=core_check_life(owner,position);
            if (capacity>0) {
                /* Prevent later loser cleanup from erasing the new town. */
                peeps[loser*22]=8; peeps[winner*22]=1;
                set_peep_word(winner,6,game_turn);
                core_frame_town(winner,capacity); core_set_town(winner,0);
            }
        }
    }
    /* Normalize before core_remove so reciprocal host cleanup cannot undo it. */
    peeps[winner*22]=((peeps[winner*22]&1) ? 1 : 2)|4;
    set_peep_word(winner,10,0); set_peep_word(winner,12,85);
    if (!map_who[position] || map_who[position]==loser+1) map_who[position]=winner+1;
    if (peeps[winner*22+20]) {
        A4_WORD(0x82d4+owner*46)=position;
        A4_WORD(0x82d2+owner*46)=2;
        --peeps[winner*22+20];
    }
    if (peeps[loser*22+20]) A4_WORD(0x82d2+enemy*46)=0;
    /* ADD.L/SUB.L wrap before the signed loser-only comparison, not core_add_mana. */
    A4_LONG(0x9a02+owner*16)+=(uint32_t)reward;
    A4_LONG(0x9a02+enemy*16)-=(uint32_t)reward;
    if ((A4_LONG(0x9a02+enemy*16)^0x80000000u) < (A4_LONG(0x8486)^0x80000000u))
        A4_LONG(0x9a02+enemy*16)=A4_LONG(0x8486);
    if (view_who==loser+1) view_who=winner+1;
    if (peep_word(loser,4)<=0) core_remove(loser);
    return 1;
}
/* _set_frame asm:5759-5809: actual combat partner, opaque heading presence.
 * The caller preserves the pre-clamp population test; water has source priority. */
static int core_frame_battle(int person, int partner, bool raw_nonzero)
{
    if (no_peeps < 0 || no_peeps > 208 || person < 0 || person >= no_peeps ||
        partner < 0 || partner >= no_peeps || person == partner || !raw_nonzero ||
        !(peeps[person*22]&8) || (peeps[person*22]&0x10)) return 0;
    uint32_t heading, partner_heading;
    memcpy(&heading,peeps+person*22+14,sizeof heading);
    memcpy(&partner_heading,peeps+partner*22+14,sizeof partner_heading);
    int base=heading && partner_heading ? 138 : !heading && !partner_heading ? 70 :
        peeps[(heading ? person : partner)*22+1]==0 ? 130 : 134;
    int frame=(int16_t)((uint16_t)peep_word(person,12)+1u);
    int reset=frame < base || frame >= base+3;
    set_peep_word(person,12,reset ? base : frame);
    return reset;
}
static void core_battle(int first, int second)
{
    /* _do_battle:6082-6158: both casualties use the lesser rolled strength. */
    int second_strength = (___newrand()%3+1)*peep_word(second,4);
    int first_strength = (___newrand()%3+1)*peep_word(first,4);
    int lesser = first_strength < second_strength ? first_strength : second_strength;
    /* SUB.W wraps before TST.W and before either raw-nonzero frame test.
     * Keep host storage normalized without losing the signed source residual. */
    uint16_t first_bits = (uint16_t)((uint32_t)peep_word(first,4)-
        ((uint32_t)(lesser/100)*peeps[second*22+3]+10u));
    uint16_t second_bits = (uint16_t)((uint32_t)peep_word(second,4)-
        ((uint32_t)(lesser/100)*peeps[first*22+3]+10u));
    int first_pop = first_bits <= INT16_MAX ? (int)first_bits : (int)first_bits-65536;
    int second_pop = second_bits <= INT16_MAX ? (int)second_bits : (int)second_bits-65536;
    set_peep_word(first,4,first_pop > 0 ? first_pop : 0);
    set_peep_word(second,4,second_pop > 0 ? second_pop : 0);
    core_frame_battle(first,second,first_pop!=0);
    core_frame_battle(second,first,second_pop!=0);
    if (first_pop > 0 && second_pop > 0) return;
    if (first_pop <= 0 && second_pop <= 0) { core_remove(first); core_remove(second); return; }
    int winner = first_pop > 0 ? first : second, loser = winner == first ? second : first;
    bool raw_loser_nonzero=(loser==first ? first_pop : second_pop)!=0;
    core_battle_over(winner,loser,raw_loser_nonzero);
}
/* _join_battle asm:6663-6765. Keep the pair's combat state intact. */
static void core_join_battle(int newcomer, int encountered)
{
    if (newcomer < 0 || newcomer >= no_peeps || newcomer >= 208 ||
        encountered < 0 || encountered >= no_peeps || encountered >= 208 ||
        newcomer == encountered || peep_word(newcomer,4) <= 0 ||
        peeps[newcomer*22] != 2 || peep_owner(newcomer) > 1 ||
        peep_word(encountered,4) <= 0 || !(peeps[encountered*22]&8) ||
        peep_owner(encountered) > 1) return;
    int partner = peep_word(encountered,6);
    if (partner < 0 || partner >= no_peeps || partner >= 208 ||
        partner == encountered || partner == newcomer ||
        peep_word(partner,4) <= 0 || !(peeps[partner*22]&8) ||
        peep_word(partner,6) != encountered ||
        peep_owner(partner) != 1-peep_owner(encountered)) return;
    int owner = peep_owner(newcomer);
    int recipient = peep_owner(encountered) == owner ? encountered : partner;
    int population = peep_word(recipient,4)+peep_word(newcomer,4);
    set_peep_word(recipient,4,population > 32000 ? 32000 : population);
    if (core_leader(owner) == newcomer+1) core_leader(owner) = recipient+1;
    if (view_who == newcomer+1) view_who = recipient+1;
    /* This is an opaque four-byte Amiga field, not a native host pointer. */
    uint32_t metadata;
    memcpy(&metadata,peeps+newcomer*22+14,sizeof metadata);
    if (metadata) memcpy(peeps+recipient*22+14,&metadata,sizeof metadata);
    if (peeps[newcomer*22+3] > peeps[recipient*22+3])
        peeps[recipient*22+3] = peeps[newcomer*22+3];
    /* asm:6725-6740: a not-yet-visited recipient will count the merged word. */
    if (recipient > newcomer)
        core_good_pop(peeps[newcomer*22+1]) -= (uint32_t)(int32_t)peep_word(newcomer,4);
    core_remove(newcomer);
}
/* _join_forces asm:5540-5640: ordinary friendly merge, not battle joining. */
static void core_join_forces(int source, int target)
{
    if (source < 0 || source >= no_peeps || source >= 208 ||
        target < 0 || target >= no_peeps || target >= 208 || source == target ||
        peep_word(source,4) <= 0 || peeps[source*22] != 2 ||
        peep_owner(source) > 1 || peep_word(target,4) <= 0 ||
        peep_owner(target) != peep_owner(source) || (peeps[target*22]&8)) return;
    /* Opaque Amiga metadata: copy four bytes, never dereference as a pointer. */
    uint32_t metadata;
    memcpy(&metadata,peeps+source*22+14,sizeof metadata);
    if (metadata && peeps[target*22] == 1) return;
    if (metadata) memcpy(peeps+target*22+14,&metadata,sizeof metadata);
    int population = peep_word(source,4)+peep_word(target,4);
    set_peep_word(target,4,population > 32000 ? 32000 : population);
    int owner = peep_owner(source);
    if (core_leader(owner) == source+1) core_leader(owner) = target+1;
    if (view_who == source+1) view_who = target+1;
    if (peeps[source*22+3] > peeps[target*22+3])
        peeps[target*22+3] = peeps[source*22+3];
    peeps[target*22] &= 0x9f;
    set_peep_word(target,12,0);
    /* asm:5605-5620: a not-yet-visited target will count the merged word. */
    if (target > source)
        core_good_pop(peeps[source*22+1]) -= (uint32_t)(int32_t)peep_word(source,4);
    /* Host cleanup clears only source-owned occupancy; transfer refs first. */
    core_remove(source);
}
static void core_move(int person)
{
    int owner = peep_owner(person), position = peep_word(person,8);
    int present = peep_reference(person) != -1;
    int use_magnet = present || A4_WORD(0xae80) || !core_tendency(owner);
    int delta = use_magnet ? core_magnet_direction(person) : core_explore(person);
    /* _move_explorer asm:4425-4437: entry does not change the frame. */
    if (delta == 999) {
        peeps[person*22] |= 0x40;
        set_peep_word(person,6,7);
        return;
    }
    peeps[person*22] &= 0xbf;
    /* asm:4438-4449: release the cell claimed by the previous departure. */
    int previous = (int16_t)(position-peep_word(person,10));
    if (previous >= 0 && previous < 4096 && map_who[previous] == person+1) map_who[previous] = 0;
    /* asm:4450-4509: a computer-controlled walker on block 0x42 asks for its
     * own cell to be lowered: command byte, x, ASR.W #6 row; no latch write. */
    int request = 0x82b6 + owner*46;
    if (A4_WORD(request+6) == 1 && map_blk[position] == 0x42 && !A4_WORD(request+8) &&
        !(game_mode&(GAME_MODE_NO_BUILD|GAME_MODE_BUILD_ONLY_UP))) {
        int16_t cell = (int16_t)position;
        A4_DISP(request)[0] = 2;
        A4_DISP(request+1)[0] = (uint8_t)(cell & 63);
        A4_DISP(request+2)[0] = (uint8_t)(cell >= 0 ? cell/64 : -((-cell+63)/64));
    }
    /* asm:4510-4559: the encounter is with the peep named at the walker's
     * own cell. Only -1, self, slot >= 208 and bit 7 are excluded. */
    int occupant = map_who[position]-1;
    if (occupant != -1 && occupant != person && occupant < 208 && !(peeps[occupant*22]&0x80)) {
        if (peeps[occupant*22]&8) core_join_battle(person,occupant);
        else if (peep_owner(occupant) == owner) core_join_forces(person,occupant);
        else {
            peeps[person*22] = 8;
            peeps[occupant*22] = (peeps[occupant*22]&7)|8;
            set_peep_word(person,6,occupant); set_peep_word(occupant,6,person);
            core_frame_battle(person,occupant,peep_word(person,4)!=0);
            core_frame_battle(occupant,person,peep_word(occupant,4)!=0);
            /* _set_battle asm:6604-6656 (its view_who store is a no-op):
             * release the defender's claims, then a town keeps its cell
             * while a walker is pulled back onto the attacker's cell. */
            int cell = peep_word(occupant,8);
            int left = (int16_t)(cell-peep_word(occupant,10));
            if (left >= 0 && left < 4096 && map_who[left] == occupant+1) map_who[left] = 0;
            if (cell >= 0 && cell < 4096 && map_who[cell] == occupant+1) map_who[cell] = 0;
            if (peeps[occupant*22]&1) {
                if (cell >= 0 && cell < 4096) map_who[cell] = person+1;
            } else {
                set_peep_word(occupant,8,position);
                map_who[position] = person+1;
            }
            ++core_battles;
            return;
        }
    }
    if (peep_word(person,4) <= 0) return;
    /* asm:4564-4589: settle on a zero heading without metadata. */
    if (!delta && !present) {
        if (!map_who[position]) map_who[position] = person+1;
        peeps[person*22] = 1; set_peep_word(person,6,game_turn);
        core_frame_town(person,core_check_life(owner,position)); core_set_town(person,0); ++core_settlements;
        return;
    }
    /* asm:4590-4617: count and claim the departure cell, then advance. The
     * destination is claimed only by the next departure. */
    A4_WORD(0xff88+position*2) = (uint16_t)A4_WORD(0xff88+position*2)+1;
    if (!map_who[position]) map_who[position] = person+1;
    set_peep_word(person,8,(int16_t)(position+delta)); set_peep_word(person,10,delta);
}
/* _move_peeps frame local -28 (asm:3331-3335): per-owner oldest town age. */
static uint16_t core_oldest_town_age[2];
/* _move_peeps frame local -24 (asm:3336-3340): per-owner best town capacity. */
static int16_t core_best_town_capacity[2];
/* _one_block_flat asm:9148-9310: queue one terrain command that would flatten
 * the walker's cell. Corner sums 4q+3 raise the low corner, 4q+1 lower the
 * high one; scan is x-major over the cell's four vertices. Writes _stats only. */
static void core_one_block_flat(int position, int owner)
{
    int block_stats = 0x82b6 + owner*46;
    if (A4_WORD(block_stats+8) || (int32_t)A4_LONG(0x9a02 + owner*16) < 20 ||
        (int16_t)A4_WORD(0x99fc + owner*16) > 50 || !(A4_WORD(block_stats+14)&1) ||
        (game_mode&GAME_MODE_NO_BUILD)) return;
    int column = position & 63, row = (int16_t)position >= 0 ? (int16_t)position/64 : -((-(int16_t)position+63)/64);
    int base = row*65 + column;
    int16_t sum = (int16_t)(A4_WORD(0xae86+base*2) + A4_WORD(0xae86+(base+1)*2) +
                            A4_WORD(0xae86+(base+66)*2) + A4_WORD(0xae86+(base+65)*2));
    if (sum == 1) return;
    int remainder = sum % 4, level = sum / 4; /* DIVS: truncating quotient/remainder */
    for (int x = column; x <= column+1; ++x) for (int y = row; y <= row+1; ++y) {
        int16_t height = A4_WORD(0xae86 + (y*65+x)*2);
        int command = remainder == 3 ? (height == level ? 1 : 0) :
                      remainder == 1 && height > level && !(game_mode&GAME_MODE_BUILD_ONLY_UP) ? 2 : 0;
        if (command) {
            A4_DISP(block_stats)[0] = command; A4_DISP(block_stats+1)[0] = x; A4_DISP(block_stats+2)[0] = y;
            A4_WORD(block_stats+8) = 1;
            return;
        }
    }
}
/* _move_peeps frame local -36 (asm:3331-3335 init $4e1f, asm:3738-3765):
 * per-owner youngest town age; -40 (asm:3640-3645): per-owner city count;
 * -32 (asm:3276-3289): good_castles*3+good_towns read at pass start. */
static uint16_t core_youngest_town_age[2];
static int16_t core_city_count[2], core_town_strength[2];
/* _a_flat asm:25174-25184 (A4+$9704): 81 signed (dx,dy) byte pairs. */
static const int8_t core_a_flat[162] = {
    -4,-4,-4,-3,-4,-2,-4,-1,-4,0,-4,1,-4,2,-4,3,-4,4,
    -3,-4,-3,-3,-3,-2,-3,-1,-3,0,-3,1,-3,2,-3,3,-3,4,
    4,-4,4,-3,4,-2,4,-1,4,0,4,1,4,2,4,3,4,4,
    3,-4,3,-3,3,-2,3,-1,3,0,3,1,3,2,3,3,3,4,
    -2,-4,-2,-3,-2,-2,-2,-1,-2,0,-2,1,-2,2,-2,3,-2,4,
    2,-4,2,-3,2,-2,2,-1,2,0,2,1,2,2,2,3,2,4,
    -1,-4,-1,-3,-1,-2,-1,-1,-1,0,-1,1,-1,2,-1,3,-1,4,
    1,-4,1,-3,1,-2,1,-1,1,0,1,1,1,2,1,3,1,4,
    0,-4,0,-3,0,-2,0,-1,0,0,0,1,0,2,0,3,0,4
};
/* _make_level's early exit (asm:9017-9031) returns D0 untouched, which still
 * holds the _stats element address A4+$82b6+owner*46; the town pass stores
 * its low byte in peep +20 (asm:3793-3797). These are the low bytes at the
 * disassembly base (A4 = $493ee: $516a4 and $516d2). The real value depends
 * on the load address and has not been measured on real hardware. It is not
 * just a flag: peep +20 is decremented in battle (asm:6477-6498), used as a
 * signed table index (asm:8689-8737) and compared with exactly 1 (asm:8771). */
#define MAKE_LEVEL_EXIT_BYTE_OWNER0 ((uint8_t)(0x516a4 + 0*46))
#define MAKE_LEVEL_EXIT_BYTE_OWNER1 ((uint8_t)(0x516a4 + 1*46))
/* _make_level asm:9017-9147: queue one raise/lower toward the town vertex
 * height over the 9x9 a_flat neighbourhood. Returns D0: 0 when a command was
 * queued, 1 when already level, else the exit byte above. */
static uint8_t core_make_level(int position, int owner)
{
    int s = 0x82b6 + owner*46;
    if (!(A4_WORD(s+14)&1) || (game_mode&GAME_MODE_NO_BUILD)) return owner ? MAKE_LEVEL_EXIT_BYTE_OWNER1 : MAKE_LEVEL_EXIT_BYTE_OWNER0;
    int16_t x = (int16_t)(position & 63), y = (int16_t)((int16_t)position >> 6);
    int16_t height = A4_WORD(0xae86 + (int16_t)(y*65 + x)*2);
    for (int index = 0; index < 162; index += 2) {
        int16_t cx = (int16_t)(core_a_flat[index] + x), cy = (int16_t)(core_a_flat[index+1] + y);
        if (cx < 0 || cx > 64 || cy < 0 || cy > 64) continue;
        /* ASL.W #6 + ADD.W: x == 64 or y == 64 reads past row/map, as the original. */
        int16_t cell = (int16_t)(cy*64 + cx);
        int command = 0;
        if (map_blk[cell] == 0x2f && !(game_mode&GAME_MODE_BUILD_ONLY_UP)) {
            ++map_blk[cell];
            command = 2;
        } else {
            int16_t diff = (int16_t)(height - A4_WORD(0xae86 + (int16_t)(cy*65 + cx)*2));
            if (diff > 0) command = 1;
            else if ((diff < 0 || map_blk[cell] == 0x42 || map_blk[cell] == 0x35) &&
                     !(game_mode&GAME_MODE_BUILD_ONLY_UP)) command = 2;
        }
        if (command) {
            A4_DISP(s)[0] = (uint8_t)command;
            A4_DISP(s+1)[0] = (uint8_t)cx; A4_DISP(s+2)[0] = (uint8_t)cy;
            A4_WORD(s+8) = 1;
            return 0;
        }
    }
    return 1;
}
/* Host guard: the AI dereferences _magnet leaders and _stats pointers as
 * peep records without validation. Outside the 208-slot table the original
 * reads unrelated memory; the port then skips that AI routine for the pass. */
static int core_ai_slot(int slot) { return slot >= 0 && slot < 208; }
/* Signed CMP.L of mana against (threshold + bonus), ADD.L wrapping. */
static int core_ai_mana_above(int owner, int threshold, uint32_t bonus)
{
    return (int32_t)A4_LONG(0x9a02+owner*16) > (int32_t)(A4_LONG(threshold)+bonus);
}
static void core_ai_put(int s, int command, int16_t cell)
{
    A4_DISP(s)[0] = (uint8_t)command;
    A4_DISP(s+1)[0] = (uint8_t)(cell & 63);
    A4_DISP(s+2)[0] = (uint8_t)(cell >> 6); /* ASR.W #6, low byte */
}
/* _do_computer_effect asm:9526-9600: aim at the computer's own magnet cell
 * when it has NO leader (TST.W _magnet; BNE LAB_450B8, asm:9530-9536) and an
 * enemy stands there; else 3 cells above and left of the _stats+$26 peep
 * (slot+1), each clamped at zero only. */
static void core_do_computer_effect(int owner)
{
    int s = 0x82b6 + owner*46;
    if (!core_leader(owner)) {
        int16_t target = core_target(owner);
        int who = target >= 0 && target < 4096 ? map_who[target] : 0; /* host bound */
        if (who && peep_owner(who-1) != owner) {
            core_ai_put(s, A4_DISP(s)[0], target);
            return;
        }
    }
    int subject = (int)A4_LONG(s+0x26) - 1;
    int16_t cell = peep_word(subject,8);
    int16_t x = (int16_t)((cell & 63) - 3), y = (int16_t)((cell >> 6) - 3);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    A4_DISP(s+1)[0] = (uint8_t)x; A4_DISP(s+2)[0] = (uint8_t)y;
}
/* _devil_effect asm:9300-9525. Called per owner from _move_peeps asm:3266-3270
 * while the _stats+8 latch is clear. The permission word is copied to a local:
 * BTST on its high byte is bit 8 (war), on its low byte bits 0..7. */
static void core_devil_effect(int owner)
{
    int s = 0x82b6 + owner*46, enemy = owner ? 0 : 1;
    if (A4_WORD(s+8)) return;
    if ((core_leader(owner) && !core_ai_slot(core_leader(owner)-1)) ||
        (core_leader(enemy) && !core_ai_slot(core_leader(enemy)-1)) ||
        A4_LONG(s+0x26) > 208) return;
    uint16_t flags = (uint16_t)A4_WORD(s+14);
    /* asm:9317-9349: armageddon when richer by >999 and more populous. */
    if ((flags&0x100) && core_ai_mana_above(owner,0x84a6,999) &&
        (int32_t)A4_LONG(0x99fe + owner*16) > (int32_t)A4_LONG(0x99fe + enemy*16)) {
        A4_DISP(s)[0] = 14; A4_DISP(s+2)[0] = 3; A4_WORD(s+8) = 1;
        return;
    }
    /* asm:9350-9365: flood. */
    if ((flags&0x80) && core_ai_mana_above(owner,0x84a2,1999)) {
        A4_DISP(s)[0] = 14; A4_DISP(s+2)[0] = 4; A4_WORD(s+8) = 1;
        return;
    }
    /* asm:9366-9396: knight for a leader above 3000; returns either way. */
    if ((flags&0x20) && core_leader(owner) && peep_word(core_leader(owner)-1,4) > 3000) {
        if (core_ai_mana_above(owner,0x849a,500)) {
            A4_DISP(s)[0] = 14; A4_DISP(s+2)[0] = 5; A4_WORD(s+8) = 1;
        }
        return;
    }
    if (!A4_LONG(s+0x26)) return;
    /* asm:9398-9419: volcano. */
    if (core_ai_mana_above(owner,0x849e,500) && (flags&0x40)) {
        A4_DISP(s)[0] = 6;
        core_do_computer_effect(owner);
        A4_WORD(s+8) = 1; A4_WORD(s+0x12) = 0;
        return;
    }
    /* asm:9420-9482: swamp on the enemy leader; no latch write. */
    if (core_ai_mana_above(owner,0x8496,500) && (flags&0x10)) {
        int16_t count = A4_WORD(s+0x12);
        if (!(count < A4_WORD(s+0x1a) && (flags&8)) && !(count > A4_WORD(s+0x18) && (flags&0x40))) {
            int16_t theirs = (int16_t)(core_leader(enemy)-1), ours = (int16_t)(core_leader(owner)-1);
            if (theirs != -1 && (ours == -1 || peep_word(theirs,4) > peep_word(ours,4))) {
                A4_WORD(s+0x12) = (int16_t)(count+1);
                core_ai_put(s, 4, peep_word(theirs,8));
                return;
            }
        }
    }
    /* asm:9483-9524: earthquake on the target town. */
    if (core_ai_mana_above(owner,0x8492,500) && peeps[((int)A4_LONG(s+0x26)-1)*22] == 1 && (flags&8)) {
        if (A4_WORD(s+0x12) >= A4_WORD(s+0x1a) && (flags&0x50)) return;
        A4_DISP(s)[0] = 3;
        core_do_computer_effect(owner);
        A4_WORD(s+8) = 1;
        A4_WORD(s+0x12) = (int16_t)(A4_WORD(s+0x12)+1);
    }
}
static void core_ai_tendency(int s, uint8_t value)
{
    A4_DISP(s)[0] = 14; A4_DISP(s+2)[0] = 1; A4_DISP(s+1)[0] = value; A4_WORD(s+8) = 1;
}
/* _stats+$22/$2a are Amiga pointers (slot+1 here). With none, the original
 * dereferences address 8 (an OS exception vector); that compare is not
 * modelled: the magnet branch is skipped and the fallback runs. */
static int core_ai_town_cell(int s, int field, int16_t *cell)
{
    uint32_t reference = A4_LONG(s+field);
    if (!reference || reference > 208) return 0;
    *cell = peep_word((int)reference-1,8);
    return 1;
}
/* _set_devil_magnet asm:9601-9800. Called per owner from _move_peeps
 * asm:3257-3261 while the latch is clear. One newrand draw on the early
 * tendency path only (asm:9628-9633). */
static void core_set_devil_magnet(int owner)
{
    int s = 0x82b6 + owner*46, enemy = owner ? 0 : 1;
    int16_t towns = (int16_t)(A4_WORD(s+0x16)+A4_WORD(s+0x14));
    int16_t level = A4_WORD(s+0xc);
    if (towns < (int16_t)(level*2+15) ||
        (uint16_t)((uint16_t)game_turn % 90u) < (uint16_t)(level+10)) {
        if (!core_tendency(owner)) {
            A4_DISP(s)[0] = 14; A4_DISP(s+2)[0] = 1;
            A4_DISP(s+1)[0] = (uint8_t)((int16_t)___newrand()%3 + 1);
            A4_WORD(s+8) = 1;
        }
        return;
    }
    int theirs = core_leader(enemy) ? core_leader(enemy)-1 : -1;
    int ours = core_leader(owner) ? core_leader(owner)-1 : -1;
    if ((theirs != -1 && !core_ai_slot(theirs)) || (ours != -1 && !core_ai_slot(ours))) return;
    if (ours < 0) {
        if (core_tendency(owner)) { core_ai_tendency(s,0); A4_WORD(s+0x1c) = 0; }
        return;
    }
    if (peep_word(ours,8) == A4_WORD(s+0x1e) && A4_WORD(s+0x1c))
        A4_WORD(s+0x1c) = (int16_t)(A4_WORD(s+0x1c)-1);
    int16_t cell;
    if (peep_word(ours,4) < 6000) {
        if (A4_WORD(s+0x1c)) {
            if (core_target(owner) != A4_WORD(s+0x1e)) {
                core_ai_put(s,5,A4_WORD(s+0x1e)); A4_WORD(s+8) = 1;
                return;
            }
        } else if (core_ai_town_cell(s,0x2a,&cell) && core_target(owner) != cell) {
            core_ai_put(s,5,cell); A4_WORD(s+0x1e) = cell; A4_WORD(s+0x1c) = 2; A4_WORD(s+8) = 1;
            return;
        }
        if (core_tendency(owner)) core_ai_tendency(s,0);
        return;
    }
    /* asm:9784-9864: a leader 500 above the enemy's, enemy tendency 0. */
    if (theirs >= 0 && peep_word(ours,4) > (int16_t)(peep_word(theirs,4)+500) && !core_tendency(enemy)) {
        if (A4_WORD(s+14)&4) {
            if (core_tendency(owner)) core_ai_tendency(s,0);
            else if (core_target(owner) != core_target(enemy)) {
                core_ai_put(s,5,core_target(enemy)); A4_WORD(s+8) = 1;
            }
        }
        return;
    }
    /* asm:9865-9898: magnet onto the enemy's best town. */
    if (A4_WORD(s+14)&2) {
        if (core_ai_town_cell(s,0x22,&cell) && core_target(owner) != cell && !A4_WORD(s+0x1c)) {
            core_ai_put(s,5,cell); A4_WORD(s+0x1e) = cell; A4_WORD(s+0x1c) = 2; A4_WORD(s+8) = 1;
        } else if (core_tendency(owner)) core_ai_tendency(s,0);
    }
}
static void core_settle_tick(int person)
{
    int owner = peep_owner(person), position = peep_word(person,8), capacity = core_check_life(owner,position);
    /* asm:3600-3633: caller dispatches exact peaceful towns. Metadata is an
     * opaque four-byte Amiga field, never a native pointer. */
    uint32_t metadata;
    memcpy(&metadata,peeps+person*22+14,sizeof metadata);
    if (capacity <= 0 || metadata || A4_WORD(0xae80)) {
        peeps[person*22] = (peeps[person*22]&0xfe)|2;
        set_peep_word(person,12,0); set_peep_word(person,10,0);
        core_set_town(person,1);
        return;
    }
    int old_frame = peep_word(person,12); /* asm:3634-3636, before reframing. */
    core_frame_town(person,capacity);
    /* asm:3658-3661: LAB_52DEA counts non-city frames during the pass. */
    if (capacity < 3050) A4_WORD(0x99fc+owner*16) = (uint16_t)A4_WORD(0x99fc+owner*16)+1;
    else if (owner < 2) ++core_city_count[owner]; /* asm:3637-3645 */
    /* asm:3667-3677: reclaim a departed walker's slot, never another occupant. */
    if (!map_who[position]) map_who[position] = person+1;
    /* asm:3684-3705: a capacity strictly above the pass maximum (signed
     * word, BGE skips ties) becomes the enemy-indexed _stats+0x22 pointer. */
    if (owner < 2 && !(core_best_town_capacity[owner] >= (int16_t)capacity)) {
        core_best_town_capacity[owner] = (int16_t)capacity;
        A4_LONG(0x82d8+(owner ? 0 : 1)*46) = (uint32_t)(person+1);
    }
    /* asm:3706-3737: the oldest-so-far town (unsigned, ties replace) becomes
     * the enemy-indexed _stats+0x26 pointer (host slot+1 encoding). */
    uint16_t age = (uint16_t)((uint16_t)game_turn-(uint16_t)peep_word(person,6));
    if (owner < 2 && !(core_oldest_town_age[owner] > age)) {
        core_oldest_town_age[owner] = age;
        A4_LONG(0x82dc+(owner ? 0 : 1)*46) = (uint32_t)(person+1);
    }
    /* asm:3738-3765: the strictly youngest town (unsigned) is this owner's
     * own _stats+0x2a pointer. */
    if (owner < 2 && core_youngest_town_age[owner] > age) {
        core_youngest_town_age[owner] = age;
        A4_LONG(0x82e0+owner*46) = (uint32_t)(person+1);
    }
    /* asm:3766-3817: town levelling while this owner's latch is clear. Flat
     * cells keep the result in peep +20 and skip an unchanged, unflagged frame. */
    if (owner < 2 && !A4_WORD(0x82be + owner*46)) {
        if (!map_alt[position]) {
            if (!peeps[person*22+20] || peep_word(person,12) != old_frame || A4_WORD(0x2fb2))
                peeps[person*22+20] = core_make_level(position,owner);
        } else if (core_town_strength[owner] < 3 && (uint16_t)game_turn > 250)
            core_make_level(position,owner);
    }
    if (!((uint16_t)game_turn&7)) {
        int level = peep_word(person,12)-32;
        /* asm:3818-3862: local split limits, never reframe or reindex economy. */
        if (peep_word(person,12) == 42 && A4_WORD(0x82bc+owner*46) == 1 &&
            peep_word(person,4) > 305 && !A4_WORD(0x82be + owner*46)) {
            capacity = 305;
            A4_WORD(0x82be + owner*46) = 1;
        }
        uint16_t cheat = (uint16_t)A4_WORD(0x2fb4);
        if (cheat && (uint16_t)owner == (uint16_t)(cheat-1u)) capacity = 50;
        /* asm:3863-3882: EXT.L signed word then ADD.L, without host mana bounds. */
        A4_LONG(0x9a02+owner*16)+=(uint32_t)(int32_t)(int16_t)A4_WORD(0x2fb6+level*2);
        peeps[person*22+3] = A4_WORD(0x2fcc+level*2)&255;
        if (peep_word(person,4) > capacity) {
            int child;
            for (child = 0; child < 208 && peep_word(child,4)>0; ++child) {}
            if (child < 208) {
                if (child >= no_peeps) no_peeps = child+1;
                /* asm:3950-3963 leaves this slot's counter and nav history intact. */
                set_peep_word(child,10,0); set_peep_word(child,12,0);
                memset(peeps+child*22+14,0,4); peeps[child*22+20] = 0;
                peeps[child*22] = 2; peeps[child*22+1] = owner;
                peeps[child*22+2] = peeps[person*22+2]; peeps[child*22+3] = peeps[person*22+3];
                set_peep_word(child,4,peep_word(person,4)-capacity/2); set_peep_word(person,4,capacity/2);
                /* asm:3913-3929: the released walker carries the leader. */
                if (core_leader(owner) == person+1) core_leader(owner) = child+1;
                if (view_who == person+1) view_who = child+1; /* asm:3931-3937 */
                set_peep_word(child,8,position); set_peep_word(child,18,position);
                /* asm:3964-3983: preserve a co-located walker or battle occupant. */
                if (!map_who[position] || map_who[position] == person+1) map_who[position] = child+1;
                if (peeps[person*22+2] < 4) ++peeps[person*22+2];
            }
        }
        /* asm:4007-4014: ADD.W can revive or kill; store before maintenance. */
        set_peep_word(person,4,(uint16_t)((uint16_t)peep_word(person,4)+(uint16_t)A4_WORD(0x2fe2+level*2)));
    }
    /* asm:4037-4055: use this resident's life-scan scratch after economy.
     * Evacuation bypasses both maintenance and signed cleanup (asm:3633). */
    int frame = peep_word(person,12);
    if (frame != old_frame || A4_WORD(0x2fb2) || (frame == 42 && (int16_t)A4_WORD(0x2ff8) < 9))
        core_set_town(person,0);
    if (peep_word(person,4)<=0) core_remove(person);
}
/* _raise_point/_lower_point: the same 8-neighbour slope constraint, bounded work. */
static int core_sculpt(int owner, int column, int row, int direction)
{
    if (owner < 0 || owner > 1 || pause || core_result || A4_WORD(0xae80) || column < 0 || column > 64 || row < 0 || row > 64 ||
        (direction != 1 && direction != -1) || (game_mode&GAME_MODE_NO_BUILD) ||
        (direction < 0 && (game_mode&GAME_MODE_BUILD_ONLY_UP)) || core_mana(owner) < 10 ||
        !(A4_WORD(0x82c4+owner*46)&1)) return 0;
    int original = core_vertex(column,row), target_height = original+direction;
    if (target_height < 0 || target_height > 8) return 0;
    if ((game_mode&GAME_MODE_BUILD_NEAR_TOWNS) && original > 0) {
        int near = 0;
        for (int person = 0; person < no_peeps; ++person) {
            int position = peep_word(person,8);
            if (peep_word(person,4)>0 && peep_owner(person)==owner && (peeps[person*22]&1) &&
                abs(position%64-column)<=2 && abs(position/64-row)<=2) near = 1;
        }
        if (!near) return 0;
    }
    int changed = 0, xmin = column, xmax = column, ymin = row, ymax = row;
    for (int y = 0; y <= 64; ++y) for (int x = 0; x <= 64; ++x) {
        int distance = abs(x-column)>abs(y-row) ? abs(x-column) : abs(y-row);
        int height = core_vertex(x,y), bound = target_height-direction*distance;
        if ((direction > 0 && height < bound) || (direction < 0 && height > bound)) {
            changed += abs(bound-height); core_set_vertex(x,y,bound);
            if (x < xmin) xmin = x;
            if (x > xmax) xmax = x;
            if (y < ymin) ymin = y;
            if (y > ymax) ymax = y;
        }
    }
    core_add_mana(owner,-(10+changed*4));
    /* Original map-update bounds include the cells on both sides of changed
     * vertices; remote terrain and navigation history must remain untouched. */
    core_make_map_region(xmin > 0 ? xmin-1 : 0, ymin > 0 ? ymin-1 : 0,
                         xmax < 64 ? xmax : 63, ymax < 64 ? ymax : 63);
    /* Host footprint refresh. A town in battle reaches only _do_battle
     * (_move_peeps asm:4269-4281); _sculpt/_make_map (asm:7271-7420,
     * 1420-1593) never call _set_town. */
    for (int person = 0; person < no_peeps; ++person)
        if (peep_word(person,4)>0 && (peeps[person*22]&1) && !(peeps[person*22]&8))
            core_set_town(person,0);
    return 1;
}

static void _place_first_people(void)
{
    memset(peeps, 0, 208 * 22);
    memset(map_who, 0, 4096);
    no_peeps = current_start_pop_you + current_start_pop_enemy;
    /* Explicit synthetic, legal flat starting plots, not original level maps. */
    for (int index = 0; index < no_peeps; ++index) {
        int owner = index >= current_start_pop_you;
        int local = owner ? index-current_start_pop_you : index;
        int position = (8+(local/5)*7)*64 + 8+(local%5)*8+owner*3;
        peeps[index * 22] = 2;
        peeps[index * 22 + 1] = index >= current_start_pop_you;
        peeps[index * 22 + 2] = 1;
        peeps[index * 22 + 3] = 1;
        set_peep_word(index, 4, 45);
        set_peep_word(index, 8, position);
        set_peep_word(index, 12, 255);
        map_who[position] = index + 1;
    }
}

/* Raw raise for generation and powers: no mana or settlement restrictions.
 * asm:1274-1416; discard out-of-map neighbours before reading their heights. */
static int original_raise_point(int column, int row)
{
    if (column < 0 || column > 64 || row < 0 || row > 64) return 0;
    int height = core_vertex(column, row);
    if (height >= 8) return height;
    core_set_vertex(column, row, ++height);
    A4_WORD(0x2f88) = (int16_t)((uint16_t)A4_WORD(0x2f88) + 1u);
    static const int neighbours[8][2] = {
        {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}
    };
    for (int index = 0; index < 8; ++index) {
        int next_column = column + neighbours[index][0];
        int next_row = row + neighbours[index][1];
        if (next_column >= 0 && next_column <= 64 && next_row >= 0 && next_row <= 64 &&
            height - core_vertex(next_column, next_row) > 1)
            original_raise_point(next_column, next_row);
    }
    if (column < A4_WORD(0x2f8a)) A4_WORD(0x2f8a) = column;
    if (column > A4_WORD(0x2f8c)) A4_WORD(0x2f8c) = column;
    if (row < A4_WORD(0x2f8e)) A4_WORD(0x2f8e) = row;
    if (row > A4_WORD(0x2f90)) A4_WORD(0x2f90) = row;
    return core_vertex(column, row);
}

/* _do_raise_point asm:7180-7246: _paint_map (asm:7189,7206) or _war skips
 * both the mana admission and the payment. */
static int core_raw_raise(int owner, int column, int row)
{
    if (owner < 0 || owner > 1 || column < 0 || column > 64 || row < 0 || row > 64)
        return 0;
    /* Convert the signed long comparison without out-of-range signed casts. */
    uint32_t mana = A4_LONG(0x9a02 + owner * 16), threshold = A4_LONG(0x848a);
    if (!paint_map && !A4_WORD(0xae80) && (mana ^ 0x80000000u) < (threshold ^ 0x80000000u)) return 0;
    A4_WORD(0x2f8a) = A4_WORD(0x2f8c) = column;
    A4_WORD(0x2f8e) = A4_WORD(0x2f90) = row;
    A4_WORD(0x2f88) = 0;
    original_raise_point(column, row);
    if (!paint_map && !A4_WORD(0xae80)) {
        unsigned word_cost = ((uint16_t)A4_WORD(0x2f88) * 4u + 10u) & 0xffffu;
        int32_t cost = word_cost < 0x8000u ? (int32_t)word_cost : (int32_t)word_cost - 65536;
        /* SUB.L wraps in 32 bits. Unlike core_sculpt/core_add_mana, no host cap. */
        A4_LONG(0x9a02 + owner * 16) = mana - (uint32_t)cost;
    }
    if (A4_WORD(0x2f8a) < 1) A4_WORD(0x2f8a) = 1;
    if (A4_WORD(0x2f8c) > 63) A4_WORD(0x2f8c) = 63;
    if (A4_WORD(0x2f8e) < 1) A4_WORD(0x2f8e) = 1;
    if (A4_WORD(0x2f90) > 63) A4_WORD(0x2f90) = 63;
    core_make_map_region(A4_WORD(0x2f8a)-1, A4_WORD(0x2f8e)-1,
                         A4_WORD(0x2f8c), A4_WORD(0x2f90));
    return 1;
}

static int original_lower_point(int column, int row);
/* _do_lower_point asm:7091-7180: bounds before the mana gate; unlike raise,
 * war does not bypass it. Charge asm:7121-7126: ASL.W #2, ADD.W #10, then
 * EXT.L sign-extends the word before SUB.L. */
static void core_do_lower_point(int owner, int column, int row)
{
    A4_WORD(0x2f8a) = A4_WORD(0x2f8c) = column;
    A4_WORD(0x2f8e) = A4_WORD(0x2f90) = row;
    if ((int32_t)A4_LONG(0x9a02+owner*16) < (int32_t)A4_LONG(0x848a) && !paint_map) return;
    A4_WORD(0x2f88) = 0;
    original_lower_point(column,row);
    if (!paint_map && !A4_WORD(0xae80))
        A4_LONG(0x9a02+owner*16) -= (uint32_t)(int32_t)(int16_t)(uint16_t)((uint16_t)A4_WORD(0x2f88)*4u+10u);
    if ((int16_t)A4_WORD(0x2f8a) < 1) A4_WORD(0x2f8a) = 1;
    if ((int16_t)A4_WORD(0x2f8c) > 63) A4_WORD(0x2f8c) = 63;
    if ((int16_t)A4_WORD(0x2f8e) < 1) A4_WORD(0x2f8e) = 1;
    if ((int16_t)A4_WORD(0x2f90) > 63) A4_WORD(0x2f90) = 63;
    core_make_map_region(A4_WORD(0x2f8a)-1, A4_WORD(0x2f8e)-1, A4_WORD(0x2f8c), A4_WORD(0x2f90));
}
/* _place_people(side,x,y,leader) asm:6982-7087: a new leader replaces the
 * current one (_zero_population), else the next free slot. Byte map
 * coordinates past 63 would write outside _map_who; the port drops them. */
static void core_place_people(int side, int x, int y, int leader)
{
    if (no_peeps >= 208 || side < 0 || side > 1 || x < 0 || x > 63 || y < 0 || y > 63) return;
    int person, magnet = core_leader(side);
    if (leader && magnet > 0 && magnet <= no_peeps) { person = magnet-1; core_remove(person); }
    else person = no_peeps++;
    int cell = y*64+x;
    set_peep_word(person,6,0); set_peep_word(person,4,45); set_peep_word(person,8,cell);
    map_who[cell] = person+1;
    peeps[person*22+1] = (uint8_t)side; peeps[person*22] = 2;
    set_peep_word(person,12,255); peeps[person*22+2] = 1;
    set_peep_word(person,10,0); peeps[person*22+3] = 1;
    memset(peeps+person*22+14,0,4);
    if (leader) core_leader(side) = person+1;
    set_peep_word(person,12,0);   /* _set_frame on frame 255, asm:5728-5743 */
}
/* _rotate_all_map asm:6823-6922: "mirror the landscape". Both halves of
 * _alt take the higher (signed) height, trees 50..55 are copied across. */
static void core_rotate_all_map(void)
{
    for (int i = 0; i < 0x1080/2; ++i) {
        int mirror = 0xae86+(0x1080-i)*2;
        if (A4_WORD(0xae86+i*2) < A4_WORD(mirror)) A4_WORD(0xae86+i*2) = A4_WORD(mirror);
        A4_WORD(mirror) = A4_WORD(0xae86+i*2);
    }
    for (int i = 0; i < 0xfff/2; ++i) {
        if (map_bk2[i] >= 50 && map_bk2[i] <= 55) map_bk2[0xfff-i] = map_bk2[i];
        else if (map_bk2[0xfff-i] >= 50 && map_bk2[0xfff-i] <= 55) map_bk2[i] = map_bk2[0xfff-i];
    }
    _make_map();
}
/* _clear_all_map asm:6926-6978: flat sea, no objects, no people. */
static void core_clear_all_map(void)
{
    for (int i = 0; i < 0x1081; ++i) {
        A4_WORD(0xae86+i*2) = 0;
        if (i < 0x1000) map_who[i] = map_bk2[i] = map_blk[i] = 0;
    }
    for (int person = 0; person < no_peeps; ++person) core_remove(person);
    no_peeps = 0;
    _make_map();
}
static int core_load_ground(int terrain);
/* _paint_the_map asm:15123-15274, editor keys into the _player mailbox at
 * _cur_x/_cur_y (low bytes): F1/F2 man (Shift: leader) for either side,
 * F3 tree, F4 rock, F5 delete, F6/F7 mana (x = _shift), F8 mirror, DEL
 * clear, 1..9 terrain (16-(inkey-0xed))/2 = digit-1. raw is the Amiga key. */
static int core_paint_the_map(int raw, int shifted)
{
    if (!paint_map || player < 0 || player > 1) return 0;
    uint8_t *box = A4_DISP(0x82b6 + player*46);
    box[1] = (uint8_t)A4_WORD(0x3028); box[2] = (uint8_t)A4_WORD(0x3026);
    switch (raw) {
    case 0x50: box[0] = shifted ? 9 : 7; return 1;
    case 0x51: box[0] = shifted ? 10 : 8; return 1;
    case 0x52: box[0] = 11; return 1;
    case 0x53: box[0] = 12; return 1;
    case 0x54: box[0] = 13; return 1;
    case 0x55: box[0] = 14; box[1] = (uint8_t)shifted; box[2] = 9; return 1;
    case 0x56: box[0] = 14; box[1] = (uint8_t)shifted; box[2] = 10; return 1;
    case 0x57: box[0] = 14; box[2] = 11; return 1;
    case 0x46: box[0] = 14; box[2] = 12; return 1;
    default:
        if (raw >= 1 && raw <= 9) { box[0] = 14; box[1] = (uint8_t)(raw-1); box[2] = 13; return 1; }
        return 0;
    }
}
/* _get_message opcodes 11/12 asm:18049-18122: cycle a tree (bk2 50..52) or
 * rock (blk 47..49) on a cell below height 7, starting from the last one. */
static void core_paint_cycle(int x, int y, int rock)
{
    if (x < 0 || x > 63 || y < 0 || y > 63) return;
    int cell = y*64+x, low = rock ? 47 : 50, high = low+2;
    if (map_alt[cell] >= 7) return;
    uint8_t *slot = rock ? &map_blk[cell] : &map_bk2[cell];
    int value = *slot;
    if (value < low || value > high) value = A4_WORD(rock ? 0x825e : 0x825c);
    else if (value == high) value = low;
    else ++value;
    A4_WORD(rock ? 0x825e : 0x825c) = (int16_t)value;
    *slot = (uint8_t)value;
}
/* Opcode 13 asm:18123-18183: a rock becomes grass, then objects and the
 * occupant go. */
static void core_paint_delete(int x, int y)
{
    if (x < 0 || x > 63 || y < 0 || y > 63) return;
    int cell = y*64+x;
    if (map_blk[cell] >= 47 && map_blk[cell] <= 49) {
        map_blk[cell] = 15;
        core_make_map_region(x,y,x+1 > 63 ? 63 : x+1,y+1 > 63 ? 63 : y+1);
    }
    map_bk2[cell] = 0;
    if (map_who[cell]) {
        int person = map_who[cell]-1;
        if (person < no_peeps) core_remove(person);
        map_bk2[cell] = 0;
        map_who[cell] = 0;
    }
}
/* _do_action y=9/10 asm:18943-18976: x (= _shift) halves the blue/red mana
 * (DIVS), else below 100000 it doubles plus 500. */
static void core_paint_mana(int owner, int halve)
{
    int32_t mana = (int32_t)A4_LONG(0x9a02+owner*16);
    if (halve) mana /= 2;
    else if (mana < 100000) mana = mana*2+500;
    A4_LONG(0x9a02+owner*16) = (uint32_t)mana;
}
/* _clear_send asm:18234-18254: only command/x/y, never the controller state. */
static void core_clear_terrain_mailboxes(void)
{
    for (int owner = 0; owner < 2; ++owner) memset(A4_DISP(0x82b6 + owner * 46), 0, 3);
}

/* _get_message asm:17929-17995,18218-18229. No serial/editor/UI producers yet.
 * Cadence clears the latch independently of the opcode, including empty slots. */
static int core_quake(int owner, int column, int row, uint16_t (*random)(void));
static int core_swamp(int owner, int column, int row, uint16_t (*random)(void));
static int core_volcano(int owner, int column, int row, uint16_t (*random)(void));
static int core_war(int owner);
static int core_flood(int owner);
static int core_knight(int owner);
static int core_do_magnet(int owner, int x, int y);
static void core_consume_terrain_mailboxes(void)
{
    for (int owner = 0; owner < 2; ++owner) {
        int displacement = owner * 46;
        if (A4_WORD(0x82bc + displacement) == 1) {
            unsigned divisor = (uint16_t)A4_WORD(0x82c6 + displacement);
            if (!divisor) {
                fprintf(stderr, "Terrain mailbox owner %d: zero cadence divisor; command discarded, latch preserved\n", owner);
                continue;
            }
            if ((uint16_t)game_turn % divisor == 0) A4_WORD(0x82be + displacement) = 0;
        }
        int x = A4_DISP(0x82b7 + displacement)[0], y = A4_DISP(0x82b8 + displacement)[0];
        /* Jump table asm:18200-18216; 7..13 come from _paint_the_map. */
        switch (A4_DISP(0x82b6 + displacement)[0]) {
        case 1: core_raw_raise(owner, x, y); break;
        case 2: core_do_lower_point(owner, x, y); break;
        case 3: core_quake(owner, x, y, ___newrand); break;
        case 4: core_swamp(owner, x, y, ___newrand); break;
        case 5: core_do_magnet(owner, x, y); break;
        case 6: core_volcano(owner, x, y, ___newrand); break;
        case 7: core_place_people(0, x, y, 0); break;
        case 8: core_place_people(1, x, y, 0); break;
        case 9: core_place_people(0, x, y, 1); break;
        case 10: core_place_people(1, x, y, 1); break;
        case 11: core_paint_cycle(x, y, 0); break;
        case 12: core_paint_cycle(x, y, 1); break;
        case 13: core_paint_delete(x, y); break;
        case 14:
            /* _do_action asm:18406-19032, switch on y; these cases return 0. */
            if (y == 1) core_tendency(owner) = (int16_t)x; /* _set_tend_icons is UI only */
            else if (y == 3) core_war(owner);
            else if (y == 4) core_flood(owner);
            else if (y == 5) core_knight(owner);
            else if (y == 9) core_paint_mana(0, x);
            else if (y == 10) core_paint_mana(1, x);
            else if (y == 11) core_rotate_all_map();
            else if (y == 12) core_clear_all_map();
            else if (y == 13) core_load_ground(x);
            /* y=7 asm:18607-18650: not while the other side's mailbox also
             * holds options; our side shows _options(x ? player : not_player).
             * y=8 asm:18729-18733: _new_map = 0, _game_options. The serial
             * exchange that follows for two players is not ported. */
            else if (y == 7 && owner == player && !core_req.kind) {
                const uint8_t *other = A4_DISP(0x82b6 + (owner ? 0 : 1) * 46);
                if (!(other[2] == 7 && other[0] == 14)) core_options_open(x ? player : not_player);
            } else if (y == 8 && owner == player && !core_req.kind) {
                A4_WORD(0x309a) = 0; core_game_options_open();
            }
            break;
        default: break;
        }
    }
    core_clear_terrain_mailboxes();
}
/* _set_magnet_to asm:8271-8293. */
static void core_set_magnet_to(int owner, int16_t cell)
{
    if (pause) return;
    core_target(owner) = cell;
    A4_WORD(owner ? 0xae76 : 0xae78) = cell;
}
/* _do_magnet asm:6769-6820: signed CMP.L, leader and war gates, then
 * payment before the 0..63 clamps. */
static int core_do_magnet(int owner, int x, int y)
{
    if ((int32_t)A4_LONG(0x9a02+owner*16) < (int32_t)A4_LONG(0x848e) ||
        !core_leader(owner) || A4_WORD(0xae80)) return 0;
    A4_WORD(0x2fac) = 77;
    A4_LONG(0x9a02+owner*16) -= A4_LONG(0x848e);
    if (x >= 64) x = 63;
    if (x < 0) x = 0;
    if (y >= 64) y = 63;
    if (y < 0) y = 0;
    core_set_magnet_to(owner,(int16_t)(y*64+x));
    return 1;
}

/* _lower_point asm:2571-2713: ADDQ.W build_count, then SUBQ.W height.
 * Keep the source recursive neighbour order, but bounds-check before reads. */
static int original_lower_point(int column, int row)
{
    if (column < 0 || column > 64 || row < 0 || row > 64) return 0;
    int height = core_vertex(column, row);
    if (!height) return 0;
    A4_WORD(0x2f88) = (int16_t)((uint16_t)A4_WORD(0x2f88) + 1u);
    core_set_vertex(column, row, --height);
    static const int neighbours[8][2] = {
        {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}
    };
    for (int index = 0; index < 8; ++index) {
        int next_column = column + neighbours[index][0];
        int next_row = row + neighbours[index][1];
        if (next_column >= 0 && next_column <= 64 && next_row >= 0 && next_row <= 64 &&
            core_vertex(next_column, next_row) - height > 1)
            original_lower_point(next_column, next_row);
    }
    if (column < A4_WORD(0x2f8a)) A4_WORD(0x2f8a) = column;
    if (column > A4_WORD(0x2f8c)) A4_WORD(0x2f8c) = column;
    if (row < A4_WORD(0x2f8e)) A4_WORD(0x2f8e) = row;
    if (row > A4_WORD(0x2f90)) A4_WORD(0x2f90) = row;
    return core_vertex(column, row);
}

/* _do_flood asm:7567-7648: _paint_map (asm:7569) skips the mana, _war,
 * _pause and permission gates and the payment, not the score. The signed BLE guard leaves negative heights unchanged. This is not a
 * recursive lower/sculpt operation; water and town consequences wait for tick. */
static int core_flood(int owner)
{
    if (owner < 0 || owner > 1 || core_result) return 0;
    if (!paint_map) {
        if (pause || A4_WORD(0xae80) || core_mana(owner) < (int32_t)A4_LONG(0x84a2) ||
            !(A4_WORD(0x82c4+owner*46)&0x80)) return 0;
        A4_LONG(0x9a02+owner*16) -= A4_LONG(0x84a2);
    }
    if (owner == player) A4_LONG(0xae72) += 250;
    A4_WORD(0x2fac) = 74;
    for (int index = 0; index < 4225; ++index)
        if (A4_WORD(0xae86+index*2) > 0) --A4_WORD(0xae86+index*2);
    _make_map();
    return 1;
}

/* _do_knight asm:8392-8478: the leader test comes first; _paint_map
 * (asm:8404) then skips the mana, _war, _pause and permission gates and the
 * payment, not the score. */
static int core_knight(int owner)
{
    if (owner < 0 || owner > 1 || no_peeps < 0 || no_peeps > 208 ||
        core_result || prg_quit) return 0;
    if (!paint_map && (pause || A4_WORD(0xae80))) return 0;
    int leader=core_leader(owner);
    if (leader < 1 || leader > no_peeps) return 0;
    int subject=leader-1, position=peep_word(subject,8);
    if (peep_word(subject,4) <= 0 || peep_owner(subject) != owner ||
        position < 0 || position >= 4096) return 0;
    uint32_t mana=A4_LONG(0x9a02+owner*16), cost=A4_LONG(0x849a);
    /* Signed CMP.L admission, unsigned SUB.L/ADD.L wrap, as in core_war. */
    if (!paint_map) {
        if ((mana ^ 0x80000000u) < (cost ^ 0x80000000u) ||
            !(A4_WORD(0x82c4+owner*46)&0x20)) return 0;
        A4_LONG(0x9a02+owner*16)=mana-cost;
    }
    if (owner == player) A4_LONG(0xae72)+=150;
    peeps[subject*22+20]=1;
    core_get_heading(subject);
    core_target(owner)=position;
    A4_WORD(owner ? 0xae76 : 0xae78)=position;
    core_leader(owner)=0;
    A4_WORD(0x2fac)=69;
    return 1;
}

/* _do_war asm:8482-8530: _paint_map (asm:8484) jumps past the gates, the
 * payment and the score straight to the peep clear. */
static int core_war(int owner)
{
    if (owner < 0 || owner > 1 || no_peeps < 0 || no_peeps > 208 ||
        core_result || prg_quit) return 0;
    if (!paint_map) {
        uint32_t mana = A4_LONG(0x9a02+owner*16), cost = A4_LONG(0x84a6);
        /* Signed CMP.L without an out-of-range signed conversion. BTST targets
         * the high byte of the original big-endian permission word. */
        if (pause || (mana ^ 0x80000000u) < (cost ^ 0x80000000u) ||
            !(A4_WORD(0x82c4+owner*46)&0x100)) return 0;
        A4_LONG(0x9a02+owner*16) = mana-cost;
        if (owner == player) A4_LONG(0xae72) += 5000;
    }
    for (int person = 0; person < no_peeps; ++person)
        memset(peeps+person*22+14,0,4);
    A4_WORD(0x2fac) = 73;
    A4_WORD(0xae80) = 1;
    return 1;
}

typedef uint16_t (*WorldRandom)(void);
/* Sound bank (gmusic1 in play) and _curmeas as an offset into _measures. */
static AudioBank core_bank;
static uint32_t core_curmeas;
/* _kill_effect(from,to) asm:20876-20894: AbortIO on from..to-1. */
static void core_kill_effect(int from, int to) { for (int ch = from; ch < to; ++ch) audio_abort(ch); }
/* _check_effect asm:20898-20908: CheckIO, nonzero once the channel is idle. */
static int core_check_effect(int ch) { return !audio_busy(ch); }
/* _setsound asm:20390-20429: CMD_WRITE of sams[d+5]+offset, d+2 - offset
 * bytes, period d+0, volume d+4 (ADIOF_PERVOL), cycles. */
static void core_setsound(int ch, const AudioKit *d, uint32_t offset, uint16_t cycles)
{
    AudioWrite w = {core_bank.samples + core_bank.sams[d->sample].offset + offset,
                    d->length - offset, d->period, (uint16_t)(d->volume < 0 ? 0 : d->volume), cycles};
    audio_write(ch,&w);
}
/* _PlaySound(sound,ch) asm:20307-20386: unsigned bounds; a busy channel
 * keeps its sound against a lower d+4 (_Soundpri). The second request's
 * long offset is built from two pushed words, sams[].+2:d+6 (asm:20371-20380);
 * its cycles slot lies past the pushed arguments, so the port uses d+6. */
static int core_PlaySound(int sound, int ch)
{
    if ((uint16_t)sound >= (uint16_t)no_sounds || (uint16_t)ch > 3 ||
        !core_bank.kit || (uint16_t)sound >= core_bank.kit_count) return 0;
    const AudioKit *d = &core_bank.kit[(uint16_t)sound];
    if (!core_check_effect(ch) && d->volume < Soundpri(ch)) return 0;
    audio_abort(ch);
    Soundpri(ch) = d->volume;
    core_setsound(ch,d,0,1);
    if (d->loop) core_setsound(ch,d,(uint32_t)core_bank.sams[d->sample].loop << 16 | d->loop,d->loop);
    return 1;
}
/* _PlayMeas asm:20433-20518: one beat on all four tracks ('A' = sound 0,
 * below 'A' is out of range), then the measure/sequence advance. _curtime
 * is never reset after _load_sound, as in the original. */
static void core_PlayMeas(void)
{
    if (!core_bank.kit || !core_bank.seq_count) return;
    const uint8_t *m = core_bank.measures + core_curmeas + (uint16_t)curbeat;
    for (int track = 0; track < 4; ++track) core_PlaySound((int16_t)(m[track*0x40] - 0x41),track);
    curbeat = (int16_t)(curbeat+1);
    if (core_bank.measln[core_bank.sequn[(uint16_t)curseq][0]] != (uint16_t)curbeat) return;
    curtime = (int16_t)(curtime+1); curbeat = 0;
    if ((uint16_t)curtime >= core_bank.sequn[(uint16_t)curseq][1] && ++curseq == seqlen) curseq = 0;
    core_curmeas = (uint32_t)core_bank.sequn[(uint16_t)curseq][0] << 8;
}
/* _free_all_sounds asm:20522-20565. */
static void core_free_all_sounds(void) { core_kill_effect(0,4); audio_bank_free(&core_bank); }
/* _load_sound asm:20712-20872, from <assets>/raw/<name>; 0 or -1. */
static int core_load_sound(const char *name)
{
    const char *directory = getenv("POPULOUS_ASSET_DIR");
    if (!directory) directory = POPULOUS_DEFAULT_ASSET_DIR;
    char path[1024], error[128];
    snprintf(path,sizeof path,"%s/raw/%s",directory,name);
    FILE *file = fopen(path,"rb");
    if (!file) { fprintf(stderr,"Sound %s: %s\n",path,strerror(errno)); return -1; }
    static uint8_t data[0x40000];
    size_t size = fread(data,1,sizeof data,file);
    int big = fgetc(file) != EOF;
    fclose(file);
    AudioBank bank;
    if (big || !audio_bank_parse(&bank,data,size,error,sizeof error)) {
        fprintf(stderr,"Sound %s: %s\n",path,big ? "file too large" : error);
        return -1;
    }
    core_free_all_sounds();
    core_bank = bank;
    no_sounds = (int16_t)bank.kit_count; seqlen = (int16_t)bank.seq_count;
    core_curmeas = (uint32_t)bank.sequn[0][0] << 8;
    curtime = curseq = curbeat = 0;
    for (int ch = 0; ch < 4; ++ch) Soundpri(ch) = 0;
    return 0;
}
/* Vertical blank sound part, asm:19620-19672: music every _music frames,
 * a pending _effect on channels 2 and 3 (delaying music to 40 frames),
 * else the population heartbeat 0x47/0x48 on channel 0. */
static void core_vbi_audio(void)
{
    if (music_off) {
        int16_t now = (int16_t)(music_now+1);
        if (now == music) { music = 10; core_PlayMeas(); now = 0; }
        music_now = now;
    }
    if (!effect_off) return;
    if (effect) {
        music = 0x28;
        core_PlaySound(effect,2); core_PlaySound(effect,3);
        effect = 0;
    } else if (!sound_in && tempo) {
        tempo_now = (int16_t)(tempo_now+1);
        if (tempo_now >= tempo) { core_PlaySound(0x47,0); tempo_now = 0; }
        else if ((int16_t)(tempo-beat_two) == tempo_now) core_PlaySound(0x48,0);
    }
}
/* _animate asm:564-605: heartbeat rate from enemy/own population, then
 * the _toggle flip (asm:606-612). Run once per main-loop pass. */
static void core_tempo_pass(void)
{
    if (core_good_pop(player)) {
        int32_t own = (int32_t)core_good_pop(player), other = (int32_t)core_good_pop(player ? 0 : 1);
        tempo = (int16_t)(0x32 - 2*(other/own));
        if (tempo < 7) tempo = 7;
        beat_two = (int16_t)(tempo/3);
    } else { tempo = 0; beat_two = 1; }
    if (tempo_now > tempo) tempo_now = tempo;
    toggle = !toggle;
}
/* _start_game asm:13966-13972 (_open_channels, _load_sound "gmusic1")
 * plus the data defaults _tempo $32, _music_off/_effect_off 1
 * (asm:24129-24134) and _music 10 (_setup_mouse asm:19687). */
static void core_sound_start(void)
{
    tempo = 0x32; music_off = effect_off = 1; music = 10;
    audio_open();
    core_load_sound("gmusic1");
}
/* _do_quake asm:7895-7925: screen_origin -40, then per frame -120/+120
 * bytes; at 40 bytes a row that is -4 lines on even c, -1 on odd c. */
static int core_quake_shake_dy(int c) { return c&1 ? -1 : -4; }
/* The vbi keeps running during the synchronous shake: _effect 0x4c plays. */
static void core_quake_delay_sdl(int ms) { core_vbi_audio(); SDL_Delay((Uint32)ms); }
static void (*core_quake_delay)(int ms) = core_quake_delay_sdl;
/* 20 synchronous frames of pre-quake terrain (no ticks), unconditional on
 * owner; each frame is presented and waits about one vblank. */
static void core_quake_shake(void)
{
    if (!prg_screen || !game_assets) return;
    for (int c = 0; c < 20; ++c) {
        core_shake_dy = core_quake_shake_dy(c);
        game_render();
        core_quake_delay(20);
    }
    core_shake_dy = 0;
}
/* _do_quake asm:7863-8042: _paint_map (asm:7865) skips the mana, _war,
 * _pause and permission gates and the payment, not the score. */
static int core_quake(int owner, int column, int row, WorldRandom random)
{
    if (owner < 0 || owner > 1 || column < 0 || column > 56 || row < 0 || row > 56 ||
        core_result) return 0;
    if (!paint_map) {
        if (pause || A4_WORD(0xae80) || core_mana(owner) < (int32_t)A4_LONG(0x8492) ||
            !(A4_WORD(0x82c4+owner*46)&8)) return 0;
        A4_LONG(0x9a02+owner*16) -= A4_LONG(0x8492);
    }
    A4_WORD(0x2f8a) = A4_WORD(0x2f8c) = column;
    A4_WORD(0x2f8e) = A4_WORD(0x2f90) = row;
    A4_WORD(0x2fac) = 76;
    if (owner == player) A4_LONG(0xae72) += 25;
    core_quake_shake();
    for (int pass = 0; pass < 2; ++pass)
        for (int x = column; x < column+9; ++x)
            for (int y = row; y < row+9; ++y) {
                if (!core_vertex(x,y)) continue;
                int action = random()%5;
                if (action == 1) original_raise_point(x,y);
                else if (action >= 2) original_lower_point(x,y);
            }
    if (A4_WORD(0x2f8a) < 1) A4_WORD(0x2f8a) = 1;
    if (A4_WORD(0x2f8c) > 63) A4_WORD(0x2f8c) = 63;
    if (A4_WORD(0x2f8e) < 1) A4_WORD(0x2f8e) = 1;
    if (A4_WORD(0x2f90) > 63) A4_WORD(0x2f90) = 63;
    core_make_map_region(A4_WORD(0x2f8a)-1,A4_WORD(0x2f8e)-1,
                         A4_WORD(0x2f8c),A4_WORD(0x2f90));
    core_kill_effect(2,4);   /* asm:8037-8040: cut the rumble */
    return 1;
}
/* _do_volcano asm:8046-8267: _paint_map (asm:8048) skips the mana, _war,
 * _pause and permission gates and the payment, not the score. */
static int core_volcano(int owner, int column, int row, WorldRandom random)
{
    if (owner < 0 || owner > 1 || column < 0 || column > 56 || row < 0 || row > 56 ||
        core_result) return 0;
    if (!paint_map && (pause || A4_WORD(0xae80) || core_mana(owner) < (int32_t)A4_LONG(0x849e) ||
        !(A4_WORD(0x82c4+owner*46)&0x40))) return 0;
    /* asm:8141-8157 reads blue twice. Zero protects cell zero, not no cell.
     * Reject unsupported stale references before payment or terrain mutation. */
    int leader = core_leader(0), protected_cell = 0;
    if (leader) {
        if (leader < 1 || leader > 208 || leader > no_peeps ||
            peep_word(leader-1,4) <= 0) return 0;
        protected_cell = peep_word(leader-1,8);
        if (protected_cell < 0 || protected_cell >= 4096) return 0;
    }
    if (!paint_map) A4_LONG(0x9a02+owner*16) -= A4_LONG(0x849e);
    if (owner == player) A4_LONG(0xae72) += 100;
    A4_WORD(0x2f8a) = A4_WORD(0x2f8c) = column;
    A4_WORD(0x2f8e) = A4_WORD(0x2f90) = row;
    A4_WORD(0x2fac) = 75;
    for (int pass = 0; pass < 5; ++pass)
        for (int x = column+pass; x <= column+8-pass; ++x)
            for (int y = row+pass; y <= row+8-pass; ++y) {
                int action = random()%5;
                if (action == 1 || action == 2 || action == 4) original_raise_point(x,y);
            }
    for (int x = column; x < column+8; ++x)
        for (int y = row; y < row+8; ++y) {
            if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
            int cell = y*64+x;
            if (random()%5 || cell == protected_cell ||
                cell == core_target(0) || cell == core_target(1)) continue;
            map_blk[cell] = 47;
            map_bk2[cell] = 0;
        }
    /* Lava never expands these source bounds, even when no point was raised. */
    if (A4_WORD(0x2f8a) < 1) A4_WORD(0x2f8a) = 1;
    if (A4_WORD(0x2f8c) > 63) A4_WORD(0x2f8c) = 63;
    if (A4_WORD(0x2f8e) < 1) A4_WORD(0x2f8e) = 1;
    if (A4_WORD(0x2f90) > 63) A4_WORD(0x2f90) = 63;
    core_make_map_region(A4_WORD(0x2f8a)-1,A4_WORD(0x2f8e)-1,
                         A4_WORD(0x2f8c),A4_WORD(0x2f90));
    A4_WORD(0x2fac) = 75;
    return 1;
}

/* _do_swamp asm:8295-8387: _paint_map (asm:8297) skips the mana, _war,
 * _pause and permission gates and the payment, not the score. Byte target coordinates can be outside the map; still consume all 60 draws. */
static int core_swamp(int owner, int column, int row, WorldRandom random)
{
    if (owner < 0 || owner > 1 || column < 0 || column > 255 || row < 0 || row > 255 ||
        core_result) return 0;
    if (!paint_map) {
        if (pause || A4_WORD(0xae80) || core_mana(owner) < (int32_t)A4_LONG(0x8496) ||
            !(A4_WORD(0x82c4+owner*46)&0x10)) return 0;
        A4_LONG(0x9a02+owner*16) -= A4_LONG(0x8496);
    }
    if (owner == player) A4_LONG(0xae72) += 50;
    for (int attempt = 0; attempt < 30; ++attempt) {
        int x = column + random()%7 - 3;
        int y = row + random()%7 - 3;
        /* Unlike the original, bounds must precede even the block read. */
        if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
        int cell = y*64+x, block = map_blk[cell];
        if ((block == 15 || block == 31 || block == 32 || block == 66) && !map_who[cell])
            map_blk[cell] = 53;
    }
    return 1;
}
static void original_make_thing(int spread_x, int spread_y, WorldRandom random)
{
    int column = random() % 64;
    int row = random() % 64;
    while (original_raise_point(column, row) != 6) {
        column += (int)(random() % (2 * spread_x + 1)) - spread_x;
        row += (int)(random() % (2 * spread_y + 1)) - spread_y;
        if (column < 0) column = 0;
        if (column > 64) column = 64;
        if (row < 0) row = 0;
        if (row > 64) row = 64;
    }
}

/* asm:8535-8626: rejected attempts consume no variant random value. */
static void original_woods(WorldRandom random)
{
    for (int cluster = 0; cluster < 22; ++cluster) {
        int base_column = random() % 59;
        int base_row = random() % 59;
        for (int attempt = 0; attempt < 30; ++attempt) {
            int column = base_column + random() % 9;
            int row = base_row + random() % 9;
            if (column >= 64 || row >= 64) continue;
            int cell = row * 64 + column;
            if (!map_blk[cell] || map_blk[cell] == 47) continue;
            int variant = random() % 3;
            if (cluster < 7) map_blk[cell] = 47 + variant;
            else map_bk2[cell] = 50 + variant;
        }
    }
}

static int original_place_person(int owner, int cell, int leader)
{
    if (cell < 0 || cell >= 4096 || no_peeps >= 208) return 0;
    int person = no_peeps++;
    peeps[person * 22] = 2;
    peeps[person * 22 + 1] = owner;
    peeps[person * 22 + 2] = peeps[person * 22 + 3] = 1;
    set_peep_word(person, 4, 45);
    set_peep_word(person, 8, cell);
    /* _place_people starts frame 255 then calls _set_frame (asm:5732-5736). */
    set_peep_word(person, 12, 0);
    map_who[cell] = person + 1;
    if (leader) core_leader(owner) = person + 1;
    return 1;
}

/* Conquest searches, asm:7688-7856. Caller stages the whole world so any
 * capacity/insufficient-land failure cannot publish a partial start. */
static int original_place_people(int friendly_count, int enemy_count)
{
    /* Zero counts enter the original decrement-before-test loop and underflow.
     * Reject that unsupported input rather than manufacture a different start. */
    if (friendly_count <= 0 || enemy_count <= 0 || friendly_count + enemy_count > 208)
        return 0;
    for (int owner = 0; owner < 2; ++owner) {
        int remaining = owner ? enemy_count : friendly_count;
        int step = owner ? -1 : 1;
        for (int cell = owner ? 3968 : 128;
             remaining && cell >= 128 && cell < 4096; cell += step) {
            if (map_blk[cell] != 15) continue;
            --remaining;
            /* Preferred scans do NOT filter occupancy in the original. */
            if (!original_place_person(owner, cell, remaining <= 0)) return 0;
        }
        /* Original enemy fallback probes logical 4096 (outside the map).
         * Safety exception: skip that probe; retain 4095..1, excluding zero. */
        for (int cell = owner ? 4095 : 0;
             remaining && cell >= (owner ? 1 : 0) && cell < 4096; cell += step) {
            if (!map_blk[cell] || map_who[cell]) continue;
            if (!owner) --remaining;
            if (!original_place_person(owner, cell, remaining <= 0)) return 0;
            if (owner) --remaining; /* enemy fallback never marks its last man leader */
        }
        if (remaining) return 0;
    }
    A4_LONG(0xae72) = friendly_count * 10;
    return 1;
}

static void original_clear(WorldRandom random, unsigned internal_level)
{
    _clear_map();
    memset(map_steps, 0, 4096 * 2);
    memset(peeps, 0, 208 * 22);
    no_peeps = 0;
    for (int owner = 0; owner < 2; ++owner) {
        core_tendency(owner) = 1;
        core_target(owner) = 0x820;
        core_leader(owner) = 0;
        A4_LONG(0x9a02 + owner * 16) = 399;
        int value = random() % 3;
        A4_WORD(0x82d0 + owner * 46) = value;
        A4_WORD(0x82ce + owner * 46) = value + random() % 5 + 1;
    }
    if (internal_level > 0x4d3) A4_LONG(0x9a12) = 2700; /* mana_values[3] + 200 */
    A4_WORD(0xae76) = A4_WORD(0xae78) = 0x820;
    pointer = 1; mode = 2; A4_WORD(0x8258) = -1;
    right_button = left_button = 2;
}

static int original_generate(const LevelStart *start)
{
    const LevelRecord *record = &start->record;
    in_conquest = A4_WORD(0xae7e) = start->internal_level;
    conquest = record->rating; conq_01_speed = record->speed;
    conq_02_enemypow = record->enemy_powers; conq_03_yourpow = record->your_powers;
    conq_04_mode = record->mode_flags; conq_05_terrain = record->terrain;
    A4_DISP(0x84c2)[0] = record->your_count; A4_DISP(0x84c3)[0] = record->enemy_count;
    conq_08_seed = (int16_t)record->seed_offset;
    seed = start_seed = (int16_t)start->initial_seed;
    ground_in = record->terrain;
    original_clear(___newrand, start->internal_level);
    LAB_516DE = record->rating; LAB_516E2 = record->speed;
    LAB_516E0 = (record->enemy_powers << 3) | 7;
    LAB_516B2 = (record->your_powers << 3) | 7;
    game_mode = record->mode_flags;
    original_make_thing(2, 4, ___newrand);
    original_make_thing(4, 2, ___newrand);
    original_make_thing(3, 3, ___newrand);
    _make_map();
    original_woods(___newrand);
    if (!original_place_people(record->your_count, record->enemy_count)) return 0;
    seed = (int16_t)((uint16_t)seed + 1u);
    return 1;
}

/* _tutorial / LAB_5172E (asm:24233-24247): the 46-byte _stats records
 * _start_game copies over _stats and strABC (asm:13976-13987). */
static const uint8_t tutorial_stats[2][46] = {
    {'a','b','c',0, 0,0,0,0, 0,1,0,0, 0,10,0,0x3f, 0,10,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,5,0,0},
    {'a','b','c',0, 0,0,0,1, 0,1,0,0, 0,10,0,3, 0,10,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0}};

/* _start_of_game == 3: _start_game asm:13975-13999, then _setup_display's
 * custom path (in_conquest == -1, asm:418-433 with argument 1: one
 * ___newrand, ground 0 kept) and _clear_map's 10000 mana (asm:1134-1136).
 * _place_first_people takes conq_06/07 (15/3, asm:24343-24346; asm:7656). */
static int tutorial_generate(void)
{
    /* Bytes +0..+3 (command, x, y), then BE words into the host-endian A4. */
    for (int owner = 0; owner < 2; ++owner) {
        memcpy(A4_DISP(0x82b6 + owner*46), tutorial_stats[owner], 4);
        for (int field = 4; field < 46; field += 2)
            A4_WORD(0x82b6 + owner*46 + field) = (int16_t)(tutorial_stats[owner][field] << 8 | tutorial_stats[owner][field+1]);
    }
    in_conquest = -1;
    conquest = 1; conq_01_speed = 1; conq_02_enemypow = 0; conq_03_yourpow = 0x3f;
    conq_04_mode = 3; conq_05_terrain = 0; conq_08_seed = 0;
    A4_DISP(0x84c2)[0] = 15; A4_DISP(0x84c3)[0] = 3;
    seed = start_seed = 0x69bc;
    game_mode = 1;                  /* _tutorial_game_mode */
    ground_in = 0;
    ___newrand();
    original_clear(___newrand, 0);
    A4_LONG(0x9a02) = 10000;        /* LAB_5188C -> LAB_52DF0 */
    original_make_thing(2, 4, ___newrand);
    original_make_thing(4, 2, ___newrand);
    original_make_thing(3, 3, ___newrand);
    _make_map();
    original_woods(___newrand);
    if (!original_place_people(15, 3)) return 0;
    seed = (int16_t)((uint16_t)seed + 1u);
    return 1;
}

/* Boot values of _stats/strABC (asm:24171-24232) and _game_mode with
 * LAB_518A7 (asm:24310-24312): rating 1/5, powers $ffff, speed 1/3. */
static const uint8_t core_boot_stats[2][46] = {
    {'a','b','c',0, 0,0,0,0, 0,1,0,0, 0,1,0xff,0xff, 0,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,5},
    {'a','b','c',0, 0,0,0,1, 0,1,0,0, 0,5,0xff,0xff, 0,3,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1}};
#define CORE_BOOT_GAME_MODE 0x0010

/* _setup_display(0, arg) with in_conquest -1, asm:430-457 then 458-500:
 * _start_seed = _seed; a nonzero _seed draws one ___newrand and, for
 * arg -1 and an odd draw, steps _ground_in 0..3 (___load_ground is the
 * caller's). No conq_ byte is read: _stats powers/rating/speed and
 * _game_mode stay as _options/_set_options left them. _clear_map
 * (asm:1023-1188) skips the level mana; _make_alt, _make_map(0,0,63,63),
 * _make_woods_rocks, then _place_first_people's custom counts
 * (asm:7664-7678, 7766-7780): 1, or 3 when _serial_off and the side's
 * _stats+$c rating > 4. Returns 0 if the people cannot be placed. */
static int custom_generate(int16_t arg, int counts[2])
{
    in_conquest = -1;
    start_seed = seed;
    if (seed && (___newrand() & 1) && arg == -1 && ++ground_in > 3) ground_in = 0;
    for (int owner = 0; owner < 2; ++owner) {   /* LAB_516B6/LAB_516C0, asm:1122-1129 */
        A4_WORD(0x82c8 + owner*46) = 0; A4_WORD(0x82d2 + owner*46) = 0;
    }
    original_clear(___newrand, 0);
    original_make_thing(2, 4, ___newrand);
    original_make_thing(4, 2, ___newrand);
    original_make_thing(3, 3, ___newrand);
    _make_map();
    original_woods(___newrand);
    for (int owner = 0; owner < 2; ++owner)
        counts[owner] = serial_off ? (A4_WORD(0x82c2 + owner*46) > 4 ? 3 : 1) : 1;
    if (!original_place_people(counts[0], counts[1])) return 0;
    A4_LONG(0xae72) = (uint32_t)counts[player ? 1 : 0] * 10;   /* asm:7679-7686, 7781-7788 */
    seed = (int16_t)((uint16_t)seed + 1u);
    return 1;
}

/* _end_text asm:24790-24843 (A4 0x8e7e): 7 rows x 46 bytes, word x, word y,
 * word (0x48/0x18, unused here), then text. strLOST is text+5 and
 * strScoreboard text+19 of each row (asm:14025,14102). */
static const struct { int16_t x, y, w; const char *text; } core_end_template[7] = {
    {0x70,0x0a,0x48,"GAME LOST"},
    {0x10,0x14,0x18,"                   YOU  HIM"},
    {0x10,0x23,0x18,"BATTLES WON        0000 0000"},
    {0x10,0x2d,0x18,"NUMBER OF KNIGHTS  0000 0000"},
    {0x10,0x37,0x18,"NUMBER OF TOWNS    0000 0000"},
    {0x10,0x41,0x18,"NUMBER OF CASTLES  0000 0000"},
    {0x10,0x50,0x18,"YOUR SCORE         000000000"},
};
#define core_end_row(r) ((char *)A4_DISP(0x8e84 + (r)*46))
static int core_end_shown, core_end_next, core_end_finished;
/* _won_conquest lord screen, asm:14503-14765. The original loops inside
 * _won_conquest, so the game stays frozen (core_result is kept) until the
 * click. w_screen is core_lord.screen, _map_steps' mouth copy is
 * core_lord.mouths (the port keeps the A4 _map_steps intact). */
static struct {
    int shown, finished, rank, wait;
    int16_t c, f, d, saved_music, saved_effect;   /* -120,-108,-110,-116,-118(A5) */
    int mouths_loaded;
    uint16_t palette[16];                         /* _tcmap[0..15] */
    uint8_t screen[320*200], mouths[0x13b0];
} core_lord;

/* ___long_asc/___word_asc asm:21046-21110: signed decimal, no padding. */
static void core_long_asc(char *out, int32_t value) { sprintf(out, "%ld", (long)value); }

/* _end_game asm:14019-14316: fill the seven rows and the final _score.
 * Row 1 keeps its template; rows 2-5 pad "you" to 5 then append "him". */
static int32_t core_end_stats(int won)
{
    for (int r = 0; r < 7; ++r) {
        A4_WORD(0x8e7e + r*46) = core_end_template[r].x;
        A4_WORD(0x8e80 + r*46) = core_end_template[r].y;
        A4_WORD(0x8e82 + r*46) = core_end_template[r].w;
        memset(core_end_row(r), 0, 40);
        strcpy(core_end_row(r), core_end_template[r].text);
    }
    strcpy(core_end_row(0) + 5, won ? "WON" : "LOST");  /* asm:14022-14044 */
    int you = player, him = not_player;
    for (int r = 2; r <= 5; ++r) {
        int16_t mine = 0, theirs = 0;
        if (r == 2) { mine = A4_WORD(0xae7a + you*2); theirs = A4_WORD(0xae7a + him*2); } /* asm:14046-14061 */
        else {
            /* asm:14063-14202 counts low words; the port clamps the unsigned
             * no_peeps loop to the 208-record pool. */
            int n = (uint16_t)no_peeps < 208 ? (uint16_t)no_peeps : 208;
            for (int p = 0; p < n; ++p) {
                uint32_t heading; memcpy(&heading, peeps + p*22 + 14, 4);
                int hit = r == 3 ? heading != 0 && peep_word(p,4) != 0 :
                    peeps[p*22] == 1 && peep_word(p,4) != 0 &&
                    (r == 4 ? peep_word(p,12) != 0x2a : peep_word(p,12) == 0x2a);
                if (hit) { if (peep_owner(p) == you) ++mine; else ++theirs; }
            }
        }
        char a[16], b[16];
        core_long_asc(a, mine); core_long_asc(b, theirs);
        size_t length = strlen(a);
        while (length < 5) a[length++] = ' ';
        a[length] = 0;
        strcpy(core_end_row(r) + 19, a); strcat(core_end_row(r), b);   /* asm:14280-14316 */
    }
    /* Score row asm:14204-14272. */
    uint32_t score = A4_LONG(0xae72);
    if (A4_WORD(0xae7a + you*2) > A4_WORD(0xae7a + him*2)) score += 5000;
    for (uint16_t bit = 8; bit <= 0x100; bit <<= 1) {
        if (!(A4_WORD(0x82c4 + you*46) & bit)) score += 1000;
        if (A4_WORD(0x82c4 + him*46) & bit) score += 1000;
    }
    /* asm:14239-14247: set for every single-player start (asm:12768). */
    if (serial_off) score += (uint32_t)(int32_t)(int16_t)((10 - A4_WORD(0x82c6 + him*46)) * 15);
    if (won) score *= 10u;
    if ((int32_t)score < 500) score = 500;
    if ((int32_t)score > 555555) score = 515090;
    A4_LONG(0xae72) = score;
    core_long_asc(core_end_row(6) + 19, (int32_t)score);
    return (int32_t)score;
}

/* _start/_mid/_end syllables asm:25075-25173. */
static const char *const core_code_start[32] = {
    "RING","VERY","KILL","SHAD","HURT","WEAV","MIN","EOA","COR","JOS","ALP","HAM","BUR","BIN","TIM","BAD",
    "FUT","MOR","SAD","CAL","IMM","SUZ","NIM","LOW","SCO","HOB","DOU","BIL","QAZ","SWA","BUG","SHI"};
static const char *const core_code_end[32] = {
    "HILL","TORY","HOLE","PERT","MAR","CON","LOW","DOR","LIN","ING","HAM","OLD","PIL","BAR","MET","END",
    "LAS","OUT","LUG","ILL","ICK","PAL","DON","ORD","OND","BOY","JOB","ER","ED","ME","AL","T"};
static const char *const core_code_mid[32] = {
    "OUT","QAZ","ING","OGO","QUE","LOP","SOD","HIP","KOP","WIL","IKE","DIE","IN","AS","MP","DI",
    "OZ","EA","US","GB","CE","ME","DE","PE","OX","A","E","I","O","U","T","Y"};

/* ___newrand of a given seed, without touching the A4 seed. */
static uint16_t core_code_rand(int level) { return (uint16_t)((uint16_t)level * 0x24a1u + 0x24dfu) & 0x7fff; }

/* _code asm:14793-14895: buffer needs 13 bytes. */
static void core_code(char *out, uint16_t n)
{
    strcpy(out, core_code_start[n & 31]);
    strcat(out, core_code_mid[(n >> 5) & 31]);
    strcat(out, core_code_end[(n >> 10) & 31]);
}

/* _decode asm:14900-15118: first matching syllable wins, then the world
 * whose seed n=0,5,..,5000 yields the code. The original leaves _seed=n;
 * the port does not. "GENESIS" is _show_world's case (asm:12280-12288). */
static int core_decode(const char *name)
{
    if (!strcmp(name, "GENESIS")) return 0;
    const char *const *tables[3] = {core_code_start, core_code_mid, core_code_end};
    size_t used = 0;
    int code = 0;
    for (int t = 0; t < 3; ++t) {
        int s = 0;
        for (; s < 32; ++s) {
            size_t k = 0;
            while (tables[t][s][k] && name[used+k] == tables[t][s][k]) ++k;
            if (!tables[t][s][k]) { used += k; break; }
        }
        if (s == 32) return -1 - t;
        code |= s << (5*t);
    }
    if (strlen(name) != used) return -4;
    for (int n = 0; n <= 5000; n += 5) if (core_code_rand(n) == code) return n;
    return -5;
}

/* _won_conquest asm:14460-14491: skip score/5000 worlds, round up to a
 * multiple of 5, cap at 2470; winning 2470 again wraps to 0 (finished). */
static int core_next_level(int level, int32_t score, int *finished)
{
    int16_t next = (int16_t)(level + (int16_t)(score / 5000 + 1));
    if (next % 5) next = (int16_t)(next + 5 - next % 5);
    *finished = 0;
    if (next > 0x9a6) {
        if (level == 0x9a6) { next = 0; *finished = 1; }
        else next = 0x9a6;
    }
    return next;
}

/* _won_conquest message asm:14520-14582 (_con_text ranks asm:24751-24760). */
static void core_conquest_text(int old_level, int level, int finished, char line[2][48])
{
    static const char *const ranks[10] = {"MORTAL","IMMORTAL","ETERNAL","DEVA","GREATER BEING",
        "DEITY","GREATER DEITY","MORTAL GOD","GREATER GOD","ETERNAL GOD"};
    char next[16], old[16];
    core_code(next, core_code_rand(level));
    if (finished) {
        strcpy(line[0], "WELL DONE YOU HAVE CONQUERED EVIL");
        snprintf(line[1], 48, "THE BATTLE IS OVER BUT TRY %s", next);
        return;
    }
    snprintf(line[0], 48, "WELL DONE %s YOU CONQUERED ", ranks[level / 250 % 10]);
    if (old_level) core_code(old, core_code_rand(old_level)); else strcpy(old, "GENESIS");
    snprintf(line[1], 48, "%s NOW BATTLE AT %s", old, next);
}

static int core_conquest_game(void) { return current_start.kind == START_ORIGINAL && in_conquest != -1; }

/* _end_game entry: stats once after the result appears. A conquest win
 * precomputes _won_conquest's next world from the final score. */
static void core_enter_end(void)
{
    int won = core_result == 1;
    int32_t score = core_end_stats(won);
    core_end_shown = 1; core_end_next = -1; core_end_finished = 0;
    if (won && core_conquest_game())
        core_end_next = core_next_level(A4_WORD(0xae7e), score, &core_end_finished);
}

/* After the click (asm:14386-14402): won conquest -> _level_number and
 * _message = the next world's code (asm:14492-14501), the lord screen
 * (core_lord_open), then _show_world until != -1 (asm:14766-14772); a
 * lost conquest keeps in_conquest, i.e. the same world; in_conquest -1
 * -> _setup_display(0,-1) from the live _seed, the next custom landscape
 * (asm:14410-14432); synthetic fixtures restart. */
static int core_leave_end(void)
{
    if (!core_end_shown) return 0;
    A4_WORD(0x82bc + player*46) = 0;
    if (core_end_next >= 0) {
        int old = A4_WORD(0xae7e);
        A4_WORD(0xae7e) = (int16_t)core_end_next;
        core_code(core_message, core_code_rand(core_end_next));
        if (!core_lord_open(old, core_end_next, core_end_finished)) core_world_open(CORE_WORLD_WON);
        return 1;
    }
    StartSelection next = current_start;
    if (in_conquest == -1 && current_start.kind != START_SYNTHETIC ? !core_custom_start(seed, -1) : !game_reset_start(next)) {
        fprintf(stderr, "End of game: next world could not be started.\n");
        return 0;
    }
    return 1;
}

static void _animate(void)
{
    if (!game_reset_start(requested_start)) { prg_status = EXIT_FAILURE; return; }
    /* _start_of_game == 2 (asm:536-551): the world screen first. */
    if (requested_world) { core_message[0] = 0; core_world_open(CORE_WORLD_START); }
    game_render();
    Uint32 next_tick = SDL_GetTicks() + 50, next_vbi = SDL_GetTicks() + 20;
    while (!prg_quit) {
        ___keyboard();
        if (prg_quit) break;
        Uint32 now = SDL_GetTicks();
        int catch_up = 0;
        /* 50 Hz vertical blank sound interrupt (asm:19620-19672). */
        while ((Sint32)(now-next_vbi) >= 0 && catch_up++ < 10) { core_vbi_audio(); core_lord_vblank(); next_vbi += 20; }
        if ((Sint32)(now-next_vbi) >= 0) next_vbi = now+20;
        catch_up = 0;
        while ((Sint32)(now-next_tick) >= 0 && catch_up++ < 5) {
            if (core_req.kind == CORE_REQ_WORLD) ++core_world.blink;
            if (core_req.kind == CORE_REQ_FILE) ++core_file.blink;
            /* Modal requesters and the end screen leave the main loop. */
            if (!core_req.kind && !core_result) core_tempo_pass();
            game_tick(); next_tick += 50;
        }
        if ((Sint32)(now-next_tick) >= 0) next_tick = now+50;
        /* ___end_game, called by _move_peeps (asm:4323-4347); deferred to
         * after the tick so game_tick's own A4 effects stay unchanged. */
        if (core_result && !core_end_shown) core_enter_end();
        game_render();
        SDL_Delay(10);
    }
}

/* A custom start from the live _ground_in; a failure keeps the old land. */
static int core_custom_start(int16_t from, int16_t arg)
{
    CustomLand old = core_land;
    core_land = (CustomLand){from, arg, ground_in};
    if (game_reset_start((StartSelection){START_CUSTOM, 0})) return 1;
    core_land = old;
    return 0;
}

static int game_reset(int scenario)
{
    return game_reset_start((StartSelection){START_SYNTHETIC, scenario});
}

/* The LAND rule tables and _map_colour of ___load_ground, into the A4. */
static void core_install_land(const LandRules *loaded)
{
    const uint16_t *tables[] = {loaded->population_add,loaded->mana_add,loaded->weapons_add,loaded->battle_add1,loaded->battle_add2,loaded->weapons_order};
    const int offsets[] = {0x2fe2,0x2fb6,0x2fcc,0x2ffa,0x3010,0x2f96};
    for (int table = 0; table < 6; ++table)
        for (int index = 0; index < (table == 4 ? 3 : 11); ++index) A4_WORD(offsets[table]+index*2) = tables[table][index];
    A4_WORD(0x2fb0) = loaded->walk_death;
    memcpy(map_colour_data,loaded->map_colour,32);
}

static int game_reset_start(StartSelection selection)
{
    int scenario = selection.number;
    if (scenario < 0 || (selection.kind != START_SYNTHETIC && selection.kind != START_ORIGINAL &&
        selection.kind != START_TUTORIAL && selection.kind != START_CUSTOM) ||
        ((selection.kind == START_TUTORIAL || selection.kind == START_CUSTOM) && scenario) ||
        scenario >= (selection.kind == START_ORIGINAL ? LEVEL_COUNT : 8)) return 0;
    ensure_prg_screen();
    if (!prg_screen) return 0;
    LandRules loaded;
    char error[160];
    const char *asset_directory = getenv("POPULOUS_ASSET_DIR");
    if (!asset_directory) asset_directory = POPULOUS_DEFAULT_ASSET_DIR;
    char default_data[4096];
    int path_length = snprintf(default_data,sizeof default_data,"%s/raw",asset_directory);
    if (path_length < 0 || (size_t)path_length >= sizeof default_data) return 0;
    const char *directory = getenv("POPULOUS_DATA_DIR");
    if (!directory) directory = default_data;
    LevelStart original = {0};
    unsigned terrain;
    _Alignas(4) uint8_t staged[sizeof prg_a4_area] = {0};
    if (selection.kind == START_CUSTOM) {
        /* The original keeps all of A4 between maps; the port carries the
         * custom inputs: _stats (both records), _game_mode, _player. */
        uint8_t kept_stats[92]; int16_t modes = game_mode, side = player;
        if (core_custom_live) memcpy(kept_stats, A4_DISP(0x82b6), 92);
        uint8_t *live = prg_a4;
        prg_a4 = staged + 32768;
        if (core_custom_live) memcpy(A4_DISP(0x82b6), kept_stats, 92);
        else {
            for (int owner = 0; owner < 2; ++owner) {
                memcpy(A4_DISP(0x82b6 + owner*46), core_boot_stats[owner], 4);
                for (int field = 4; field < 46; field += 2)
                    A4_WORD(0x82b6 + owner*46 + field) = (int16_t)(core_boot_stats[owner][field] << 8 | core_boot_stats[owner][field+1]);
            }
            modes = CORE_BOOT_GAME_MODE; side = 0;
        }
        game_mode = modes; player = side; serial_off = 1;   /* asm:24135-24138 */
        seed = core_land.from; ground_in = core_land.ground;
        int counts[2];
        int generated = custom_generate(core_land.arg, counts);
        terrain = (unsigned)ground_in;
        int16_t speed = A4_WORD(0x82c6 + 46);   /* LAB_516E2 */
        original.record.speed = speed > 0 && speed < 256 ? (uint8_t)speed : 0;
        original.record.your_count = (uint8_t)counts[0]; original.record.enemy_count = (uint8_t)counts[1];
        prg_a4 = live;
        if (!generated || terrain > 3 || !original.record.speed) {
            fprintf(stderr, "Custom landscape %d rejected: unsupported starting population or speed\n", (int)core_land.from);
            return 0;
        }
    } else if (selection.kind == START_ORIGINAL) {
        LevelTable levels;
        if (!level_data_load(directory, &levels, error, sizeof error) ||
            !level_data_select(&levels, (unsigned)scenario, &original)) {
            fprintf(stderr, "Original level data: %s\n", error);
            return 0;
        }
        terrain = original.record.terrain;
    } else if (selection.kind == START_TUTORIAL) {
        /* strABC+$10 speed 10, ground 0, conq_06/07 populations. */
        original.record = (LevelRecord){10, 10, 0, 0, 1, 0, 15, 3, 0};
        terrain = 0;
    } else terrain = demo_levels[scenario][5];
    unsigned selected_speed = selection.kind != START_SYNTHETIC ? original.record.speed : demo_levels[scenario][1];
    if (!selected_speed) {
        fprintf(stderr, "Start %d rejected: unsupported zero terrain mailbox cadence divisor\n", scenario);
        return 0;
    }
    if (!land_rules_load(directory, terrain, &loaded, error, sizeof error)) {
        fprintf(stderr,"Original LAND data: %s. Set POPULOUS_DATA_DIR to extracted land0..3.\n",error);
        return 0;
    }
    /* raw/font.dat is byte-identical to _font_data (asm:21949-22372);
     * optional, the end screen omits its text without it. */
    char font_path[4160];
    snprintf(font_path, sizeof font_path, "%s/font.dat", directory);
    FILE *font_file = fopen(font_path, "rb");
    game_font_loaded = font_file && fread(game_font, 1, sizeof game_font, font_file) == sizeof game_font &&
                       fgetc(font_file) == EOF;
    if (font_file) fclose(font_file);
    if (selection.kind == START_ORIGINAL) {
        uint8_t *live = prg_a4;
        prg_a4 = staged + 32768;
        int generated = original_generate(&original);
        prg_a4 = live;
        if (!generated) {
            fprintf(stderr, "Original level %d rejected: insufficient terrain or unsupported starting population\n", scenario);
            return 0;
        }
    } else if (selection.kind == START_TUTORIAL) {
        uint8_t *live = prg_a4;
        prg_a4 = staged + 32768;
        int generated = tutorial_generate();
        prg_a4 = live;
        if (!generated) { fprintf(stderr, "Tutorial rejected: unsupported starting population\n"); return 0; }
    }
    GraphicsAssets *loaded_assets = graphics_load(asset_directory,terrain,loaded.sprite_set,error,sizeof error);
    if (!loaded_assets) { fprintf(stderr,"Original graphics: %s\n",error); return 0; }
    graphics_free(game_assets);
    game_assets = loaded_assets;
    diagnostic_view = 0;
    current_start = selection;
    current_scenario = scenario;
    if (selection.kind != START_SYNTHETIC) core_custom_live = 1;
    /* _clear_map (asm:1023-1188) and _clear_send (asm:18234-18254) never write
     * 0x99fc, good_pop/bad_pop or _stats+0x14/+0x16/+0x22/+0x26/+0x2a; only
     * _move_peeps zeroes them (asm:3280-3320), after _devil_effect (asm:3268). */
    for (int owner = 0; owner < 2; ++owner) {
        static const struct { int offset, size; } kept[] = {
            {0x99fc,2},{0x99fe,4},{0x82ca,2},{0x82cc,2},{0x82d8,4},{0x82dc,4},{0x82e0,4}};
        for (size_t field = 0; field < sizeof kept / sizeof kept[0]; ++field) {
            int offset = kept[field].offset + owner * (kept[field].offset >= 0x99fc ? 16 : 46);
            memcpy(staged + 32768 + (int16_t)offset, A4_DISP(offset), (size_t)kept[field].size);
        }
    }
    /* Sound state is static data in the original, never reset by a start:
     * _music_now.._sound_in, _seqlen, _no_sounds.._Soundpri, the toggles. */
    static const struct { int offset, size; } sound_kept[] = {
        {0x3152,6},{0x3378,2},{0x3542,0x14},{0x8250,4}};
    for (size_t field = 0; field < sizeof sound_kept / sizeof sound_kept[0]; ++field)
        memcpy(staged + 32768 + (int16_t)sound_kept[field].offset, A4_DISP(sound_kept[field].offset),
               (size_t)sound_kept[field].size);
    memcpy(prg_a4_area, staged, sizeof prg_a4_area);
    core_install_land(&loaded);
    for (int index = 0; index < 25; ++index) A4_WORD(0x842c+index*2) = core_offsets[index];
    for (int index = 0; index < 9; ++index) A4_WORD(0x8474+index*2) = core_city[index];
    for (int index = 0; index < 11; ++index) A4_LONG(0x8486+index*4) = (uint32_t)core_mana_values[index];
    if (selection.kind == START_SYNTHETIC) {
    for (int owner = 0; owner < 2; ++owner) {
        A4_LONG(0x9a02+owner*16) = 399; /* _setup_display asm:1070 */
        core_tendency(owner) = 1; core_target(owner) = 0x820;
    }
    core_result = core_battles = core_settlements = core_tool = 0; core_set_pointer();
    const uint8_t *entry = demo_levels[scenario];
    conquest = entry[0];
    conq_01_speed = entry[1];
    conq_02_enemypow = entry[2];
    conq_03_yourpow = entry[3];
    conq_04_mode = entry[4];
    conq_05_terrain = entry[5];
    conq_08_seed = entry[8];
    in_conquest = scenario * 5;
    current_start_pop_you = entry[6];
    current_start_pop_enemy = entry[7];
    demo_mana = 50;
    demo_shield_timer = growth_tick = 0;
    prg_quit = 0;
    _setup_display();
    } else {
        current_start_pop_you = original.record.your_count;
        current_start_pop_enemy = original.record.enemy_count;
        demo_mana = 50;
        demo_shield_timer = growth_tick = 0;
        prg_quit = 0;
    }
    /* Conquest start asm:12762-12767 and speed asm:409-411, not static "abc".
     * Publish only after all fallible generation/asset work has succeeded. */
    core_clear_terrain_mailboxes();
    /* Every start: _setup_display clears _surender (asm:1149) and
     * _two_players derives _not_player from _player (asm:11779-11786). */
    A4_WORD(0x8258) = -1; not_player = player ? 0 : 1;
    /* Single-player conquest start sets _serial_off (asm:12768); the end
     * score reads it (asm:14239). _score was cleared with the staged A4. */
    serial_off = 1;
    core_end_shown = 0; core_end_next = -1;
    if (selection.kind != START_CUSTOM) {   /* the custom _stats stand (asm:430-457) */
        A4_WORD(0x82bc) = 0; A4_WORD(0x82bc + 46) = 1;
        A4_WORD(0x82be) = 1; A4_WORD(0x82be + 46) = 0;
        A4_WORD(0x82c6) = 1; A4_WORD(0x82c6 + 46) = selected_speed;
    }
    if (selection.kind == START_TUTORIAL) {
        /* The copied _stats words stand; _start_game sets _pause (asm:13988). */
        for (int owner = 0; owner < 2; ++owner)
            for (int field = 6; field <= 0x10; field += 2)
                A4_WORD(0x82b6 + owner*46 + field) = (int16_t)(tutorial_stats[owner][field] << 8 | tutorial_stats[owner][field+1]);
        pause = 1;
    }
    core_result = core_battles = core_settlements = core_tool = core_swamp_selected = 0; core_set_pointer();
    xoff = yoff = 6;
    if (selection.kind != START_SYNTHETIC) {
        /* Host camera accommodation: original inhabitants are not on demo plots. */
        int cell = peep_word(core_leader(0) - 1, 8);
        xoff = cell % 64 - 3;
        yoff = cell / 64 - 3;
        if (xoff < 0) xoff = 0;
        if (xoff > 56) xoff = 56;
        if (yoff < 0) yoff = 0;
        if (yoff > 56) yoff = 56;
    }
    SDL_SetWindowTitle(prg_screen->window,"Populous - original-rule intermediate port");
    return 1;
}

static void game_tick(void)
{
    if (pause || prg_quit || core_result || core_req.kind) return;
    /* _animate asm:676-738: with _paint_map set neither _do_funny nor
     * _move_peeps runs (people stand still, _game_turn stays, no end-game
     * check); the frame only plots the peeps. Mailboxes are still read. */
    if (paint_map) {
        if (view_timer && !--view_timer) view_who = old_view_who;
        core_consume_terrain_mailboxes();
        return;
    }
    game_turn = (int16_t)((uint16_t)game_turn + 1u);
    /* asm:740-750: a temporary view (_set_temp_view) lapses after 10 turns.
     * The original also lapses it while a button is held; the port has no
     * held-button state, so only the countdown applies. */
    if (view_timer && !--view_timer) view_who = old_view_who;
    _move_peeps();
    /* _move_peeps asm:4323-4347, after the peep loop: the pass's pre-move
     * good_pop or a surrender ends the game, _player first (___end_game(1)
     * lost, else ___end_game(0) won). There is no draw. */
    if (!core_good_pop(player) || player == A4_WORD(0x8258)) core_result = 2;
    else if (!core_good_pop(not_player) || not_player == A4_WORD(0x8258)) core_result = 1;
    /* Preserve the C population/result timing; never modify terrain mid-movement. */
    if (core_result) core_clear_terrain_mailboxes();
    else core_consume_terrain_mailboxes();
}

static void game_render(void)
{
    ensure_prg_screen();
    if (core_lord.shown && prg_screen) { core_draw_lord(); amiga_present(prg_screen); return; }
    ___draw_it(yoff, xoff);
    if (prg_screen) amiga_present(prg_screen);
}

static void _keyboard(void) { printf("[PRG] _keyboard\n"); }
static void _mouse(void)    { printf("[PRG] _mouse\n"); }

/* =========================================================================
 * TRANSLATED FROM THE ASSEMBLY CHUNK YOU PASTED (populous.prg, 1993 Hit Squad)
 * _main, _rs, _closeall, mouse/serial setup, open/close lib, _setup_display
 * (full conquest variable handling with the conq_ bytes, seed/terrain/powers/mode),
 * _animate (the heartbeat with icon setup, conquest show_world path, move_mana +
 * draw + sprite layers, peeps overview dots, do_funny/move_peeps, view_timer,
 * game_turn funny, show_shield, keyboard numpad scrolls + debug keys A(win)/F9(stats)/*(cheat),
 * sculpt/interogate/zoom/button handling).
 *
 * 1993 notes ("new section not in 1989", "1989 has two lines here", "Peter's interrupt",
 * conquest comments on seed/terrain/power bitfields/mode) are kept as comments.
 * A4 accesses use the DISP macros (exact d16 displacements from the listing).
 * Graphics are routed to the SDL layer (or printf + TODO for unimplemented pieces).
 * More bodies (the callees like _move_peeps, _draw_map etc.) will be filled when
 * you paste their assembly.
 * ========================================================================= */

/* --- translated small init / cleanup / interrupt / lib functions --- */

static void ensure_prg_screen(void)
{
    if (!prg_screen) {
        prg_screen = amiga_create_screen(320, 256, 4);  /* 16 colors for starters */
        if (prg_screen) {
            for (int index = 0; index < 16; ++index)
                amiga_set_color(prg_screen,index,graphics_palette[index]);
        }
    }
}

static void _main(void)
{
    ensure_prg_screen();
    if (!prg_screen) { prg_status = EXIT_FAILURE; return; }
    _openlibrary();

    /* AllocMem sequence for the big buffers (exact sizes + attributes from the PEA/JSR) */
    sprite_data    = amiga_alloc_mem(0x5960, 0);
    sprite_data_32 = amiga_alloc_mem(0x2080, 0);
    blk_data       = amiga_alloc_mem(0x8340, 0);
    back_scr       = amiga_alloc_mem(0x7D00, 2);
    screen_pointer = amiga_alloc_mem(0xfa00, 0x00010002);
    copper_list    = amiga_alloc_mem(0x384, 0x00010002);
    spare_cl       = amiga_alloc_mem(0x12c, 0x00010002);
    fb             = amiga_alloc_mem(0x104, 0x00010002);
    for (int index = 0; index < 8; ++index) {
        if (!host_buffers[index]) {
            fprintf(stderr, "PRG allocation failed\n");
            prg_status = EXIT_FAILURE;
            _closeall();
            return;
        }
    }

    A4_WORD(0x825c) = 0x32; A4_WORD(0x825e) = 0x2f;   /* _last_tree/_last_rock data, asm:24149-24152 */
    /* start_of_game from WB/cli (the $42 path or atoi of arg) */
    if (0 /* argc path */) {
        start_of_game = 0; /* ___atoi */
    } else {
        start_of_game = 0x42;
    }

    ___open_screen();
    ___read_sprites(sprites_in);
    ___sprite_to_amiga();
    ___start_game();

    /* 1989 had two lines (call + move); 1993 version */
    _setup_serial_interrupt();

    A4_WORD(0x98a2) = 0x2580;
    ___set_serial();

    _animate();

    _closeall();
}

static void _rs(void)
{
    ___reset_screen(0);
}

static void _closeall(void)
{
    ___reset_screen(0);
    _free_inter();
    ___free_amiga_sprites();
    ___free_all_sounds();
    ___close_channels();

    for (int index = 0; index < 8; ++index) {
        amiga_free_mem(host_buffers[index]);
        host_buffers[index] = NULL;
    }
    graphics_free(game_assets);
    game_assets = NULL;
    amiga_destroy_screen(prg_screen);
    prg_screen = NULL;

    printf("[HW] INTENA = 0x0010\n");
    _free_serial_interrupt();
    _closelibrary();
}

static void _create_mouse(void)
{
    mouse_limit = 0x018f;
    _setup_interrupt();
}

static void _setup_interrupt(void)
{
    LAB_52C40 = 0x02; LAB_52C41 = 0;
    /* "petersint" (Peter Molyneux) + mousex + ___mouse handler */
    big_mousex = 0; big_mousey = 0;
    ___setup_mouse();
    ___AddIntServer(5, (void*)dint);
}

static void _set_mouse(void)
{
    ___setup_mouse();
}

static void _free_inter(void)
{
    ___RemIntServer(5, (void*)dint);
    ___Delay(9);
}

static void _setup_serial_interrupt(void)
{
    LAB_52DAA = 0x02; LAB_52DAB = 0;
    ___SetIntVector(0xb, (void*)A4_DISP(0x99b4)); /* serial_read */

    LAB_52DC0 = 0x02; LAB_52DC1 = 0;
    ___SetIntVector(0, (void*)A4_DISP(0x99ca)); /* serial_write */
}

static void _free_serial_interrupt(void)
{
    printf("[HW] INTENA = 0x0801\n");
}

static void _closelibrary(void)
{
    _set_pri();
    ___CloseLibrary((void*)(uintptr_t)GfxBase_prg);
    ___CloseLibrary((void*)(uintptr_t)IntuitionBase_prg);
}

static void _set_pri(void) {}
static void _debug(void)   {}   /* empty in both 1989 and 1993 */

static void _openlibrary(void)
{
    _set_pri();
    GfxBase_prg = (uint32_t)(uintptr_t)___OpenLibrary("graphics.library", 0x1f);
    if (!GfxBase_prg) ___exit(1);
    IntuitionBase_prg = (uint32_t)(uintptr_t)___OpenLibrary("intuition.library", 0x1f);
    if (!IntuitionBase_prg) ___exit(1);
    pal = 0; /* real: (GfxBase+$ce & 4) */
}

/* _setup_display (conquest path + normal path, with the detailed comments from source) */
static void _setup_display(void)
{
    if (in_conquest != (int16_t)0xffff) {
        /* Load map variables
         * seed = level mod 8 + bytes8+9
         * added last, suggesting the 5x expansion from 100 to 495 levels was late */
        seed = (in_conquest & 7) + conq_08_seed;
        start_seed = seed;

        /* Terrain type 0-3 */
        ground_in = conq_05_terrain;
        ___load_ground(0, ground_in);

        _clear_map();

        /* #0 and #1 can be 0 to 10 */
        LAB_516DE = conquest;
        LAB_516E2 = conq_01_speed;

        /* 02 and #3 can be one of 25 values from 0 to 16,135
         * bitfields for which powers can be used */
        LAB_516E0 = (conq_02_enemypow << 3) | 7;
        LAB_516B2 = (conq_03_yourpow << 3) | 7;

        /* #4 game mode - build on people/towns/up only/mo build, swamps, water */
        game_mode = conq_04_mode;
    } else {
        start_seed = seed;
        if (seed) {
            if (___newrand() & 1) {
                if (1 /* $A,A5 == -1 */) {
                    ground_in++;
                    if (ground_in > 3) ground_in = 0;
                    ___load_ground(0, ground_in);
                }
            }
        }
        if (0xffff != ground_in) {
            ___load_ground(0, ground_in);
        }
        _clear_map();
    }

    { /* Generate the synthetic map before the first frame. */
        _make_alt();
        _make_map();   /* real version takes 4 args - stub for now */
        _make_woods_rocks();
    }

    ___Setscreen(0xffff, (void*)(uintptr_t)back_scr, (void*)(uintptr_t)back_scr);
    _draw_map();   /* real version takes 4 args from the chunk */

    ___copy_screen((void*)(uintptr_t)w_screen, (void*)(uintptr_t)back_scr);
    ___copy_screen((void*)(uintptr_t)d_screen, (void*)(uintptr_t)back_scr);

    ___Setscreen(0xffff, (void*)-1, (void*)(uintptr_t)w_screen);

    _place_first_people();
    seed++;

    /* Quick visible game init status for testing different levels via CLI */
    printf("[INIT] conquest=%d speed=%d enemy_pow=0x%02x your_pow=0x%02x mode=%d terrain=%d seed=%d\n",
           conquest, conq_01_speed, conq_02_enemypow, conq_03_yourpow, game_mode, ground_in, start_seed);
}


/* Visible minimal drawing so you actually SEE something (motivation!).
 * These touch the real chunky_buffer and call present.
 * Full accurate versions come when you paste more assembly.
 */
static void _clear_map(void)
{
    /* _alt is 65*65 words; cell buffers are each 64*64 bytes (asm 54274). */
    memset(A4_DISP(0xae86), 0, 65 * 65 * 2);
    memset(map_blk, 0, 4096);
    memset(map_alt, 0, 4096);
    memset(map_bk2, 0, 4096);
    memset(map_who, 0, 4096);
}

static void _make_alt(void)
{
    /* Valid synthetic plane for the bounded scenarios; original generator pending. */
    for (int row = 0; row <= 64; ++row)
        for (int column = 0; column <= 64; ++column) core_set_vertex(column,row,1);
}
static void _make_woods_rocks(void) {}

#if 0
static void _place_first_people(void)
{
    ensure_prg_screen();
    if (!prg_screen) return;
    /* seed a few bright demo peeps dots */
    for (int i = 0; i < 20; i++) {
        int px = 50 + (i * 7) % 200;
        int py = 50 + (i * 5) % 140;
        if (px < prg_screen->width && py < prg_screen->height)
            prg_screen->chunky_buffer[py * prg_screen->width + px] = prg_screen->palette[14]; /* bright */
    }
}
#endif

/* _move_mana asm:3164-3244, presentation only: the first _mana_values (A4
 * 0x8486) entry not below the player's mana picks the scale step, the
 * remainder (*8, divs) the eighth within it; sprite 69 is drawn at
 * x 160+16*(step-1)+2*eighth, y 8+8*(step-1)+eighth, past step 9 at
 * (0x137,0x57). Mana <= _mana_values[0] (unreachable: asm reads the long
 * before the table) is clamped to step 1 here. */
static void _move_mana(void)
{
    if (!game_assets || player < 0 || player > 1) return;
    int32_t mana = (int32_t)A4_LONG(0x9a02 + player*16);
    int step = 0;
    while (step < 10 && mana > (int32_t)A4_LONG(0x8486 + step*4)) ++step;
    int x = 0x137, y = 0x57;
    if (step <= 9) {
        if (!step) step = 1;
        int32_t low = (int32_t)A4_LONG(0x8486 + (step-1)*4), high = (int32_t)A4_LONG(0x8486 + step*4);
        int eighth = high != low ? (int)(int16_t)((mana - low) * 8 / (high - low)) : 0;
        x = (step-1)*16 + eighth*2 + 0xa0; y = (step-1)*8 + eighth + 8;
    }
    graphics_blit(game_pixels,64000,320,200,graphics_sprites(game_assets),69,x,y);
}

/* _pointer (A4 0x84b6) indexes _a_sprite[], built by _sprite_to_amiga
 * asm:16783-16830 from these _sprite_data frames. Values as the original:
 * query 0, terrain player*3+1 (asm:615-620, 1054-1056, 2110-2116), magnet
 * player+2 (asm:2125-2131), swamp 5 (asm:2042), after the swamp target
 * mode-1 (asm:7480-7487). Port deviation: the port keeps its magnet tool
 * after a swamp target, so its magnet pointer is restored there. */
static const uint8_t core_pointer_frames[8] = {0x44,0x4e,0x50,0x51,0x4f,0x53,0x52,0x81};
static void core_set_pointer(void)
{
    int own = player == 1;
    pointer = (int16_t)(core_swamp_selected ? 5 : core_tool == 2 ? 0 : core_tool == 1 ? own+2 : own*3+1);
}

/* Hardware sprite: _convert_sprite asm:19823-19846 drops the mask word and
 * attaches planes 0-1/2-3, so any non-zero plane value is opaque colour
 * 16+value (_palette 16-31 repeat 0-15, asm:24264). _move_pointer
 * asm:19928-19972 with x+0x40 (asm:19551) gives HSTART mousex+0x80 and VSTART
 * mousey+0x2c: the hotspot is the top-left pixel. Drawn over the whole
 * 320x256 frame, HUD included. */
static void core_draw_pointer(void)
{
    /* The host cursor would double the drawn game pointer; show it only when none is drawn. */
    int drawn = game_assets && (uint16_t)pointer <= 7;
    if (SDL_ShowCursor(SDL_QUERY) != (drawn ? SDL_DISABLE : SDL_ENABLE))
        SDL_ShowCursor(drawn ? SDL_DISABLE : SDL_ENABLE);
    if (!drawn) return;
    GraphicsSheet small = graphics_sprites(game_assets);
    size_t base = (size_t)core_pointer_frames[pointer] * 256;
    if (base + 256 > small.size) return;
    for (int row = 0; row < 16; ++row) for (int column = 0; column < 16; ++column) {
        int x = mousex + column, y = mousey + row, colour = small.indices[base + row*16 + column] & 15;
        if (colour && x >= 0 && x < 320 && y >= 0 && y < 256)
            prg_screen->chunky_buffer[y*320+x] = prg_screen->palette[colour];
    }
}

/* ___text/_text asm:19973-20052 on the 320x200 game_pixels: x is byte
 * aligned, '\n' moves 8 lines down, each 8x8 glyph is 8 x {mask, plane0..3}
 * from char 0x20. Kept pixels: (old & mask) | planes, as the bitplane AND/OR.
 * Linear plane addressing is emulated: overflowing columns wrap like the
 * original, beyond 8000 bytes per plane is skipped. */
static void amiga_text(uint8_t *pixels, int x, int y, const char *text)
{
    if (!game_font_loaded || !text) return;
    long line = (long)y * 40 + (x >> 3), at = line;
    for (; *text; ++text) {
        unsigned c = (unsigned char)*text;
        if (c == '\n') { line += 320; at = line; continue; }
        if (c >= 0x20 && c - 0x20 < sizeof game_font / 40) {
            const uint8_t *glyph = game_font + (c - 0x20) * 40;
            for (int row = 0; row < 8; ++row, glyph += 5) {
                long byte = at + row * 40;
                if (byte < 0 || byte >= 8000) continue;
                uint8_t *out = pixels + (byte / 40) * 320 + (byte % 40) * 8;
                for (int bit = 0; bit < 8; ++bit) {
                    int b = 7 - bit, colour = 0;
                    for (int plane = 0; plane < 4; ++plane)
                        colour |= ((((out[bit] >> plane) & (glyph[0] >> b)) | (glyph[1+plane] >> b)) & 1) << plane;
                    out[bit] = (uint8_t)colour;
                }
            }
        }
        ++at;
    }
}

/* _icon_data icons 12..23 (asm:21763-21927), the _requester tiles: per row
 * the plane 0..3 words, 16 rows. Other icons are not exported. */
static const uint16_t core_requester_icons[12][64] = {
    {0xffff,0x0000,0xffff,0x0000,0xc000,0x0000,0xffff,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdc2b,0x1000,0xefff,0x0000,0xdfff,0x17ff,0xe800,0x0000,0xdfd4,0x102b,0xe800,0x0000,0xdfff,0x13ff,0xe800,0x0000,0xde00,0x13ff,0xe800,0x0000,
     0xde00,0x13ff,0xe8ff,0x0000,0xde00,0x13ff,0xe8ff,0x0000,0xd200,0x17ff,0xe8ff,0x0000,0xde00,0x13ff,0xe8ff,0x0000,0xd200,0x17ff,0xe8ff,0x0000,0xde00,0x13ff,0xe8ff,0x0000,0xd200,0x17ff,0xe8ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000},
    {0xffff,0x0000,0xffff,0x0000,0x0000,0x0000,0xffff,0x0000,0xffff,0xffff,0x0000,0x0000,0xd42b,0x0000,0xffff,0x0000,0xffff,0xffff,0x0000,0x0000,0x2bd4,0xd42b,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,
     0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000},
    {0xfffd,0x0001,0xfffe,0x0000,0x0002,0x0003,0xfffc,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xd43b,0x001c,0xffe0,0x0000,0xfffb,0xffec,0x0000,0x0000,0x2bfb,0xd42c,0x0000,0x0000,0xfffb,0xffcc,0x0020,0x0000,0x00fb,0xffcc,0x0020,0x0000,
     0x007b,0xff4c,0xffa0,0x0000,0x007b,0xff4c,0xffa0,0x0000,0x004b,0xff5c,0xffa0,0x0000,0x007b,0xff4c,0xffa0,0x0000,0x004b,0xff5c,0xffa0,0x0000,0x007b,0xff4c,0xffa0,0x0000,0x004b,0xff5c,0xffa0,0x0000,0x007b,0xff7c,0xff80,0x0000},
    {0xdf00,0x1f00,0xe0ff,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,
     0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xa000,0x3fff,0xc000,0x0000,0xbfff,0xc000,0x0000,0x0000},
    {0x0000,0x0000,0xffff,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,
     0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
    {0x00fb,0x00fc,0xff00,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,
     0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0x0003,0xfffc,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
    {0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,
     0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000},
    {0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,
     0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000},
    {0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,
     0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000},
    {0x0000,0x0000,0xffff,0x0000,0xf800,0xffff,0x0000,0x0000,0xfbff,0xfdff,0x01ff,0x0200,0xfbff,0xfeff,0x02ff,0x0100,0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,
     0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,0xfa00,0xff00,0x0300,0x0000,0xf800,0xfe00,0x0200,0x0000,0xfc00,0xfc00,0x03ff,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0xffff,0xffff,0x0000,0xffff,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,
     0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0xffff,0x0000,0x003f,0xffff,0x0000,0x0000,0xff9f,0xffdf,0xffe0,0x0000,0xff1f,0xff9f,0xffa0,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,
     0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0x001f,0x0020,0x0000,0x001f,0x001f,0x0020,0x0000,0x001f,0x001f,0xffe0,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
};

/* _draw_icon asm:19184-19210: x in words (16 px), y in lines, the 4 plane
 * words replace the screen words. */
static void core_draw_icon(int x, int y, int icon)
{
    if (icon < 12 || icon > 23) return;
    const uint16_t *data = core_requester_icons[icon-12];
    for (int row = 0; row < 16; ++row) for (int bit = 0; bit < 16; ++bit) {
        int px = x*16 + bit, py = y + row, colour = 0;
        for (int plane = 0; plane < 4; ++plane) colour |= ((data[row*4+plane] >> (15-bit)) & 1) << plane;
        if (px >= 0 && px < 320 && py >= 0 && py < 200) game_pixels[py*320+px] = (uint8_t)colour;
    }
}

/* Button/menu records as the original's 46-byte entries: word x, word y,
 * word x2 (buttons) or width (menus), text at +6. */
#define core_rec_w(r,o) (((A4Word *)((r)+(o)))->value)
#define core_rec_text(r) ((char *)(r)+6)
#define core_end_ok     A4_DISP(0x84c6)
#define core_end_cancel A4_DISP(0x84f4)
#define core_option_entry(i) A4_DISP(0x8522 + (i)*46)
#define core_game_entry(i)   A4_DISP(0x87ee + (i)*46)
#define core_set_entry(i)    A4_DISP(0x8fc0 + (i)*46)
typedef struct { int16_t x, y, w; const char *text; } CoreMenuEntry;
static void core_menu_install(uint8_t *table, const CoreMenuEntry *entries, int count)
{
    for (int i = 0; i < count; ++i, table += 46) {
        core_rec_w(table,0) = entries[i].x; core_rec_w(table,2) = entries[i].y; core_rec_w(table,4) = entries[i].w;
        memset(table+6, 0, 40); strcpy(core_rec_text(table), entries[i].text);
    }
}
/* _end_ok/_end_cancel asm:24349-24362. */
static void core_buttons_install(void)
{
    static const CoreMenuEntry buttons[] = {{0,0,0,"OK"},{0,0,0,"CANCEL"}};
    core_menu_install(core_end_ok, buttons, 1); core_menu_install(core_end_cancel, buttons+1, 1);
}

/* _requester asm:9901-10270: a 16 px tile frame at x/16 (title 12,13,14;
 * sides 18,20,19 every 16 lines; bottom 15,16,17), ok (and cancel) as
 * 21,22..,23 button tiles in the bottom row. The buttons get x, y (the
 * bottom row) and x2 = x + (strlen+4)*8; labels at x+16, y+3. The
 * original clears _tempo on every call; the port clears it when a
 * requester opens (core_requester_layout), and draws it every frame. */
static void core_requester(int x, int y, int w, int h, uint8_t *ok, uint8_t *cancel, int draw)
{
    int left = x >> 4, tiles = w >> 4, half = (int)strlen(core_rec_text(ok)) >> 1, row, column;
#define tile(c,r,i) do { if (draw) core_draw_icon(left+(c),(r),(i)); } while (0)
    tile(0,y,12);
    for (column = 1; column < tiles-1; ++column) tile(column,y,13);
    tile(column,y,14);
    for (row = y+16; row < y+h-16; row += 16) {
        tile(0,row,18);
        for (column = 1; column < tiles-1; ++column) tile(column,row,20);
        tile(column,row,19);
    }
    tile(0,row,15);
    column = 1; core_rec_w(ok,2) = (int16_t)row;
    if (cancel) {
        int length = (int)strlen(core_rec_text(cancel)) >> 1;
        core_rec_w(ok,0) = (int16_t)((left+column) << 4); core_rec_w(cancel,2) = core_rec_w(ok,2);
        tile(column,row,21); ++column;
        for (int i = 0; i < half; ++i, ++column) tile(column,row,22);
        tile(column,row,23); ++column;
        if (draw) amiga_text(game_pixels, core_rec_w(ok,0)+16, row+3, core_rec_text(ok));
        for (; column < tiles - length - 3; ++column) tile(column,row,16);
        core_rec_w(cancel,0) = (int16_t)((left+column) << 4);
        tile(column,row,21); ++column;
        for (int i = 0; i < length; ++i, ++column) tile(column,row,22);
        tile(column,row,23); ++column;
        core_rec_w(cancel,4) = (int16_t)(core_rec_w(cancel,0) + ((int)strlen(core_rec_text(cancel))+4)*8);
        if (draw) amiga_text(game_pixels, core_rec_w(cancel,0)+16, row+3, core_rec_text(cancel));
    } else {
        for (; column < (tiles>>1) - (half>>1) - 1; ++column) tile(column,row,16);
        core_rec_w(ok,0) = (int16_t)((left+column) << 4);
        tile(column,row,21); ++column;
        for (int i = 0; i < half; ++i, ++column) tile(column,row,22);
        tile(column,row,23); ++column;
        for (; column < tiles-1; ++column) tile(column,row,16);
    }
    core_rec_w(ok,4) = (int16_t)(core_rec_w(ok,0) + ((int)strlen(core_rec_text(ok))+4)*8);
    tile(column,row,17);
#undef tile
    if (draw) amiga_text(game_pixels, core_rec_w(ok,0)+16, row+3, core_rec_text(ok));
}

/* _check_cancel asm:12106-12153 at _mousex/_mousey: 1 inside ok, 2 inside
 * cancel (x..x2, y..y+16 inclusive), else 0. The port calls it for a left
 * press only, so the _left_button gate is the event itself. */
static int core_check_cancel(const uint8_t *ok, const uint8_t *cancel)
{
    if (ok && mousex >= core_rec_w(ok,0) && mousex <= core_rec_w(ok,4) &&
        mousey >= core_rec_w(ok,2) && mousey <= core_rec_w(ok,2)+16) return 1;
    if (cancel && mousex >= core_rec_w(cancel,0) && mousex <= core_rec_w(cancel,4) &&
        mousey >= core_rec_w(cancel,2) && mousey <= core_rec_w(cancel,2)+16) return 2;
    return 0;
}

/* Menu hit test of _options/_game_options/_set_options: x+dx..x+w+dx,
 * y+dy..y+dy+8 inclusive. */
static int core_menu_hit(const uint8_t *entry, int dx, int dy)
{
    return mousex >= core_rec_w(entry,0)+dx && mousex <= core_rec_w(entry,0)+core_rec_w(entry,4)+dx &&
           mousey >= core_rec_w(entry,2)+dy && mousey <= core_rec_w(entry,2)+dy+8;
}

/* _option_text asm:24363-24493 (A4 0x8522), _game_text asm:24494-24789
 * (0x87ee), _set_text asm:24845-24914 (0x8fc0). 0x80/0x81 are the font's
 * empty/ticked box, 0x84..0x86 the slider, 0x87 its marker. */
static const CoreMenuEntry core_option_template[14] = {
    {0x24,0x10,0x80,"OPTIONS FOR EVIL"},
    {0x20,0x20,0x08,"\x80 CAN MODIFY LAND"},     {0x20,0x29,0x08,"\x80 CAN ATTACK TOWNS"},
    {0x20,0x32,0x08,"\x80 CAN ATTACK LEADER"},   {0x20,0x3b,0x08,"\x80 CAN USE EARTHQUAKES"},
    {0x20,0x43,0x08,"\x80 CAN USE SWAMP"},       {0x20,0x4c,0x08,"\x80 CAN USE KNIGHT"},
    {0x20,0x55,0x08,"\x80 CAN USE VOLCANO"},     {0x20,0x5e,0x08,"\x80 CAN USE FLOOD"},
    {0x20,0x67,0x08,"\x80 CAN USE ARMAGEDDON"},
    {0x64,0x7a,0x20,"AGGRESSION"}, {0x48,0x83,0x68,"LOW\x84\x85\x85\x85\x85\x85\x85\x85\x85\x86HIGH"},
    {0x78,0x8d,0x20,"RATE"},       {0x40,0x96,0x70,"SLOW\x84\x85\x85\x85\x85\x85\x85\x85\x85\x86""FAST"},
};
static const CoreMenuEntry core_game_template[17] = {
    {0x70,0x10,0x48,"GAME SETUP"},
    {0x10,0x24,0x08,"\x80 ONE PLAYER"},     {0x98,0x24,0x08,"\x80 TWO PLAYERS"},
    {0x10,0x2e,0x08,"\x80 PLAY GAME"},      {0x98,0x2e,0x08,"\x80 PAINT MAP"},
    {0x10,0x38,0x08,"\x80 GOOD"},           {0x98,0x38,0x08,"\x80 EVIL"},
    {0x10,0x42,0x08,"\x80 HUMAN VS AMIGA"}, {0x98,0x42,0x08,"\x80 AMIGA VS AMIGA"},
    {0x10,0x4c,0x08,"\x80 CONQUEST"},       {0x98,0x4c,0x08,"\x80 CUSTOM GAME"},
    {0x50,0x5f,0x80,"\x80 GAME OPTIONS"},   {0x50,0x69,0x80,"\x80 SAVE A GAME"},
    {0x50,0x73,0x80,"\x80 LOAD A GAME"},    {0x50,0x7d,0x80,"\x80 MOVE TO NEXT MAP"},
    {0x50,0x87,0x80,"\x80 RESTART THIS MAP"}, {0x50,0x91,0x80,"\x80 SURRENDER THIS GAME"},
};
static const CoreMenuEntry core_set_template[11] = {
    {0x30,0x19,0x76,"SET GAME OPTIONS"},
    {0x10,0x28,0x08,"\x80 WATER IS HARMFUL"}, {0x10,0x28,0x08,"\x81 WATER IS FATAL"},
    {0x10,0x32,0x08,"\x80 SWAMPS SHALLOW"},   {0x10,0x32,0x08,"\x81 SWAMPS BOTTOMLESS"},
    {0x10,0x3c,0x08,"\x80 CAN BUILD"},        {0x10,0x3c,0x08,"\x81 CANNOT BUILD"},
    {0x10,0x46,0x08,"\x80 BUILD UP AND DOWN"},{0x10,0x46,0x08,"\x81 ONLY BUILD UP"},
    {0x10,0x50,0x08,"\x80 BUILD NEAR PEOPLE"},{0x10,0x50,0x08,"\x81 BUILD NEAR TOWNS"},
};
/* _blank asm:24915-24923: 28 '^' glyphs. */
static const char core_blank[] = "^^^^^^^^^^^^^^^^^^^^^^^^^^^^";

static uint8_t *core_stats(int side) { return A4_DISP(0x82b6 + side*46); }

/* _options asm:13617-13660 window, then each pass of LAB_48228 (asm:13661-13740):
 * entry 0 names the side, 1..9 tick the _stats+0xe power bits. */
static void core_options_refresh(void)
{
    strcpy(core_rec_text(core_option_entry(0)) + 12, core_req.side ? "EVIL" : "GOOD");
    for (int i = 1; i < 10; ++i)
        core_rec_text(core_option_entry(i))[0] = (char)(0x80 + !!(core_rec_w(core_stats(core_req.side),0xe) & (1 << (i-1))));
}
static void core_requester_layout(void)
{
    if (core_req.kind == CORE_REQ_OPTIONS) core_requester(0x10,0x10,0x120,0xa6,core_end_ok,core_end_cancel,0);
    else if (core_req.kind == CORE_REQ_GAME) core_requester(0x10,0x10,0x120,0xb0,core_end_ok,core_end_cancel,0);
    else if (core_req.kind == CORE_REQ_SET) core_requester(0x20,0x18,0x100,0x70,core_end_ok,core_end_cancel,0);
    else if (core_req.kind == CORE_REQ_WORLD) core_world_layout(0);
    else if (core_req.kind == CORE_REQ_FILE) core_file_layout(0);
    if (core_req.kind) tempo = 0;
}
/* _options(side) asm:13617-13660: keep a copy of _stats[side], show the
 * aggression (+0xc) and rate (+0x10) as 10-value. */
static void core_options_open(int side)
{
    uint8_t *own = core_stats(side);
    core_req.kind = CORE_REQ_OPTIONS; core_req.side = side;
    memcpy(core_req.saved_stats, own, 46);
    core_buttons_install(); core_menu_install(core_option_entry(0), core_option_template, 14);
    core_requester_layout();
    core_rec_w(own,0xc) = (int16_t)(10 - core_rec_w(own,0xc));
    core_rec_w(own,0x10) = (int16_t)(10 - core_rec_w(own,0x10));
    core_options_refresh();
}
/* _options click, asm:13741-13873: only a custom game (in_conquest -1)
 * edits; entry 1..9 toggles bit i-1, the sliders take (mousex-(x+0x28 or
 * 0x30))/8 if 0..9. Ok re-inverts, cancel restores the copy. The copy's
 * first 3 bytes are the _do_action mailbox, which the original clears after
 * the return (asm:18189-18215); the port cleared it already, so they stay. */
static void core_options_click(void)
{
    uint8_t *own = core_stats(core_req.side);
    int i = 1;
    while (i < 14 && !core_menu_hit(core_option_entry(i),16,16)) ++i;
    if (in_conquest == -1) {
        if (i < 10) core_rec_w(own,0xe) ^= (int16_t)(1 << (i-1));
        else if (i == 11 || i == 13) {
            int value = (int16_t)(mousex - (core_rec_w(core_option_entry(i),0) + (i == 11 ? 0x28 : 0x30))) / 8;
            if (value >= 0 && value <= 9) core_rec_w(own, i == 11 ? 0xc : 0x10) = (int16_t)value;
        }
    }
    int result = core_check_cancel(core_end_ok, core_end_cancel);
    if (!result) { core_options_refresh(); return; }
    if (result == 2) memcpy(own+3, core_req.saved_stats+3, 43);
    else {
        core_rec_w(own,0xc) = (int16_t)(10 - core_rec_w(own,0xc));
        core_rec_w(own,0x10) = (int16_t)(10 - core_rec_w(own,0x10));
    }
    core_req.kind = CORE_REQ_NONE; left_button = 2;  /* _return_to_game asm:13874-13885 */
}

/* _game_options asm:12801-12877: the saved values and the ticks. */
static void core_game_options_open(void)
{
    core_req.kind = CORE_REQ_GAME;
    core_req.saved_serial = serial_off; core_req.saved_paint = paint_map; core_req.saved_player = player;
    core_req.saved_human = core_rec_w(core_stats(player),6); core_req.saved_conquest = in_conquest;
    A4_WORD(0x3080) = 0;  /* _bring_up_two */
    core_buttons_install(); core_menu_install(core_game_entry(0), core_game_template, 17);
    static const int ticks[] = {1,2,3,4,5,6,7,8,9,10};
    int16_t human = core_rec_w(core_stats(player),6);
    int16_t values[] = {serial_off, !serial_off, !paint_map, paint_map, !player, player, !human, human,
                        in_conquest != -1, in_conquest == -1};
    for (int k = 0; k < 10; ++k) core_rec_text(core_game_entry(ticks[k]))[0] = (char)(0x80 + values[k]);
    A4_WORD(0x3054) = 0;  /* _saved */
    core_requester_layout();
}
static void core_game_tick(int on, int off)
{
    core_rec_text(core_game_entry(on))[0] = (char)0x81; core_rec_text(core_game_entry(off))[0] = (char)0x80;
}
/* GOOD/EVIL asm:12974-13113: swap the _stats+6 human flags into the new
 * side; _set_tend_icons/_set_mode_icons and _pointer follow from state. */
static void core_game_side(int side)
{
    int16_t other = core_rec_w(core_stats(not_player),6);
    core_rec_w(core_stats(side),6) = core_rec_w(core_stats(player),6);
    core_rec_w(core_stats(!side),6) = other;
    player = (int16_t)side; not_player = player ? 0 : 1;
    core_set_pointer();
}
static void core_set_options_open(void);
/* _do_action's _new_map (asm:18917-18932) with in_conquest -1: 1 (MOVE
 * TO NEXT MAP, CONQUEST's number) keeps the live _seed, anything else
 * (RESTART, ground_in+2) reloads _start_seed; _setup_display(0, new_map-2). */
static int core_custom_new_map(int16_t new_map)
{
    return core_custom_start(new_map == 1 ? seed : start_seed, (int16_t)(new_map - 2));
}
/* Leaving _game_options: _return_to_game, then _do_action's _new_map
 * (asm:18917-18932): _setup_display restarts the map. A conquest start
 * restarts the current world (both 1 and ground_in+2 rebuild the same
 * conquest level); with in_conquest -1 it builds a custom landscape. */
static void core_game_options_close(void)
{
    core_req.kind = CORE_REQ_NONE; left_button = 2;
    int16_t new_map = A4_WORD(0x309a);
    if (new_map && !(in_conquest == -1 ? core_custom_new_map(new_map) : game_reset_start(current_start)))
        fprintf(stderr, "Game options: map restart failed; keeping the current game.\n");
}
/* _game_options click, asm:12878-13350. In a conquest game entries 0..8
 * are ignored. Inert here: TWO PLAYERS (_two_players, serial); SAVE A GAME / LOAD A GAME open
 * _save_load (core_file_open), LOAD only with _serial_off; ONE PLAYER only
 * ticks, as _serial_off is always 1.
 * CONQUEST (asm:13221-13239) opens _show_world with an empty _message. */
static void core_game_options_click(void)
{
    int i = 0;
    while (i < 17 && !core_menu_hit(core_game_entry(i),16,16)) ++i;
    if (i < 9 && in_conquest != -1) i = 0x13;
    switch (i) {
    case 1:
        core_game_tick(1,2);
        if (!serial_off) {
            core_rec_w(core_stats(player),6) = 1;  /* CLR then MOVE #1, asm:12930-12935 */
            core_game_tick(7,8); serial_off = 1;
        }
        break;
    case 3: core_game_tick(3,4); paint_map = 0; break;
    case 4: core_game_tick(4,3); paint_map = 1; break;
    case 5: core_game_tick(5,6); core_game_side(0); break;
    case 6: core_game_tick(6,5); core_game_side(1); break;
    case 7:
        core_game_tick(7,8); core_rec_w(core_stats(player),6) = 0;
        if (serial_off) core_rec_w(core_stats(!player),6) = 1;
        break;
    case 8:
        core_game_tick(8,7); core_rec_w(core_stats(player),6) = 1;
        if (serial_off) core_rec_w(core_stats(!player),6) = 1;
        break;
    case 9: core_game_tick(9,10); core_message[0] = 0; core_world_open(CORE_WORLD_OPTIONS); return;
    case 10: core_game_tick(10,9); in_conquest = -1; break;
    case 11: core_set_options_open(); return;
    case 12: core_file_open(1); return;                     /* asm:13259-13266 */
    case 13: if (serial_off) { core_file_open(0); return; } break;   /* asm:13267-13276 */
    case 14: A4_WORD(0x309a) = 1; core_game_options_close(); return;
    case 15: A4_WORD(0x309a) = (int16_t)(ground_in + 2); core_game_options_close(); return;
    case 16: A4_WORD(0x8258) = player; core_game_options_close(); return;  /* _surender */
    default: break;
    }
    int result = core_check_cancel(core_end_ok, core_end_cancel);
    if (!result) return;
    if (result == 2) {
        /* asm:13325-13350 */
        serial_off = core_req.saved_serial; paint_map = core_req.saved_paint;
        if (player != core_req.saved_player) player = core_req.saved_player;
        not_player = player ? 0 : 1;
        core_rec_w(core_stats(player),6) = core_req.saved_human;
        in_conquest = core_req.saved_conquest;
        core_set_pointer();
    }
    core_game_options_close();
}

/* _set_options asm:13362-13411. */
static void core_set_options_open(void)
{
    core_req.kind = CORE_REQ_SET; core_req.saved_mode = game_mode;
    core_menu_install(core_set_entry(0), core_set_template, 11);
    core_requester_layout();
}
/* _set_options click, asm:13412-13509: custom games only; entries 1,3,..9
 * toggle _game_mode bit i/2. Then back to _game_options (-4 = 1 redraws). */
static void core_set_options_click(void)
{
    if (in_conquest == -1)
        for (int i = 1; i < 10; i += 2)
            if (core_menu_hit(core_set_entry(i),0x20,0x18)) { game_mode ^= (int16_t)(1 << (i >> 1)); break; }
    int result = core_check_cancel(core_end_ok, core_end_cancel);
    if (!result) return;
    if (result == 2) game_mode = core_req.saved_mode;
    left_button = 2; core_req.kind = CORE_REQ_GAME; core_requester_layout();
}

/* ---- SAVE A GAME / LOAD A GAME: _save_game asm:17123-17385, _checksum
 * asm:17389-17407, _load_game asm:17412-17703.
 * The file is the raw A4 regions in _Write order, big-endian as the 68000
 * wrote them. The port's A4 holds host-endian words/longs, so each region
 * is converted per field; peep pointers (peep +14, _stats +$22/$26/$2a,
 * here slot+1) are written as Amiga _peeps addresses $53014+22*slot. */
#define CORE_SAVE_SIGNATURE 0xffffb615u   /* asm:17125, 17453 */
#define CORE_SAVE_PEEPS     0x53014u      /* _peeps (LAB_53018-4) in the IRA image */
enum { CORE_SAVE_BYTES, CORE_SAVE_WORDS, CORE_SAVE_LONG, CORE_SAVE_MAGNET, CORE_SAVE_PEEPLIST,
       CORE_SAVE_STATS, CORE_SAVE_CONQUEST };
static const struct { int offset, size, kind; } core_save_regions[] = {
    {0xae86,0x2102,CORE_SAVE_WORDS},     /* _alt, 65x65 words */
    {0xdf88,0x1000,CORE_SAVE_BYTES},     /* _map_alt */
    {0xcf88,0x1000,CORE_SAVE_BYTES},     /* _map_blk */
    {0xef88,0x1000,CORE_SAVE_BYTES},     /* _map_bk2 */
    {0x1f88,0x1000,CORE_SAVE_BYTES},     /* _map_who */
    {0x99f6,0x20,CORE_SAVE_MAGNET},      /* _magnet[2]: 4 words, good_pop/bad_pop, mana */
    {0x9c26,0x1222,CORE_SAVE_PEEPLIST},  /* _peeps, 211 22-byte records */
    {0x82b6,0x2e,CORE_SAVE_STATS},       /* _stats */
    {0x82e4,0x2e,CORE_SAVE_STATS},       /* strABC, the second _stats record */
    {0x8256,2,CORE_SAVE_WORDS},          /* _mode */
    {0x99e2,2,CORE_SAVE_WORDS},          /* _seed */
    {0x99e4,2,CORE_SAVE_WORDS},          /* _start_seed */
    {0x99f4,2,CORE_SAVE_WORDS},          /* _player */
    {0x99e6,2,CORE_SAVE_WORDS},          /* _ground_in */
    {0xae80,2,CORE_SAVE_WORDS},          /* _war */
    {0xae6a,2,CORE_SAVE_WORDS},          /* _game_turn */
    {0x84bc,10,CORE_SAVE_CONQUEST},      /* _conquest: 8 bytes, seed word */
    {0x84ba,2,CORE_SAVE_WORDS},          /* _in_conquest */
    {0x9c24,2,CORE_SAVE_WORDS},          /* _paint_map */
    {0xae7a,4,CORE_SAVE_WORDS},          /* _battle_won[2] */
    {0x84b8,2,CORE_SAVE_WORDS},          /* _game_mode */
    {0xae7e,2,CORE_SAVE_WORDS},          /* _level_number */
    {0xae72,4,CORE_SAVE_LONG},           /* _score */
};
/* 4 + regions + the checksum word: 29646 bytes. */
#define CORE_SAVE_SIZE (4 + 0x2102 + 4*0x1000 + 0x20 + 0x1222 + 2*0x2e + 7*2 + 10 + 2 + 2 + 4 + 2 + 2 + 4 + 2)
#define CORE_SAVE_MAGNET_AT (4 + 0x2102 + 4*0x1000)
#define CORE_SAVE_SCALARS_AT (CORE_SAVE_MAGNET_AT + 0x20 + 0x1222 + 2*0x2e)   /* _mode */

/* Field width at byte "at" of a region; *is_pointer marks a peep pointer. */
static int core_save_unit(int kind, int at, int *is_pointer)
{
    *is_pointer = 0;
    switch (kind) {
    case CORE_SAVE_WORDS: return 2;
    case CORE_SAVE_LONG: return 4;
    case CORE_SAVE_MAGNET: return at % 16 < 8 ? 2 : 4;
    case CORE_SAVE_PEEPLIST:
        at %= 22;   /* bytes 0-3, words 4-12, pointer 14, word 18, bytes 20-21 */
        if (at == 14) { *is_pointer = 1; return 4; }
        return at < 4 || at >= 20 ? 1 : 2;
    case CORE_SAVE_STATS:   /* bytes 0-3, words 4-$20, pointers $22/$26/$2a */
        if (at >= 0x22) { *is_pointer = 1; return 4; }
        return at < 4 ? 1 : 2;
    case CORE_SAVE_CONQUEST: return at < 8 ? 1 : 2;
    default: return 1;
    }
}
static void core_put_be(uint8_t *out, uint32_t value, int size)
{
    for (int i = size-1; i >= 0; --i, value >>= 8) out[i] = (uint8_t)value;
}
static uint32_t core_get_be(const uint8_t *in, int size)
{
    uint32_t value = 0;
    for (int i = 0; i < size; ++i) value = value << 8 | in[i];
    return value;
}
/* _checksum(_alt,$1081) + 4x _checksum(map,$800) + the low word of
 * good_pop+bad_pop (asm:17161-17189), over the big-endian file image:
 * _alt and the four maps are contiguous from offset 4. */
static uint16_t core_save_checksum(const uint8_t *file)
{
    uint16_t sum = 0;
    for (int at = 4; at < CORE_SAVE_MAGNET_AT; at += 2) sum = (uint16_t)(sum + core_get_be(file+at,2));
    return (uint16_t)(sum + core_get_be(file+CORE_SAVE_MAGNET_AT+8,4) + core_get_be(file+CORE_SAVE_MAGNET_AT+0x18,4));
}
/* The live A4 as the file image, checksum included. */
static uint16_t core_save_encode(uint8_t *file)
{
    int at = 4;
    core_put_be(file, CORE_SAVE_SIGNATURE, 4);
    for (size_t r = 0; r < sizeof core_save_regions / sizeof *core_save_regions; ++r) {
        const uint8_t *base = A4_DISP(core_save_regions[r].offset);
        for (int k = 0, size, is_pointer; k < core_save_regions[r].size; k += size, at += size) {
            size = core_save_unit(core_save_regions[r].kind, k, &is_pointer);
            uint32_t value = size == 1 ? base[k] : size == 2 ? (uint16_t)((const A4Word *)(base+k))->value
                                                             : ((const A4Long *)(base+k))->value;
            if (is_pointer) value = value && value <= 211 ? CORE_SAVE_PEEPS + 22*(value-1) : 0;
            core_put_be(file+at, value, size);
        }
    }
    uint16_t sum = core_save_checksum(file);
    core_put_be(file+at, sum, 2);
    return sum;
}
/* The file image into the (staged) A4. An address that is not a _peeps
 * record becomes 0, in a peep the peep itself so _get_heading still runs. */
static void core_save_decode(const uint8_t *file)
{
    int at = 4;
    for (size_t r = 0; r < sizeof core_save_regions / sizeof *core_save_regions; ++r) {
        uint8_t *base = A4_DISP(core_save_regions[r].offset);
        int kind = core_save_regions[r].kind;
        for (int k = 0, size, is_pointer; k < core_save_regions[r].size; k += size, at += size) {
            size = core_save_unit(kind, k, &is_pointer);
            uint32_t value = core_get_be(file+at, size);
            if (is_pointer && value) {
                uint32_t slot = (value - CORE_SAVE_PEEPS) / 22;
                if (value >= CORE_SAVE_PEEPS && (value - CORE_SAVE_PEEPS) % 22 == 0 && slot < 208) value = slot+1;
                else value = kind == CORE_SAVE_PEEPLIST && k/22 < 208 ? (uint32_t)(k/22+1) : 0;
            }
            if (size == 1) base[k] = (uint8_t)value;
            else if (size == 2) ((A4Word *)(base+k))->value = (int16_t)value;
            else ((A4Long *)(base+k))->value = value;
        }
    }
}

/* Port: save files live under POPULOUS_SAVE_DIR, else $XDG_DATA_HOME/populous/saves,
 * else ~/.local/share/populous/saves, else ./saves. The original's paths
 * were relative to the current AmigaDOS directory; here a relative drawer or
 * name is relative to that base, an absolute one ('/') is used as it is. */
static int core_save_base(char *out, size_t size)
{
    const char *env = getenv("POPULOUS_SAVE_DIR"), *home = getenv("HOME");
    int length;
    if (env && *env) length = snprintf(out, size, "%s", env);
    else if ((env = getenv("XDG_DATA_HOME")) && *env) length = snprintf(out, size, "%s/populous/saves", env);
    else if (home && *home) length = snprintf(out, size, "%s/.local/share/populous/saves", home);
    else length = snprintf(out, size, "saves");
    return length > 0 && (size_t)length < size;
}
static int core_save_path(char *out, size_t size, const char *name)
{
    char base[4096];
    if (name[0] == '/') return (size_t)snprintf(out, size, "%s", name) < size;
    if (!core_save_base(base, sizeof base)) return 0;
    return (size_t)snprintf(out, size, "%s/%s", base, name) < size;
}
static void core_save_mkdirs(void)
{
    char path[4096];
    if (!core_save_base(path, sizeof path)) return;
    for (char *slash = path + 1; ; ++slash) {
        if (*slash && *slash != '/') continue;
        char end = *slash; *slash = 0;
        mkdir(path, 0777);   /* existing components fail with EEXIST */
        if (!end) break;
        *slash = end;
    }
}
/* _save_game/_load_game name: ".GAM" unless it already ends in it
 * (asm:17128-17154, 17414-17440). */
static int core_save_file_name(char *out, size_t size, const char *name)
{
    size_t length = strlen(name);
    char full[4096];
    if ((size_t)snprintf(full, sizeof full, "%s%s", name,
                         length >= 4 && !strcmp(name+length-4, ".GAM") ? "" : ".GAM") >= sizeof full) return 0;
    return core_save_path(out, size, full);
}

/* _save_game: 1 on success (the original returns 0 then). Port: written to
 * a temporary file and renamed, so a failed save leaves no partial file. */
static int core_save_game(const char *name)
{
    char path[4160], temporary[4200];
    static uint8_t file[CORE_SAVE_SIZE];
    if (!core_save_file_name(path, sizeof path, name)) return 0;
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    core_save_mkdirs();
    FILE *out = fopen(temporary, "wb");
    if (!out) return 0;
    A4_WORD(0x3082) = (int16_t)core_save_encode(file);   /* _land_checksum, asm:17189 */
    int ok = fwrite(file, 1, sizeof file, out) == sizeof file;
    if (fclose(out)) ok = 0;
    if (ok && rename(temporary, path)) ok = 0;
    if (!ok) remove(temporary);
    return ok;
}

/* _load_game: 1 if the game was loaded. The original only checks the
 * signature and then reads field by field. Port: the whole file is read
 * and checked first (exact size, signature, checksum, _ground_in 0..3,
 * _player 0..1, the LAND rules and graphics of _ground_in), then staged and
 * published at once, so a rejected file leaves the game untouched. */
static int core_load_game(const char *name)
{
    char path[4160], error[160];
    static uint8_t file[CORE_SAVE_SIZE + 1];
    if (!core_save_file_name(path, sizeof path, name)) return 0;
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    size_t count = fread(file, 1, sizeof file, in);
    fclose(in);
    if (count != CORE_SAVE_SIZE || core_get_be(file,4) != CORE_SAVE_SIGNATURE ||
        core_get_be(file+CORE_SAVE_SIZE-2,2) != core_save_checksum(file)) {
        fprintf(stderr, "Load game: %s is not a saved game.\n", path);
        return 0;
    }
    uint32_t loaded_player = core_get_be(file+CORE_SAVE_SCALARS_AT+6,2);
    uint32_t terrain = core_get_be(file+CORE_SAVE_SCALARS_AT+8,2);
    if (loaded_player > 1 || terrain > 3) {
        fprintf(stderr, "Load game: %s has an unsupported player or landscape.\n", path);
        return 0;
    }
    /* ___load_ground(_ground_in,0), asm:17586-17589. */
    const char *asset_directory = getenv("POPULOUS_ASSET_DIR");
    if (!asset_directory) asset_directory = POPULOUS_DEFAULT_ASSET_DIR;
    char default_data[4096];
    if ((size_t)snprintf(default_data, sizeof default_data, "%s/raw", asset_directory) >= sizeof default_data) return 0;
    const char *directory = getenv("POPULOUS_DATA_DIR");
    if (!directory) directory = default_data;
    LandRules rules;
    if (!land_rules_load(directory, terrain, &rules, error, sizeof error)) {
        fprintf(stderr, "Load game: LAND data: %s\n", error);
        return 0;
    }
    GraphicsAssets *assets = graphics_load(asset_directory, terrain, rules.sprite_set, error, sizeof error);
    if (!assets) { fprintf(stderr, "Load game: graphics: %s\n", error); return 0; }
    _Alignas(4) static uint8_t staged[sizeof prg_a4_area];
    memcpy(staged, prg_a4_area, sizeof staged);
    uint8_t *live = prg_a4;
    prg_a4 = staged + 32768;
    core_save_decode(file);
    A4_WORD(0x3082) = (int16_t)core_get_be(file+CORE_SAVE_SIZE-2,2);   /* _land_checksum */
    core_install_land(&rules);
    /* asm:17590-17682 */
    not_player = player ? 0 : 1;
    A4_WORD(0xae78) = core_target(0); A4_WORD(0xae76) = core_target(1);   /* _god_magnet/_devil_magnet */
    int last = 207;
    while (last >= 0 && !peep_word(last,4)) --last;
    no_peeps = (int16_t)(last + 1);
    for (int owner = 0; owner < 2; ++owner) memset(A4_DISP(0x82b6 + owner*46), 0, 3);
    for (int person = 0; person < no_peeps; ++person) {
        uint32_t raw; memcpy(&raw, peeps+person*22+14, sizeof raw);
        if (raw) core_get_heading(person);
    }
    prg_a4 = live;
    memcpy(prg_a4_area, staged, sizeof prg_a4_area);
    graphics_free(game_assets);
    game_assets = assets;
    /* _mode 2 sets the terrain _pointer (asm:17671-17682); the port's
     * pointer follows its tool, reset to terrain. */
    core_result = core_tool = core_swamp_selected = 0; core_set_pointer();
    core_end_shown = 0; core_end_next = -1;
    core_custom_live = 1;
    /* RESTART THIS MAP rebuilds the loaded world (port bookkeeping; the
     * synthetic demo starts have no original level to point at). */
    if (in_conquest == -1) {
        current_start = (StartSelection){START_CUSTOM, 0};
        core_land = (CustomLand){start_seed, 0, ground_in};
    } else if (current_start.kind != START_SYNTHETIC && in_conquest >= 0 && in_conquest / 5 < LEVEL_COUNT)
        current_start = (StartSelection){START_ORIGINAL, in_conquest / 5};
    return 1;
}

/* _do_action y=13 asm:18983-18999: _ground_in = x, ___load_ground(x,1),
 * redraw. The port has LAND rules and graphics only for 0..3 (keys 1..4);
 * 4..8 ("custom", keys 5..9) keep the current landscape. */
static int core_load_ground(int terrain)
{
    if (terrain < 0 || terrain > 3) {
        fprintf(stderr, "Paint map: landscape %d is not available.\n", terrain);
        return 0;
    }
    char error[256];
    const char *asset_directory = getenv("POPULOUS_ASSET_DIR");
    if (!asset_directory) asset_directory = POPULOUS_DEFAULT_ASSET_DIR;
    char default_data[4096];
    if ((size_t)snprintf(default_data, sizeof default_data, "%s/raw", asset_directory) >= sizeof default_data) return 0;
    const char *directory = getenv("POPULOUS_DATA_DIR");
    if (!directory) directory = default_data;
    LandRules rules;
    if (!land_rules_load(directory, (unsigned)terrain, &rules, error, sizeof error)) {
        fprintf(stderr, "Paint map: LAND data: %s\n", error);
        return 0;
    }
    GraphicsAssets *assets = graphics_load(asset_directory, (unsigned)terrain, rules.sprite_set, error, sizeof error);
    if (!assets) { fprintf(stderr, "Paint map: graphics: %s\n", error); return 0; }
    ground_in = (int16_t)terrain;
    core_install_land(&rules);
    graphics_free(game_assets);
    game_assets = assets;
    return 1;
}

/* _save_load(save) asm:10493-10972: _requester(d_screen,0x48,0x10,0xb0,0xb0)
 * with "SAVE"/"LOAD" and CANCEL, 12 rows of the _drawer listing at
 * 0x58,0x22+8*row, arrows 0x88/0x89 at 0xd8, then _drawer (0x8d) and
 * _filename (0xa0), each 16 boxes. _drawer/_filename (20 bytes each) persist
 * in the A4. The original loops until OK or CANCEL; here it is a requester. */
#define core_drawer   ((char *)A4_DISP(0x302c))
#define core_filename ((char *)A4_DISP(0x3040))
static char *core_file_field(void) { return core_file.field == 1 ? core_drawer : core_filename; }
static int core_file_compare(const void *a, const void *b) { return strcmp(a, b); }
/* _get_a_dir asm:16841-16915 + _sort asm:11016-11079: files only, names
 * upper-cased and cut at 29 characters; those ending ".GAM" without it. */
static void core_file_directory(void)
{
    char path[4160];
    core_file.count = core_file.top = 0;
    if (!core_save_path(path, sizeof path, core_drawer[0] ? core_drawer : ".")) return;
    DIR *directory = opendir(path);
    if (!directory) return;
    for (struct dirent *entry; core_file.count < CORE_FILE_MAX && (entry = readdir(directory));) {
        char full[8320]; struct stat info;
        snprintf(full, sizeof full, "%s/%s", path, entry->d_name);
        if (stat(full, &info) || S_ISDIR(info.st_mode)) continue;
        char *name = core_file.names[core_file.count];
        size_t length = 0;
        for (; length < 29 && entry->d_name[length]; ++length) {
            unsigned char c = (unsigned char)entry->d_name[length];
            name[length] = (char)(c >= 'a' && c <= 'z' ? c - 0x20 : c);
        }
        name[length] = 0;
        if (length < 4 || strcmp(name+length-4, ".GAM")) continue;
        name[length-4] = 0;
        ++core_file.count;
    }
    closedir(directory);
    qsort(core_file.names, (size_t)core_file.count, sizeof *core_file.names, core_file_compare);
}
static void core_file_layout(int draw)
{
    static const CoreMenuEntry buttons[] = {{0,0,0,"SAVE"},{0,0,0,"LOAD"}};
    core_menu_install(core_file_ok, buttons + !core_file.save, 1);
    core_requester(0x48,0x10,0xb0,0xb0,core_file_ok,core_end_cancel,draw);
}
static void core_file_open(int save)
{
    memset(&core_file, 0, sizeof core_file);
    core_file.save = save;
    core_req.kind = CORE_REQ_FILE; left_button = 2;
    core_file_directory();
    core_requester_layout();
}
/* _get_name(d_screen,field,0x58,y,16) entry as core_world_entry. */
static void core_file_entry(int field)
{
    core_file.field = field; core_file.blink = 0;
    char *text = core_file_field();
    core_file.length = (int)strnlen(text, 16);
    int end = 0;
    for (int i = 0; i < 16; ++i) {
        if (!text[i]) end = 1;
        if ((uint8_t)text[i] <= 0x20 || end) text[i] = (char)0x82;
    }
    text[16] = 0; left_button = 2;
}
/* LAB_45ABA: drop the trailing boxes; a new _drawer reloads (-144 = 3). */
static void core_file_entry_done(void)
{
    char *text = core_file_field();
    text[core_file.length] = (char)0x82;
    for (int i = 15; i >= 0 && (uint8_t)text[i] > 0x80; --i) text[i] = 0;
    if (core_file.field == 1) core_file_directory();
    core_file.field = 0;
}
/* _get_name keys as core_world_key. */
static void core_file_key(int key)
{
    if (!core_file.field) return;
    char *text = core_file_field();
    if (key >= 0x20 && key <= 0x7a) {
        if (core_file.length >= 15) return;
        text[core_file.length++] = (char)(key >= 'a' ? key - 0x20 : key);
    } else if ((key == 8 || key == 0x7f) && core_file.length) {
        text[core_file.length--] = (char)0x82;
        text[core_file.length] = (char)0x82;
    } else if (key == 13) core_file_entry_done();
}
/* _save_load returning 0: _game_options redraws (-4 = 1). */
static void core_file_back(void)
{
    core_req.kind = CORE_REQ_GAME; left_button = 2; core_requester_layout();
}
static void core_game_options_close(void);
/* The click loop LAB_45EAC..LAB_4615C (asm:10760-10972). */
static void core_file_click(void)
{
    int result;
    if (core_file.error) { core_file.error = 0; left_button = 2; return; }   /* asm:10907-10928 */
    if (core_file.field) {
        /* _get_name's _check_cancel (asm:10432-10446): OK ends the entry,
         * CANCEL empties the field; both then act on the requester. */
        if (!(result = core_check_cancel(core_file_ok, core_end_cancel))) return;
        if (result == 2) { core_file_field()[0] = 0; core_file.field = 0; }
        else core_file_entry_done();
    } else {
        if (mousex >= 0x58 && mousex <= 0xd8) {
            if (mousey >= 0x22 && mousey <= 0x82) {
                int index = ((mousey - 0x22) >> 3) + core_file.top;
                if (index < core_file.count) snprintf(core_filename, 20, "%s", core_file.names[index]);
            } else if (mousey >= 0x8d && mousey <= 0x95) { core_file_entry(1); return; }
            else if (mousey >= 0xa0 && mousey <= 0xa8) { core_file_entry(2); return; }
        } else if (mousex >= 0xd8 && mousex <= 0xe0) {
            if (mousey >= 0x22 && mousey <= 0x2a && core_file.top) --core_file.top;
            else if (mousey >= 0x7a && mousey <= 0x82 && core_file.top + 5 < core_file.count) ++core_file.top;
        }
        result = core_check_cancel(core_file_ok, core_end_cancel);
    }
    left_button = 2;
    if (!result) return;
    if (result == 2 || !core_filename[0]) { core_file_back(); return; }
    /* _drawer, '/' unless it ends in ':' or '/' (or is one character), _filename. */
    char name[48];
    snprintf(name, sizeof name, "%.19s", core_drawer);
    size_t last = strlen(name);
    if (last > 1 && name[last-1] != ':' && name[last-1] != '/') { name[last] = '/'; name[last+1] = 0; }
    snprintf(name + strlen(name), sizeof name - strlen(name), "%.19s", core_filename);
    if (core_file.save) {
        if (!core_save_game(name)) { core_file.error = 1; return; }   /* "ERROR IN SAVING" */
        A4_WORD(0x3054) = 1;   /* _saved; the level.dat disk check is not needed here */
    } else if (!core_load_game(name)) fprintf(stderr, "Load game: %s could not be loaded.\n", name);
    core_game_options_close();   /* _save_load returns 1 either way */
}
static void core_draw_game_options(void);
static void core_draw_file(void)
{
    core_draw_game_options();
    core_file_layout(1);
    amiga_text(game_pixels, 0x58, 0x18, core_file.save ? "SAVE GAME" : "LOAD GAME");
    amiga_text(game_pixels, 0xd8, 0x22, "\x88"); amiga_text(game_pixels, 0xd8, 0x7a, "\x89");
    amiga_text(game_pixels, 0x58, 0x84, "DIRECTORY");
    amiga_text(game_pixels, 0x58, 0x97, core_file.error ? "ERROR IN SAVING" : "FILE NAME");
    char row[17];
    for (int i = 0; i < 12; ++i) {
        int index = core_file.top + i;
        const char *name = index < core_file.count ? core_file.names[index] : "";
        int length = (int)strlen(name);
        for (int k = 0; k < 16; ++k) row[k] = k < length ? name[k] : (char)0x82;
        row[16] = 0;
        amiga_text(game_pixels, 0x58, 0x22 + i*8, row);
    }
    for (int field = 1; field <= 2; ++field) {
        const char *text = field == 1 ? core_drawer : core_filename;
        int length = (int)strnlen(text, 16);
        for (int k = 0; k < 16; ++k) row[k] = k < length ? text[k] : (char)0x82;
        row[16] = 0;
        if (core_file.field == field && !(core_file.blink / 10 & 1)) row[core_file.length] = (char)0x83;
        amiga_text(game_pixels, 0x58, field == 1 ? 0x8d : 0xa0, row);
    }
}

static void core_requester_click(void)
{
    if (core_req.kind == CORE_REQ_OPTIONS) core_options_click();
    else if (core_req.kind == CORE_REQ_GAME) core_game_options_click();
    else if (core_req.kind == CORE_REQ_SET) core_set_options_click();
    else if (core_req.kind == CORE_REQ_WORLD) core_world_click();
    else if (core_req.kind == CORE_REQ_FILE) core_file_click();
}

static void core_draw_game_options(void)
{
    core_requester(0x10,0x10,0x120,0xb0,core_end_ok,core_end_cancel,1);
    for (int i = 0; i < 17; ++i)   /* asm:12847-12872 */
        amiga_text(game_pixels, core_rec_w(core_game_entry(i),0)+16, core_rec_w(core_game_entry(i),2)+16,
                   core_rec_text(core_game_entry(i)));
}
/* Draw the open requester over the frame, as the original's d_screen. */
static void core_draw_requester(void)
{
    if (core_req.kind == CORE_REQ_OPTIONS) {
        core_requester(0x10,0x10,0x120,0xa6,core_end_ok,core_end_cancel,1);
        uint8_t *own = core_stats(core_req.side);
        static const char marker[] = "\x87";
        for (int i = 0; i < 14; ++i) {   /* asm:13671-13740 */
            uint8_t *entry = core_option_entry(i);
            amiga_text(game_pixels, core_rec_w(entry,0)+16, core_rec_w(entry,2)+16, core_rec_text(entry));
            if (i == 11) amiga_text(game_pixels, core_rec_w(own,0xc)*8 + core_rec_w(entry,0) + 0x28,
                                    core_rec_w(entry,2)+16, marker);
            if (i == 13) amiga_text(game_pixels, core_rec_w(own,0x10)*8 + core_rec_w(entry,0) + 0x30,
                                    core_rec_w(entry,2)+16, marker);
        }
    } else if (core_req.kind == CORE_REQ_GAME) core_draw_game_options();
    else if (core_req.kind == CORE_REQ_WORLD) core_draw_world();
    else if (core_req.kind == CORE_REQ_FILE) core_draw_file();
    else if (core_req.kind == CORE_REQ_SET) {
        /* _set_options draws over the game options screen. */
        core_draw_game_options();
        core_requester(0x20,0x18,0x100,0x70,core_end_ok,core_end_cancel,1);
        amiga_text(game_pixels, core_rec_w(core_set_entry(0),0)+0x20, core_rec_w(core_set_entry(0),2)+0x18,
                   core_rec_text(core_set_entry(0)));
        int shown = 0;
        for (int i = 0; i < 5; ++i) {   /* asm:13386-13410: the set choice of each pair */
            shown = i*2 + ((game_mode & (1 << i)) ? 1 : 0) + 1;
            uint8_t *entry = core_set_entry(shown);
            amiga_text(game_pixels, core_rec_w(entry,0)+0x20, core_rec_w(entry,2)+0x18, core_blank);
            amiga_text(game_pixels, core_rec_w(entry,0)+0x20, core_rec_w(entry,2)+0x18, core_rec_text(entry));
        }
        /* Landscape number, asm:13412-13437 (1993 addition): below the last pair. */
        char landscape[32]; snprintf(landscape, sizeof landscape, "LANDSCAPE NO %d", (int)start_seed);
        amiga_text(game_pixels, core_rec_w(core_set_entry(shown),0)+0x20, core_rec_w(core_set_entry(shown),2)+0x22, landscape);
    }
}

/* _end_game display asm:14317-14385: _requester(d_screen,16,16,0x120,0xa6,
 * button) with the -60(A5) button "TRY IT AGAIN" (lost conquest) or
 * "NEW GAME", the rows at _end_text+16, then wait for a click on the
 * button (_check_cancel(button,0)). */
static uint8_t core_end_button[46];
static void core_end_button_layout(int draw)
{
    memset(core_end_button, 0, sizeof core_end_button);
    strcpy(core_rec_text(core_end_button), core_result == 2 && core_conquest_game() ? "TRY IT AGAIN" : "NEW GAME");
    core_requester(16,16,0x120,0xa6,core_end_button,NULL,draw);
}
static void core_draw_end_screen(void)
{
    core_end_button_layout(1);
    for (int r = 0; r < 7; ++r)
        amiga_text(game_pixels, A4_WORD(0x8e7e + r*46) + 16, A4_WORD(0x8e80 + r*46) + 16, core_end_row(r));
}

/* _end_words asm:24783-24785, _words asm:24786-24788 (two per rank, -1 none). */
static const int16_t core_end_words[5] = {0,1,3,5,6};
static const int16_t core_words[20] = {9,-1,5,-1,3,-1,7,-1,1,8,2,-1,1,2,9,6,1,6,3,6};
/* The original's working directory: POPULOUS_DATA_DIR, else <assets>/raw. */
static FILE *core_open_raw(const char *name)
{
    const char *directory = getenv("POPULOUS_DATA_DIR"), *assets = getenv("POPULOUS_ASSET_DIR");
    char path[4160];
    if (directory) snprintf(path,sizeof path,"%s/%s",directory,name);
    else snprintf(path,sizeof path,"%s/raw/%s",assets ? assets : POPULOUS_DEFAULT_ASSET_DIR,name);
    return fopen(path,"rb");
}
/* _read_lord asm:16631-16663: 0x7d00 bytes of four plane-major planes
 * (8000 bytes, 40 per row, MSB left) into _w_screen, then 0x20 bytes =
 * 16 BE16 RGB12 words into _tcmap. The original fails only on Open; the
 * port also rejects a short file instead of keeping stale w_screen data. */
static int core_read_lord(void)
{
    FILE *file = core_open_raw("lord.pic");
    if (!file) return 0;
    static uint8_t data[0x7d00 + 0x20];
    int ok = fread(data,1,sizeof data,file) == sizeof data;
    fclose(file);
    if (!ok) return 0;
    for (int pixel = 0; pixel < 320*200; ++pixel) {
        int colour = 0;
        for (int plane = 0; plane < 4; ++plane)
            colour |= ((data[plane*8000 + pixel/8] >> (7 - pixel%8)) & 1) << plane;
        core_lord.screen[pixel] = (uint8_t)colour;
    }
    for (int i = 0; i < 16; ++i) core_lord.palette[i] = (uint16_t)(data[0x7d00+i*2] << 8 | data[0x7d01+i*2]);
    return 1;
}
/* _read_mouth asm:16668-16690: 0x13b0 bytes, no failure path. Without the
 * file the port skips _draw_mouth (the original would copy stale _map_steps). */
static void core_read_mouth(void)
{
    FILE *file = core_open_raw("mouths.pic");
    core_lord.mouths_loaded = file && fread(core_lord.mouths,1,sizeof core_lord.mouths,file) == sizeof core_lord.mouths;
    if (file) fclose(file);
}
/* _draw_mouth(screen,x,y,rows,frame) asm:19894-19922: x in bytes; frame
 * 0x348 bytes of rows of 6 bytes (48 pixels) per plane 0..3, each row
 * replacing all four planes (plane stride 0x1f40). */
static void core_draw_mouth(int x, int y, int rows, int frame)
{
    if (!core_lord.mouths_loaded || frame < 0 || (frame+1)*0x348 > (int)sizeof core_lord.mouths) return;
    const uint8_t *m = core_lord.mouths + frame*0x348;
    for (int r = 0; r < rows && y+r < 200; ++r) for (int px = 0; px < 48 && x*8+px < 320; ++px) {
        int colour = 0;
        for (int plane = 0; plane < 4; ++plane) colour |= ((m[r*24 + plane*6 + px/8] >> (7 - px%8)) & 1) << plane;
        core_lord.screen[(y+r)*320 + x*8 + px] = (uint8_t)colour;
    }
}
/* One pass of the loop before its four _waittof, LAB_48DCE-LAB_48EF2. */
static void core_lord_speak(void)
{
    if (core_lord.finished) {
        if (core_lord.c >= 10 && core_lord.c <= 14) {
            if (core_check_effect(2)) { core_PlaySound(core_end_words[core_lord.c-10],2); ++core_lord.c; }
            return;
        }
        if (core_lord.c > 200) core_lord.c = 0;
        if (core_lord.c == 16) core_draw_mouth(0x10,0x4b,0x23,5);
    } else if (core_lord.c >= 10 && core_lord.c <= 12) {
        if (core_lord.c == 10) { core_PlaySound(0,2); ++core_lord.c; }
        else if (core_check_effect(2)) {
            int16_t word = core_words[core_lord.rank*2 + core_lord.c - 11];
            if (word >= 0) core_PlaySound(word,2);
            ++core_lord.c;
        }
        return;
    } else if (core_lord.c > 250) core_lord.c = 0;
    ++core_lord.c; core_lord.f = 0; core_lord.d = 1;
}
/* Per vertical blank: after four _waittof the mouth frame -108(A5) at
 * byte 0x12, y 0x87, then while channel 2 talks it bounces 0..4
 * (asm:14740-14757), and the next pass begins. */
static void core_lord_vblank(void)
{
    if (!core_lord.shown || ++core_lord.wait < 4) return;
    core_lord.wait = 0;
    core_draw_mouth(0x12,0x87,0x23,core_lord.f);
    if (!core_check_effect(2)) {
        core_lord.f = (int16_t)(core_lord.f + core_lord.d);
        if (core_lord.f >= 4 || core_lord.f <= 0) core_lord.d = (int16_t)-core_lord.d;
    }
    core_lord_speak();
}
/* asm:14503-14594: sound off, _read_lord (0: _level_number back, empty
 * _message, asm:14762-14765), _read_mouth, gwords, the two text lines at
 * x 8, y 0xb2/0xba (core_conquest_text), _tcmap 16..31 = _palette for the
 * pointer (the port's prg_screen palette), _left_button 2. */
static int core_lord_open(int old_level, int level, int finished)
{
    core_lord.saved_music = music_off; music_off = 0;
    core_lord.saved_effect = effect_off; effect_off = 0;
    core_kill_effect(0,4);
    if (!core_read_lord()) {
        A4_WORD(0xae7e) = (int16_t)(finished ? 0 : old_level);
        core_message[0] = 0;
        music_off = core_lord.saved_music; effect_off = core_lord.saved_effect;
        return 0;
    }
    core_read_mouth();
    core_free_all_sounds();
    core_load_sound("gwords");
    if (finished) old_level = 0;
    seed = (uint16_t)old_level;   /* asm:14518 */
    char line[2][48];
    core_conquest_text(old_level, level, finished, line);
    if (!finished && seed) ___newrand();
    amiga_text(core_lord.screen, 8, 0xb2, line[0]);
    amiga_text(core_lord.screen, 8, 0xba, line[1]);
    core_lord.finished = finished; core_lord.rank = level / 250;
    core_lord.c = core_lord.f = 0; core_lord.d = 1; core_lord.wait = 0;
    core_lord.shown = 1; left_button = 2;
    core_lord_speak();
    return 1;
}
/* The click, LAB_48F4A asm:14740-14765: kill, gmusic1 back, the saved
 * _music_off/_effect_off, then _show_world. */
static void core_lord_leave(void)
{
    if (!core_lord.shown) return;
    core_lord.shown = 0; left_button = 2;
    core_kill_effect(0,4);
    core_free_all_sounds();
    core_load_sound("gmusic1");
    music_off = core_lord.saved_music; effect_off = core_lord.saved_effect;
    core_world_open(CORE_WORLD_WON);
}
/* _make_copper(4 planes)/_show_screen of w_screen with the lord _tcmap;
 * the 200-line screen leaves the port HUD rows black but for one line. */
static void core_draw_lord(void)
{
    uint32_t colours[16];
    for (int i = 0; i < 16; ++i) {
        uint16_t w = core_lord.palette[i];
        colours[i] = 0xff000000u | (uint32_t)((w >> 8) & 15) * 17 << 16 | (uint32_t)((w >> 4) & 15) * 17 << 8 | (uint32_t)(w & 15) * 17;
    }
    for (int pixel = 0; pixel < 320*200; ++pixel) prg_screen->chunky_buffer[pixel] = colours[core_lord.screen[pixel] & 15];
    static uint8_t hud[320*200];
    memset(hud,0,320*56);
    amiga_text(hud,0,0,"CLICK TO CONTINUE, ESC QUIT");
    for (int pixel = 0; pixel < 320*56; ++pixel) prg_screen->chunky_buffer[200*320+pixel] = prg_screen->palette[hud[pixel]];
    core_draw_pointer();
}

/* _show_world asm:12190-12773, run as a requester (CORE_REQ_WORLD): the
 * original loops inside the caller, so the game is paused while it is
 * open. _requester(d_screen,16,0x18,0x120,0xa0,"START GAME","NEW GAME")
 * (asm:12193-12216); the buttons are _show_world's locals -72/-118(A5).
 * Phases: shown (the parameter lines, waiting for START GAME/NEW GAME,
 * asm:12744-12761) or entry (_get_name, asm:12240-12260). */
/* _c_text asm:24621-24725 (A4 0x8afc): word x, y, then the put_in_string
 * offset (lines 9..14: the pixel offset of the YOU/HIM columns), text. */
static const CoreMenuEntry core_world_template[15] = {
    {0x10,0x0a,0x10,"WORLD TO CONQUER"},     {0x10,0x1e,0x11,"BATTLE NUMBER IS "},
    {0x10,0x26,0x14,"WORLDS LANDSCAPE IS "}, {0x10,0x2e,0x12,"HIS REACTIONS ARE "},
    {0x10,0x36,0x0e,"HIS RATING IS "},       {0x10,0x3e,0x05,"LAND "},
    {0x10,0x46,0x0f,"THE SWAMPS ARE "},      {0x10,0x4e,0x09,"WATER IS "},
    {0x10,0x58,0x48,"               YOU    HIM"}, {0x10,0x60,0x78,"POPULATION"},
    {0x10,0x68,0x78,"EARTHQUAKES"}, {0x10,0x70,0x78,"SWAMPS"}, {0x10,0x78,0x78,"KNIGHTS"},
    {0x10,0x80,0x78,"VOLCANOS"},    {0x10,0x88,0x78,"FLOODS"},
};
/* _con_text asm:24726-24750 (strings asm:21257-21330). */
static const char *const core_con_text[24] = {"GRASS PLANES","DESERT","SNOW AND ICE","ROCKY",
    "VERY SLOW","SLOW","MEDIUM","FAST","VERY FAST","VERY POOR","POOR","AVERAGE","GOOD","VERY GOOD",
    "CANNOT BE BUILT","BUILT JUST ON TOWNS","ONLY BUILT UP","BUILT ON PEOPLE",
    "SHALLOW","BOTTOMLESS","HARMFUL","FATAL","YES","NO"};
static const char *core_con(unsigned index) { return index < 24 ? core_con_text[index] : ""; }

static void core_world_layout(int draw)
{
    static const CoreMenuEntry buttons[] = {{0,0,0,"START GAME"},{0,0,0,"NEW GAME"}};
    core_menu_install(core_world_ok, buttons, 1); core_menu_install(core_world_cancel, buttons+1, 1);
    core_requester(0x10,0x18,0x120,0xa0,core_world_ok,core_world_cancel,draw);
}
/* Line i of the jump table asm:12380-12743 (put_in_string asm:12163-12186
 * truncates the text at the offset and appends). Lines 9..14 also give
 * the YOU/HIM columns (x+off+16, x+off+0x48). */
static void core_world_line(int i, char *line, char you[8], char him[8])
{
    const CoreMenuEntry *entry = &core_world_template[i];
    const char *value = NULL;
    snprintf(line, 48, "%s", entry->text); you[0] = him[0] = 0;
    switch (i) {
    case 1: { char number[12]; snprintf(number, sizeof number, "%d", core_world.level / 5); value = number;
              line[entry->w] = 0; strcat(line, value); return; }
    case 2: value = core_con(conq_05_terrain); break;
    case 3: value = core_con((uint16_t)(10 - conq_01_speed) / 2 + 4); break;   /* LSR.W asm:12441 */
    case 4: value = core_con((uint16_t)(10 - conquest) / 2 + 9); break;
    case 5: value = core_con(conq_04_mode & 4 ? 14 : conq_04_mode & 0x10 ? 15 : conq_04_mode & 8 ? 16 : 17); break;
    case 6: value = core_con(conq_04_mode & 2 ? 19 : 18); break;
    case 7: value = core_con(conq_04_mode & 1 ? 21 : 20); break;
    case 9: snprintf(you, 8, "%u", A4_DISP(0x84c2)[0]); snprintf(him, 8, "%u", A4_DISP(0x84c3)[0]); return;
    default:
        if (i >= 10) {
            strcpy(you, core_con(conq_03_yourpow & (1 << (i-10)) ? 22 : 23));
            strcpy(him, core_con(conq_02_enemypow & (1 << (i-10)) ? 22 : 23));
        }
        return;
    }
    line[entry->w] = 0; strncat(line, value, 47 - strlen(line));
}
/* level.dat record level/25 into _conquest 0x84bc..0x84c5 (asm:12315-12368). */
static int core_world_read(int record)
{
    char fallback[4096], path[4200];
    const char *directory = getenv("POPULOUS_DATA_DIR"), *assets = getenv("POPULOUS_ASSET_DIR");
    if (!assets) assets = POPULOUS_DEFAULT_ASSET_DIR;
    snprintf(fallback, sizeof fallback, "%s/raw", assets);
    if (!directory) directory = fallback;
    snprintf(path, sizeof path, "%s/level.dat", directory);
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    uint8_t bytes[10];
    /* ___Seek/___Read: a short read past the end keeps the old bytes. */
    if (!fseek(file, (long)record * 10, SEEK_SET) && fread(bytes, 1, 10, file) == 10) {
        memcpy(A4_DISP(0x84bc), bytes, 8);
        conq_08_seed = (int16_t)(bytes[8] << 8 | bytes[9]);
    }
    fclose(file);
    return 1;
}
/* The callers' MOVE.W D0,_in_conquest: the conquest start (asm:543-551)
 * and _won_conquest (asm:14766-14772) loop while it is -1, then start
 * the world; _game_options (asm:13236-13239) sets _new_map, whose restart
 * with -1 (a number) builds that custom landscape from _seed. */
static void core_world_return(int result)
{
    in_conquest = (int16_t)result;
    if (result == -1 && core_world.origin != CORE_WORLD_OPTIONS) { core_world_open(core_world.origin); return; }
    if (core_world.origin == CORE_WORLD_OPTIONS) A4_WORD(0x309a) = 1;
    if (result == -1 && !core_custom_new_map(1)) {
        fprintf(stderr, "World selection: landscape %d could not be started.\n", (int)seed);
        in_conquest = core_req.saved_conquest;
    }
    if (result != -1 && !game_reset_start((StartSelection){START_ORIGINAL, result / 5})) {
        fprintf(stderr, "World selection: world %d could not be started.\n", result);
        in_conquest = (int16_t)A4_WORD(0xae7e);
        if (core_world.origin != CORE_WORLD_OPTIONS) return;   /* keep the screen */
    }
    core_req.kind = CORE_REQ_NONE; left_button = 2;
}
/* _get_name(d_screen,_message,0x20,0x2c,16) entry, asm:10307-10330:
 * from strlen, bytes <= 0x20 and past the end become 0x82 boxes. */
static void core_world_entry(void)
{
    core_world.shown = 0; core_world.entry = 1; core_world.blink = 0;
    core_world.length = (int)strlen(core_message);
    int end = 0;
    for (int i = 0; i < 16; ++i) {
        if (!core_message[i]) end = 1;
        if ((uint8_t)core_message[i] <= 0x20 || end) core_message[i] = (char)0x82;
    }
    core_message[16] = 0; left_button = 2;
}
/* LAB_4712A..LAB_47236, asm:12262-12378. */
static void core_world_evaluate(void)
{
    int16_t value = (int16_t)atoi(core_message);   /* ___atoi's low word, TST.W */
    if (value) {   /* a number: landscape seed for a custom game */
        start_seed = seed = (int16_t)value; core_message[0] = 0;
        core_world_return(-1); return;
    }
    if (core_message[0]) {
        core_world.level = core_decode(core_message);   /* GENESIS: 0 */
        if (core_world.level < 0) core_world.error = 1;   /* "NO SUCH WORLD" */
    }
    if (core_world.level < 0) { core_world_entry(); return; }
    /* Port: a missing level.dat stays in the entry ("insert disk" loops). */
    if (!core_world_read(core_world.level / 25)) {
        fprintf(stderr, "World selection: level.dat not found.\n");
        core_world_entry(); return;
    }
    core_world.shown = 1; core_world.entry = 0; left_button = 2;
}
static void core_world_open(int origin)
{
    core_req.kind = CORE_REQ_WORLD;
    memset(&core_world, 0, sizeof core_world); core_world.origin = origin;
    if (!core_message[0]) strcpy(core_message, "GENESIS");
    core_requester_layout();
    core_world_evaluate();
}
/* _get_name end (LAB_45ABA asm:10461-10488): drop the trailing boxes;
 * then _show_world blanks the "NO SUCH WORLD" line (asm:12251-12260). */
static void core_world_entry_done(void)
{
    core_message[core_world.length] = (char)0x82;
    for (int i = 15; i >= 0 && (uint8_t)core_message[i] > 0x80; --i) core_message[i] = 0;
    core_world.entry = 0; core_world.error = 0;
    core_world_evaluate();
}
/* _get_name keys, asm:10397-10431 (_asckey from ___keyboard): 0x20..0x7a
 * appended upper-cased while length < 15, 8/0x7f delete, 13 ends. */
static void core_world_key(int key)
{
    if (!core_world.entry) return;
    if (key >= 0x20 && key <= 0x7a) {
        if (core_world.length >= 15) return;
        core_message[core_world.length++] = (char)(key >= 'a' ? key - 0x20 : key);
    } else if ((key == 8 || key == 0x7f) && core_world.length) {
        core_message[core_world.length--] = (char)0x82;
        core_message[core_world.length] = (char)0x82;   /* the original waits for the cursor blink */
    } else if (key == 13) core_world_entry_done();
}
/* _check_cancel: in _get_name (asm:10432-10446) START GAME ends the
 * entry, NEW GAME empties it; on the shown world (asm:12749-12773) NEW
 * GAME clears _message and re-enters, START GAME starts it. */
static void core_world_click(void)
{
    int result = core_check_cancel(core_world_ok, core_world_cancel);
    if (core_world.entry) {
        if (result == 1) core_world_entry_done();
        else if (result == 2) { core_message[0] = 0; core_world.entry = core_world.error = 0; core_world_evaluate(); }
        return;
    }
    if (result == 2) { core_message[0] = 0; core_world_entry(); return; }
    if (result != 1) return;
    if (core_world.level / 5 >= LEVEL_COUNT) {   /* port: no level.dat record above 2470 */
        fprintf(stderr, "World selection: world %d has no level.dat record.\n", core_world.level);
        return;
    }
    /* asm:12762-12771 */
    player = 0; not_player = 1; A4_WORD(0x82ea) = 1; A4_WORD(0x82bc) = 0; A4_WORD(0x82be) = 1;
    A4_WORD(0x82ec) = 0; serial_off = 1; paint_map = 0; A4_WORD(0xae80) = 0;
    A4_WORD(0xae7e) = (int16_t)core_world.level;
    core_world_return(core_world.level);
}
/* The caller clears the screen first (___clr_wsc asm:540, 14757), except
 * _game_options, which draws over its own setup screen. */
static void core_draw_world(void)
{
    if (core_world.origin == CORE_WORLD_OPTIONS) core_draw_game_options();
    else memset(game_pixels, 0, sizeof game_pixels);
    core_world_layout(1);
    const CoreMenuEntry *t = core_world_template;
    amiga_text(game_pixels, t[0].x+16, t[0].y+24, t[0].text);
    /* LAB_525C8: the 16-box field, then _message (asm:12228-12239). */
    char field[17], name[17];
    memset(field, 0x82, 16); field[16] = 0;
    snprintf(name, sizeof name, "%s", core_message);
    if (core_world.entry && !(core_world.blink / 10 & 1)) name[core_world.length] = (char)0x83;   /* cursor, asm:10351-10372 */
    amiga_text(game_pixels, 0x20, 0x2c, field);
    amiga_text(game_pixels, 0x20, 0x2c, name);
    if (core_world.error) amiga_text(game_pixels, t[1].x+16, t[1].y+24, "NO SUCH WORLD");
    if (!core_world.shown) return;
    for (int i = 1; i < 15; ++i) {
        char line[48], you[8], him[8];
        core_world_line(i, line, you, him);
        if (you[0]) amiga_text(game_pixels, t[i].x+t[i].w+16, t[i].y+24, you);
        if (him[0]) amiga_text(game_pixels, t[i].x+t[i].w+0x48, t[i].y+24, him);
        amiga_text(game_pixels, t[i].x+16, t[i].y+24, line);
    }
}

static int game_pick(int sx, int sy, int *column, int *row)
{
    int16_t heights[81];
    for (int v=0; v<=8; ++v) for (int u=0; u<=8; ++u)
        heights[v*9+u]=core_vertex(xoff+u,yoff+v);
    int u,v;
    if (!graphics_pick(heights,sx,sy,&u,&v)) return 0;
    *column=xoff+u; *row=yoff+v;
    return 1;
}

/* _toggle_icon asm:19493-19521: XOR bitplane 0 (palette bit 0) with the
 * LAB_4C750 diamond; a steps +8 rows/+16px, b +8 rows/-16px. */
static void core_xor_icon(int base, int a, int b)
{
    static const uint32_t mask[16] = {0,0x0000c000,0x0003f000,0x000ffc00,0x003fff00,
        0x00ffffc0,0x03fffff0,0x0ffffffc,0x3fffffff,0x0ffffffc,0x03fffff0,0x00ffffc0,
        0x003fff00,0x000ffc00,0x0003f000,0x0000c000};
    int x0 = 8*(base%40) + 16*a - 16*b, y0 = base/40 + 8*a + 8*b;
    for (int r = 1; r < 16; ++r) for (int bit = 0; bit < 32; ++bit) {
        int x = x0+bit, y = y0+r;
        if ((mask[r] >> (31-bit) & 1) && x >= 0 && x < 320 && y >= 0 && y < 200)
            game_pixels[y*320+x] ^= 1;
    }
}

/* Persistent highlights, derived from state rather than toggled: tendency
 * (_set_tend_icons asm:2455-2522), mode (_set_mode_icons asm:2526-2567,
 * magnet bit before swamp bit before _mode), sound (initial toggles
 * asm:513-536) and pause (_do_action y=6 asm:18591). The short press
 * flashes on the power and view icons are not reproduced. */
static void core_draw_icons(void)
{
    if (player < 0 || player > 1 || diagnostic_view) return;
    static const int8_t tend[4][2] = {{3,3},{4,3},{5,3},{4,4}};
    int t = core_tendency(player);
    if (t >= 0 && t <= 3) core_xor_icon(0x12c0,tend[t][0],tend[t][1]);
    if (core_tool == 1) core_xor_icon(0x12c0,6,2);
    else if (core_swamp_selected) core_xor_icon(0x12c0,2,2);
    else if (core_tool == 2) core_xor_icon(0x12c0,6,0);
    else core_xor_icon(0x12c0,6,1);
    /* Toggled at startup with both flags 1 (asm:513-536,24131-24134). */
    if (music_off) core_xor_icon(0x17e4,0,3);
    if (effect_off) core_xor_icon(0x17e4,0,4);
    if (pause) core_xor_icon(0x17e4,1,3);
}

/* _draw_bar asm:19213-19282: 4px column at byte x (pixels 2-5), growing
 * up from y; value rows in colour, the rest of height in colour 2. */
static void core_draw_bar(int x, int y, int height, int value, int colour)
{
    if (value < 0) value = 0;
    if (value > height) value = height;
    for (int k = 0; k < height; ++k) {
        int row = y-k;
        if (row < 0 || row >= 200) continue;
        for (int px = 8*x+2; px < 8*x+6; ++px) game_pixels[row*320+px] = (uint8_t)(k < value ? colour : 2);
    }
}

/* _show_the_shield state part asm:2769-2790: a dead view_who is dropped. */
static int core_shield_state(void)
{
    if (!view_who) return 1;
    if (view_who < 0 || view_who > no_peeps || peep_word(view_who-1,4) <= 0) { view_who = 0; return 0; }
    return 1;
}

/* _show_the_shield asm:2790-3147. The owner flag and weapon icons use
 * _icon_data, which the asset export lacks; they are not drawn. */
static void core_show_the_shield(GraphicsSheet small)
{
    if (!core_shield_state()) return;
    if (view_who) {
        int person = view_who-1, flags = peeps[person*22], owner = peeps[person*22+1]&1;
        int16_t pop = peep_word(person,4);
        uint32_t heading; memcpy(&heading,peeps+person*22+14,sizeof heading);
        if (flags & 8) {
            /* Battle: sprite +0xC and strength bars, blue left (asm:2840-2895). */
            int other = peep_word(person,6);
            if (other >= 0 && other < no_peeps) {
                int blue = owner ? other : person, red = owner ? person : other;
                int32_t sum = peep_word(blue,4) + peep_word(red,4);
                int frame = peep_word(person,12);
                if (frame >= 0 && frame < 143) graphics_blit(game_pixels,64000,320,200,small,frame,0x110,0x16);
                core_draw_bar(0x24,0x25,16,(int)___divs(16*peep_word(blue,4),sum),15);
                core_draw_bar(0x25,0x25,16,(int)___divs(16*peep_word(red,4),sum),8);
            }
        } else if (flags == 1) {
            /* Town: capacity and fill bars (asm:2896-2953). */
            graphics_blit(game_pixels,64000,320,200,small,0x40+owner*2+(toggle&1),0x10c,0x16);
            int capacity = core_check_life(owner,peep_word(person,8));
            if (!capacity) capacity = 1;
            core_draw_bar(0x24,0x25,16,capacity == 0xbea ? 16 : (uint16_t)(capacity*16)/0x131,10);
            int fill = (uint16_t)(pop*16)/(uint16_t)capacity;
            core_draw_bar(0x25,0x25,16,fill > 16 ? 16 : fill,12);
        } else {
            /* Walker sprite (asm:2954-3020) and population bars (asm:3021-3061). */
            int frame;
            if (flags & 0x60) frame = (heading ? 0x7d : 0x65) + owner*2 + (toggle&1);
            else if (flags & 0x10) frame = (heading ? 0x6d : 0x5d) + owner*4 + ((uint16_t)game_turn&3);
            else {
                static const int16_t dir_sprite[9] = {-65,-64,-63,1,65,64,63,-1,0};
                int d = 0;
                while (d < 9 && dir_sprite[d] != peep_word(person,10)) ++d;
                if (d > 7) d = 4;
                frame = d*2 + (toggle&1) + (heading ? 0x20 : 0) + owner*16;
            }
            if (frame < 143) graphics_blit(game_pixels,64000,320,200,small,frame,0x110,0x16);
            if (pop > 0x1000) { core_draw_bar(0x24,0x25,16,pop/0x400,10); core_draw_bar(0x25,0x25,16,pop%0x400,9); }
            else { core_draw_bar(0x24,0x25,16,pop/0x100,9); core_draw_bar(0x25,0x25,16,(pop%0x100)/16,9); }
        }
    }
    /* good_pop/bad_pop bars asm:3062-3147. */
    int32_t good = (int32_t)core_good_pop(0), bad = (int32_t)core_good_pop(1);
    core_draw_bar(0x20,0x1f,0x20,good ? (int)(31*good/50000+1) : 0,15);
    core_draw_bar(0x27,0x1f,0x20,bad ? (int)(31*bad/50000+1) : 0,8);
}

static void draw_original(int view_x, int view_y)
{
    if (!game_assets || view_x<0 || view_x>56 || view_y<0 || view_y>56) return;
    memset(game_pixels,0,sizeof game_pixels);
    memcpy(game_pixels,game_assets->background,sizeof game_assets->background);
    core_draw_icons();
    /* asm:615-622: terrain mode refreshes _pointer, then _move_mana. */
    if (!core_tool && !core_swamp_selected) core_set_pointer();
    _move_mana();
    int dy = core_shake_dy;
    GraphicsSheet land=graphics_land(game_assets), small=graphics_sprites(game_assets);
    int phase=((uint16_t)game_turn/4)&1;
    /* _draw_it, asm:15794-15949: row-major tiles, raised overlays, front faces. */
    for (int v=0; v<8; ++v) for (int u=0; u<8; ++u) {
        int cell=(view_y+v)*64+view_x+u, left,top;
        if (!graphics_tile(u,v,map_alt[cell],&left,&top)) continue;
        top+=dy;
        int frame=map_blk[cell];
        if (!frame && !phase) frame=16;
        graphics_blit(game_pixels,64000,320,200,land,frame,left,top);
        if (map_bk2[cell]) graphics_blit(game_pixels,64000,320,200,land,map_bk2[cell],left,top-8);
        for (int owner=0; owner<2; ++owner)
            if (core_tendency(owner)==0 && core_target(owner)==cell)
                graphics_blit(game_pixels,64000,320,200,land,45+owner,left,top-8);
    }
    for (int v=0; v<8; ++v)
        for (int level=0; level<map_alt[(view_y+v)*64+view_x+7]; ++level)
            graphics_blit(game_pixels,64000,320,200,small,77,304-16*v,128+8*v-8*level+dy);
    for (int u=7; u>=0; --u)
        for (int level=0; level<map_alt[(view_y+7)*64+view_x+u]; ++level)
            graphics_blit(game_pixels,64000,320,200,small,76,64+16*u,128+8*u-8*level+dy);
    /* Original queues occupants then draws them after terrain (asm:623-657).
     * Core movement commits its destination immediately: no second interpolation.
     * A shaken screen_origin skips _move_sprite entirely (asm:15842-15849). */
    if (!dy) no_sprites = 0;
    sound_effect = 0;   /* asm:614, before _draw_it */
    for (int v=0; v<8 && !dy; ++v) for (int u=0; u<8; ++u) {
        int cell=(view_y+v)*64+view_x+u, person=(int)map_who[cell]-1;
        if (person<0 || person>=no_peeps || peep_word(person,4)<=0 || peep_word(person,8)!=cell) continue;
        int left,top,owner=peep_owner(person),flags=peeps[person*22];
        if (owner>1 || !graphics_tile(u,v,map_alt[cell],&left,&top)) continue;
        /* _move_sprite dispatch asm:15996-16016,16042: ==1, ==2 and bits
         * 4..6 take other branches; then battle (bit 3) before bit 7. */
        if (flags != 1 && flags != 2 && !(flags & 0x70))
            sound_effect |= flags & 8 ? 8 : flags & 0x80;
        int frame=graphics_walker_frame(peep_word(person,10),owner,phase);
        int sx=left+8, sy=top-8+(map_blk[cell]==15 ? 0 : 4);
        if (flags==5 || flags==6 || flags==0x80 || flags==0x22 ||
            flags==0x42 || flags==0x62 || flags==2 || flags==18) {
            /* Presence spans all four host reference bytes, including opaque
             * values. Select only canonical states; mixed flags keep the fallback. */
            uint32_t raw_heading;
            memcpy(&raw_heading,peeps+person*22+14,sizeof raw_heading);
            int headed=raw_heading!=0;
            if (flags==5 || flags==6) {
                frame=(int)peep_word(person,12)+32*headed+4*owner;
                sx=left+16; sy=top-8;
            } else if (flags==0x80) {
                frame=105+((uint16_t)game_turn&3); sy=top-8;
            } else if (flags==0x22 || flags==0x42 || flags==0x62) {
                frame=(int)peep_word(person,12)+24*headed+2*owner;
                sx=left+16; sy=top-8;
            } else if (flags==2) frame+=32*headed;
            else frame=93+((uint16_t)game_turn&3)+16*headed+4*owner;
            /* Bound the selected frame, not the stored simulation frame. The
             * source's extra Y lift is already top-8 (_draw_it, asm:15831). */
            if (frame<0 || frame>=143) continue;
        } else if (flags==1) { frame=64+phase+owner*2; sx=left+16; sy=top-8; }
        else if (flags&8) {
            int stored=peep_word(person,12);
            /* Preserve a render-only fallback for malformed or legacy frames. */
            frame=((stored>=70 && stored<=72) || (stored>=130 && stored<=132) ||
                   (stored>=134 && stored<=136) || (stored>=138 && stored<=140)) ? stored : 138+phase;
            sx=left+16; sy=top-8;
        }
        else if (flags&16) frame=93+((uint16_t)game_turn&3)+owner*4;
        graphics_blit(game_pixels,64000,320,200,small,frame,sx,sy);
        /* _sprite table A4 0x9a22 (+0 x, +2 y, +6 who), read by _interogate. */
        if (no_sprites < 64) {
            int entry = 0x9a22 + no_sprites*8;
            A4_WORD(entry) = (int16_t)sx; A4_WORD(entry+2) = (int16_t)sy;
            A4_WORD(entry+4) = 0; A4_WORD(entry+6) = (int16_t)(person+1);
            ++no_sprites;
        }
    }
    core_show_the_shield(small);
    /* Original overview projection (_a_putpixel, asm:16758-16777). */
    for (int row=0; row<64; ++row) for (int column=0; column<64; ++column) {
        int cell=row*64+column, mx=64+column-row, my=(column+row)/2;
        int colour=map_colour_data[map_blk[cell]&15];
        int person=(int)map_who[cell]-1;
        if (person>=0 && person<no_peeps && peep_word(person,4)>0) colour=peep_owner(person) ? 8 : 15;
        if (column>=view_x && column<=view_x+7 && row>=view_y && row<=view_y+7 &&
            (column==view_x || column==view_x+7 || row==view_y || row==view_y+7)) colour=5;
        game_pixels[my*320+mx]=(uint8_t)colour;
    }
    if (core_end_shown && core_result) core_draw_end_screen();
    core_draw_requester();
    for (size_t pixel=0; pixel<sizeof game_pixels; ++pixel)
        prg_screen->chunky_buffer[pixel]=prg_screen->palette[game_pixels[pixel]];
    /* Port HUD (not in the original, whose screen ends at y 200): 7 lines of
     * 40 columns in the original font (amiga_text) on y 200..255. */
    static uint8_t hud[320*200];
    char lines[7][48]={{0}};
    snprintf(lines[0],48,"%s BLUE %d RED %d MANA %d",core_result==1 ? "VICTORY" : core_result==2 ? "DEFEAT" : pause ? "PAUSED" : A4_WORD(0xae80) ? "WAR" : "PLAY",core_population(0),core_population(1),core_mana(0));
    snprintf(lines[1],48,"%s",core_end_shown && core_result ? "GAME OVER: CLICK/KEY CONTINUE, ESC QUIT" : A4_WORD(0xae80) ?
        "WAR: TERRAIN+MAGNET LOCKED, ARROWS/MAP" : core_swamp_selected ?
        "SWAMP ARMED 5000: LMB TARGET, RMB INERT" : "LMB RAISE, RMB LOWER, ARROWS/MAP SCROLL");
    snprintf(lines[2],48,"%s",core_swamp_selected ?
        (core_tool == 1 ? "AFTER TARGET: MAGNET / D VIEW" : core_tool == 2 ?
                          "AFTER TARGET: QUERY / D VIEW" : "AFTER TARGET: TERRAIN / D VIEW") :
        (core_tool == 1 ? "MAGNET TOOL, ICONS ACTIVE / D VIEW" : core_tool == 2 ?
                          "QUERY HOVER/LMB TEMP RMB KEEP / D VIEW" : "TERRAIN TOOL, ICONS ACTIVE / D VIEW"));
    snprintf(lines[3],48,"0 GATHER 1 SETTLE 2 JOIN 3 FIGHT R RESET");
    snprintf(lines[4],48,"S SWAMP 5000 Q QUAKE 2500 K KNIGHT 7500");
    snprintf(lines[5],48,"V VOLCANO 10000 F FLOOD 40000 M MAGNET");
    snprintf(lines[6],48,"A ARMAGEDDON 80000 SPC PAUSE ESC QUIT");
    /* Re-rasterise only when the text or the loaded font changes. */
    static char shown[7][48]; static uint8_t font[sizeof game_font]; static int valid;
    for (int line=0; line<7; ++line) lines[line][40]=0;
    if (!valid || memcmp(shown,lines,sizeof shown) || valid != game_font_loaded+1 || memcmp(font,game_font,sizeof font)) {
        memset(hud,0,320*56);
        for (int line=0; line<7; ++line) amiga_text(hud,0,line*8,lines[line]);
        memcpy(shown,lines,sizeof shown); memcpy(font,game_font,sizeof font); valid=game_font_loaded+1;
    }
    for (int pixel=0; pixel<320*56; ++pixel)
        prg_screen->chunky_buffer[200*320+pixel]=prg_screen->palette[hud[pixel]];
    core_draw_pointer();
}

static void _draw_it(int16_t y, int16_t x)
{
    ensure_prg_screen();
    if (!prg_screen) return;
    const int width = 320, height = 256;
    if (diagnostic_view) {
    for (int screen_y = 0; screen_y < height; ++screen_y) {
        for (int screen_x = 0; screen_x < width; ++screen_x) {
            int column = (screen_x+x)/4, row = (screen_y+y)/4;
            uint32_t colour = 0xff182028;
            if (column < 64 && row < 64 && screen_y < 240) {
                int cell = row*64+column, block = map_blk[cell];
                colour = !block ? 0xff285899 : 0xff427346 + map_alt[cell]*0x090909;
                if (block && block != 15 && block != 31 && block != 32) colour = 0xff70805a;
                if (block == 31) colour = 0xff568980;
                if (block == 32) colour = 0xff88544c;
                if (((screen_x+x)%4 == 0 || (screen_y+y)%4 == 0) && block) colour &= 0xffeeeeee;
            }
            prg_screen->chunky_buffer[screen_y*width+screen_x] = colour;
        }
    }
    for (int person = 0; person < no_peeps; ++person) {
        if (peep_word(person,4) <= 0) continue;
        int sx = peep_screen_x(person)-x, sy = peep_screen_y(person)-y;
        int town = peeps[person*22]&1, battle = peeps[person*22]&8;
        uint32_t colour = peep_owner(person) ? 0xffff795d : 0xff8cffff;
        int radius = paint_map ? 0 : town ? 3 : 1;
        if (battle) colour = 0xffffff00;
        for (int dy = -radius; dy <= radius; ++dy) for (int dx = -radius; dx <= radius; ++dx) {
            int xx = sx+dx, yy = sy+dy;
            if (xx < 0 || xx >= width || yy < 0 || yy >= 240) continue;
            if (town && abs(dx)<radius && abs(dy)<radius && !paint_map) continue;
            prg_screen->chunky_buffer[yy*width+xx] = colour;
        }
    }
    int population[2] = {core_population(0),core_population(1)};
    int total = population[0]+population[1];
    for (int owner = 0; owner < 2; ++owner) {
        int length = total ? population[owner]*300/total : 0;
        for (int pixel = 0; pixel < length; ++pixel)
            prg_screen->chunky_buffer[(244+owner*3)*width+10+pixel] = owner ? 0xffff795d : 0xff8cffff;
    }
    int mana_length = core_mana(0) > 0 ? core_mana(0)/10 : 0;
    if (mana_length > 300) mana_length = 300;
    for (int pixel = 0; pixel < mana_length; ++pixel)
        prg_screen->chunky_buffer[251*width+10+pixel] = 0xffd8c060;
    if (pause || core_result) {
        uint32_t colour = core_result == 1 ? 0xff8cffff : core_result ? 0xffff795d : 0xffffff80;
        for (int pixel=0; pixel<width; ++pixel) prg_screen->chunky_buffer[pixel] = colour;
    }
    } else draw_original(x,y);
    char title[512];
    snprintf(title,sizeof title,"Populous | %s | cyan %d / red %d | mana %d | %s | 0 gather 1 settle 2 join 3 fight; M magnet; S swamp 5000; K knight 7500; Q quake 2500; F flood 40000; V volcano 10000; A Armageddon 80000; Space pause R reset Esc quit",
        core_result==1 ? "VICTORY" : core_result==2 ? "DEFEAT" : pause ? "PAUSED" : A4_WORD(0xae80) ? "WAR" : "RUNNING",
        core_population(0),core_population(1),core_mana(0),A4_WORD(0xae80) ?
        "TERRAIN AND MAGNET INPUT LOCKED / ARROWS SCROLL / MAP CLICK" : core_swamp_selected ?
        (core_tool == 1 ? "SWAMP ARMED (5000) LMB target then MAGNET / RMB inert" : core_tool == 2 ?
                          "SWAMP ARMED (5000) LMB target then QUERY / RMB inert" :
                          "SWAMP ARMED (5000) LMB target then TERRAIN / RMB inert") :
        (core_tool == 1 ? "MAGNET (200)" : core_tool == 2 ? "QUERY" : "TERRAIN (10+4/point)"));
    SDL_SetWindowTitle(prg_screen->window,title);
}

static void _do_knight(void) { core_knight(player); }
static void _do_war(void) { core_war(player); }
static void _do_flood(void) { core_flood(player); }
static void _do_quake(void) { core_quake(player,xoff,yoff,___newrand); }
static void _do_volcano(void) { core_volcano(player,xoff,yoff,___newrand); }
static void _do_funny(void) {}
/* Ordinary _set_frame branches, asm:5728-5743,5846-5861. */
static int core_frame_walker(int person)
{
    if (!peep_word(person,4)) return 0;
    int flags = peeps[person*22], previous = peep_word(person,12);
    if (flags == 2) {
        set_peep_word(person,12,previous >= 7 ? 0 : previous+1);
        return previous >= 7;
    }
    /* asm:5833-5845: exact-5/6 simulation consumers, not general mask 0x04.
     * Keep the signed incremented word even on completion; rendering selects
     * the corresponding original sprites separately. */
    if (flags == 5 || flags == 6) {
        int frame = (int16_t)(previous+1);
        set_peep_word(person,12,frame);
        return frame < 85 || frame >= 88;
    }
    if ((flags&0x60) && !(flags&0x1c)) {
        int frame = (int16_t)(previous+1);
        int complete = frame < 101 || frame > 102;
        set_peep_word(person,12,complete ? 101 : frame);
        return complete;
    }
    return 0;
}
/* _move_peeps asm:4058-4067,4145-4184: an exact-2 walker still alive after
 * framing (and any move) marks the peep map_who names at its own cell. No
 * owner, liveness or no_peeps test. With the port's immediate destination
 * occupancy a successful move names the walker itself, so this fires only
 * when it stays on a cell represented by another peep. */
static void core_collision_wait(int person)
{
    if (peep_word(person,4) <= 0) return;
    int other = map_who[peep_word(person,8)]-1;
    if (other == person || other == -1 || peep_word(other,12) || other >= 208 ||
        (peeps[other*22]&0x10)) return;
    peeps[other*22] |= 0x20;
    set_peep_word(other,12,101); set_peep_word(other,6,0);
}
static void _move_peeps(void)
{
    /* asm:3254-3270: the computer player acts on last pass's pointers while
     * its latch is clear; the latch is re-read between the two calls. */
    for (int owner = 0; owner < 2; ++owner) {
        if (!A4_WORD(0x82be + owner*46)) core_set_devil_magnet(owner);
        if (!A4_WORD(0x82be + owner*46)) core_devil_effect(owner);
    }
    /* asm:3271-3335: per-pass AI bookkeeping, including the good_pop reset. */
    for (int owner = 0; owner < 2; ++owner) {
        core_town_strength[owner] = (int16_t)(A4_WORD(0x82ca+owner*46)*3 + A4_WORD(0x82cc+owner*46));
        A4_WORD(0x99fc+owner*16) = 0;
        core_good_pop(owner) = 0;
        core_city_count[owner] = 0; core_youngest_town_age[owner] = 0x4e1f;
        A4_LONG(0x82d8+owner*46) = A4_LONG(0x82dc+owner*46) = A4_LONG(0x82e0+owner*46) = 0;
        core_oldest_town_age[owner] = 0; core_best_town_capacity[owner] = 0;
    }
    /* asm:3357-3424: release heading-bearing leaders before war recenters.
     * Presence, not liveness or reference validity, controls this prepass. */
    if (no_peeps >= 0 && no_peeps <= 208) for (int owner=0;owner<2;++owner) {
        int leader = core_leader(owner)-1;
        if (leader < 0 || leader >= no_peeps || leader >= 208) continue;
        if (peep_reference(leader) == -1) continue;
        int position = peep_word(leader,8);
        if (!pause && position >= 0 && position < 4096) {
            core_target(owner) = position;
            A4_WORD(owner ? 0xae76 : 0xae78) = position;
        }
        core_leader(owner) = 0;
    }
    /* asm:3425-3432: consume war state even with no living residents. */
    if (A4_WORD(0xae80)) {
        core_target(0) = core_target(1) = 0x820;
        A4_WORD(0xae78) = A4_WORD(0xae76) = 0x820;
    }
    /* asm:3434-3454: sounds flagged by the last _move_sprite pass. */
    if (sound_effect & 8) effect = (int16_t)(toggle + 0x43);
    if (sound_effect & 0x80) { if (!last_fire) effect = 0x46; last_fire = 3; }
    else if (last_fire) --last_fire;
    /* Existing walker/town/battle/remnant consumption; nonwar AI remains disabled. */
    for (int person = 0; person < no_peeps; ++person) {
        if (peep_word(person,4) <= 0) continue;
        /* asm:3459-3474: sum the pre-movement word (EXT.L) by owner byte. */
        core_good_pop(peeps[person*22+1]) += (uint32_t)(int32_t)peep_word(person,4);
        int flags = peeps[person*22];
        int position = peep_word(person,8);
        /* Entry flag, before water mutation or land recovery (asm:3481-3543).
         * Retain the host battle exemption and existing recovery behavior. */
        if ((flags&16) && !(flags&8) && !(game_mode&GAME_MODE_WATER_FATAL))
            core_queue_terrain_raise(peep_owner(person),position);
        if (!map_blk[position] && !(flags&8)) {
            if (flags&16) {
                /* asm:3484-3490,3544-3559,4308-4315: fatal removal sees
                 * original flags; otherwise store the word before signed death.
                 * Still wet skips framing and all lower-priority consumers. */
                if (game_mode&GAME_MODE_WATER_FATAL) { core_remove(person); continue; }
                uint16_t loss = (uint16_t)(2u*(uint16_t)A4_WORD(0x2fb0));
                set_peep_word(person,4,(uint16_t)((uint16_t)peep_word(person,4)-loss));
                if (peep_word(person,4)<=0) core_remove(person);
                continue;
            }
            if (flags&1) core_set_town(person,1);
            peeps[person*22] = 18;
            /* asm:3562-3580: new water entry only reframes; exact 18 then
             * reaches the common positive-population check without other work. */
            if (peep_word(person,12) >= 8) set_peep_word(person,12,0);
            continue;
        }
        if ((flags == 0x11 || flags == 18 || flags == 0x15 || flags == 0x16 ||
             flags == 0x32 || flags == 0x52 || flags == 0x72 || flags == 0x90) && map_blk[position]) {
            /* asm:3484-3490,3544-3557: dry recovery still loses first.
             * Frame even a negative word, but never zero; ignore completion.
             * Exact-2 dies before navigation; 5/6 finish celebration first;
             * 0x22/42/62 finish waiting loss and any retry before cleanup.
             * Exact-0x80 framing is inert; even nonpositive loss reaches its timer. */
            if (game_mode&GAME_MODE_WATER_FATAL) { core_remove(person); continue; }
            uint16_t loss = (uint16_t)(2u*(uint16_t)A4_WORD(0x2fb0));
            set_peep_word(person,4,(uint16_t)((uint16_t)peep_word(person,4)-loss));
            peeps[person*22] = flags &= 0xef;
            /* asm:5728,5810-5832: zero skips this first frame, negative does
             * not. Exact-1 still enters the ordinary town consumer below:
             * evacuation bypasses death; eligible growth can revive the word.
             * This does not enable town AI, candidate selection or funny work. */
            if (flags == 1) {
                if (peep_word(person,4)!=0)
                    core_frame_town(person,core_check_life(peep_owner(person),position));
            } else core_frame_walker(person);
            if (flags == 2 && peep_word(person,4)<=0) { core_remove(person); continue; }
        } else if (flags&16) { peeps[person*22] = 2; set_peep_word(person,12,0); flags = 2; }
        /* asm:3584-3599, scoped to exact 5/6 after existing water handling.
         * Completion clears only mask 0x04 and reframes without town maintenance,
         * evacuation or walking movement. Always end this person's tick. */
        if (flags == 5 || flags == 6) {
            if (core_frame_walker(person)) {
                peeps[person*22] &= 0xfb;
                /* Exact-1 _set_frame branch, asm:5810-5832, not _set_town. */
                if (peeps[person*22] == 1)
                    core_frame_town(person,core_check_life(peep_owner(person),position));
                else core_frame_walker(person);
            }
            if (peep_word(person,4)<=0) core_remove(person);
            continue;
        }
        if (flags&8) {
            int other = peep_word(person,6);
            if (other > person && other < no_peeps && peep_word(other,4)>0) core_battle(person,other);
        } else if (flags&0x60) {
            /* _move_peeps asm:4213-4268: loss every tick, unsigned old counter. */
            core_frame_walker(person);
            set_peep_word(person,4,peep_word(person,4)-A4_WORD(0x2fb0));
            unsigned previous = (uint16_t)peep_word(person,6);
            set_peep_word(person,6,(uint16_t)(previous+1));
            if (previous > 14) {
                peeps[person*22] &= 0x9f;
                core_frame_walker(person);
                if (peeps[person*22] == 2) core_move(person);
            }
            /* A retry can create a reciprocal battle before attrition kills us. */
            if (peep_word(person,4)<=0) core_remove(person);
        } else if (flags == 1) core_settle_tick(person);
        else if (flags == 0x80) {
            /* asm:4283-4315: ordinary remnant, not a knight flag. Water and
             * waiting/battle dispatch stay ahead of the heading-winner remnant. */
            if (!map_who[position]) map_who[position] = person+1;
            unsigned previous = (uint16_t)peep_word(person,6);
            set_peep_word(person,6,(uint16_t)(previous-1u));
            if (!previous || peep_word(person,4)<=0) core_remove(person);
        } else {
            /* asm:4040 (CMPI.B #2; BNE LAB_411C0): only exact 2 walks. Other
             * residual flags reach only the signed death check, and are
             * positive here. */
            if (flags != 2) continue;
            if (!core_frame_walker(person)) { core_collision_wait(person); continue; }
            int position = peep_word(person,8);
            /* asm:4061-4081: ordinary frame completion, before move/loss.
             * Water cells never get here: they end the turn at the top. */
            if (map_blk[position] == 53) {
                core_remove(person);
                A4_WORD(0x2fac) = 66;
                if (!(game_mode&2)) map_blk[position] = 15;
                continue;
            }
            /* asm:4092-4122: first walker of the pass for the enemy's AI
             * (host slot+1 encoding), then terrain flattening. */
            int owner = peep_owner(person);
            if (!A4_LONG(0x82dc+(owner ? 0 : 1)*46)) A4_LONG(0x82dc+(owner ? 0 : 1)*46) = (uint32_t)(person+1);
            if (!A4_WORD(0x82be + owner*46)) core_one_block_flat(position,owner);
            core_move(person);
            /* asm:4100-4117: drop a stale +0x12 cell, then this path's loss. */
            if (peep_word(person,18) &&
                (int16_t)(peep_word(person,8)-peep_word(person,10)) != peep_word(person,18))
                set_peep_word(person,18,0);
            set_peep_word(person,4,peep_word(person,4)-A4_WORD(0x2fb0));
            core_collision_wait(person);
            if (peep_word(person,4)<=0) core_remove(person);
        }
    }
    /* asm:4349-4370: publish this pass's town and city counts. */
    for (int owner = 0; owner < 2; ++owner) {
        A4_WORD(0x82cc+owner*46) = A4_WORD(0x99fc+owner*16);
        A4_WORD(0x82ca+owner*46) = core_city_count[owner];
    }
}
static void _show_the_shield(void) { core_shield_state(); }
static void _sculpt(void) {}
static void _zoom_map(int16_t d) {}
/* _set_temp_view asm:3151-3160; restored by the countdown in game_tick. */
static void core_set_temp_view(int16_t who)
{
    if (!view_timer) old_view_who = view_who;
    view_timer = 10;
    view_who = who;
}
/* _interogate asm:2719-2765: first sprite entry with mx in [x,x+12] and
 * my in [y,y+8]; query mode only. Right keeps the view, else temporary. */
static void core_interogate(int mx, int my, int right)
{
    for (int i = 0; i < no_sprites && i < 64; ++i) {
        int16_t x = A4_WORD(0x9a22+i*8), y = A4_WORD(0x9a24+i*8), who = A4_WORD(0x9a28+i*8);
        if (mx < x || mx > x+12 || my < y || my > y+8) continue;
        if (core_tool == 2) {
            if (right) { view_timer = 0; view_who = who; }
            else core_set_temp_view(who);
        }
        return;
    }
}
static void _interogate(void) { core_interogate(mousex,mousey,right_button); }
/* xoff/yoff = cell column/row - 3; ASR.W #6 floors negative rows. */
static void core_view_cell(int16_t cell)
{
    xoff = (int16_t)((cell & 63) - 3);
    yoff = (int16_t)((cell >= 0 ? cell/64 : -((-cell+63)/64)) - 3);
}
#define view_fight  A4_WORD(0x2f92)
#define view_people A4_WORD(0x2f94)
/* Battle cycle, asm:2229-2290. The original never stops when no battle exists
 * and view_fight+1 == no_peeps (the wrap skips the end test); bound the scan. */
static void core_next_battle(void)
{
    if (!no_peeps) return;
    if (view_fight >= no_peeps) view_fight = 0;
    int start = view_fight + 1, person = start;
    for (int seen = 0; seen <= no_peeps; ++seen, ++person) {
        if (person >= no_peeps) person = 0;
        if (person == start && seen) return;
        if ((peeps[person*22] & 0x08) && peep_word(person,4)) {
            core_view_cell(peep_word(person,8));
            view_fight = (int16_t)person;
            core_set_temp_view((int16_t)(person+1));
            return;
        }
    }
}
/* Own-walker cycle, asm:2291-2395: left picks peeps with a heading reference,
 * right picks ordinary walkers (type 1); both need positive population. */
static void core_next_person(int left)
{
    if (!no_peeps) return;
    if (view_people >= no_peeps) view_people = 0;
    int start = view_people + 1, person = start;
    for (int seen = 0; ; ++person) {
        if (person == start && seen) return;
        if (person == no_peeps) person = 0;
        seen = 1;
        if (peep_owner(person) != player || peep_word(person,4) <= 0) continue;
        uint32_t reference; memcpy(&reference,peeps+person*22+14,sizeof reference);
        if (left ? reference != 0 : peeps[person*22] == 1) {
            core_view_cell(peep_word(person,8));
            view_people = (int16_t)person;
            core_set_temp_view((int16_t)(person+1));
            return;
        }
    }
}
/* Toolbar clicks of _zoom_map, asm:1700-2451 (power grid jump table asm:2406-2424).
 * left is the original argument (left button held). Mailbox posts happen only
 * when post is set; costs, permissions, war and pause stay with the consumer
 * and the power routines. Swamp alone checks mana > cost and permission bit 4
 * before arming the map target. Camera, tool and audio-flag icons always act.
 * Returns whether (sx,sy) hits a handled icon. */
static int core_toolbar(int sx, int sy, int left, int post)
{
    int c = sx/2 + sy - 32, r = sy - sx/2 + 32;
    if (sy >= 200) return 0;
    int owner_ok = player >= 0 && player <= 1;
    uint8_t *mailbox = owner_ok ? A4_DISP(0x82b6 + player*46) : NULL;
    post = post && owner_ok;
    if (c > 0x112) {
        /* System icons asm:1700-1829: options, game options, pause, sound. */
        int icon = (c-0x110)/16, line = (r-0x20)/16;
        if (icon == 0 && line == 0) { if (post) { mailbox[0] = 14; mailbox[1] = (uint8_t)left; mailbox[2] = 7; } }
        /* asm:1744-1781: switching on also stops all four channels. */
        else if (icon == 0 && line == 3) { if ((music_off = !music_off)) core_kill_effect(0,4); }
        else if (icon == 0 && line == 4) { if ((effect_off = !effect_off)) core_kill_effect(0,4); }
        else if (icon == 1 && line == 1) { if (post) { mailbox[0] = 14; mailbox[2] = 8; } }
        /* _do_action y=6 asm:18591-18608. The port's pause also stops the
         * mailbox consumer, so this icon toggles directly rather than posting. */
        else if (icon == 1 && line == 3) { pause = !pause; A4_WORD(0x9a1a) = 0; }
        else if (icon == 2 && line == 2) { if (post) { mailbox[0] = 14; mailbox[2] = 2; } }
        else return 0;
        return 1;
    }
    if (r < 0x92) return 0;
    int icon = (c-0x60)/16, line = (r-0x90)/16;
    if (icon < 0 || icon > 8) return 0;
    if (icon >= 3 && icon <= 5 && line <= 2) {
        /* Scroll pad asm:1838-1902; its centre recentres on view_who. */
        if (icon == 4 && line == 1) {
            if (view_who > 0 && view_who <= no_peeps) core_view_cell(peep_word(view_who-1,8));
        } else {
            xoff = (int16_t)(xoff + (icon == 5) - (icon == 3));
            yoff = (int16_t)(yoff + (line == 2) - (line == 0));
        }
    } else if (icon <= 2) {
        if (line > 2) return 0;
        if (!post) return 1;
        if (icon == 0 && line == 0) { mailbox[0] = 14; mailbox[2] = 4; }
        else if (icon == 1 && line == 0) { if (!paint_map) { mailbox[0] = 14; mailbox[2] = 3; } }
        else if (icon == 1 && line == 1) { mailbox[0] = 6; mailbox[1] = (uint8_t)xoff; mailbox[2] = (uint8_t)yoff; }
        else if (icon == 2 && line == 0) { mailbox[0] = 3; mailbox[1] = (uint8_t)xoff; mailbox[2] = (uint8_t)yoff; }
        else if (icon == 2 && line == 1) { mailbox[0] = 14; mailbox[2] = 5; }
        else if (icon == 2 && line == 2) {
            if ((core_mana(player) > (int32_t)A4_LONG(0x8496) && (A4_WORD(0x82c4+player*46)&0x10)) || paint_map)
                { core_swamp_selected = 1; core_set_pointer(); }
        }
    } else if (icon <= 5) {
        /* Tendency icons asm:2043-2089: _do_action y=1 with x = tendency. */
        static const int8_t tendency[3][2] = {{0,-1},{1,3},{2,-1}};
        if (line < 3 || line > 4 || tendency[icon-3][line-3] < 0) return 0;
        if (post) { mailbox[0] = 14; mailbox[1] = (uint8_t)tendency[icon-3][line-3]; mailbox[2] = 1; }
    } else if (icon == 6) {
        /* Mode icons asm:2090-2133: 0 query, 1 terrain, 2 papal magnet; each
         * clears the swamp bit. core_tool: 0 terrain, 1 magnet, 2 query. */
        if (line > 2) return 0;
        core_tool = line == 0 ? 2 : line == 2; core_swamp_selected = 0; core_set_pointer();
    } else if (icon == 7) {
        /* asm:2134-2228: left views the leader (else the magnet), right the magnet. */
        if (line == 0) {
            if (!owner_ok) return 1;
            int16_t leader = core_leader(player);
            if (left && leader > 0 && leader <= no_peeps) {
                core_view_cell(peep_word(leader-1,8));
                core_set_temp_view(leader);
            } else if (left || core_target(player)) core_view_cell(core_target(player));
        } else if (line == 1) core_next_battle();
        else return 0;
    } else {
        if (line != 0) return 0;
        core_next_person(left);
    }
    /* Common tail asm:2425-2440. */
    if (xoff < 0) xoff = 0;
    if (xoff > 56) xoff = 56;
    if (yoff < 0) yoff = 0;
    if (yoff > 56) yoff = 56;
    return 1;
}
static void _do_place_funny(int16_t a) { (void)a; }   /* matches the chunk call */

/* ___ shims (SDL / stdlib backed) */
static void *amiga_alloc_mem(uint32_t s,uint32_t a){(void)a;return calloc(1,s);}
static void amiga_free_mem(void *p){free(p);}
static void ___open_screen(void)
{
    ensure_prg_screen();
    printf("[SDL] open_screen (real window created)\n");
}
static void ___read_sprites(int16_t n){printf("[PRG] read_sprites %d\n",n);}
static void ___sprite_to_amiga(void){printf("[PRG] sprite_to_amiga\n");}
static void ___start_game(void){printf("[PRG] start_game\n"); core_sound_start();}
static void ___set_serial(void){printf("[PRG] set_serial\n");}
static void ___load_ground(int16_t a, int16_t terrain)
{
    /* Rule tables and indexed graphics are loaded transactionally by game_reset. */
    (void)a; (void)terrain;
}
static void ___Setscreen(int16_t m,void *s1,void *s2){printf("[SDL] Setscreen\n");}
static void ___copy_screen(void *d,void *s){printf("[SDL] copy_screen\n");}
static void ___toggle_icon(int16_t i,int16_t a,int16_t b,void *scr){printf("[SDL] toggle_icon 0x%04x\n",(uint16_t)i);}
static void ___draw_it(int16_t y,int16_t x){_draw_it(y, x);}
static void ___draw_s_32(int16_t x,int16_t y,void *scr){printf("[SDL] draw_s_32\n");}
static void ___draw_sprite(int16_t f,int16_t x,int16_t y,void *scr){printf("[SDL] draw_sprite\n");}
static void ___clr_wsc(void)
{
    /* Called every frame before main draw in the _animate path.
     * For the demo, we can use it to occasionally "age" or reset minor overlay state
     * (e.g. help the shield timer or dim a temp effect). Keeps the transcribed loop active.
     */
    if (demo_shield_timer > 0 && (growth_tick & 1)) {
        demo_shield_timer--;  /* gentle decay if not refreshed */
    }
    /* printf removed for clean demo runs */
}
static void ___swap_screens(void)
{
    ensure_prg_screen();
    if (prg_screen) amiga_present(prg_screen);
}
static int  ___show_world(void){printf("[PRG] show_world\n");return 0;}
static void ___return_to_game(void){printf("[PRG] return_to_game\n");}
static void ___display_debug(void){printf("[PRG] display_debug (1993)\n");}
static void ___paint_the_map(void *p){printf("[PRG] paint_the_map\n");}
static void ___text(const char *s,int16_t y,int16_t x,void *scr){printf("[SDL] text %s\n",s?s:"");}
static void ___a_putpixel(int16_t c,int16_t pos){printf("[SDL] a_putpixel\n");}
static void ___keyboard(void)
{
    SDL_Event event;
    while (!prg_quit && SDL_PollEvent(&event)) game_handle_event(&event);
}

static void game_handle_event(const SDL_Event *event)
{
    if (event->type == SDL_QUIT) { prg_quit = 1; return; }
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_ESCAPE) { prg_quit = 1; return; }
        /* _show_world's _get_name reads ___keyboard (_asckey). */
        if (core_req.kind == CORE_REQ_WORLD) {
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER) core_world_key(13);
            else if (key >= 0 && key < 0x80) core_world_key((int)key);
            return;
        }
        if (core_req.kind == CORE_REQ_FILE) {   /* _save_load's _get_name */
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER) core_file_key(13);
            else if (key >= 0 && key < 0x80) core_file_key((int)key);
            return;
        }
        if (core_req.kind || core_lord.shown) return;   /* the requester and lord loops read only the mouse */
        /* _end_game waits for the requester (asm:14370-14385): any other key. */
        if (core_end_shown && core_result) { if (!event->key.repeat) core_leave_end(); return; }
        if (!event->key.repeat) {
            /* _animate asm:917: unhandled keys go to _paint_the_map, which
             * acts only with _paint_map set (Amiga raw codes). */
            int raw = key >= SDLK_F1 && key <= SDLK_F8 ? 0x50 + (int)(key - SDLK_F1) :
                      key == SDLK_DELETE ? 0x46 : key >= SDLK_1 && key <= SDLK_9 ? (int)(key - SDLK_0) : -1;
            if (raw >= 0 && core_paint_the_map(raw, (event->key.keysym.mod & KMOD_SHIFT) != 0)) return;
            if (key == SDLK_SPACE) pause = !pause;
            if (key == SDLK_d) { diagnostic_view = !diagnostic_view; xoff = yoff = diagnostic_view ? 0 : 6; }
            if (key == SDLK_r && !game_reset_start(current_start))
            fprintf(stderr, "Reset failed; keeping the current game.\n");
            if (!pause && !core_result && key >= SDLK_0 && key <= SDLK_3) core_tendency(0) = key-SDLK_0;
            if (key == SDLK_m) { core_tool = core_tool != 1; core_set_pointer(); }
            if (key == SDLK_q) _do_quake();
            if (key == SDLK_f) _do_flood();
            if (key == SDLK_v) _do_volcano();
            if (key == SDLK_k) _do_knight();
            /* Host binding, not original A (which was a debug win). */
            if (key == SDLK_a) _do_war();
            /* Host S mapping for source selection asm:2021-2042 (> cost). */
            if (key == SDLK_s && player >= 0 && player <= 1 &&
                ((core_mana(player) > (int32_t)A4_LONG(0x8496) &&
                  (A4_WORD(0x82c4+player*46)&0x10)) || paint_map)) { core_swamp_selected = 1; core_set_pointer(); }
        }
        if ((key == SDLK_LEFT || key == SDLK_KP_4 || key == SDLK_KP_7 || key == SDLK_KP_1) && xoff > 0) --xoff;
        if ((key == SDLK_RIGHT || key == SDLK_KP_6 || key == SDLK_KP_9 || key == SDLK_KP_3) && xoff < 56) ++xoff;
        if ((key == SDLK_UP || key == SDLK_KP_8 || key == SDLK_KP_7 || key == SDLK_KP_9) && yoff > 0) --yoff;
        if ((key == SDLK_DOWN || key == SDLK_KP_2 || key == SDLK_KP_1 || key == SDLK_KP_3) && yoff < 56) ++yoff;

    }
    if (event->type == SDL_MOUSEMOTION && prg_screen) {
        int width, height;
        SDL_GetWindowSize(prg_screen->window, &width, &height);
        if (width > 0 && height > 0) {
            int x = event->motion.x, y = event->motion.y;
            if (x < 0) x = 0;
            if (x >= width) x = width - 1;
            if (y < 0) y = 0;
            if (y >= height) y = height - 1;
            mousex = (int16_t)((int64_t)x * prg_screen->width / width);
            mousey = (int16_t)((int64_t)y * prg_screen->height / height);
            big_mousex = mousex;
            big_mousey = mousey;
            /* _sculpt asm:7406-7416 keeps _cur_x/_cur_y on the vertex under
             * the pointer; _paint_the_map posts there. */
            int column, row;
            if (!diagnostic_view && !core_req.kind && game_pick(mousex,mousey,&column,&row)) {
                A4_WORD(0x3028) = (int16_t)column; A4_WORD(0x3026) = (int16_t)row;
            }
            /* continue_3EC20 asm:963-975 also interrogates while hovering. */
            if (!event->motion.state && !diagnostic_view && !core_req.kind && !core_lord.shown) core_interogate(mousex,mousey,0);
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && prg_screen) {
        int width,height; SDL_GetWindowSize(prg_screen->window,&width,&height);
        if (width <= 0 || height <= 0 || event->button.x < 0 || event->button.y < 0 ||
            event->button.x >= width || event->button.y >= height) return;
        int sx = (int64_t)event->button.x*320/width, sy = (int64_t)event->button.y*256/height;
        /* Requesters take the left press at the pointer (_left_button,
         * _mousex/_mousey); _end_game continues only on its button. */
        if (core_req.kind || core_lord.shown || (core_end_shown && core_result)) {
            if (event->button.button != SDL_BUTTON_LEFT) return;
            mousex = (int16_t)sx; mousey = (int16_t)sy;
            if (core_lord.shown) core_lord_leave();   /* _left_button cleared, asm:14503 loop */
            else if (core_req.kind) core_requester_click();
            else { core_end_button_layout(0); if (core_check_cancel(core_end_button,NULL)) core_leave_end(); }
            return;
        }
        int column,row;
        if (!diagnostic_view && sy<64 && sx>=1 && sx<=127) {
            column=sy+(sx-64)/2; row=sy-(sx-64)/2;
            if (column>=0 && column<64 && row>=0 && row<64) {
                if (event->button.button==SDL_BUTTON_LEFT) {
                    xoff=column<4 ? 0 : column>60 ? 56 : column-4;
                    yoff=row<4 ? 0 : row>60 ? 56 : row-4;
                }
                return;
            }
        }
        /* A paused or finished game never consumes mailboxes; drop the post.
         * _zoom_map runs only for the left or right button (asm:976-989). */
        if (!diagnostic_view && (event->button.button == SDL_BUTTON_LEFT ||
            event->button.button == SDL_BUTTON_RIGHT) &&
            core_toolbar(sx,sy,event->button.button == SDL_BUTTON_LEFT,
                         !pause && !core_result)) return;
        /* Query mode skips _sculpt: clicks only interrogate (asm:963-975). */
        if (core_tool == 2 && !core_swamp_selected) {
            if (!diagnostic_view && (event->button.button == SDL_BUTTON_LEFT ||
                event->button.button == SDL_BUTTON_RIGHT))
                core_interogate(sx,sy,event->button.button == SDL_BUTTON_RIGHT);
            return;
        }
        /* _sculpt asm:7295 / _do_magnet asm:6784: keep camera usable,
         * but reject world input before consuming an armed target. */
        if (pause || core_result || A4_WORD(0xae80)) return;
        if (diagnostic_view) {
            if (sy>=240) return;
            column=(sx+xoff)/4; row=(sy+yoff)/4;
            if (column>=64 || row>=64) return;
        } else if (!game_pick(sx,sy,&column,&row)) return;
        if (core_swamp_selected) {
            if (event->button.button == SDL_BUTTON_LEFT) {
                /* asm:7451-7489 clears selection before command execution.
                 * Diagnostic picks use this same vertex-to-byte mapping. */
                core_swamp_selected = 0; core_set_pointer();
                core_swamp(player,(column-1)&255,(row-1)&255,___newrand);
            }
            return;
        }
        if (core_tool == 1) {
            if (column>=64 || row>=64) return;
            if (event->button.button == SDL_BUTTON_LEFT && core_leader(0) && core_mana(0)>=200) {
                core_add_mana(0,-200); core_target(0)=row*64+column; core_tendency(0)=0;
            }
        } else if (paint_map && (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)) {
            /* _sculpt asm:7307-7312,7417-7438: the editor skips _ok_to_build
             * and the game-mode build limits; the raise/lower opcodes then
             * run free (_do_raise_point/_do_lower_point). */
            A4_WORD(0x3028) = (int16_t)column; A4_WORD(0x3026) = (int16_t)row;
            if (event->button.button == SDL_BUTTON_LEFT) core_raw_raise(player,column,row);
            else if (column <= 64 && row <= 64) core_do_lower_point(player,column,row);
        } else if (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)
            core_sculpt(0,column,row,event->button.button == SDL_BUTTON_LEFT ? 1 : -1);
    }
    /* Unsupported special powers remain inert. */
}
static void ___setup_mouse(void){printf("[SDL] setup_mouse\n");}
static void ___AddIntServer(int16_t l,void *is){printf("[PRG] AddIntServer %d\n",l);}
static void ___RemIntServer(int16_t l,void *is){printf("[PRG] RemIntServer %d\n",l);}
static void ___Delay(int16_t t){/*SDL_Delay*/;}
static void ___reset_screen(void *v){printf("[SDL] reset\n");}
static void ___free_amiga_sprites(void){printf("[PRG] free sprites\n");}
static void ___free_all_sounds(void){core_free_all_sounds();}
static void ___close_channels(void){audio_close();}
static void *___FindTask(void *t){return (void*)1;}
static void ___exit(int16_t c){printf("[PRG] exit\n");exit(c);}
static int32_t ___mulu(int32_t a,int32_t b){return a*b;}
static int32_t ___divs(int32_t a,int32_t b){return b?a/b:0;}
/* Correct implementation of the game's _newrand (from the disassembly BANNER and tetracorp analysis).
 * It is a 16-bit LCG that only produces values 0..32767.
 * Uses the emulated global 'seed' variable (A4 offset 0x99e2).
 */
static uint16_t ___newrand(void)
{
    uint16_t d0 = seed;

    /* MULU #$24a1 */
    uint32_t temp = (uint32_t)d0 * 0x24a1;
    d0 = (uint16_t)temp;

    /* ADDI.W #$24df */
    d0 += 0x24df;

    /* BCLR #$F  (clear bit 15) */
    d0 &= 0x7fff;

    seed = d0;   /* write back to the global the game uses */
    return d0;
}

/* When you paste the next chunk (more of _animate, _move_peeps, _draw_map, any _do_* etc.)
 * we will replace the corresponding stub or extend the logic above with the real 68k translation,
 * keeping A4 + SDL low-level + all the 1993 notes.
 */

#ifdef PRG_STANDALONE_TEST
int main(void)
{
    prg_main();
    return 0;
}
#endif
