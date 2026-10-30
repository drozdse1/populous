#ifndef POPULOUS_GRAPHICS_H
#define POPULOUS_GRAPHICS_H
#include <stddef.h>
#include <stdint.h>

#define GFX_WIDTH 320
#define GFX_HEIGHT 256
#define GFX_LAND_PIXELS (70 * 32 * 24)
#define GFX_SPRITE_PIXELS (147 * 16 * 16)
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
} GraphicsAssets;
extern const uint16_t graphics_palette[16];
GraphicsAssets *graphics_load(const char *root, unsigned land, unsigned sprites,
                              char *error, size_t error_size);
void graphics_free(GraphicsAssets *assets);
GraphicsSheet graphics_land(const GraphicsAssets *assets);
GraphicsSheet graphics_sprites(const GraphicsAssets *assets);
int graphics_blit(uint8_t *dest, size_t size, int width, int height,
                  GraphicsSheet sheet, int frame, int left, int top);
int graphics_tile(int u, int v, int altitude, int *left, int *top);
int graphics_vertex(int u, int v, int height, int *x, int *y);
int graphics_pick(const int16_t heights[81], int x, int y, int *u, int *v);
int graphics_walker_frame(int delta, int owner, int phase);
#endif
