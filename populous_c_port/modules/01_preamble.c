#include "../pop_log.h"
/* modules/01_preamble.c — unity-included from populous_prg.c (do not compile alone). */
/* Original populous_prg.c lines 238-797. */

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
static void ___SetIntVector(int16_t vec, void *handler) { POP_LOGD("PRG", "SetIntVector %d", vec); }
static void *___OpenLibrary(const char *name, uint32_t ver) { POP_LOGD("PRG", "OpenLibrary %s", name); return (void*)0x1000; }
static void  ___CloseLibrary(void *base) { POP_LOGD("PRG", "CloseLibrary"); }

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
    pop_log_init();
    prg_status = EXIT_SUCCESS;
    prg_quit = 0;
    if (!parse_start(g_argc, g_argv, &requested_start)) {
        POP_LOG_INFO("Usage: populous_init_port [nonnegative integer <= INT_MAX | --level 0..494 | --code WORLDNAME | --conquest | --tutorial]");
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
    POP_LOGD("PRG", "_setup_display (old)");
}
#endif

static void _make_copper(void)
{
    POP_LOGD("PRG", "_make_copper");
}

/* _make_map, asm:1420-1539. Cells derive from the separate 65x65 vertices. */
