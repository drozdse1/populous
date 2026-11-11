#ifndef POPULOUS_A4_STATE_H
#define POPULOUS_A4_STATE_H

/*
 * Emulated A4 globals for populous.prg
 * ------------------------------------
 * The Amiga binary keeps almost all game state relative to A4 ($593EE).
 * We allocate a 64 KiB host buffer, point `prg_a4` at its midpoint so both
 * positive and negative d16 displacements are valid, and expose the common
 * fields through speaking macros.
 *
 * Prefer the named macros below over raw A4_WORD(0x....) in new code.
 * Keep A4_DISP / A4_WORD / A4_LONG for true dynamic offsets (stats+N, etc.).
 */

#include <stdint.h>

/* Signed d16 displacements straddle A4, including the 65x65 word vertex map.
 * Packed/may_alias accessors support the original two-byte long alignment
 * without violating C aliasing. Values are host-endian, not disk images. */
_Alignas(4) static uint8_t prg_a4_area[65536];
static uint8_t *prg_a4 = prg_a4_area + 32768;

#if defined(__GNUC__) || defined(__clang__)
typedef struct __attribute__((packed, may_alias)) { int16_t value; } A4Word;
typedef struct __attribute__((packed, may_alias)) { uint32_t value; } A4Long;
#elif defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct { int16_t value; } A4Word;
typedef struct { uint32_t value; } A4Long;
#pragma pack(pop)
#else
typedef struct { int16_t value; } A4Word;
typedef struct { uint32_t value; } A4Long;
#endif

#define A4_DISP(d16)   (prg_a4 + (int16_t)(d16))
#define A4_WORD(offset) (((A4Word *)A4_DISP(offset))->value)
#define A4_LONG(offset) (((A4Long *)A4_DISP(offset))->value)

/* --- Display / host buffers (not A4; filled by Amiga alloc shims) --- */
/* Defined by the including translation unit: host_buffers[8]. */
#ifndef POPULOUS_A4_SKIP_HOST_BUFFER_MACROS
#define sprite_data    host_buffers[0]
#define sprite_data_32 host_buffers[1]
#define blk_data       host_buffers[2]
#define back_scr       host_buffers[3]
#define screen_pointer host_buffers[4]
#define copper_list    host_buffers[5]
#define spare_cl       host_buffers[6]
#define fb             host_buffers[7]
#endif

/* --- Game start / conquest --- */
#define start_of_game    A4_WORD(0x9898)
#define sprites_in       A4_WORD(0x989a)
#define in_conquest      A4_WORD(0x84ba)
#define conq_08_seed     A4_WORD(0x84c4)
#define conq_05_terrain  (*(uint8_t *)A4_DISP(0x84c1))
#define seed             A4_WORD(0x99e2)
#define start_seed       A4_WORD(0x99e4)
#define ground_in        A4_WORD(0x99e6)
#define conquest_bytes   (A4_DISP(0x84bc)) /* 8-byte conquest record + seed follows */
#define conquest         (*(uint8_t *)A4_DISP(0x84bc))
#define conq_01_speed    (*(uint8_t *)A4_DISP(0x84bd))
#define conq_02_enemypow (*(uint8_t *)A4_DISP(0x84be))
#define conq_03_yourpow  (*(uint8_t *)A4_DISP(0x84bf))
#define conq_04_mode     (*(uint8_t *)A4_DISP(0x84c0))
#define conq_06_your_count  (*(uint8_t *)A4_DISP(0x84c2))
#define conq_07_enemy_count (*(uint8_t *)A4_DISP(0x84c3))
#define game_mode        A4_WORD(0x84b8)
#define level_number     A4_WORD(0xae7e)   /* _level_number */

/* --- Screens / palette / mouse --- */
#define w_screen         A4_LONG(0x99e8)
#define d_screen         A4_LONG(0x99ec)
#define pal              A4_WORD(0x99e0)
#define mouse_limit      A4_WORD(0x99a8)
#define mousex           A4_WORD(0x99aa)
#define mousey           A4_WORD(0xae6e)
#define big_mousex       A4_WORD(0x99b0)
#define big_mousey       A4_WORD(0x99b2)
#define dint             (A4_DISP(0x984a))
#define xoff             A4_WORD(0x99f0)
#define yoff             A4_WORD(0x99f2)
#define player           A4_WORD(0x99f4)
#define good_pop         (A4_DISP(0x99fe))

/* --- Audio / VBI --- */
#define tempo            A4_WORD(0x824e)
#define tempo_now        A4_WORD(0x9a18)
#define beat_two         A4_WORD(0x9a16)
#define toggle           A4_WORD(0x9a1a)
#define no_sprites       A4_WORD(0x9a1c)
#define ok_to_build      A4_WORD(0x9a1e)
#define sound_effect     (*(uint8_t *)A4_DISP(0x9a20))
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
#define music_off        A4_WORD(0x8250)  /* nonzero = music plays */
#define effect_off       A4_WORD(0x8252)  /* nonzero = effects play */

/* --- Mode / pointer / sprites / peeps --- */
#define mode             A4_WORD(0x8256)
#define pointer          A4_WORD(0x84b6)
#define sprite           (A4_DISP(0x9a22))
#define peeps            (A4_DISP(0x9c26))
#define no_peeps         A4_WORD(0xae5e)

/* --- Map arrays (byte bases unless noted) --- */
#define map_who          (A4_DISP(0x1f88))
#define map_blk          (A4_DISP(0xcf88))
#define map_alt          (A4_DISP(0xdf88))
#define map_bk2          (A4_DISP(0xef88))
#define map_steps        (A4_DISP(0xff88))
#define map_colour_data  (A4_DISP(0x8370))
/* 65x65 signed vertex heights (_alt). Prefer map_vertex_word(col,row). */
#define map_vertices     (A4_DISP(0xae86))
#define map_vertex_word(column, row) A4_WORD(0xae86 + 2 * ((row) * 65 + (column)))
#define map_vertex_at(index)         A4_WORD(0xae86 + 2 * (index)) /* linear over 65x65 */
#define map_step_word(cell)          A4_WORD(0xff88 + (cell) * 2)

/* --- View / input / turn --- */
#define view_timer       A4_WORD(0xae60)
#define funny_done       A4_WORD(0xae70)  /* once-per-game funny place flag */
#define left_button      A4_WORD(0xae62)
#define right_button     A4_WORD(0xae64)
#define old_view_who     A4_WORD(0xae66)
#define view_who         A4_WORD(0xae68)
#define game_turn        A4_WORD(0xae6a)
#define pause            A4_WORD(0x9c22)
#define paint_map        A4_WORD(0x9c24)
#define serial_off       A4_WORD(0x8254)
#define not_player       A4_WORD(0x825a)
#define stats            (A4_DISP(0x82b6))

/* Per-owner _stats record (46 bytes) starting at 0x82b6 (good) / 0x82e4 (bad). */
#define stats_base(owner)            (0x82b6 + (owner) * 46)
#define stats_bytes(owner)           (A4_DISP(stats_base(owner)))
#define stats_cmd_byte(owner)        (stats_bytes(owner)[0])
#define stats_cmd_x(owner)           (stats_bytes(owner)[1])
#define stats_cmd_y(owner)           (stats_bytes(owner)[2])
#define stats_build_mode(owner)      A4_WORD(stats_base(owner) + 0x06) /* 0x82bc */
#define stats_build_latch(owner)     A4_WORD(stats_base(owner) + 0x08) /* 0x82be */
#define stats_power_mask(owner)      A4_WORD(stats_base(owner) + 0x0e) /* 0x82c4 */
#define stats_build_period(owner)    A4_WORD(stats_base(owner) + 0x10) /* 0x82c6 */
#define stats_rating(owner)          A4_WORD(stats_base(owner) + 0x0c) /* 0x82c2; custom peep count */
#define stats_send_flag(owner)       A4_WORD(stats_base(owner) + 0x12) /* 0x82c8 */
#define stats_city_count(owner)      A4_WORD(stats_base(owner) + 0x14) /* 0x82ca */
#define stats_town_tally(owner)      A4_WORD(stats_base(owner) + 0x16) /* 0x82cc; published town_age */
#define stats_ai_seed(owner)         A4_WORD(stats_base(owner) + 0x18) /* 0x82ce */
#define stats_ai_delay(owner)        A4_WORD(stats_base(owner) + 0x1a) /* 0x82d0 */
#define stats_go_to_mode(owner)      A4_WORD(stats_base(owner) + 0x1c) /* 0x82d2; battle/goto view */
#define stats_go_to_pos(owner)       A4_WORD(stats_base(owner) + 0x1e) /* 0x82d4 */
#define stats_best_town_ref(owner)   A4_LONG(stats_base(owner) + 0x22) /* 0x82d8; peep slot+1 */
#define stats_oldest_town_ref(owner) A4_LONG(stats_base(owner) + 0x26) /* 0x82dc */
#define stats_youngest_town_ref(owner) A4_LONG(stats_base(owner) + 0x2a) /* 0x82e0 */

#define inkey            (*(uint8_t *)A4_DISP(0xae6c))
#define bitfield_51645   (*(uint8_t *)A4_DISP(0x8257))
#define GfxBase_prg      A4_LONG(0x99a4)
#define IntuitionBase_prg A4_LONG(0x99ac)

/* Editor (_paint_map) cursor and last placed tree/rock sprite ids. */
#define cur_y            A4_WORD(0x3026)          /* _cur_y: vertex row under pointer */
#define cur_x            A4_WORD(0x3028)          /* _cur_x: vertex column under pointer */
#define last_tree        A4_WORD(0x825c)          /* _last_tree (bk2 50..52) */
#define last_rock        A4_WORD(0x825e)          /* _last_rock (blk 47..49) */

/* --- War / magnets / score / battle tallies (from .GAM region table) --- */
#define war_flag         A4_WORD(0xae80)          /* _war; HUD shows "WAR" */
#define god_magnet       A4_WORD(0xae78)          /* owner 0 magnet cell */
#define devil_magnet     A4_WORD(0xae76)          /* owner 1 magnet cell */
#define owner_magnet(o)  A4_WORD((o) ? 0xae76 : 0xae78)
#define battle_won(o)    A4_WORD(0xae7a + (o) * 2)
#define score_long       A4_LONG(0xae72)          /* _score */

/* Per-owner magnet/mana block at 0x99f6 (16 bytes each). */
#define core_leader(owner)   A4_WORD(0x99f6 + (owner) * 16)
#define core_target(owner)   A4_WORD(0x99f8 + (owner) * 16)
#define core_tendency(owner) A4_WORD(0x99fa + (owner) * 16)
#define core_town_age(owner) A4_WORD(0x99fc + (owner) * 16)
#define core_good_pop(owner) A4_LONG(0x99fe + (owner) * 16)
#define core_mana(owner)     ((int32_t)A4_LONG(0x9a02 + (owner) * 16))
#define core_mana_long(owner) A4_LONG(0x9a02 + (owner) * 16)

/* Sculpt / raise-lower scratch (build_count + dirty bbox). */
#define build_count      A4_WORD(0x2f88)
#define raise_min_col    A4_WORD(0x2f8a)
#define raise_max_col    A4_WORD(0x2f8c)
#define raise_min_row    A4_WORD(0x2f8e)
#define raise_max_row    A4_WORD(0x2f90)

/* Battle / water-loss scratch and HUD view toggles. */
#define a4_walk_death  A4_WORD(0x2fb0)          /* per-frame peep water/battle loss */
#define view_fight       A4_WORD(0x2f92)
#define view_people      A4_WORD(0x2f94)

/* Game-options / requester scratch. */
#define bring_up_two     A4_WORD(0x3080)          /* _bring_up_two */
#define saved_flag       A4_WORD(0x3054)          /* _saved */
#define options_new_map  A4_WORD(0x309a)          /* map-restart / landscape pick */
#define surrender_side   A4_WORD(0x8258)          /* _surender; -1 = none */

/* Life-scan scratch used by _check_life / settlement growth. */
#define life_water_flag  A4_WORD(0x2fb2)
#define life_swamp_count A4_WORD(0x2ff8)

/* Mana / power cost tables (longs) referenced by gameplay gates.
 * Index into the 11-entry mana_value_table installed at reset. */
#define mana_floor         A4_LONG(0x8486)
#define mana_raise_cost    A4_LONG(0x848a)
#define mana_magnet_cost   A4_LONG(0x848e)
#define mana_quake_cost    A4_LONG(0x8492)
#define mana_swamp_cost    A4_LONG(0x8496)
#define mana_knight_cost   A4_LONG(0x849a)
#define mana_volcano_cost  A4_LONG(0x849e)
#define mana_flood_cost    A4_LONG(0x84a2)
#define mana_war_cost      A4_LONG(0x84a6)   /* Armageddon */
#define mana_value_table(i) A4_LONG(0x8486 + (i) * 4)
#define mana_high_bonus    A4_LONG(0x9a12)   /* set to 2700 on high internal levels */

/* LAND rule tables (words, 11 entries unless noted). */
#define land_weapons_order(i)  A4_WORD(0x2f96 + (i) * 2)
#define land_cheat_owner       A4_WORD(0x2fb4)          /* nonzero = force capacity cheat */
#define land_mana_add(i)       A4_WORD(0x2fb6 + (i) * 2)
#define land_weapons_add(i)    A4_WORD(0x2fcc + (i) * 2)
#define land_population_add(i) A4_WORD(0x2fe2 + (i) * 2)
#define land_battle_add1(i)    A4_WORD(0x2ffa + (i) * 2)
#define land_battle_add2(i)    A4_WORD(0x3010 + (i) * 2)

/* Direction offsets (25) and city block ids (9) copied into A4 at reset. */
#define dir_offset_table(i)    A4_WORD(0x842c + (i) * 2)
#define city_block_table(i)    A4_WORD(0x8474 + (i) * 2)

/* Sprite records: 8 bytes each at 0x9a22 (x, y, pad, who). */
#define sprite_x(i)    A4_WORD(0x9a22 + (i) * 8)
#define sprite_y(i)    A4_WORD(0x9a24 + (i) * 8)
#define sprite_who(i)  A4_WORD(0x9a28 + (i) * 8)

/* End-game scoreboard rows: 7 x 46 bytes at 0x8e7e (x, y, w, text). */
#define end_row_base(r)  (0x8e7e + (r) * 46)
#define end_row_x(r)     A4_WORD(end_row_base(r) + 0)
#define end_row_y(r)     A4_WORD(end_row_base(r) + 2)
#define end_row_w(r)     A4_WORD(end_row_base(r) + 4)
#define end_row_text(r)  ((char *)A4_DISP(end_row_base(r) + 6))
#define core_end_row(r)   end_row_text(r)   /* legacy test alias */

/* UI / save path / checksum scratch. */
#define core_message     ((char *)A4_DISP(0x3058))
#define core_drawer      ((char *)A4_DISP(0x302c))
#define core_filename    ((char *)A4_DISP(0x3040))
#define land_checksum    A4_WORD(0x3082)

/* Requester OK/CANCEL and menu tables (46-byte entries). */
#define core_end_ok          A4_DISP(0x84c6)
#define core_end_cancel      A4_DISP(0x84f4)
#define core_option_entry(i) A4_DISP(0x8522 + (i) * 46)
#define core_game_entry(i)   A4_DISP(0x87ee + (i) * 46)
#define core_set_entry(i)    A4_DISP(0x8fc0 + (i) * 46)

/* Serial baud/period scratch used around _set_serial. */
#define serial_period    A4_WORD(0x98a2)
#define serial_read_vec  A4_DISP(0x99b4)
#define serial_write_vec A4_DISP(0x99ca)

/* Interrupt / serial scratch labels kept for faithful layout. */
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
#define serial           (A4_DISP(0x989c))
/* _serial (asm:25254-25259): +0 read index, +2 write index (LAB_52C8C),
 * +4 transmit busy, the 0x78-byte receive ring at +8 (_r_ser_buff
 * asm:20058-20071, _read_serial asm:20080-20137). */
#define serial_rd        A4_WORD(0x989c)
#define serial_wr        A4_WORD(0x989e)
#define serial_tx_busy   A4_WORD(0x98a0)
#define serial_ring      (A4_DISP(0x98a4))
#define SERIAL_RING_SIZE 0x78
/* Two-player state (asm:25512-25548): _quick_io (1 = NO SERIAL CHECKING),
 * _use_modem, _login (20 bytes), _baud_rate; _message (asm:25514) is
 * core_message, also the 5/12-byte option packets and the text message. */
#define quick_io         A4_WORD(0x3056)
#define use_modem        A4_WORD(0x3084)
#define core_login       ((char *)A4_DISP(0x3086))
#define baud_rate        A4_WORD(0x83b8)
/* _serial_text asm:24930-25140 (A4 0x91ec): 20 entries of 46 bytes. */
#define core_serial_entry(i) A4_DISP(0x91ec + (i) * 46)
#define _my_task         A4_LONG(0x9860)
#define LAB_516DE        A4_WORD(0x82f0)
#define LAB_516E2        A4_WORD(0x82f4)
#define LAB_516E0        A4_WORD(0x82f2)
#define LAB_516B2        A4_WORD(0x82c4)
#define LAB_516C4        (A4_DISP(0x82d6))
#define LAB_516A6        (A4_DISP(0x82b8))
#define LAB_516A5        (A4_DISP(0x82b7))
#define LAB_53015        (A4_DISP(0x9c27))
#define LAB_53018        (A4_DISP(0x9c2a))

#endif /* POPULOUS_A4_STATE_H */
