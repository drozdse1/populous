/*
 * test_graphics.c - Graphics tests: sprite compositor, clipping and asset frame handling.
 *
 * Run via `make test`. Each test_* function is self-contained and
 * aborts through assert() on the first mismatch.
 */
#define _POSIX_C_SOURCE 200809L
#include "../graphics.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Breaks caught: alpha instead of AND/OR, clipping wrap, unchecked bad frames. */
static void test_compositor(void)
{
    uint8_t indices[] = {2, 4, 0, 7}, keep[] = {1, 0, 1, 0};
    GraphicsSheet sheet = {indices, keep, 4, 2, 2};
    uint8_t buffer[18];
    memset(buffer, 9, sizeof buffer);
    assert(graphics_blit(buffer + 1, 16, 4, 4, sheet, 0, 1, 1));
    assert(buffer[6] == 11 && buffer[7] == 4 && buffer[10] == 9 && buffer[11] == 7);
    assert(buffer[0] == 9 && buffer[17] == 9);
    assert(graphics_blit(buffer + 1, 16, 4, 4, sheet, 0, -1, -1));
    assert(buffer[1] == 7);
    uint8_t before[18];
    memcpy(before, buffer, sizeof buffer);
    assert(!graphics_blit(buffer + 1, 16, 4, 4, sheet, 1, 0, 0));
    assert(!graphics_blit(buffer + 1, 16, 4, 4, sheet, -1, 0, 0));
    assert(!graphics_blit(buffer + 1, 15, 4, 4, sheet, 0, 0, 0));
    indices[3] = 16;
    assert(!graphics_blit(buffer + 1, 16, 4, 4, sheet, 0, 0, 0));
    indices[3] = 7;
    keep[3] = 2;
    assert(!graphics_blit(buffer + 1, 16, 4, 4, sheet, 0, 0, 0));
    keep[3] = 0;
    assert(graphics_blit(buffer + 1, 16, 4, 4, sheet, 0, INT_MAX, INT_MIN));
    sheet.width = INT_MAX;
    assert(!graphics_blit(buffer + 1, 16, 4, 4, sheet, 0, 0, 0));
    assert(memcmp(before, buffer, sizeof buffer) == 0);
}

/* Literal assembly projection: tile origin 0xa16; vertex top = 72 - 8*h. */
static void test_projection(void)
{
    int x, y, u, v;
    assert(graphics_tile(0, 0, 0, &x, &y) && x == 176 && y == 64);
    assert(graphics_tile(7, 0, 2, &x, &y) && x == 288 && y == 104);
    assert(graphics_vertex(2, 3, 1, &x, &y) && x == 176 && y == 104);
    assert(!graphics_tile(8, 0, 0, &x, &y));
    int16_t heights[81];
    for (int n = 0; n < 81; ++n)
        heights[n] = 1;
    assert(graphics_pick(heights, 192, 112, &u, &v) && u == 3 && v == 3);
    heights[3 * 9 + 3] = 3;
    assert(graphics_pick(heights, 192, 96, &u, &v) && u == 3 && v == 3);
    assert(!graphics_pick(heights, 20, 20, &u, &v));
    assert(!graphics_pick(heights, 192, 220, &u, &v));
    assert(graphics_pick(heights, 192, 192, &u, &v) && u == 8 && v == 8);
    heights[0] = -1;
    assert(!graphics_pick(heights, 192, 64, &u, &v));
    const int deltas[] = {-65, -64, -63, 1, 65, 64, 63, -1};
    for (int n = 0; n < 8; ++n) {
        assert(graphics_walker_frame(deltas[n], 0, 0) == n * 2);
        assert(graphics_walker_frame(deltas[n], 1, 1) == n * 2 + 17);
    }
    assert(graphics_walker_frame(0, 0, 0) == 8);
    assert(graphics_walker_frame(1, 2, 0) == -1);
}

static void put_file(const char *directory, const char *name, size_t size, int value)
{
    char path[512];
    snprintf(path, sizeof path, "%s/pixels/%s", directory, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    for (size_t n = 0; n < size; ++n)
        assert(fputc(value, file) != EOF);
    assert(!fclose(file));
}

/* Loader. */
static void test_loader(void)
{
    char root[] = "/tmp/populous-graphics-XXXXXX", path[512], error[160];
    assert(mkdtemp(root));
    snprintf(path, sizeof path, "%s/pixels", root);
    assert(!mkdir(path, 0700));
    const char *names[] = {"qaz.pic.indices.u8", "land2.indices.u8", "land2.keep.u8",
                           "sprites0.dat.indices.u8", "sprites0.dat.keep.u8"};
    const size_t sizes[] = {64000, GFX_LAND_PIXELS, GFX_LAND_PIXELS, GFX_SPRITE_PIXELS,
                            GFX_SPRITE_PIXELS};
    for (int n = 0; n < 5; ++n)
        put_file(root, names[n], sizes[n], n == 2 || n == 4 ? 1 : 3);
    GraphicsAssets *assets = graphics_load(root, 2, 0, error, sizeof error);
    assert(assets && assets->background[63999] == 3 && assets->sprite_size == GFX_SPRITE_PIXELS);
    uint8_t buffer[32 * 24];
    memset(buffer, 4, sizeof buffer);
    assert(graphics_blit(buffer, sizeof buffer, 32, 24, graphics_land(assets), 69, 0, 0));
    assert(buffer[0] == 7 && buffer[767] == 7);
    graphics_free(assets);
    put_file(root, names[0], 64001, 0);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    put_file(root, names[0], 64000, 16);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    put_file(root, names[0], 64000, 0);
    put_file(root, names[2], GFX_LAND_PIXELS, 2);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    put_file(root, names[2], GFX_LAND_PIXELS, 1);
    put_file(root, names[3], 143 * 256, 0);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    put_file(root, names[4], 143 * 256, 1);
    assets = graphics_load(root, 2, 0, error, sizeof error);
    assert(assets && assets->sprite_size == 143 * 256);
    graphics_free(assets);
    put_file(root, names[3], 143 * 256 - 1, 0);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    assert(!graphics_load(root, 4, 0, error, sizeof error));
    assert(!graphics_load(root, 0, 10, error, sizeof error));
    /* spr_32N.dat (asm:16426-16449) is optional, but a present export is checked. */
    put_file(root, names[3], 143 * 256, 0);
    assets = graphics_load(root, 2, 0, error, sizeof error);
    assert(assets && assets->big_size == 0 && !graphics_big_sprites(assets).indices);
    graphics_free(assets);
    const char *big[] = {"spr_320.dat.indices.u8", "spr_320.dat.keep.u8"};
    put_file(root, big[0], GFX_BIG_PIXELS, 5);
    put_file(root, big[1], GFX_BIG_PIXELS, 1);
    assets = graphics_load(root, 2, 0, error, sizeof error);
    assert(assets && assets->big_size == GFX_BIG_PIXELS);
    static uint8_t screen[320 * 200];
    memset(screen, 2, sizeof screen);
    assert(graphics_draw_s_32(screen, sizeof screen, graphics_big_sprites(assets), 12, 288, 167));
    assert(screen[167 * 320 + 288] == 7 && screen[198 * 320 + 319] == 7 &&
           screen[166 * 320 + 288] == 2 && screen[167 * 320 + 287] == 2);
    assert(!graphics_draw_s_32(screen, sizeof screen, graphics_big_sprites(assets), 13, 0, 0));
    graphics_free(assets);
    put_file(root, big[0], GFX_BIG_PIXELS + 1024, 5);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    put_file(root, big[0], 1023, 5);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    put_file(root, big[0], 1024, 16);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    put_file(root, big[0], 1024, 5);
    assert(!graphics_load(root, 2, 0, error, sizeof error)); /* keep size differs */
    put_file(root, big[1], 1024, 2);
    assert(!graphics_load(root, 2, 0, error, sizeof error));
    put_file(root, big[1], 1024, 0);
    assets = graphics_load(root, 2, 0, error, sizeof error);
    assert(assets && assets->big_size == 1024);
    graphics_free(assets);
    snprintf(path, sizeof path, "%s/pixels/%s", root, big[1]);
    assert(!unlink(path));
    assert(!graphics_load(root, 2, 0, error, sizeof error)); /* indices without keep */
    snprintf(path, sizeof path, "%s/pixels/%s", root, big[0]);
    assert(!unlink(path));
    for (int n = 0; n < 5; ++n) {
        snprintf(path, sizeof path, "%s/pixels/%s", root, names[n]);
        assert(!unlink(path));
    }
    snprintf(path, sizeof path, "%s/pixels", root);
    assert(!rmdir(path));
    assert(!rmdir(root));
}

/* ========================================================================
 * Test runner (focused --flags first, then the full suite)
 * ======================================================================== */
int main(int argc, char **argv)
{
    test_compositor();
    test_projection();
    test_loader();
    if (argc == 2)
        for (unsigned land = 0; land < 4; ++land) {
            char error[160];
            GraphicsAssets *assets = graphics_load(argv[1], land, 0, error, sizeof error);
            if (!assets) fprintf(stderr, "%s\n", error);
            assert(assets);
            if (land == 2) {
                /* Supplied LAND2 frame 36: keep+colour conflicts are OR operations,
             * while a zero index with keep=0 is opaque black, not transparency. */
                GraphicsSheet sheet = graphics_land(assets);
                const size_t frame = 36 * 768;
                assert(sheet.indices[frame + 4 * 32 + 10] == 1 &&
                       sheet.keep[frame + 4 * 32 + 10] == 1);
                assert(sheet.indices[frame + 6 * 32 + 22] == 1 &&
                       sheet.keep[frame + 6 * 32 + 22] == 1);
                assert(sheet.indices[frame + 9 * 32 + 27] == 0 &&
                       sheet.keep[frame + 9 * 32 + 27] == 0);
                uint8_t buffer[32 * 24];
                memset(buffer, 2, sizeof buffer);
                assert(graphics_blit(buffer, sizeof buffer, 32, 24, sheet, 36, 0, 0));
                assert(buffer[4 * 32 + 10] == 3 && buffer[6 * 32 + 22] == 3);
                assert(buffer[9 * 32 + 27] == 0);
            }
            graphics_free(assets);
        }
    puts("graphics compositor/projection/assets: PASS");
    return 0;
}
