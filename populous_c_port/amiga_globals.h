#ifndef AMIGA_GLOBALS_H
#define AMIGA_GLOBALS_H

#include <stdint.h>
#include "symbols.h"   /* full IRA symbol map for accurate addresses and names */

/* 
 * This struct emulates the large A4-relative data area used throughout
 * the original 68000 code. Offsets like 0x8238, 0x823c etc. from the
 * disassembly are turned into named fields here.
 * 
 * Many of these were dynamically allocated CHIP RAM blocks.
 */
typedef struct AmigaGlobals {
    /* === Library bases (from SUB_384 / OpenLibrary) === */
    void *graphics_base;           /* -32124 from A4 (0x8284) */
    void *intuition_base;          /* -32082 from A4 (0x82ae) */

    /* === Major CHIP RAM allocations (from SUB_11A / SUB_6C4A) === */
    uint8_t *alloc_8340;           /* 0x8340 bytes  (often copper / small screen) */
    uint8_t *alloc_7d00;           /* 0x7D00 bytes */
    uint8_t *alloc_1f400;          /* 0x1F400 bytes - likely main display bitplanes */
    uint8_t *alloc_4b0;            /* 0x4B0 bytes   */

    /* === Other important pointers from early init === */
    uint8_t *screen_struct;        /* -32124 in some contexts */
    uint8_t *load_pic_struct;      /* for demo.pic / load.pic loader */
    uint8_t *startup_struct;       /* the one built in SUB_16E */
    uint8_t *copper_area;          /* the 0x480 block from SUB_226 */

    /* === Display / input state (populated by SUB_456 etc.) === */
    uint16_t mouse_x;
    uint16_t mouse_y;
    uint16_t button_state;
    uint16_t choice;               /* result of the load.pic button selection */

    /* Display size (set during early init) */
    uint16_t screen_width;
    uint16_t screen_height;

    /* === Data pointers (will be filled when we translate more) === */
    uint8_t *level_data;
    uint8_t *main_prg_base;        /* "populous.prg" loaded here */

    /* Big data tables from the end of the disassembly (SECSTRT_1 / data section) */
    /* These are the large DC.L arrays you see at the bottom of the paste.
       They contain copper lists, palettes, screen descriptors, object/tile tables, etc. */
    uint16_t *copper_list_base;
    uint16_t *palette_data;
    uint8_t  *object_table;
    uint32_t *level_init_data;     /* the big numeric blocks after SUB_536 */

    /* Safety / unknown offsets */
    uint32_t unknown[512];
} AmigaGlobals;

extern AmigaGlobals g;

#endif /* AMIGA_GLOBALS_H */