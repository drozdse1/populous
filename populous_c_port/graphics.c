#define _POSIX_C_SOURCE 200809L
#include "graphics.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define POPULOUS_WIN_POSIX_MACROS
#include "win_posix.h"
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* src/populous_prg.asm:24264-24268, four-plane gameplay RGB12. */
const uint16_t graphics_palette[16] = {
    0x000,0x444,0x666,0x888,0xaaa,0xccc,0x620,0x840,
    0xa00,0xa60,0xaa0,0x4a0,0x280,0x262,0x248,0x26c
};

static int read_pixels(int directory, const char *name, uint8_t *out,
                       size_t minimum, size_t maximum, size_t multiple,
                       unsigned limit, size_t *length)
{
    int descriptor = openat(directory,name,O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) return 0;
    struct stat status;
    int valid = !fstat(descriptor,&status) && S_ISREG(status.st_mode) &&
        status.st_size >= (off_t)minimum && status.st_size <= (off_t)maximum &&
        status.st_size % (off_t)multiple == 0;
    size_t used = 0;
    while (valid && used < (size_t)status.st_size) {
        ssize_t count = read(descriptor,out+used,(size_t)status.st_size-used);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { valid = 0; break; }
        used += (size_t)count;
    }
    uint8_t extra;
    if (valid && read(descriptor,&extra,1) != 0) valid = 0;
    close(descriptor);
    for (size_t pixel = 0; valid && pixel < used; ++pixel)
        if (out[pixel] > limit) valid = 0;
    if (valid && length) *length = used;
    return valid;
}

GraphicsAssets *graphics_load(const char *root, unsigned land, unsigned sprites,
                              char *error, size_t error_size)
{
    GraphicsAssets *assets = NULL;
    int directory = -1, root_fd = -1;
    char name[64] = "asset directory";
    if (!root || !*root || land > 3 || sprites > 9) goto failed;
    root_fd = open(root,O_RDONLY | O_DIRECTORY);
    if (root_fd < 0) goto failed;
    directory = openat(root_fd,"pixels",O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    close(root_fd);
    if (directory < 0) goto failed;
    assets = calloc(1,sizeof *assets);
    if (!assets) goto failed;
    snprintf(name,sizeof name,"qaz.pic.indices.u8");
    if (!read_pixels(directory,name,assets->background,64000,64000,1,15,NULL)) goto failed;
    snprintf(name,sizeof name,"land%u.indices.u8",land);
    if (!read_pixels(directory,name,assets->land_indices,GFX_LAND_PIXELS,GFX_LAND_PIXELS,1,15,NULL)) goto failed;
    snprintf(name,sizeof name,"land%u.keep.u8",land);
    if (!read_pixels(directory,name,assets->land_keep,GFX_LAND_PIXELS,GFX_LAND_PIXELS,1,1,NULL)) goto failed;
    snprintf(name,sizeof name,"sprites%u.dat.indices.u8",sprites);
    if (!read_pixels(directory,name,assets->sprite_indices,143*256,GFX_SPRITE_PIXELS,256,15,&assets->sprite_size)) goto failed;
    snprintf(name,sizeof name,"sprites%u.dat.keep.u8",sprites);
    if (!read_pixels(directory,name,assets->sprite_keep,assets->sprite_size,assets->sprite_size,256,1,NULL)) goto failed;
    /* _read_sprites asm:16426-16449: spr_32N.dat is optional (Open failure
     * skips the Read); a present export must still be valid. */
    snprintf(name,sizeof name,"spr_32%u.dat.indices.u8",sprites);
    if (!faccessat(directory,name,F_OK,0)) {
        if (!read_pixels(directory,name,assets->big_indices,1024,GFX_BIG_PIXELS,1024,15,&assets->big_size)) goto failed;
        snprintf(name,sizeof name,"spr_32%u.dat.keep.u8",sprites);
        if (!read_pixels(directory,name,assets->big_keep,assets->big_size,assets->big_size,1024,1,NULL)) goto failed;
    }
    close(directory);
    if (error && error_size) error[0] = 0;
    return assets;
failed:
    if (directory >= 0) close(directory);
    free(assets);
    if (error && error_size) snprintf(error,error_size,"missing/invalid %s (size, index or mask); set POPULOUS_ASSET_DIR to an extract_assets.py export",name);
    return NULL;
}

void graphics_free(GraphicsAssets *assets) { free(assets); }
GraphicsSheet graphics_land(const GraphicsAssets *assets)
{
    return assets ? (GraphicsSheet){assets->land_indices,assets->land_keep,GFX_LAND_PIXELS,32,24} : (GraphicsSheet){0};
}
GraphicsSheet graphics_sprites(const GraphicsAssets *assets)
{
    return assets ? (GraphicsSheet){assets->sprite_indices,assets->sprite_keep,assets->sprite_size,16,16} : (GraphicsSheet){0};
}

GraphicsSheet graphics_big_sprites(const GraphicsAssets *assets)
{
    return assets && assets->big_size ? (GraphicsSheet){assets->big_indices,assets->big_keep,assets->big_size,32,32} : (GraphicsSheet){0};
}

int graphics_blit(uint8_t *dest, size_t size, int width, int height,
                  GraphicsSheet sheet, int frame, int left, int top)
{
    if (!dest || width <= 0 || height <= 0 || (size_t)width > size/(size_t)height ||
        !sheet.indices || !sheet.keep || sheet.width <= 0 || sheet.height <= 0 || frame < 0 ||
        (size_t)sheet.width > sheet.size/(size_t)sheet.height) return 0;
    size_t frame_size = (size_t)sheet.width * (size_t)sheet.height;
    if (sheet.size % frame_size || (size_t)frame >= sheet.size/frame_size) return 0;
    size_t offset = (size_t)frame * frame_size;
    /* Validate the entire frame before touching any destination pixel. */
    for (size_t pixel = offset; pixel < offset+frame_size; ++pixel)
        if (sheet.indices[pixel] > 15 || sheet.keep[pixel] > 1) return 0;
    for (int row = 0; row < sheet.height; ++row) {
        int64_t dest_y = (int64_t)top+row;
        if (dest_y < 0 || dest_y >= height) continue;
        for (int column = 0; column < sheet.width; ++column) {
            int64_t dest_x = (int64_t)left+column;
            if (dest_x < 0 || dest_x >= width) continue;
            size_t src = offset+(size_t)row*sheet.width+column;
            size_t dst = (size_t)dest_y*width+(size_t)dest_x;
            /* Mask is one keep bit for ALL four planes, not PNG alpha. */
            dest[dst] = (dest[dst] & (sheet.keep[src] ? 15 : 0)) | sheet.indices[src];
        }
    }
    return 1;
}

int graphics_tile(int u, int v, int altitude, int *left, int *top)
{
    if (!left || !top || u < 0 || u > 7 || v < 0 || v > 7 || altitude < 0 || altitude > 8) return 0;
    *left = 176+16*(u-v); *top = 64+8*(u+v)-8*altitude;
    return 1;
}
int graphics_vertex(int u, int v, int height, int *x, int *y)
{
    if (!x || !y || u < 0 || u > 8 || v < 0 || v > 8 || height < 0 || height > 8) return 0;
    *x = 192+16*(u-v); *y = 72+8*(u+v)-8*height;
    return 1;
}
int graphics_pick(const int16_t heights[81], int x, int y, int *u, int *v)
{
    if (!heights || !u || !v || x < 64 || x >= 320 || y < 0 || y >= 200) return 0;
    int distance = 17, found = 0;
    /* Height-aware projected vertex diamonds; frontmost wins coincident points.
     * Same lattice as _sculpt (asm:7316-7416), bounded to the visible 9x9 vertices. */
    for (int row = 0; row <= 8; ++row) for (int column = 0; column <= 8; ++column) {
        int px,py;
        if (!graphics_vertex(column,row,heights[row*9+column],&px,&py)) return 0;
        int candidate = abs(px-x)+2*abs(py-y);
        if (candidate <= 16 && candidate <= distance) {
            distance = candidate; *u = column; *v = row; found = 1;
        }
    }
    return found;
}
int graphics_walker_frame(int delta, int owner, int phase)
{
    if (owner < 0 || owner > 1 || phase < 0 || phase > 1) return -1;
    const int directions[] = {-65,-64,-63,1,65,64,63,-1};
    int frame = 8;
    for (int index = 0; index < 8; ++index)
        if (directions[index] == delta) frame = index*2;
    return frame+owner*16+phase;
}

/* _draw_s_32 asm:19365-19504 on a 320x200 four-plane screen: y < 0 draws
 * nothing; from y 168 (#$a8) the DBF count is 199-y, i.e. 200-y rows (the
 * screen's own bottom clip) and none at all at y 199. Right-hand words past
 * column 320 are skipped (D7 $26/$28), exactly a clip at 320. Callers never
 * pass x < 0 or x >= 320 (the original would wrap into the next row); they
 * are rejected. Pixels: (screen AND keep) OR planes, as graphics_blit. */
int graphics_draw_s_32(uint8_t *dest, size_t size, GraphicsSheet sheet, int frame, int x, int y)
{
    if (!dest || size < 320*200 || sheet.width != 32 || sheet.height != 32 ||
        y < 0 || y >= 0xc7 || x < 0 || x >= 320) return 0;
    return graphics_blit(dest,size,320,200,sheet,frame,x,y);
}
