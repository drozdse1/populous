#ifndef POPULOUS_GRAPHICS_H
#define POPULOUS_GRAPHICS_H
#include <stddef.h>
#include <stdint.h>

#define GFX_WIDTH 320
#define GFX_HEIGHT 256
#define GFX_LAND_PIXELS (70 * 32 * 24)
#define GFX_SPRITE_PIXELS (147 * 16 * 16)
/* _sprite_data_32: AllocMem/Read $2080 bytes = 13 frames of 640 (asm:16443-16449). */
#define GFX_BIG_FRAMES 13
#define GFX_BIG_PIXELS (GFX_BIG_FRAMES * 32 * 32)
typedef struct {
    const uint8_t *indices, *keep;
    size_t size;
    int width, height;
} GraphicsSheet;
typedef struct {
    uint8_t background[320 * 200];
    uint8_t land_indices[GFX_LAND_PIXELS], land_keep[GFX_LAND_PIXELS];
    uint8_t sprite_indices[GFX_SPRITE_PIXELS], sprite_keep[GFX_SPRITE_PIXELS];
    size_t sprite_size;
    uint8_t big_indices[GFX_BIG_PIXELS], big_keep[GFX_BIG_PIXELS];
    size_t big_size;   /* 0: spr_32N.dat absent (the original skips the Read) */
} GraphicsAssets;
extern const uint16_t graphics_palette[16];
GraphicsAssets *graphics_load(const char *root, unsigned land, unsigned sprites,
                              char *error, size_t error_size);
/* _blk_data of a LAND file (asm:16514-16518, 0x8340 bytes): 70 frames of
 * 24 rows, each BE32 keep,p0,p1,p2,p3, MSB = left pixel. 1 on success. */
int graphics_decode_land(const uint8_t *blk, size_t size, uint8_t *indices, uint8_t *keep);
void graphics_free(GraphicsAssets *assets);
GraphicsSheet graphics_land(const GraphicsAssets *assets);
GraphicsSheet graphics_sprites(const GraphicsAssets *assets);
GraphicsSheet graphics_big_sprites(const GraphicsAssets *assets);
int graphics_blit(uint8_t *dest, size_t size, int width, int height,
                  GraphicsSheet sheet, int frame, int left, int top);
int graphics_draw_s_32(uint8_t *dest, size_t size, GraphicsSheet sheet, int frame, int x, int y);
int graphics_tile(int u, int v, int altitude, int *left, int *top);
int graphics_vertex(int u, int v, int height, int *x, int *y);
int graphics_pick(const int16_t heights[81], int x, int y, int *u, int *v);
int graphics_walker_frame(int delta, int owner, int phase);
#endif
