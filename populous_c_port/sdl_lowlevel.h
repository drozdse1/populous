#ifndef SDL_LOWLEVEL_H
#define SDL_LOWLEVEL_H

#include <SDL.h>
#include <stdint.h>

/* 
 * Very low-level graphics abstraction that tries to feel a bit like
 * the Amiga graphics.library + custom chips, but using SDL2.
 * 
 * We keep pixel buffers in "Amiga style" (bitplanes or chunky for simplicity)
 * and only convert to SDL texture when we want to present.
 */

typedef struct AmigaScreen {
    int width;
    int height;
    int depth;                  /* bitplanes */
    uint8_t *bitplanes[8];      /* raw bitplane pointers (CHIP style) */
    uint32_t *palette;          /* 32-bit ARGB palette (up to 256 colors) */
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;       /* we render the chunky buffer to this */
    uint32_t *chunky_buffer;    /* 32-bit converted buffer for SDL */
} AmigaScreen;

/* Initialize a low-level "Amiga" screen */
AmigaScreen *amiga_create_screen(int width, int height, int depth);

/* Free everything */
void amiga_destroy_screen(AmigaScreen *scr);

/* Set a color in the palette (Amiga style 0x0RGB) */
void amiga_set_color(AmigaScreen *scr, int index, uint16_t amiga_rgb);

/* Blit a rectangular area from bitplanes into the chunky buffer (simplified) */
void amiga_blit_bitplanes(AmigaScreen *scr, int plane, int x, int y, int w, int h, uint8_t *src);

/* Present the current buffer to the window (like WaitTOF + copper) */
void amiga_present(AmigaScreen *scr);

/* Very simple "copper list" stub - we can expand this later */
void amiga_set_copper_list(AmigaScreen *scr, uint16_t *copperlist, int size);

#endif /* SDL_LOWLEVEL_H */