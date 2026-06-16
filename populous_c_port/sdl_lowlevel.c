#include "sdl_lowlevel.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

AmigaScreen *amiga_create_screen(int width, int height, int depth)
{
    AmigaScreen *scr = calloc(1, sizeof(AmigaScreen));
    if (!scr) return NULL;

    scr->width = width;
    scr->height = height;
    scr->depth = depth;

    /* Allocate bitplane memory (CHIP style - we just use normal RAM here) */
    for (int p = 0; p < depth && p < 8; p++) {
        scr->bitplanes[p] = calloc(1, (width * height) / 8);
        if (!scr->bitplanes[p]) {
            fprintf(stderr, "Failed to allocate bitplane %d\n", p);
        }
    }

    scr->palette = calloc(256, sizeof(uint32_t));

    /* Default Amiga-like palette (very rough) */
    for (int i = 0; i < 16; i++) {
        int v = (i * 17) & 0xFF; /* rough 4-bit to 8-bit */
        scr->palette[i] = 0xFF000000 | (v << 16) | (v << 8) | v;
    }

    scr->chunky_buffer = calloc(width * height, sizeof(uint32_t));

    /* Create SDL window + renderer (low level feel) */
    scr->window = SDL_CreateWindow("Populous Amiga Port (SDL2 low-level)",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   width * 2, height * 2,  /* integer scale for visibility */
                                   SDL_WINDOW_SHOWN);
    if (!scr->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    }

    scr->renderer = SDL_CreateRenderer(scr->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!scr->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    }

    scr->texture = SDL_CreateTexture(scr->renderer,
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     width, height);

    return scr;
}

void amiga_destroy_screen(AmigaScreen *scr)
{
    if (!scr) return;

    for (int p = 0; p < 8; p++) {
        free(scr->bitplanes[p]);
    }
    free(scr->palette);
    free(scr->chunky_buffer);

    if (scr->texture) SDL_DestroyTexture(scr->texture);
    if (scr->renderer) SDL_DestroyRenderer(scr->renderer);
    if (scr->window) SDL_DestroyWindow(scr->window);

    free(scr);
}

void amiga_set_color(AmigaScreen *scr, int index, uint16_t amiga_rgb)
{
    if (!scr || index < 0 || index > 255) return;

    /* Amiga 0x0RGB format */
    uint8_t r = ((amiga_rgb >> 8) & 0xF) * 17;
    uint8_t g = ((amiga_rgb >> 4) & 0xF) * 17;
    uint8_t b = (amiga_rgb & 0xF) * 17;

    scr->palette[index] = 0xFF000000 | (r << 16) | (g << 8) | b;
}

void amiga_present(AmigaScreen *scr)
{
    if (!scr || !scr->renderer || !scr->texture) return;

    /* For now we just upload the chunky buffer */
    SDL_UpdateTexture(scr->texture, NULL, scr->chunky_buffer, scr->width * sizeof(uint32_t));
    SDL_RenderClear(scr->renderer);
    SDL_RenderCopy(scr->renderer, scr->texture, NULL, NULL);
    SDL_RenderPresent(scr->renderer);
}

/* Extremely simplified "blit from bitplanes" - real Amiga would do proper planar->chunky here */
void amiga_blit_bitplanes(AmigaScreen *scr, int plane, int x, int y, int w, int h, uint8_t *src)
{
    if (!scr || !src || plane >= scr->depth) return;

    /* This is a placeholder. A real port would decode the bitplanes properly. */
    for (int j = 0; j < h && (y + j) < scr->height; j++) {
        for (int i = 0; i < w && (x + i) < scr->width; i++) {
            int idx = (y + j) * scr->width + (x + i);
            /* Very crude - just take bits from the plane as intensity */
            uint8_t bit = (src[(j * ((w+7)/8)) + (i/8)] >> (7 - (i%8))) & 1;
            if (bit) {
                scr->chunky_buffer[idx] = scr->palette[1]; /* use color 1 for this plane demo */
            }
        }
    }
}

void amiga_set_copper_list(AmigaScreen *scr, uint16_t *copperlist, int size)
{
    /* Stub: In a real low-level port we would interpret the copper list
     * (move to color regs, wait, etc). For now we just print that we got one. */
    (void)scr; (void)copperlist; (void)size;
    printf("[SDL] Copper list set (size=%d) - stubbed\n", size);
}