#include "sdl_lowlevel.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "pop_log.h"

AmigaScreen *amiga_create_screen(int width, int height, int depth)
{
    /* SDL dimensions/pitch are signed ints; buffer sizes must also fit size_t. */
    if (width <= 0 || height <= 0 || depth < 1 || depth > 8 ||
        width > INT_MAX / (int)sizeof(uint32_t) || height > INT_MAX / 2 ||
        (size_t)width > SIZE_MAX / sizeof(uint32_t) / (size_t)height) {
        SDL_SetError("Invalid Amiga screen dimensions or depth");
        POP_LOG_ERROR("amiga_create_screen: %s", SDL_GetError());
        return NULL;
    }
    AmigaScreen *scr = calloc(1, sizeof(AmigaScreen));
    if (!scr) goto allocation_failed;

    scr->width = width;
    scr->height = height;
    scr->depth = depth;

    /* Bitplane rows include padding when the width is not a multiple of eight. */
    size_t row_bytes = ((size_t)width + 7) / 8;
    for (int plane = 0; plane < depth; ++plane) {
        scr->bitplanes[plane] = calloc((size_t)height, row_bytes);
        if (!scr->bitplanes[plane]) goto allocation_failed;
    }
    scr->palette = calloc(256, sizeof(uint32_t));
    if (!scr->palette) goto allocation_failed;
    scr->chunky_buffer = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    if (!scr->chunky_buffer) goto allocation_failed;

    for (int index = 0; index < 16; ++index) {
        unsigned value = (unsigned)index * 17;
        scr->palette[index] = 0xFF000000u | (value << 16) | (value << 8) | value;
    }

    scr->window = SDL_CreateWindow("Populous Amiga Port (SDL2 low-level)",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   width * 2, height * 2, SDL_WINDOW_SHOWN);
    if (!scr->window) {
        POP_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        goto failed;
    }
    scr->renderer = SDL_CreateRenderer(scr->window, -1,
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!scr->renderer) {
        POP_LOG_WARN("Accelerated renderer unavailable: %s; trying software", SDL_GetError());
        scr->renderer = SDL_CreateRenderer(scr->window, -1, SDL_RENDERER_SOFTWARE);
        if (!scr->renderer) {
            POP_LOG_ERROR("Software renderer failed: %s", SDL_GetError());
            goto failed;
        }
    }
    scr->texture = SDL_CreateTexture(scr->renderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!scr->texture) {
        POP_LOG_ERROR("SDL_CreateTexture failed: %s", SDL_GetError());
        goto failed;
    }
    return scr;

allocation_failed:
    SDL_OutOfMemory();
    POP_LOG_ERROR("amiga_create_screen: %s", SDL_GetError());
failed:
    amiga_destroy_screen(scr);
    return NULL;
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
    POP_LOGD("SDL", "Copper list set (size=%d) - stubbed", size);
}