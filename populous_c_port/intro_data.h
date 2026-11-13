#ifndef INTRO_DATA_H
#define INTRO_DATA_H
#include <stdint.h>

#define INTRO_GLYPHS 8
extern const uint16_t intro_title_glyphs[INTRO_GLYPHS][32][5];
extern const uint8_t intro_title_word[8];
extern const uint16_t intro_fade_mask[30];

#define INTRO_SCRIPT_BYTES 0x344
#define INTRO_TABLE_BYTES 0x6c
#define INTRO_CORNER_WORDS 0x800
extern const uint8_t intro_script_memory[INTRO_SCRIPT_BYTES];
extern const uint16_t intro_script_start[16];
extern const int16_t intro_actor_init[16][7];
extern const int16_t intro_actor_cells[16];
extern const uint8_t intro_actor_tables[INTRO_TABLE_BYTES];
extern const uint8_t intro_height_tail[8];
extern const uint16_t intro_corner_words[INTRO_CORNER_WORDS];

#endif
