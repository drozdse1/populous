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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "symbols_prg.h"  /* all the _function addresses and data from the map */
#include "sdl_lowlevel.h" /* our low-level Amiga gfx emulation (bitplanes, sprites, copper/vsync, palette) */

/* Emulated A4 base for this prg's globals.
 * The original uses BASEREG A4 at $000593EE for almost all game state
 * (map data, peeps, UI, etc.). We allocate a block and use it as the
 * A4 "register".
 */
static uint8_t prg_a4_area[256 * 1024];  /* larger for map/peeps/sprites/back buffers + everything */
#define prg_a4 ((void *)&prg_a4_area[0])

/* Visible demo support for motivation */
static AmigaScreen *prg_screen = NULL;
static int prg_quit = 0;   /* set by ESC in keyboard */

/* Forward so we can use it in prg_main for the immediate test pattern */
static void ensure_prg_screen(void);

/* A4-relative displacements from this assembly chunk (BASEREG _A4,A4).
 * The numbers come from the (d16,A4) modes in the opcodes (e.g. 29409868 = MOVE.L D0,(A4,$9868)).
 * These land at the absolute locations listed in the symbol map / labels.
 * Use these for direct faithful emulation of the original data layout.
 */
#define A4_DISP(d16)   (prg_a4 + (int16_t)(d16))

/* Common ones from _main + init + _setup_display + _animate in this chunk */
#define sprite_data      (*(uint32_t *)A4_DISP(0x9868))
#define sprite_data_32   (*(uint32_t *)A4_DISP(0x986c))
#define blk_data         (*(uint32_t *)A4_DISP(0x9870))
#define back_scr         (*(uint32_t *)A4_DISP(0x9874))
#define screen_pointer   (*(uint32_t *)A4_DISP(0x9878))
#define copper_list      (*(uint32_t *)A4_DISP(0x9884))
#define spare_cl         (*(uint32_t *)A4_DISP(0x9890))
#define fb               (*(uint32_t *)A4_DISP(0x9894))
#define start_of_game    (*(int16_t *)A4_DISP(0x9898))
#define sprites_in       (*(int16_t *)A4_DISP(0x989a))
#define in_conquest      (*(int16_t *)A4_DISP(0x84ba))
#define conq_08_seed     (*(int16_t *)A4_DISP(0x84c4))
#define conq_05_terrain  (*(uint8_t *)A4_DISP(0x84c1))
#define seed             (*(int16_t *)A4_DISP(0x99e2))
#define start_seed       (*(int16_t *)A4_DISP(0x99e4))
#define ground_in        (*(int16_t *)A4_DISP(0x99e6))
#define conquest         (*(uint8_t *)A4_DISP(0x84bc))
#define conq_01_speed    (*(uint8_t *)A4_DISP(0x84bd))
#define conq_02_enemypow (*(uint8_t *)A4_DISP(0x84be))
#define conq_03_yourpow  (*(uint8_t *)A4_DISP(0x84bf))
#define conq_04_mode     (*(uint8_t *)A4_DISP(0x84c0))
#define game_mode        (*(int16_t *)A4_DISP(0x84b8))
#define w_screen         (*(uint32_t *)A4_DISP(0x99e8))
#define d_screen         (*(uint32_t *)A4_DISP(0x99ec))
#define pal              (*(int16_t *)A4_DISP(0x99e0))
#define mouse_limit      (*(int16_t *)A4_DISP(0x99a8))
#define mousex           (*(int16_t *)A4_DISP(0x99aa))
#define mousey           (*(int16_t *)A4_DISP(0xae6e))  /* from the assembly chunk */
#define big_mousex       (*(int16_t *)A4_DISP(0x99b0))
#define big_mousey       (*(int16_t *)A4_DISP(0x99b2))
#define dint             (A4_DISP(0x984a))          /* interrupt server struct */
#define xoff             (*(int16_t *)A4_DISP(0x99f0))
#define yoff             (*(int16_t *)A4_DISP(0x99f2))
#define player           (*(int16_t *)A4_DISP(0x99f4))
#define good_pop         (A4_DISP(0x99fe))          /* array base */
#define tempo            (*(int16_t *)A4_DISP(0x824e))
#define tempo_now        (*(int16_t *)A4_DISP(0x9a18))
#define beat_two         (*(int16_t *)A4_DISP(0x9a16))
#define toggle           (*(int16_t *)A4_DISP(0x9a1a))
#define no_sprites       (*(int16_t *)A4_DISP(0x9a1c))
#define ok_to_build      (*(int16_t *)A4_DISP(0x9a1e))
#define sound_effect     (*(uint8_t *)A4_DISP(0x9a20))
#define mode             (*(int16_t *)A4_DISP(0x8256))
#define pointer          (*(int16_t *)A4_DISP(0x84b6))
#define sprite           (A4_DISP(0x9a22))          /* sprite list base */
#define peeps            (A4_DISP(0x9c26))          /* peeps array base */
#define no_peeps         (*(int16_t *)A4_DISP(0xae5e))
#define view_timer       (*(int16_t *)A4_DISP(0xae60))
#define left_button      (*(int16_t *)A4_DISP(0xae62))
#define right_button     (*(int16_t *)A4_DISP(0xae64))
#define old_view_who     (*(int16_t *)A4_DISP(0xae66))
#define view_who         (*(int16_t *)A4_DISP(0xae68))
#define game_turn        (*(int16_t *)A4_DISP(0xae6a))
#define pause            (*(int16_t *)A4_DISP(0x9c22))
#define paint_map        (*(int16_t *)A4_DISP(0x9c24))
#define serial_off       (*(int16_t *)A4_DISP(0x8254))
#define not_player       (*(int16_t *)A4_DISP(0x825a))
#define stats            (A4_DISP(0x82b6))          /* stats array */
#define inkey            (*(uint8_t *)A4_DISP(0xae6c))
#define bitfield_51645   (*(uint8_t *)A4_DISP(0x8257))
#define GfxBase_prg      (*(uint32_t *)A4_DISP(0x99a4)) /* from _rs etc */
#define IntuitionBase_prg (*(uint32_t *)A4_DISP(0x99ac))

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
#define _my_task         (*(uint32_t *)A4_DISP(0x9860)) /* used in openlibrary */

/* Conquest / other small vars referenced */
#define LAB_516DE        (*(int16_t *)A4_DISP(0x82f0))
#define LAB_516E2        (*(int16_t *)A4_DISP(0x82f4))
#define LAB_516E0        (*(int16_t *)A4_DISP(0x82f2))
#define LAB_516B2        (*(int16_t *)A4_DISP(0x82c4))
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
 * These are used by the current motivation-mode implementations of
 * _make_map, _place_first_people, _draw_it and _move_peeps.
 * They let you immediately see the effect of different level seeds / conquest params.
 */
static int8_t  demo_height[64][64];
static int     demo_peep_x[32];
static int     demo_peep_y[32];
static int     num_demo_peeps = 0;

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

static void _PlaySound(void);
static void _setsound(void);
static void _PlayMeas(void);
static void _load_sound(void);
static void _open_channels(void);
static void _close_channels(void);
static void _free_all_sounds(void);
static void _kill_effect(void);
static void _check_effect(void);

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

/* Minimal sample conquest level table for quick testing of "level loading" and init.
 * Based on the level.dat format from the tetracorp analysis:
 * [enemy_rating, speed, enemy_powers, your_powers, game_mode, terrain, pop_you, pop_enemy, seed_offset]
 */
static const uint8_t demo_levels[8][9] = {
    /* lvl 0 */ { 5, 5, 0x07, 0x07, 0x00, 0, 12, 10, 0x0000 },  /* classic grass */
    /* lvl 5 */ { 4, 6, 0x0f, 0x07, 0x01, 1,  8, 14, 0x1234 },  /* desert */
    /* lvl 10*/ { 3, 7, 0x1f, 0x0f, 0x02, 2, 15,  9, 0xabcd },  /* snow */
    /* lvl 15*/ { 6, 4, 0x07, 0x1f, 0x04, 3, 10, 12, 0x5678 },  /* rocky, you strong */
    /* lvl 20*/ { 2, 8, 0x3f, 0x07, 0x08, 0,  6, 18, 0x9abc },  /* hard enemy */
    /* lvl 25*/ { 7, 3, 0x01, 0x3f, 0x10, 1, 20,  5, 0xdef0 },  /* you very strong */
    /* lvl 30*/ { 4, 5, 0x15, 0x15, 0x00, 2, 11, 11, 0x1111 },  /* balanced snow */
    /* lvl 35*/ { 5, 6, 0x2a, 0x15, 0x03, 3,  9, 13, 0x2222 },  /* mixed rocky */
};

void prg_main(void)
{
    printf("[PRG] prg_main (1993) A4=0x%08X -> real _main from chunk\n",
           (unsigned)PRG_A4_BASE);

    /* === Fast testing of levels / game init ===
     *   ./populous_init_port 5
     *   ./populous_init_port 42
     * This directly influences the conquest variables (in_conquest, seed, terrain, etc.)
     * so you can rapidly iterate on "level loading" and "init game status" and see
     * different maps + different numbers of peeps immediately in the window.
     */
    if (g_argc > 1) {
        int idx = atoi(g_argv[1]) % 8;   /* map to our small demo table */
        const uint8_t *e = demo_levels[idx];

        /* Apply the "level data" like the real game would from level.dat + code decode */
        conquest        = e[0];
        conq_01_speed   = e[1];
        conq_02_enemypow= e[2];
        conq_03_yourpow = e[3];
        game_mode       = e[4];
        conq_05_terrain = e[5];
        /* We don't have full pop bytes yet, but we can fake starting pop for demo */
        /* For now just set a hint in a visible place */
        /* The real code would also set starting populations from e[6]/e[7] */
        conq_08_seed    = e[8];
        in_conquest     = (idx * 5) & 0x1ff;   /* simulate battle number */

        printf("[QUICKTEST] Loaded demo level slot %d (simulating level.dat entry)\n", idx);
    } else {
        /* Default */
        in_conquest = 0;
    }

    /* Make sure _place_first_people sees the updated conquest value */
    /* (it already reads conquest and in_conquest) */

    /* Immediate visible test pattern — create screen inline so we don't hit declaration order issues */
    if (!prg_screen) {
        prg_screen = amiga_create_screen(320, 256, 4);
        if (prg_screen) {
            for (int i = 0; i < 16; i++) {
                int v = (i * 17) & 0xFF;
                amiga_set_color(prg_screen, i, (i << 8) | (i << 4) | i);
            }
        }
    }
    if (prg_screen) {
        /* Bright obvious green background */
        uint32_t green = prg_screen->palette[10];   /* pick a bright color from the palette we just set */
        for (int i = 0; i < prg_screen->width * prg_screen->height; i++) {
            prg_screen->chunky_buffer[i] = green;
        }
        /* Big obvious white rectangle in the middle so it's impossible to miss */
        for (int y = 80; y < 160; y++) {
            for (int x = 40; x < 280; x++) {
                prg_screen->chunky_buffer[y * prg_screen->width + x] = prg_screen->palette[15];
            }
        }
        amiga_present(prg_screen);
        printf("[PRG] *** VISIBLE TEST: green screen + white box should be on screen now ***\n");
        /* Give you time to see the test window before the real game init runs */
        for (volatile int d=0; d < 200000000; d++) {}   /* crude delay so the window stays visible a moment */
    }

    _main();   /* the translated body from the assembly you pasted */
}

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

static void _make_map(void)
{
    /* Procedural height map using the *real* game RNG (_newrand).
     * This is much better than the previous generic 32-bit LCG with magic numbers like 1103515245.
     * The game only has a 15-bit RNG (0..32767), and we now call the correct implementation.
     *
     * The seed is already set up by the translated _setup_display code from conquest data.
     */
    /* use the shared demo_height declared at file scope */

    /* Make sure the global 'seed' is initialized from start_seed if needed */
    if (seed == 0) {
        seed = start_seed;
    }

    /* Multi-pass "raise/lower" using the authentic ___newrand() */
    for (int pass = 0; pass < 5; pass++) {
        int strength = 6 - (pass / 2);

        for (int i = 0; i < 180; i++) {
            uint16_t r1 = ___newrand();
            uint16_t r2 = ___newrand();
            uint16_t r3 = ___newrand();

            int cx = (r1 % 58) + 3;
            int cy = (r2 % 58) + 3;

            int delta = (r3 & 1) ? strength : -strength;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int x = cx + dx;
                    int y = cy + dy;
                    if (x >= 0 && x < 64 && y >= 0 && y < 64) {
                        int newh = demo_height[y][x] + delta;
                        if (newh < -24) newh = -24;
                        if (newh > 24)  newh = 24;
                        demo_height[y][x] = newh;
                    }
                }
            }
        }
    }

    /* Optional: rough stats so you can see that different CLI numbers really change the map */
    int min_h = 99, max_h = -99;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            if (demo_height[y][x] < min_h) min_h = demo_height[y][x];
            if (demo_height[y][x] > max_h) max_h = demo_height[y][x];
        }

    printf("[PRG] _make_map (real _newrand) seed=%d terrain=%d height range %d..%d  peeps=%d\n",
           start_seed, ground_in, min_h, max_h, num_demo_peeps);
}



static void _place_first_people(void)
{
    ensure_prg_screen();

    /* Place demo population using the loaded level data when available.
     * This makes "level laden + init game status" directly visible:
     * different CLI numbers now produce clearly different starting populations.
     */
    num_demo_peeps = 10;

    if (in_conquest != (int16_t)0xffff) {
        /* When a demo level was loaded via CLI, we can make this more accurate.
         * For now we still use a simple derivation, but the table drives the other params.
         */
        num_demo_peeps = 6 + (conquest % 12);
    }
    if (num_demo_peeps > 32) num_demo_peeps = 32;

    /* Position the demo peeps using the real game RNG */
    if (seed == 0) seed = start_seed;

    for (int i = 0; i < num_demo_peeps; i++) {
        uint16_t r1 = ___newrand();
        uint16_t r2 = ___newrand();
        demo_peep_x[i] = 40 + (r1 % 220);
        demo_peep_y[i] = 40 + (r2 % 160);
    }

    printf("[PRG] _place_first_people - %d peeps (conquest-aware for quick testing)\n", num_demo_peeps);
}

#if 0
/* old duplicate _animate skeleton */
static void _animate(void)
{
    printf("[PRG] _animate (old skeleton)\n");
}
#endif

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
            /* Rough Amiga-like palette */
            for (int i = 0; i < 16; i++) {
                int v = (i * 17) & 0xFF;
                amiga_set_color(prg_screen, i, (i << 8) | (i << 4) | i);
            }
        }
    }
}

static void _main(void)
{
    ensure_prg_screen();   /* create the visible window early for motivation */
    _openlibrary();

    /* AllocMem sequence for the big buffers (exact sizes + attributes from the PEA/JSR) */
    sprite_data    = (uint32_t)(uintptr_t)amiga_alloc_mem(0x5960, 0);
    sprite_data_32 = (uint32_t)(uintptr_t)amiga_alloc_mem(0x2080, 0);
    blk_data       = (uint32_t)(uintptr_t)amiga_alloc_mem(0x8340, 0);
    back_scr       = (uint32_t)(uintptr_t)amiga_alloc_mem(0x7D00, 2);
    screen_pointer = (uint32_t)(uintptr_t)amiga_alloc_mem(0xfa00, 0x00010002);
    copper_list    = (uint32_t)(uintptr_t)amiga_alloc_mem(0x384, 0x00010002);
    spare_cl       = (uint32_t)(uintptr_t)amiga_alloc_mem(0x12c, 0x00010002);
    fb             = (uint32_t)(uintptr_t)amiga_alloc_mem(0x104, 0x00010002);

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

    *(int16_t *)A4_DISP(0x98a2) = 0x2580;
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

    amiga_free_mem((void*)(uintptr_t)sprite_data);
    amiga_free_mem((void*)(uintptr_t)sprite_data_32);
    amiga_free_mem((void*)(uintptr_t)blk_data);
    amiga_free_mem((void*)(uintptr_t)back_scr);
    amiga_free_mem((void*)(uintptr_t)screen_pointer);
    amiga_free_mem((void*)(uintptr_t)copper_list);
    amiga_free_mem((void*)(uintptr_t)spare_cl);
    amiga_free_mem((void*)(uintptr_t)fb);

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

    if (0 /* 8,A5 == 0 */) {
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

/* _animate - core game loop from the chunk (scrolls, debug keys, peeps dots, etc.) */
static void _animate(void)
{
    _setup_display();

    ___toggle_icon(0x17e4, 3, 0, (void*)(uintptr_t)back_scr);
    ___toggle_icon(0x17e4, 4, 0, (void*)(uintptr_t)back_scr);
    ___toggle_icon(0x12c0, 1, 6, (void*)(uintptr_t)back_scr);
    ___toggle_icon(0x12c0, 3, 4, (void*)(uintptr_t)back_scr);

    if (start_of_game == 2) {
        start_of_game = 0;
        ___clr_wsc();
        ___swap_screens();
        for (;;) {
            in_conquest = ___show_world();
            if (in_conquest == (int16_t)0xffff) continue;
            break;
        }
        ___return_to_game();
        _setup_display();
    }
    start_of_game = 0;

    for (;;) {
        /* tempo / beat from pop (simplified) */
        if (1 /* good_pop[player] */) {
            tempo = 50 - 10;
            if (tempo < 7) tempo = 7;
            beat_two = tempo / 3;
        } else {
            tempo = 0; beat_two = 1;
        }
        if (tempo_now > tempo) tempo_now = tempo;

        toggle = !toggle;

        ___clr_wsc();
        no_sprites = 0;
        ok_to_build = 0;
        sound_effect = 0;

        if (mode == 2) pointer = player * 3 + 1;

        _move_mana();
        ___draw_it(yoff, xoff);

        /* sprite list draw (32px or normal) - placeholder coords */
        for (int s = 0; s < no_sprites; s++) {
            ___draw_sprite(0, 0, 0, (void*)(uintptr_t)w_screen);
        }

        ___draw_sprite(0x54,
            xoff + yoff + 6/2 - 3,
            xoff + 0x43 - (yoff + 3) - 3,
            (void*)(uintptr_t)w_screen);

        if (pause || paint_map) {
            /* view-mode peeps dots */
            for (int p = 0; p < no_peeps; p++) {
                uint8_t *pp = (uint8_t *)peeps + p*0x16;
                if (*(int16_t*)(pp+4) == 0) continue;
                if (pp[1] != player && !serial_off) continue;
                if (pp[0] & 1) ___a_putpixel(1, *(int16_t*)(pp+8));
                if (pp[0] & 2) ___a_putpixel(8 + (pp[1]?0:7), *(int16_t*)(pp+8));
            }
        } else {
            _do_funny();
            _move_peeps();
        }

        if (view_timer) {
            view_timer--;
            if (!(left_button && right_button)) view_who = old_view_who;
        }

        if (game_turn == 0x1000) {
            _do_place_funny(0); /* real takes arg from chunk */
        }

        _show_the_shield();
        ___swap_screens();

        ___keyboard();

        if (inkey) {
            uint8_t k = inkey;
            if (k == 0x45 /* * */) {
                if (mousex > 0x136 && mousey >= 0xa && mousey <= 0x14 && (bitfield_51645 & 1)) {
                    ___text("CHEAT", 0xa, 0, (void*)(uintptr_t)w_screen);
                    /* stat writes for the corner cheat */
                }
            } else if (k == 0x59 /* F9 */) {
                ___display_debug();
            } else {
                /* numpad scroll directions (exact labels from source) */
                if (k == /* right */ 0x4e) { if (xoff < 0x38) xoff++; }
                /* ... nw/n/ne/w/e/sw/s/se + esc return ... */
            }
        }

        if (mode & 0xe) {
            _sculpt();
        } else if (!(left_button && right_button)) {
            _interogate();
        }

        _zoom_map(left_button ? 1 : 0);

        if (left_button == 2) { /* final button test from chunk */ }

        /* Real present + input so you see the window update */
        ___swap_screens();
        ___keyboard();

        static int t = 0;
        if ((++t % 20) == 0) {
            printf("[animate] tick player=%d xoff=%d yoff=%d (use numpad to scroll, ESC to quit, A for demo effect)\n",
                   player, xoff, yoff);
        }

        if (prg_quit) break;
    }
}

/* Visible minimal drawing so you actually SEE something (motivation!).
 * These touch the real chunky_buffer and call present.
 * Full accurate versions come when you paste more assembly.
 */
static void _clear_map(void)
{
    ensure_prg_screen();

    /* use shared demo_height (file scope). Zero it the first time we are called. */
    static int cleared = 0;
    if (!cleared) {
        memset(demo_height, 0, sizeof(demo_height));
        cleared = 1;
    }

    /* Clear visual */
    if (prg_screen) {
        uint32_t bg = prg_screen->palette[2 + (ground_in & 3)];
        for (int i = 0; i < prg_screen->width * prg_screen->height; i++)
            prg_screen->chunky_buffer[i] = bg;
    }
}

static void _make_alt(void) {}
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

static void _move_mana(void) {}

static void _draw_it(int16_t y, int16_t x)
{
    ensure_prg_screen();
    if (!prg_screen) return;

    /* Visible height-map + peeps renderer.
     * Uses the shared demo_height + demo_peep_* variables (populated by _make_map and _place_first_people).
     */

    int w = prg_screen->width;
    int h = prg_screen->height;

    uint32_t base_col = prg_screen->palette[2 + (ground_in & 3)];

    /* Draw shaded landscape - height gives real visual difference per "level" */
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            int mx = ((xx + x) >> 2) & 63;
            int my = ((yy + y) >> 2) & 63;

            int8_t height = demo_height[my][mx];
            int shade = 5 + (height / 2);     /* height shading */
            if (shade < 0) shade = 0;
            if (shade > 15) shade = 15;

            uint32_t col = prg_screen->palette[shade];

            /* terrain tint */
            if (((xx + yy) & 2) == 0)
                col = base_col;

            prg_screen->chunky_buffer[yy * w + xx] = col;
        }
    }

    /* Draw demo peeps as clearly visible bright squares on the map */
    for (int i = 0; i < num_demo_peeps; i++) {
        int sx = demo_peep_x[i] - x;
        int sy = demo_peep_y[i] - y;

        if (sx >= 2 && sx < w-2 && sy >= 2 && sy < h-2) {
            uint32_t pcol = prg_screen->palette[14];  /* bright */
            /* 5x5 blob so they stand out well when testing */
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int idx = (sy + dy) * w + (sx + dx);
                    if (idx >= 0 && idx < w * h)
                        prg_screen->chunky_buffer[idx] = pcol;
                }
            }
        }
    }
}

static void _do_funny(void) {}
static void _move_peeps(void)
{
    /* Lightweight movement so different level inits feel alive.
     * The peeps positions are set by _place_first_people using conquest data.
     */
    static int tick = 0;
    tick++;

    for (int i = 0; i < num_demo_peeps; i++) {
        if ((tick + i) % 4 == 0) {
            demo_peep_x[i] += (i % 3) - 1;
            demo_peep_y[i] += ((i + 1) % 3) - 1;

            /* keep on screen */
            if (demo_peep_x[i] < 20) demo_peep_x[i] = 20;
            if (demo_peep_x[i] > 280) demo_peep_x[i] = 280;
            if (demo_peep_y[i] < 20) demo_peep_y[i] = 20;
            if (demo_peep_y[i] > 220) demo_peep_y[i] = 220;
        }
    }
}
static void _show_the_shield(void) {}
static void _sculpt(void) {}
static void _interogate(void) {}
static void _zoom_map(int16_t d) {}
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
static void ___start_game(void){printf("[PRG] start_game\n");}
static void ___set_serial(void){printf("[PRG] set_serial\n");}
static void ___load_ground(int16_t a, int16_t terrain)
{
    ensure_prg_screen();
    if (!prg_screen) return;

    /* Very early "ground loading" shim for quick visual testing.
     * Different terrain types now produce different base color palettes
     * so you can immediately see the effect of conquest level data.
     */
    printf("[PRG] ___load_ground terrain=%d\n", terrain);

    /* Rough Amiga-style palette shifts per terrain */
    int base = (terrain & 3) * 4;
    for (int i = 0; i < 16; i++) {
        int v = ((i + base) * 13) & 0xFF;
        amiga_set_color(prg_screen, i, (i << 8) | (v << 4) | (base & 15));
    }
}
static void ___Setscreen(int16_t m,void *s1,void *s2){printf("[SDL] Setscreen\n");}
static void ___copy_screen(void *d,void *s){printf("[SDL] copy_screen\n");}
static void ___toggle_icon(int16_t i,int16_t a,int16_t b,void *scr){printf("[SDL] toggle_icon 0x%04x\n",(uint16_t)i);}
static void ___draw_it(int16_t y,int16_t x){printf("[SDL] draw_it\n");}
static void ___draw_s_32(int16_t x,int16_t y,void *scr){printf("[SDL] draw_s_32\n");}
static void ___draw_sprite(int16_t f,int16_t x,int16_t y,void *scr){printf("[SDL] draw_sprite\n");}
static void ___clr_wsc(void){printf("[SDL] clr_wsc\n");}
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
    ensure_prg_screen();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) prg_quit = 1;
        if (ev.type == SDL_KEYDOWN) {
            switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: prg_quit = 1; break;
                case SDLK_KP_4: case SDLK_LEFT:  if (xoff > 0) xoff--; break;
                case SDLK_KP_6: case SDLK_RIGHT: if (xoff < 0x38) xoff++; break;
                case SDLK_KP_8: case SDLK_UP:    if (yoff > 0) yoff--; break;
                case SDLK_KP_2: case SDLK_DOWN:  if (yoff < 0x38) yoff++; break;
                case SDLK_KP_7: if (xoff>0) xoff--; if (yoff>0) yoff--; break; /* nw etc. simplified */
                case SDLK_KP_9: if (xoff<0x38) xoff++; if (yoff>0) yoff--; break;
                case SDLK_KP_1: if (xoff>0) xoff--; if (yoff<0x38) yoff++; break;
                case SDLK_KP_3: if (xoff<0x38) xoff++; if (yoff<0x38) yoff++; break;
                case SDLK_a: /* demo "win" - clear some peeps area */
                    for (int i = 100; i < 200 && i < prg_screen->width*prg_screen->height; i+=3)
                        prg_screen->chunky_buffer[i] = prg_screen->palette[0];
                    break;
                case SDLK_F9:
                    printf("[debug] F9 stats (stub)\n");
                    break;
                default: break;
            }
            /* crude inkey for the asm logic */
            /* inkey = ... ; left_button/right_button can be set from mouse too */
        }
    }
}
static void ___setup_mouse(void){printf("[SDL] setup_mouse\n");}
static void ___AddIntServer(int16_t l,void *is){printf("[PRG] AddIntServer %d\n",l);}
static void ___RemIntServer(int16_t l,void *is){printf("[PRG] RemIntServer %d\n",l);}
static void ___Delay(int16_t t){/*SDL_Delay*/;}
static void ___reset_screen(void *v){printf("[SDL] reset\n");}
static void ___free_amiga_sprites(void){printf("[PRG] free sprites\n");}
static void ___free_all_sounds(void){printf("[PRG] free sounds\n");}
static void ___close_channels(void){printf("[PRG] close channels\n");}
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