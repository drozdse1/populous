/*
 * symbols_prg.h
 *
 * Complete symbol map from the IRA disassembly of "populous.prg"
 * (the actual game module loaded by the launcher).
 *
 * MACHINE 68000
 * ENTRY $0003E1E8
 * OFFSET $0003E1E8
 * BASEREG A4
 * BASEADR $000593EE
 *
 * This is from the Hit Squad 1993 budget release (v2.7 "POPULOUS 2.7").
 * Lacks the binary obfuscation of the original 1989 release.
 * A few labels were originally .begin, .mulu etc. but changed for vasm.
 *
 * Reassemble:
 *   vasmm68k_mot -Fhunkexe -kick1hunks -databss -ldots -no-opt -wfail -o populous.prg populous_prg.asm
 *
 * The launcher loads "populous.prg" and jumps to its _main.
 * In the C port: emulate load at PRG_LOAD_BASE, A4 = prg_a4 pointing at BASEADR emulation.
 */

#ifndef POPULOUS_SYMBOLS_PRG_H
#define POPULOUS_SYMBOLS_PRG_H

#include <stdint.h>

/* Base info for this module (loaded by launcher) */
#define PRG_ENTRY           0x0003E1E8
#define PRG_A4_BASE         0x000593EE   /* A4 points here for globals access (map, peeps, etc.) */
#define PRG_LOAD_BASE       0x0003E1E8

/* NOTE on symbol naming:
 * All the original 68k labels (_main, _animate, _do_quake, strRING, SUB_*, etc.)
 * are listed below strictly as comments with their IRA addresses for reference
 * during translation.  We do NOT emit live #defines for them because that would
 * collide with the C function / variable names we actually implement in the port
 * (populous_prg.c declares real static void _main(void); etc.).
 *
 * If you need the numeric value in code for some debug/print purpose, use the
 * comments or add a local const uint32_t later.
 */

/* =========================================================================
 * Full CODE ranges (non-contiguous because TEXT strings and data are
 * interspersed). From the map paste.
 * ========================================================================= */
 /*
CODE $0003E1E8 - $0003E4DA
CODE $0003E4F4 - $0003E5AA
CODE $0003E5CE - $0003EC98
CODE $0003EC9E - $0003FC92
CODE $0003FCA4 - $000413C4
CODE $000413CA - $00043D16
CODE $00043D20 - $00046174
CODE $000461CE - $00046BE4
CODE $00046BF4 - $00046C80
CODE $00046C8C - $00046F30
CODE $00046F50 - $000475F8
CODE $0004760C - $000476E6
CODE $00047722 - $00047D96
CODE $00047DB8 - $0004817A
CODE $00048188 - $000485C6
CODE $000485D0 - $00048690
CODE $00048698 - $000489CC
CODE $000489DA - $00048B8C
CODE $00048BAC - $00048FB6
CODE $00048FCE - $00049778
CODE $000497A0 - $0004A01E
CODE $0004A020 - $0004A16E
CODE $0004A176 - $0004A224
CODE $0004A23E - $0004A446
CODE $0004A44C - $0004A4A2
CODE $0004A4AC - $0004A4E8
CODE $0004A4F4 - $0004A7EA
CODE $0004A7F0 - $0004A94E
CODE $0004AA66 - $0004AE12
CODE $0004AE22 - $0004B22A
CODE $0004B23A - $0004B81A
CODE $0004B838 - $0004C16A
CODE $0004C18A - $0004C21A
CODE $0004C2E6 - $0004C300
CODE $0004C302 - $0004C374
CODE $0004C376 - $0004C708
CODE $0004C70A - $0004C750
CODE $0004C792 - $0004CA5C
CODE $0004CA66 - $0004CBF4
CODE $0004CBF6 - $0004CCC4
CODE $0004CCC6 - $0004CD2C
CODE $0004CD2E - $0004CDB8
CODE $0004CDBA - $0004CE54
CODE $0004CE56 - $0004D092
CODE $0004D5C6 - $0004D5EA
CODE $0004D5F8 - $0004D9B8
CODE $0004D9C6 - $0004DF3A
CODE $0004F3C6 - $0004F3CE
CODE $000504FE - $00050642
CODE $00050646 - $00050668
CODE $00050674 - $00050A8A
CODE $00050AA0 - $00051020
CODE $00051024 - $000513EE
CODE $000513F0 - $000514E6
CODE $000514EC - $00051618
*/

/* =========================================================================
 * Full TEXT (string/data) ranges from the map (interspersed with code).
 * ========================================================================= */
 /*
TEXT $0003E4DA - $0003E4F3
TEXT $0003E5AA - $0003E5CD
TEXT $0003EC98 - $0003EC9E
TEXT $000413C4 - $000413CA
TEXT $00046174 - $00046192
TEXT $00046196 - $000461CE
TEXT $00046C80 - $00046C89
TEXT $00046F30 - $00046F3C
TEXT $00046F40 - $00046F50
TEXT $000476E6 - $00047722
TEXT $0004817A - $00048188
TEXT $000485C6 - $000485D0
TEXT $00048690 - $00048698
TEXT $00048B8C - $00048BAB
TEXT $00048FB6 - $00048FCD
TEXT $00049778 - $0004979F
TEXT $0004A16E - $0004A176
TEXT $0004A224 - $0004A23D
TEXT $0004A446 - $0004A44C
TEXT $0004A4A2 - $0004A4AC
TEXT $0004A4E8 - $0004A4F4
TEXT $0004A7EA - $0004A7F0
TEXT $0004A94E - $0004AA66
TEXT $0004AE12 - $0004AE21
TEXT $0004B22A - $0004B239
TEXT $0004C220 - $0004C261
TEXT $0004C280 - $0004C2C1
TEXT $0004D5EA - $0004D5F8
TEXT $0004D9B8 - $0004D9C6
TEXT $0004DF3C - $0004E4C5
TEXT $00050668 - $00050674
TEXT $00050A8A - $00050AA0
... (many more TEXT ranges for the huge level-code fragment tables at the end)
*/

/* === Core init / library / mouse / serial setup (prg side) - reference only === */
/* SYMBOL __H0_org                0x0003E1E8 */
/* SYMBOL _main                   0x0003E1EE */
/* SYMBOL _rs                     0x0003E2EE */
/* SYMBOL _closeall               0x0003E304 */
/* SYMBOL _create_mouse           0x0003E3AE */
/* SYMBOL _setup_interrupt        0x0003E3BE */
/* SYMBOL _set_mouse              0x0003E418 */
/* SYMBOL _free_inter             0x0003E454 */
/* SYMBOL _setup_serial_interrupt 0x0003E474 */
/* SYMBOL _free_serial_interrupt  0x0003E4F4 */
/* SYMBOL _closelibrary           0x0003E504 */
/* SYMBOL _set_pri                0x0003E526 */
/* SYMBOL _debug                  0x0003E52E */
/* SYMBOL _openlibrary            0x0003E536 */

/* Library name strings (TEXT areas) - addresses documented for the loader/prg split */
 /* SYMBOL GraphicsLibName         0x0003E5AA */
/* SYMBOL IntuitionLibName        0x0003E5BB   (also LABEL) */
/* SYMBOL DosLibName              0x00050668 */
/* SYMBOL IconLibName             0x00050A8A */

/* SYMBOL _setup_display          0x0003E5CE */
/* SYMBOL _animate                0x0003E730 */

/* Cheat / test strings */
 /* SYMBOL strCHEAT                0x0003EC98 */
/* SYMBOL strCHEAT2               0x000413C4 */

/* SYMBOL _clear_map              0x0003EC9E */
/* SYMBOL _make_alt               0x0003EE96 */
/* SYMBOL _make_thing             0x0003EEC2 */
/* SYMBOL _raise_point            0x0003EF52 */
/* SYMBOL _make_map               0x0003F0B2 */
/* SYMBOL _draw_map               0x0003F1E0 */
/* SYMBOL _mod_map                0x0003F25C */
/* SYMBOL _zoom_map               0x0003F2F6 */

/* SYMBOL _set_tend_icons         0x0003FD02 */
/* SYMBOL _set_mode_icons         0x0003FDDE */
/* SYMBOL _lower_point            0x0003FE64 */
/* SYMBOL _interogate             0x0003FFC2 */
/* SYMBOL _show_the_shield        0x0004003A */
/* SYMBOL _set_temp_view          0x000404AC */
/* SYMBOL _move_mana              0x000404CC */
/* SYMBOL _move_peeps             0x0004059A */

/* SYMBOL _move_explorer          0x000413CA */
/* SYMBOL _where_do_i_go          0x000416A0 */
/* SYMBOL _move_magnet_peeps      0x000419D4 */
/* SYMBOL _get_heading            0x00041FEA */
/* SYMBOL _join_forces            0x000420B4 */
/* SYMBOL _zero_population        0x000421F4 */
/* SYMBOL _set_frame              0x000422D2 */
/* SYMBOL _set_town               0x0004243C */
/* SYMBOL _do_battle              0x0004268A */
/* SYMBOL _battle_over            0x0004283A */
/* SYMBOL _set_battle             0x00042CBA */
/* SYMBOL _join_battle            0x00042DEC */
/* SYMBOL _do_magnet              0x00042F34 */
/* SYMBOL _rotate_all_map         0x00042FCA */
/* SYMBOL _clear_all_map          0x000430D8 */
/* SYMBOL _place_people           0x00043164 */
/* SYMBOL _do_lower_point         0x000432D4 */
/* SYMBOL _do_raise_point         0x000433EA */
/* SYMBOL _sculpt                 0x00043506 */
/* SYMBOL _do_flood               0x000438B6 */
/* SYMBOL _place_first_people     0x000439BA */
/* SYMBOL _do_quake               0x00043BE0 */
/* SYMBOL _do_volcano             0x00043E18 */
/* SYMBOL _set_magnet_to          0x000440CC */
/* SYMBOL _do_swamp               0x00044102 */
/* SYMBOL _do_knight              0x0004422A */
/* SYMBOL _do_war                 0x0004432E */
/* SYMBOL _make_woods_rocks       0x000443C4 */
/* SYMBOL _do_funny               0x000444EC */
/* SYMBOL _do_place_funny         0x000447F2 */
/* SYMBOL _make_level             0x000449CA */
/* SYMBOL _one_block_flat         0x00044B78 */
/* SYMBOL _devil_effect           0x00044D5A */
/* SYMBOL _do_computer_effect     0x0004502A */
/* SYMBOL _set_devil_magnet       0x0004510C */
/* SYMBOL _requester              0x00045484 */
/* SYMBOL _toggle_button          0x00045878 */
/* SYMBOL _get_name               0x000458D0 */
/* SYMBOL _save_load              0x00045B18 */

/* Level data string + sort */
 /* SYMBOL str_leveldat            0x000461C4 */
/* SYMBOL _sort                   0x000461CE */

/* SYMBOL _do_message             0x00046292 */
/* SYMBOL _get_a_message          0x00046316 */
/* SYMBOL _serial_message         0x000463A0 */
/* SYMBOL _two_players            0x000464A8 */
/* SYMBOL _try_serial             0x00046C8C */
/* SYMBOL _check_cancel           0x00046F50 */
/* SYMBOL _put_in_string          0x00046FEE */
/* SYMBOL _show_world             0x0004702C */

/* Start / new game strings (TEXT) */
 /* SYMBOL strStartGame            0x000476E6 */
/* SYMBOL strNewGame              0x000476F1 */
/* SYMBOL strGENESIS1             0x000476FA */
/* SYMBOL strGENESIS2             0x00047702 */
/* SYMBOL strNoSuchWorld          0x0004770A */
/* SYMBOL str_level_dat           0x00047718 */

/* SYMBOL _game_options           0x00047722 */
/* SYMBOL _set_options            0x00047E56 */
/* SYMBOL loop_47FE4              0x00047FE4   (loop label) */

/* SYMBOL strLandscapeNo          0x0004817A   (new in this 1993 version - used in options) */
/* SYMBOL _options                0x00048188 */
/* SYMBOL _return_to_game         0x000485D0 */
/* SYMBOL _start_game             0x000485EC */
/* SYMBOL _end_game               0x00048698 */

/* Win/lose strings */
 /* SYMBOL strLost                 0x00048B8C */
/* SYMBOL strWon                  0x00048B91 */
/* SYMBOL strTryItAgain           0x00048B95 */
/* SYMBOL strNew_Game             0x00048BA2 */

/* SYMBOL _won_conquest           0x00048BAC */

/* SYMBOL strGENESIS              0x00048FBD */
/* SYMBOL _code                   0x00048FCE */
/* SYMBOL _decode                 0x000490D4 */
/* SYMBOL _paint_the_map          0x000492CE */
/* SYMBOL _waitfor                0x000493EA   (wait for vertical blank) */
/* SYMBOL _write_iff              0x0004940A   (NEW in 1993 - tester screenshot tool (IFF), not called in normal play) */
/* SYMBOL _save_picture_body      0x000497A0 */
/* SYMBOL _pack_picture_bits      0x0004982A */
/* SYMBOL _drw_blk                0x00049914 */
/* SYMBOL SUB_49938               0x00049938 */
/* SYMBOL _draw_it                0x000499B4 */
/* SYMBOL SUB_49B90               0x00049B90 */
/* SYMBOL _move_sprite            0x00049BDE */
/* SYMBOL SUB_49F72               0x00049F72 */

/* SYMBOL _clr_wsc                0x0004A020 */
/* SYMBOL _copy_screen            0x0004A046 */
/* SYMBOL _open_screen            0x0004A068 */
/* SYMBOL _read_back_scr          0x0004A0F8 */
/* SYMBOL _read_sprites           0x0004A176 */
/* SYMBOL _load_ground            0x0004A23E */
/* SYMBOL _read_lord              0x0004A44C */
/* SYMBOL _read_mouth             0x0004A4AC */
/* SYMBOL _Setscreen              0x0004A4F4 */
/* SYMBOL _swap_screens           0x0004A57E */
/* SYMBOL _a_putpixel             0x0004A5AC */
/* SYMBOL _sprite_to_amiga        0x0004A5EE */
/* SYMBOL _get_a_dir              0x0004A6AA */
/* SYMBOL _set_baud_rate          0x0004A7F0 */
/* SYMBOL _free_amiga_sprites     0x0004A802 */
/* SYMBOL _check_serial           0x0004A834 */
/* SYMBOL _display_debug          0x0004A870   (NEW in this release - debug code absent from 1989) */
/* SYMBOL loop_4A932              0x0004A932   (mousewait) */

/* SYMBOL strGameStats            0x0004A94E */

/* SYMBOL _save_game              0x0004AA66   (1993 save game routine) */
/* SYMBOL strGAM1                 0x0004AE12 */
/* SYMBOL strGAM2                 0x0004AE17 */
/* SYMBOL strGAM3                 0x0004AE1C */
/* SYMBOL _checksum               0x0004AE22 */
/* SYMBOL _load_game              0x0004AE58   (1993 load game) */
/* SYMBOL strGAM4                 0x0004B22A */
/* SYMBOL strGAM5                 0x0004B22F */
/* SYMBOL strGAM6                 0x0004B234 */

/* SYMBOL _get_message            0x0004B23A */
/* SYMBOL _clear_send             0x0004B858 */
/* SYMBOL _write_full             0x0004B8A0 */
/* SYMBOL _read_full              0x0004B90A */
/* SYMBOL _do_action              0x0004B9E0 */
/* SYMBOL _keyboard               0x0004C1A2 */
/* SYMBOL _the_prot1              0x0004C21B */
/* SYMBOL _prot_num4              0x0004C2E0 */
/* SYMBOL _newrand                0x0004C2E6   (Linear congruential generator, loops every 2^15 (32768)) */
/* SYMBOL _pixel                  0x0004C302 */
/* SYMBOL _draw_icon              0x0004C376 */
/* SYMBOL _draw_bar               0x0004C3CA */
/* SYMBOL _draw_sprite            0x0004C49A */
/* SYMBOL _draw_s_32              0x0004C58A */
/* SYMBOL _toggle_icon            0x0004C70A */
/* SYMBOL _mouse                  0x0004C792 */
/* SYMBOL _setup_mouse            0x0004CA2C */
/* SYMBOL _vbi_timer              0x0004CA60 */
/* SYMBOL _make_copper            0x0004CA66 */
/* SYMBOL _reset_screen           0x0004CB6A */
/* SYMBOL _show_screen            0x0004CB82 */
/* SYMBOL _waittof                0x0004CBA2 */
/* SYMBOL _change_palette         0x0004CBBA */
/* SYMBOL _convert_sprite         0x0004CBF6 */
/* SYMBOL _insert_sprite          0x0004CC4C */
/* SYMBOL _draw_mouth             0x0004CCC6 */
/* SYMBOL _move_pointer           0x0004CD2E */
/* SYMBOL _text                   0x0004CDBA */
/* SYMBOL _set_serial             0x0004CE56 */
/* SYMBOL _clear_serial           0x0004CEAC */
/* SYMBOL _r_ser_buff             0x0004CEC4 */
/* SYMBOL _w_ser_buff             0x0004CEFA */
/* SYMBOL _read_serial            0x0004CF10 */
/* SYMBOL _write_serial           0x0004CFD2 */
/* SYMBOL _PROTECT3               0x0004D078 */

/* Encrypted section note (BANNER): $4D092 Encrypted code. Decrypts just before it runs and re-encrypts after.
   The 1989 release has several differences in this section. */
/* SYMBOL _PlaySound              0x0004D5F8 */
/* SYMBOL _setsound               0x0004D6CE */
/* SYMBOL _PlayMeas               0x0004D752 */
/* SYMBOL _free_all_sounds        0x0004D854 */
/* SYMBOL _open_channels          0x0004D8D2 */
/* SYMBOL _close_channels         0x0004D9C6 */
/* SYMBOL _load_sound             0x0004DA7A */
/* SYMBOL _kill_effect            0x0004DC96 */
/* SYMBOL _check_effect           0x0004DCCA */
/* SYMBOL _valid_move             0x0004DCE8 */
/* SYMBOL _check_life             0x0004DD56 */
/* SYMBOL _long_asc               0x0004DE56 */
/* SYMBOL _word_asc               0x0004DE64 */
/* SYMBOL _uword_asc              0x0004DE74 */
/* SYMBOL _compress               0x0004DECC */
/* SYMBOL _decompress             0x0004DF04 */

/* Huge block of level code word fragments (used by _code / _decode for conquest passwords).
   These are all the short strings like "RING","VERY","KILL",...,"Y". */
/* SYMBOL strRING                 0x0004E35C */
/* SYMBOL strVERY                 0x0004E361 */
/* SYMBOL strKILL                 0x0004E366 */
/* SYMBOL strSHAD                 0x0004E36B */
/* SYMBOL strHURT                 0x0004E370 */
/* SYMBOL strWEAV                 0x0004E375 */
/* SYMBOL strMIN                  0x0004E37A */
/* SYMBOL strEOA                  0x0004E37E */
/* SYMBOL strCOR                  0x0004E382 */
/* SYMBOL strJOS                  0x0004E386 */
/* SYMBOL strALP                  0x0004E38A */
/* SYMBOL strHAM2                 0x0004E38E */
/* SYMBOL strBUR                  0x0004E392 */
/* SYMBOL strBIN                  0x0004E396 */
/* SYMBOL strTIM                  0x0004E39A */
/* SYMBOL strBAD                  0x0004E39E */
/* SYMBOL strFUT                  0x0004E3A2 */
/* SYMBOL strMOR                  0x0004E3A6 */
/* SYMBOL strSAD                  0x0004E3AA */
/* SYMBOL strCAL                  0x0004E3AE */
/* SYMBOL strIMM                  0x0004E3B2 */
/* SYMBOL strSUZ                  0x0004E3B6 */
/* SYMBOL strNIM                  0x0004E3BA */
/* SYMBOL strLOW                  0x0004E3BE */
/* SYMBOL strSCO                  0x0004E3C2 */
/* SYMBOL strHOB                  0x0004E3C6 */
/* SYMBOL strDOU                  0x0004E3CA */
/* SYMBOL strBIL                  0x0004E3CE */
/* SYMBOL strQAZ2                 0x0004E3D2 */
/* SYMBOL strSWA                  0x0004E3D6 */
/* SYMBOL strBUG                  0x0004E3DA */
/* SYMBOL strSHI                  0x0004E3DE */
/* SYMBOL strHILL                 0x0004E3E2 */
/* SYMBOL strTORY                 0x0004E3E7 */
/* SYMBOL strHOLE                 0x0004E3EC */
/* SYMBOL strPERT                 0x0004E3F1 */
/* SYMBOL strMAR                  0x0004E3F6 */
/* SYMBOL strCON                  0x0004E3FA */
/* SYMBOL strLOW2                 0x0004E3FE */
/* SYMBOL strDOR                  0x0004E402 */
/* SYMBOL strLIN                  0x0004E406 */
/* SYMBOL strING                  0x0004E40A */
/* SYMBOL strHAM                  0x0004E40E */
/* SYMBOL strOLD                  0x0004E412 */
/* SYMBOL strPIL                  0x0004E416 */
/* SYMBOL strBAR                  0x0004E41A */
/* SYMBOL strMET                  0x0004E41E */
/* SYMBOL strEND                  0x0004E422 */
/* SYMBOL strLAS                  0x0004E426 */
/* SYMBOL strOUT                  0x0004E42A */
/* SYMBOL strLUG                  0x0004E42E */
/* SYMBOL strILL                  0x0004E432 */
/* SYMBOL strICK                  0x0004E436 */
/* SYMBOL strPAL                  0x0004E43A */
/* SYMBOL strDON                  0x0004E43E */
/* SYMBOL strORD                  0x0004E442 */
/* SYMBOL strOND                  0x0004E446 */
/* SYMBOL strBOY                  0x0004E44A */
/* SYMBOL strJOB                  0x0004E44E */
/* SYMBOL strER                   0x0004E452 */
/* SYMBOL strED                   0x0004E455 */
/* SYMBOL strME                   0x0004E458 */
/* SYMBOL strAL                   0x0004E45B */
/* SYMBOL strT                    0x0004E45E */
/* SYMBOL strOUT2                 0x0004E460 */
/* SYMBOL strQAZ                  0x0004E464 */
/* SYMBOL strING2                 0x0004E468 */
/* SYMBOL strOGO                  0x0004E46C */
/* SYMBOL strQUE                  0x0004E470 */
/* SYMBOL strLOP                  0x0004E474 */
/* SYMBOL strSOD                  0x0004E478 */
/* SYMBOL strHIP                  0x0004E47C */
/* SYMBOL strKOP                  0x0004E480 */
/* SYMBOL strWIL                  0x0004E484 */
/* SYMBOL strIKE                  0x0004E488 */
/* SYMBOL strDIE                  0x0004E48C */
/* SYMBOL strIN                   0x0004E490 */
/* SYMBOL strAS                   0x0004E493 */
/* SYMBOL strMP                   0x0004E496 */
/* SYMBOL strDI                   0x0004E499 */
/* SYMBOL strOZ                   0x0004E49C */
/* SYMBOL strEA                   0x0004E49F */
/* SYMBOL strUS                   0x0004E4A2 */
/* SYMBOL strGB                   0x0004E4A5 */
/* SYMBOL strCE                   0x0004E4A8 */
/* SYMBOL strME2                  0x0004E4AB */
/* SYMBOL strDE                   0x0004E4AE */
/* SYMBOL strPE                   0x0004E4B1 */
/* SYMBOL strOX                   0x0004E4B4 */
/* SYMBOL strA                    0x0004E4B7 */
/* SYMBOL strE                    0x0004E4B9 */
/* SYMBOL strI                    0x0004E4BB */
/* SYMBOL strO                    0x0004E4BD */
/* SYMBOL strU                    0x0004E4BF */
/* SYMBOL strT2                   0x0004E4C1 */
/* SYMBOL strY                    0x0004E4C3 */

/* Graphics data tables */
/* SYMBOL _icon_data              0x0004E4C6 */
/* SYMBOL _PROTECT1               0x0004F3C6   (copy protection - original 1989 had several lines here; removed in Hit Squad 1993) */
/* SYMBOL _the_prot5              0x0004F3C8 */
/* SYMBOL _PROTECT2               0x0004F3CC */
/* SYMBOL _font_data              0x0004F3CE */

/* LAB / protection notes (BANNER) */
/* SYMBOL LAB_4D5C6               0x0004D5C6 */
/* SYMBOL LAB_52B94               0x00052B94 */

/* C runtime / support shims (many are new or expanded vs launcher) - reference only */
/* SYMBOL _strcpy                 0x000504FE */
/* SYMBOL _strlen                 0x0005050E */
/* SYMBOL _atoi                   0x00050520 */
/* SYMBOL _strcmp                 0x00050592 */
/* SYMBOL _strncmp                0x00050598 */
/* SYMBOL _int_start              0x000505C2 */
/* SYMBOL _int_end                0x000505D0 */
/* SYMBOL _movmem                 0x000505D8   (NEW code section) */
/* SYMBOL _geta4                  0x00050674 */
/* SYMBOL __main                  0x0005067C */
/* SYMBOL __cli_parse             0x000507B4 */
/* SYMBOL _strcat                 0x0005097C */
/* SYMBOL _strncat                0x00050982 */
/* SYMBOL _strncpy                0x000509A6 */
/* SYMBOL _dot_mulu               0x000509C8 */
/* SYMBOL __wb_parse              0x000509EC */
/* SYMBOL strWINDOW               0x00050A97 */
/* SYMBOL strAsterisk             0x00050A9E */
/* SYMBOL _sprintf                0x00050AA0 */
/* SYMBOL SUB_50AEC               0x00050AEC */
/* SYMBOL _format                 0x00050B7A */
/* SYMBOL _dot_divs               0x00050E96   (end of some new code) */
/* SYMBOL _dot_mods               0x00050EBE */
/* SYMBOL _dot_modu               0x00050ED8 */
/* SYMBOL _dot_divu               0x00050EE4 */
/* SYMBOL SUB_50EEE               0x00050EEE */
/* SYMBOL _exit                   0x00050F3E */
/* SYMBOL __exit                  0x00050F5C */
/* SYMBOL _close                  0x00051078 */
/* SYMBOL _Close                  0x000510C4 */
/* SYMBOL __Close                 0x000510C8 */
/* SYMBOL __CurrentDir            0x000510D4 */
/* SYMBOL _Delay                  0x000510E0 */
/* SYMBOL _Examine                0x000510EC */
/* SYMBOL __Examine               0x000510F0 */
/* SYMBOL _ExNext                 0x000510FE */
/* SYMBOL __Input                 0x0005110C */
/* SYMBOL _IoErr                  0x00051114 */
/* SYMBOL __IoErr                 0x00051118 */
/* SYMBOL _Lock                   0x00051120 */
/* SYMBOL __Lock                  0x00051124 */
/* SYMBOL _Open                   0x00051132 */
/* SYMBOL __Open                  0x00051136 */
/* SYMBOL __Output                0x00051144 */
/* SYMBOL _Read                   0x0005114C */
/* SYMBOL __Read                  0x00051150 */
/* SYMBOL _Seek                   0x0005115E */
/* SYMBOL __Seek                  0x00051162 */
/* SYMBOL _UnLock                 0x00051170 */
/* SYMBOL __UnLock                0x00051174 */
/* SYMBOL _Write                  0x00051180 */
/* SYMBOL __Write                 0x00051184 */
/* SYMBOL _AbortIO                0x00051192 */
/* SYMBOL _AddIntServer           0x0005119E */
/* SYMBOL _Alert                  0x000511AC */
/* SYMBOL _AvailMem               0x000511C4   (availmem is new (BANNER)) */
/* SYMBOL _BeginIO                0x000511D0 */
/* SYMBOL _CheckIO                0x000511DC */
/* SYMBOL _CloseDevice            0x000511E8 */
/* SYMBOL _CloseLibrary           0x000511F4 */
/* SYMBOL __CloseLibrary          0x000511F8 */
/* SYMBOL _CreatePort             0x00051204 */
/* SYMBOL _DeletePort             0x00051290 */
/* SYMBOL _AddPort                0x000512D6 */
/* SYMBOL _AllocSignal            0x000512E2 */
/* SYMBOL _AllocMem               0x000512EE */
/* SYMBOL __AllocMem              0x000512F2 */
/* SYMBOL _FindTask               0x00051300 */
/* SYMBOL __FindTask              0x00051304 */
/* SYMBOL __Forbid                0x00051310 */
/* SYMBOL _FreeMem                0x00051318 */
/* SYMBOL __FreeMem               0x0005131C */
/* SYMBOL _FreeSignal             0x0005132C */
/* SYMBOL __GetMsg                0x00051338 */
/* SYMBOL _NewList                0x00051344 */
/* SYMBOL _OpenDevice             0x00051356 */
/* SYMBOL _OpenLibrary            0x0005136C */
/* SYMBOL __OpenLibrary           0x00051370 */
/* SYMBOL _RemIntServer           0x00051380 */
/* SYMBOL _RemPort                0x0005138E */
/* SYMBOL __ReplyMsg              0x0005139A */
/* SYMBOL _SetIntVector           0x000513A6 */
/* SYMBOL __WaitPort              0x000513B4 */
/* SYMBOL _FindToolType           0x000513C0 */
/* SYMBOL _FreeDiskObject         0x000513CE */
/* SYMBOL _GetDiskObject          0x000513DA */
/* SYMBOL _CloseWorkBench         0x000513E6 */

/* SYMBOL __H0_end                0x000513F0 */
/* SYMBOL ___main                 0x000513F6 */
/* SYMBOL ___create_mouse         0x000513FC */
/* SYMBOL ___set_pri              0x00051402 */
/* SYMBOL ___setup_display        0x00051408 */
/* SYMBOL ___make_map             0x0005140E */
/* SYMBOL ___draw_map             0x00051414 */

/* Amiga globals area (high addresses, A4 base points into / near here in original layout) */
/* SYMBOL _IntuitionBase          ~0x0005Cxxx (real values around 5Cxxx in full map) */
/* SYMBOL _GfxBase                ~0x0005Cxxx */
/* SYMBOL _MathTransBase          0x0005C988 */
/* SYMBOL _MathBase               0x0005C98C */
/* SYMBOL _MathIeeeDoubBasBase    0x0005C990 */
/* SYMBOL _MathIeeeDoubTransBase  0x0005C994 */
/* SYMBOL _errno                  0x0005C998 */
/* SYMBOL __H2_end                0x0005C99C */

/* Selected BANNER / COMMENT notes from the full map (important 1993 vs 1989 diffs):
 * BANNER $3E1E8 POPULOUS 2.7 - populous.prg - Hit Squad 1993 budget release
 * BANNER $3E1E8 Lacks the binary obfuscation of the original release
 * BANNER $409D0 large number of lines cut
 * BANNER $412F0 lines cut
 * BANNER $435FE lines cut
 * BANNER $449B0 lines cut
 * BANNER $47F76 new code added here
 * BANNER $47FDE new code ends here
 * BANNER $4817A "LANDSCAPE NO" string is new to this version
 * BANNER $4817A It's used in the options page
 * BANNER $48B3E lines cut here. original: 397c 00001 8e12
 * BANNER $48B50 Several lines removed from original build here
 * BANNER $48B54 Conquest changes end
 * BANNER $48B88 lines cut: 4a6c 3050 66a2
 * BANNER $4940A this whole function is new to the Hit Squad 1993 release
 * BANNER $4940A Appears to be a screenshot tool for testers
 * BANNER $4940A Not called anywhere
 * BANNER $49914 End of new IFF routine
 * BANNER $4A110 lines cut
 * BANNER $4A2C0 lines cut here
 * BANNER $4A870 This debug code is absent from the original 1989 release
 * BANNER $4AA66 Save game routine.
 * BANNER $4AE58 Load game function
 * BANNER $4C2E6 Random number generator
 * BANNER $4C2E6 Linear congruential generator
 * BANNER $4C2E6 Loops every 2^15 (32,768)
 * BANNER $4D092 Encrypted code.
 * BANNER $4D092 Decrypts just before it runs and re-encrypts after.
 * BANNER $4D092 The 1989 release has several differences in this section.
 * BANNER $4F3C6 The original release has several lines of code here
 * BANNER $4F3C6 Used for copy protection, they are removed in the
 * BANNER $4F3C6 Hit Squad 1993 budget release
 * BANNER $505D8 new code
 * BANNER $50602 end new code
 * BANNER $50AA0 new code
 * BANNER $50E96 end of new code
 * BANNER $593EE A4 is a pointer to this
 *
 * Specific scoring / effect comments (BANNER):
 * $405A6 Increment game turn
 * $43918 +250 points
 * $43A08 +10 points per
 * $43B16 +10 points per
 * $43C68 +25 points for earthquake
 * $43E7A +100 points per volcano
 * $44164 +50 points per swamp
 * $442A0 +150 points for knight
 * $4438A +5000 points for armageddon
 * $488FE +5000 points for winning armageddon?
 * $48922 +5000 points for ?
 * $48940 +5000 points for?
 * $4896C +15 points per ?
 * $4897E x10 points
 * $4898A Minimum points 500
 * $4899C maximum points 555,555
 * $493EA wait for vertical blank
 * $4A8AA battles won wil always read n / 208
 * $4A8D6 five copy protection variables
 * $4A8D6 shown as long hex
 * $4A8E2 SIG 2
 * $4A8EC fast memory
 * $4A8F8 chip memory
 * $4A932 mousewait
 * $511C4 availmem is new
 * $514CE _display_debug is new to this release
 * $514E6 Should be JMP _the_prot1 but can't jump to odd value
 * $52B94 new
 * $52C2E seems to be new
 *
 * END OF MAP
 */

#endif /* POPULOUS_SYMBOLS_PRG_H */