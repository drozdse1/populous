#include "../pop_log.h"
/*
 * modules/03_ui_save.c
 * --------------------
 * Unity-included from populous_prg.c (do not compile alone).
 * Original populous_prg.c lines 3188-4763.
 *
 * Pointer / HUD, options requesters, .GAM save/load, world / lord screens.
 * Prefer a4_state.h names over raw A4_WORD(0x…).
 */

/* ---- Pointer / HUD ----------------------------------------------------- */

static void core_set_pointer(void)
{
    int own = player == 1;
    pointer = (int16_t)(core_swamp_selected ? 5 : core_tool == 2 ? 0 : core_tool == 1 ? own+2 : own*3+1);
}

/* Hardware sprite: _convert_sprite asm:19823-19846 drops the mask word and
 * attaches planes 0-1/2-3, so any non-zero plane value is opaque colour
 * 16+value (_palette 16-31 repeat 0-15, asm:24264). _move_pointer
 * asm:19928-19972 with x+0x40 (asm:19551) gives HSTART mousex+0x80 and VSTART
 * mousey+0x2c: the hotspot is the top-left pixel. Drawn over the whole
 * 320x256 frame, HUD included. */
static void core_draw_pointer(void)
{
    /* The host cursor would double the drawn game pointer; show it only when none is drawn. */
    int drawn = game_assets && (uint16_t)pointer <= 7;
    if (SDL_ShowCursor(SDL_QUERY) != (drawn ? SDL_DISABLE : SDL_ENABLE))
        SDL_ShowCursor(drawn ? SDL_DISABLE : SDL_ENABLE);
    if (!drawn) return;
    GraphicsSheet small = graphics_sprites(game_assets);
    size_t base = (size_t)core_pointer_frames[pointer] * 256;
    if (base + 256 > small.size) return;
    for (int row = 0; row < 16; ++row) for (int column = 0; column < 16; ++column) {
        int x = mousex + column, y = mousey + row, colour = small.indices[base + row*16 + column] & 15;
        if (colour && x >= 0 && x < 320 && y >= 0 && y < 256)
            prg_screen->chunky_buffer[y*320+x] = prg_screen->palette[colour];
    }
}

/* ___text/_text asm:19973-20052 on the 320x200 game_pixels: x is byte
 * aligned, '\n' moves 8 lines down, each 8x8 glyph is 8 x {mask, plane0..3}
 * from char 0x20. Kept pixels: (old & mask) | planes, as the bitplane AND/OR.
 * Linear plane addressing is emulated: overflowing columns wrap like the
 * original, beyond 8000 bytes per plane is skipped. */
static void amiga_text(uint8_t *pixels, int x, int y, const char *text)
{
    if (!game_font_loaded || !text) return;
    long line = (long)y * 40 + (x >> 3), at = line;
    for (; *text; ++text) {
        unsigned c = (unsigned char)*text;
        if (c == '\n') { line += 320; at = line; continue; }
        if (c >= 0x20 && c - 0x20 < sizeof game_font / 40) {
            const uint8_t *glyph = game_font + (c - 0x20) * 40;
            for (int row = 0; row < 8; ++row, glyph += 5) {
                long byte = at + row * 40;
                if (byte < 0 || byte >= 8000) continue;
                uint8_t *out = pixels + (byte / 40) * 320 + (byte % 40) * 8;
                for (int bit = 0; bit < 8; ++bit) {
                    int b = 7 - bit, colour = 0;
                    for (int plane = 0; plane < 4; ++plane)
                        colour |= ((((out[bit] >> plane) & (glyph[0] >> b)) | (glyph[1+plane] >> b)) & 1) << plane;
                    out[bit] = (uint8_t)colour;
                }
            }
        }
        ++at;
    }
}

/* _icon_data icons 12..23 (asm:21763-21927), the _requester tiles: per row
 * the plane 0..3 words, 16 rows. Other icons are not exported. */
static const uint16_t core_shield_icons[12][64] = {
    {0xffbe,0xff7f,0x0080,0x0000,0xff7e,0xfebf,0x0140,0x0000,0xfefe,0xfdbf,0x0240,0x0000,0xfebe,0xfd67,0x0298,0x0000,0xfd66,0xfe9f,0x0160,0x0000,0xff9e,0xfe7f,0x0180,0x0000,0xfe7e,0xf9ff,0x0600,0x0000,0xf97e,0xe6ff,0x1900,0x0000,
     0xe77e,0xfeff,0x0100,0x0000,0xff7e,0xfeff,0x0100,0x0000,0xff7e,0xfeff,0x0100,0x0000,0xff7e,0xfeff,0x0100,0x0000,0xff7e,0xfeff,0x0100,0x0000,0xff7e,0xfeff,0x0100,0x0000,0xfffe,0xffff,0x0000,0x0000,0x0000,0xffff,0x0000,0x0000},
    {0xfffe,0xffff,0x0000,0x0000,0xfffe,0xffff,0x0000,0x0000,0xfffe,0xffff,0x0000,0x0000,0xffff,0xffff,0x8001,0x0000,0xfffe,0xce73,0x4db2,0x300c,0xfffe,0xf00f,0x17e8,0x0810,0xdf3a,0xe007,0x1ff8,0x0000,0xbef8,0xc003,0x3ffc,0x0000,
     0xb7e8,0xca53,0x318c,0x0000,0xdbd2,0xe007,0x1188,0x0420,0xe5a6,0xf19f,0x0a40,0x0000,0xe3e6,0xf40f,0x0bd0,0x0000,0xf42e,0xfbdf,0x0420,0x0000,0xfa1e,0xfdbf,0x0240,0x0000,0xfdbe,0xffff,0x0000,0x0000,0x0000,0xffff,0x0000,0x0000},
    {0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,
     0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x0000,0x7fff,0x7fff,0x8000},
    {0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8180,0x0000,0x0000,0xffff,0x83e0,0x0200,0x0200,0xfdff,0x9ff8,0x00c0,0x00c0,0xff3f,0xbf9f,0x0de0,0x0de0,0xf21f,0xbdbf,0x02e0,0x02e0,0xfd1f,0x9f3f,0x0e7f,0x0e7f,0xf100,
     0x9e1f,0x017f,0x017f,0xfe00,0x8f00,0x06bf,0x06bf,0xf900,0x8e00,0x01df,0x01df,0xfe00,0x87c0,0x0340,0x0340,0xfc80,0x8700,0x0000,0x0000,0xff3f,0x8000,0x0000,0x0000,0xfcff,0x8000,0x0000,0x0000,0xffff,0x0000,0x7fff,0x7fff,0x8000},
    {0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x82c0,0x03a0,0x0020,0xfc1f,0x8540,0x0590,0x0210,0xf80f,0x8240,0x0b90,0x0410,0xf00f,0x8b60,0x0a90,0x0410,0xf00f,
     0x8d60,0x0e90,0x0000,0xf00f,0x82c0,0x0d30,0x0810,0xf00f,0x8000,0x06e0,0x06a0,0xf81f,0x8000,0x0180,0x0180,0xfe7f,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x0000,0x7fff,0x7fff,0x8000},
    {0x8010,0x0008,0x0008,0xfff7,0x8038,0x0004,0x0004,0xfffb,0x807c,0x0012,0x0012,0xffec,0x807c,0x000e,0x000e,0xfff0,0x80f0,0x003e,0x003e,0xffc0,0x81e8,0x00be,0x00be,0xff40,0x81f0,0x007c,0x007c,0xff81,0x81a0,0x00f8,0x00f8,0xff03,
     0x8340,0x01e0,0x01e0,0xfe07,0x8780,0x01c0,0x01c0,0xfe0f,0x8700,0x0300,0x0300,0xfc3f,0x8e00,0x0600,0x0600,0xf8ff,0x8800,0x0400,0x0400,0xf9ff,0x9000,0x0800,0x0800,0xf3ff,0x8000,0x0000,0x0000,0xffff,0x0000,0x7fff,0x7fff,0x8000},
    {0x8003,0x0001,0x0001,0xfffe,0x8006,0x0003,0x0003,0xfffc,0x800c,0x0006,0x0006,0xfff8,0x8018,0x000c,0x000c,0xfff1,0x8030,0x0018,0x0018,0xffe3,0x8060,0x0030,0x0030,0xffc7,0x80c0,0x0060,0x0060,0xff8f,0x8180,0x00c0,0x00c0,0xff1f,
     0x8300,0x0180,0x0180,0xfe3f,0x8600,0x0300,0x0300,0xfc7f,0x8c00,0x0600,0x0600,0xf8ff,0x9800,0x0c00,0x0c00,0xf1ff,0xb000,0x3800,0x5800,0x83ff,0x5000,0x6000,0x8000,0x07ff,0x2000,0x4000,0x8000,0x0fff,0xc000,0x9fff,0x1fff,0x0000},
    {0x9100,0x0080,0x1c80,0xe37f,0x9660,0x07e0,0x1800,0xe01f,0x9598,0x0678,0x1800,0xe007,0x9180,0x0640,0x1800,0xe003,0x9100,0x0080,0x1c80,0xe13f,0x9100,0x0080,0x1080,0xe13f,0x8100,0x0080,0x0080,0xf73f,0x8100,0x0080,0x0080,0xff3f,
     0x8100,0x0080,0x0080,0xff3f,0x8100,0x0080,0x0080,0xff3f,0x8100,0x0080,0x0080,0xff3f,0x8100,0x0080,0x0080,0xff3f,0x8100,0x0080,0x0080,0xff3f,0x8100,0x0080,0x0080,0xff3f,0x8100,0x0080,0x0080,0xff3f,0x0000,0x7f3f,0x7f3f,0x8000},
    {0x0007,0x6000,0x8000,0x1fff,0x4039,0x4007,0x8006,0x1ff8,0x40c1,0x603b,0xe03a,0x0fc4,0xa302,0x30c2,0x30c0,0xc73d,0x9002,0x1b02,0x1b00,0xe0fd,0x8804,0x0c04,0x0c00,0xf1fb,0x9404,0x0e04,0x0e00,0xf0fb,0x9204,0x0b04,0x0b00,0xf47b,
     0xa168,0x1188,0x11e0,0xee77,0xa038,0x1048,0x10f0,0xefb7,0x8010,0x6030,0x20e0,0xdfcf,0x8030,0x6030,0x2040,0xdfcf,0x80c0,0x60c0,0x2000,0xdf3f,0x0700,0xc700,0x4000,0xb8ff,0x1800,0xf800,0x6000,0x87ff,0x6000,0xdfff,0x1fff,0x8000},
    {0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x8000,0x0000,0x0000,0xffff,0x81c0,0x0030,0x0000,0xfe0f,0x8220,0x0118,0x0100,0xfc07,0x8410,0x020c,0x0200,0xf9c3,0x8808,0x0406,0x0400,0xf3e1,0x8804,0x0403,0x0400,0xf3f0,
     0x8401,0x0000,0x0000,0xfbf8,0x8200,0x0000,0x0000,0xfdfc,0x8200,0x0100,0x0100,0xfcff,0x9400,0x4200,0x0200,0xa9ff,0x3800,0xc400,0x0400,0x03ff,0xe000,0x0800,0x0800,0x87ff,0xc000,0x0000,0x0000,0x07ff,0x4000,0x2fff,0x2fff,0x8000},
    {0x8000,0x0000,0x0000,0xffff,0x8200,0x0200,0x0000,0xfdff,0x8400,0x0500,0x0100,0xfaff,0x9400,0x0480,0x0080,0xfb7f,0xb800,0x1840,0x1040,0xe7bf,0x9800,0x0820,0x0020,0xf7df,0x9808,0x0818,0x0018,0xf7e7,0x9ff4,0x105c,0x401c,0xafe3,
     0x00a0,0x403f,0x80ff,0x00c0,0xd040,0x1fb4,0x0ff4,0xa043,0x9800,0x1818,0x1018,0xe7e7,0x9800,0x1820,0x1020,0xe7df,0xa800,0x3840,0x3040,0xc7bf,0x9400,0x1480,0x1080,0xeb7f,0x8400,0x0500,0x0100,0xfaff,0x0200,0x7fff,0x7dff,0x8000},
    {0x8001,0x0001,0x0002,0xfffc,0x8002,0x0002,0x0004,0xfff8,0x8006,0x0005,0x0009,0xfff0,0x8008,0x000c,0x0010,0xffe1,0x8010,0x0018,0x0020,0xffc3,0x8020,0x0030,0x0040,0xff87,0x8040,0x0060,0x0080,0xff0f,0x9080,0x00c0,0x0100,0xee1f,
     0xa100,0x0180,0x0200,0xcc3f,0xaa00,0x0300,0x0400,0xc07f,0x9400,0x0600,0x0000,0xe0ff,0x9a00,0x0000,0x0000,0xe1ff,0xa100,0x0000,0x0000,0xc07f,0xc000,0x0000,0x0000,0x84ff,0x8000,0x0000,0x0000,0x0fff,0x0000,0x1fff,0x1fff,0x0000},
};

static const uint16_t core_requester_icons[12][64] = {
    {0xffff,0x0000,0xffff,0x0000,0xc000,0x0000,0xffff,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdc2b,0x1000,0xefff,0x0000,0xdfff,0x17ff,0xe800,0x0000,0xdfd4,0x102b,0xe800,0x0000,0xdfff,0x13ff,0xe800,0x0000,0xde00,0x13ff,0xe800,0x0000,
     0xde00,0x13ff,0xe8ff,0x0000,0xde00,0x13ff,0xe8ff,0x0000,0xd200,0x17ff,0xe8ff,0x0000,0xde00,0x13ff,0xe8ff,0x0000,0xd200,0x17ff,0xe8ff,0x0000,0xde00,0x13ff,0xe8ff,0x0000,0xd200,0x17ff,0xe8ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000},
    {0xffff,0x0000,0xffff,0x0000,0x0000,0x0000,0xffff,0x0000,0xffff,0xffff,0x0000,0x0000,0xd42b,0x0000,0xffff,0x0000,0xffff,0xffff,0x0000,0x0000,0x2bd4,0xd42b,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,
     0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000},
    {0xfffd,0x0001,0xfffe,0x0000,0x0002,0x0003,0xfffc,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xd43b,0x001c,0xffe0,0x0000,0xfffb,0xffec,0x0000,0x0000,0x2bfb,0xd42c,0x0000,0x0000,0xfffb,0xffcc,0x0020,0x0000,0x00fb,0xffcc,0x0020,0x0000,
     0x007b,0xff4c,0xffa0,0x0000,0x007b,0xff4c,0xffa0,0x0000,0x004b,0xff5c,0xffa0,0x0000,0x007b,0xff4c,0xffa0,0x0000,0x004b,0xff5c,0xffa0,0x0000,0x007b,0xff4c,0xffa0,0x0000,0x004b,0xff5c,0xffa0,0x0000,0x007b,0xff7c,0xff80,0x0000},
    {0xdf00,0x1f00,0xe0ff,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,
     0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xdfff,0x1fff,0xe000,0x0000,0xa000,0x3fff,0xc000,0x0000,0xbfff,0xc000,0x0000,0x0000},
    {0x0000,0x0000,0xffff,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,
     0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
    {0x00fb,0x00fc,0xff00,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,
     0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0xfffb,0xfffc,0x0000,0x0000,0x0003,0xfffc,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
    {0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,
     0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000,0xde00,0x1fff,0xe0ff,0x0000},
    {0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,
     0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000,0x007b,0xff7c,0xff80,0x0000},
    {0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,
     0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000},
    {0x0000,0x0000,0xffff,0x0000,0xf800,0xffff,0x0000,0x0000,0xfbff,0xfdff,0x01ff,0x0200,0xfbff,0xfeff,0x02ff,0x0100,0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,
     0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,0xfb00,0xffff,0x03ff,0x0000,0xfa00,0xff00,0x0300,0x0000,0xf800,0xfe00,0x0200,0x0000,0xfc00,0xfc00,0x03ff,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0xffff,0xffff,0x0000,0xffff,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,
     0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0xffff,0xffff,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
    {0x0000,0x0000,0xffff,0x0000,0x003f,0xffff,0x0000,0x0000,0xff9f,0xffdf,0xffe0,0x0000,0xff1f,0xff9f,0xffa0,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,
     0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0xff1f,0xff20,0x0000,0x001f,0x001f,0x0020,0x0000,0x001f,0x001f,0x0020,0x0000,0x001f,0x001f,0xffe0,0x0000,0x0000,0xffff,0x0000,0x0000,0xffff,0x0000,0x0000,0x0000},
};

/* _draw_icon asm:19184-19210: x in words (16 px), y in lines, the 4 plane
 * words replace the screen words. */
static void core_draw_icon(int x, int y, int icon)
{
    if (icon < 0 || icon > 23) return;
    const uint16_t *data = icon < 12 ? core_shield_icons[icon] : core_requester_icons[icon-12];
    for (int row = 0; row < 16; ++row) for (int bit = 0; bit < 16; ++bit) {
        int px = x*16 + bit, py = y + row, colour = 0;
        for (int plane = 0; plane < 4; ++plane) colour |= ((data[row*4+plane] >> (15-bit)) & 1) << plane;
        if (px >= 0 && px < 320 && py >= 0 && py < 200) game_pixels[py*320+px] = (uint8_t)colour;
    }
}

/* Button/menu records as the original's 46-byte entries: word x, word y,
 * word x2 (buttons) or width (menus), text at +6. */
#define core_rec_w(r,o) (((A4Word *)((r)+(o)))->value)
#define core_rec_text(r) ((char *)(r)+6)
/* core_end_ok / core_end_cancel / *_entry live in a4_state.h */
typedef struct { int16_t x, y, w; const char *text; } CoreMenuEntry;
static void core_menu_install(uint8_t *table, const CoreMenuEntry *entries, int count)
{
    for (int i = 0; i < count; ++i, table += 46) {
        core_rec_w(table,0) = entries[i].x; core_rec_w(table,2) = entries[i].y; core_rec_w(table,4) = entries[i].w;
        memset(table+6, 0, 40); strcpy(core_rec_text(table), entries[i].text);
    }
}
/* _end_ok/_end_cancel asm:24349-24362. */
static void core_buttons_install(void)
{
    static const CoreMenuEntry buttons[] = {{0,0,0,"OK"},{0,0,0,"CANCEL"}};
    core_menu_install(core_end_ok, buttons, 1); core_menu_install(core_end_cancel, buttons+1, 1);
}

/* _requester asm:9901-10270: a 16 px tile frame at x/16 (title 12,13,14;
 * sides 18,20,19 every 16 lines; bottom 15,16,17), ok (and cancel) as
 * 21,22..,23 button tiles in the bottom row. The buttons get x, y (the
 * bottom row) and x2 = x + (strlen+4)*8; labels at x+16, y+3. The
 * original clears _tempo on every call; the port clears it when a
 * requester opens (core_requester_layout), and draws it every frame. */
static void core_requester(int x, int y, int w, int h, uint8_t *ok, uint8_t *cancel, int draw)
{
    int left = x >> 4, tiles = w >> 4, half = (int)strlen(core_rec_text(ok)) >> 1, row, column;
#define tile(c,r,i) do { if (draw) core_draw_icon(left+(c),(r),(i)); } while (0)
    tile(0,y,12);
    for (column = 1; column < tiles-1; ++column) tile(column,y,13);
    tile(column,y,14);
    for (row = y+16; row < y+h-16; row += 16) {
        tile(0,row,18);
        for (column = 1; column < tiles-1; ++column) tile(column,row,20);
        tile(column,row,19);
    }
    tile(0,row,15);
    column = 1; core_rec_w(ok,2) = (int16_t)row;
    if (cancel) {
        int length = (int)strlen(core_rec_text(cancel)) >> 1;
        core_rec_w(ok,0) = (int16_t)((left+column) << 4); core_rec_w(cancel,2) = core_rec_w(ok,2);
        tile(column,row,21); ++column;
        for (int i = 0; i < half; ++i, ++column) tile(column,row,22);
        tile(column,row,23); ++column;
        if (draw) amiga_text(game_pixels, core_rec_w(ok,0)+16, row+3, core_rec_text(ok));
        for (; column < tiles - length - 3; ++column) tile(column,row,16);
        core_rec_w(cancel,0) = (int16_t)((left+column) << 4);
        tile(column,row,21); ++column;
        for (int i = 0; i < length; ++i, ++column) tile(column,row,22);
        tile(column,row,23); ++column;
        core_rec_w(cancel,4) = (int16_t)(core_rec_w(cancel,0) + ((int)strlen(core_rec_text(cancel))+4)*8);
        if (draw) amiga_text(game_pixels, core_rec_w(cancel,0)+16, row+3, core_rec_text(cancel));
    } else {
        for (; column < (tiles>>1) - (half>>1) - 1; ++column) tile(column,row,16);
        core_rec_w(ok,0) = (int16_t)((left+column) << 4);
        tile(column,row,21); ++column;
        for (int i = 0; i < half; ++i, ++column) tile(column,row,22);
        tile(column,row,23); ++column;
        for (; column < tiles-1; ++column) tile(column,row,16);
    }
    core_rec_w(ok,4) = (int16_t)(core_rec_w(ok,0) + ((int)strlen(core_rec_text(ok))+4)*8);
    tile(column,row,17);
#undef tile
    if (draw) amiga_text(game_pixels, core_rec_w(ok,0)+16, row+3, core_rec_text(ok));
}

/* _check_cancel asm:12106-12153 at _mousex/_mousey: 1 inside ok, 2 inside
 * cancel (x..x2, y..y+16 inclusive), else 0. The port calls it for a left
 * press only, so the _left_button gate is the event itself. */
static int core_check_cancel(const uint8_t *ok, const uint8_t *cancel)
{
    if (ok && mousex >= core_rec_w(ok,0) && mousex <= core_rec_w(ok,4) &&
        mousey >= core_rec_w(ok,2) && mousey <= core_rec_w(ok,2)+16) return 1;
    if (cancel && mousex >= core_rec_w(cancel,0) && mousex <= core_rec_w(cancel,4) &&
        mousey >= core_rec_w(cancel,2) && mousey <= core_rec_w(cancel,2)+16) return 2;
    return 0;
}

/* Menu hit test of _options/_game_options/_set_options: x+dx..x+w+dx,
 * y+dy..y+dy+8 inclusive. */
static int core_menu_hit(const uint8_t *entry, int dx, int dy)
{
    return mousex >= core_rec_w(entry,0)+dx && mousex <= core_rec_w(entry,0)+core_rec_w(entry,4)+dx &&
           mousey >= core_rec_w(entry,2)+dy && mousey <= core_rec_w(entry,2)+dy+8;
}

/* _option_text asm:24363-24493 (A4 0x8522), _game_text asm:24494-24789
 * (0x87ee), _set_text asm:24845-24914 (0x8fc0). 0x80/0x81 are the font's
 * empty/ticked box, 0x84..0x86 the slider, 0x87 its marker. */
static const CoreMenuEntry core_option_template[14] = {
    {0x24,0x10,0x80,"OPTIONS FOR EVIL"},
    {0x20,0x20,0x08,"\x80 CAN MODIFY LAND"},     {0x20,0x29,0x08,"\x80 CAN ATTACK TOWNS"},
    {0x20,0x32,0x08,"\x80 CAN ATTACK LEADER"},   {0x20,0x3b,0x08,"\x80 CAN USE EARTHQUAKES"},
    {0x20,0x43,0x08,"\x80 CAN USE SWAMP"},       {0x20,0x4c,0x08,"\x80 CAN USE KNIGHT"},
    {0x20,0x55,0x08,"\x80 CAN USE VOLCANO"},     {0x20,0x5e,0x08,"\x80 CAN USE FLOOD"},
    {0x20,0x67,0x08,"\x80 CAN USE ARMAGEDDON"},
    {0x64,0x7a,0x20,"AGGRESSION"}, {0x48,0x83,0x68,"LOW\x84\x85\x85\x85\x85\x85\x85\x85\x85\x86HIGH"},
    {0x78,0x8d,0x20,"RATE"},       {0x40,0x96,0x70,"SLOW\x84\x85\x85\x85\x85\x85\x85\x85\x85\x86""FAST"},
};
static const CoreMenuEntry core_game_template[17] = {
    {0x70,0x10,0x48,"GAME SETUP"},
    {0x10,0x24,0x08,"\x80 ONE PLAYER"},     {0x98,0x24,0x08,"\x80 TWO PLAYERS"},
    {0x10,0x2e,0x08,"\x80 PLAY GAME"},      {0x98,0x2e,0x08,"\x80 PAINT MAP"},
    {0x10,0x38,0x08,"\x80 GOOD"},           {0x98,0x38,0x08,"\x80 EVIL"},
    {0x10,0x42,0x08,"\x80 HUMAN VS AMIGA"}, {0x98,0x42,0x08,"\x80 AMIGA VS AMIGA"},
    {0x10,0x4c,0x08,"\x80 CONQUEST"},       {0x98,0x4c,0x08,"\x80 CUSTOM GAME"},
    {0x50,0x5f,0x80,"\x80 GAME OPTIONS"},   {0x50,0x69,0x80,"\x80 SAVE A GAME"},
    {0x50,0x73,0x80,"\x80 LOAD A GAME"},    {0x50,0x7d,0x80,"\x80 MOVE TO NEXT MAP"},
    {0x50,0x87,0x80,"\x80 RESTART THIS MAP"}, {0x50,0x91,0x80,"\x80 SURRENDER THIS GAME"},
};
static const CoreMenuEntry core_set_template[11] = {
    {0x30,0x19,0x76,"SET GAME OPTIONS"},
    {0x10,0x28,0x08,"\x80 WATER IS HARMFUL"}, {0x10,0x28,0x08,"\x81 WATER IS FATAL"},
    {0x10,0x32,0x08,"\x80 SWAMPS SHALLOW"},   {0x10,0x32,0x08,"\x81 SWAMPS BOTTOMLESS"},
    {0x10,0x3c,0x08,"\x80 CAN BUILD"},        {0x10,0x3c,0x08,"\x81 CANNOT BUILD"},
    {0x10,0x46,0x08,"\x80 BUILD UP AND DOWN"},{0x10,0x46,0x08,"\x81 ONLY BUILD UP"},
    {0x10,0x50,0x08,"\x80 BUILD NEAR PEOPLE"},{0x10,0x50,0x08,"\x81 BUILD NEAR TOWNS"},
};
/* _blank asm:24915-24923: 28 '^' glyphs. */
static const char core_blank[] = "^^^^^^^^^^^^^^^^^^^^^^^^^^^^";

static uint8_t *core_stats(int side) { return stats_bytes(side); }

/* _options asm:13617-13660 window, then each pass of LAB_48228 (asm:13661-13740):
 * entry 0 names the side, 1..9 tick the _stats+0xe power bits. */
static void core_options_refresh(void)
{
    strcpy(core_rec_text(core_option_entry(0)) + 12, core_req.side ? "EVIL" : "GOOD");
    for (int i = 1; i < 10; ++i)
        core_rec_text(core_option_entry(i))[0] = (char)(0x80 + !!(core_rec_w(core_stats(core_req.side),0xe) & (1 << (i-1))));
}
static void core_requester_layout(void)
{
    if (core_req.kind == CORE_REQ_OPTIONS) core_requester(0x10,0x10,0x120,0xa6,core_end_ok,core_end_cancel,0);
    else if (core_req.kind == CORE_REQ_GAME) core_requester(0x10,0x10,0x120,0xb0,core_end_ok,core_end_cancel,0);
    else if (core_req.kind == CORE_REQ_SET) core_requester(0x20,0x18,0x100,0x70,core_end_ok,core_end_cancel,0);
    else if (core_req.kind == CORE_REQ_WORLD) core_world_layout(0);
    else if (core_req.kind == CORE_REQ_FILE) core_file_layout(0);
    if (core_req.kind) tempo = 0;
}
/* _options(side) asm:13617-13660: keep a copy of _stats[side], show the
 * aggression (+0xc) and rate (+0x10) as 10-value. */
static void core_options_open(int side)
{
    uint8_t *own = core_stats(side);
    core_req.kind = CORE_REQ_OPTIONS; core_req.side = side;
    memcpy(core_req.saved_stats, own, 46);
    core_buttons_install(); core_menu_install(core_option_entry(0), core_option_template, 14);
    core_requester_layout();
    core_rec_w(own,0xc) = (int16_t)(10 - core_rec_w(own,0xc));
    core_rec_w(own,0x10) = (int16_t)(10 - core_rec_w(own,0x10));
    core_options_refresh();
}
/* _options click, asm:13741-13873: only a custom game (in_conquest -1)
 * edits; entry 1..9 toggles bit i-1, the sliders take (mousex-(x+0x28 or
 * 0x30))/8 if 0..9. Ok re-inverts, cancel restores the copy. The copy's
 * first 3 bytes are the _do_action mailbox, which the original clears after
 * the return (asm:18189-18215); the port cleared it already, so they stay. */
static void core_options_click(void)
{
    uint8_t *own = core_stats(core_req.side);
    int i = 1;
    while (i < 14 && !core_menu_hit(core_option_entry(i),16,16)) ++i;
    if (in_conquest == -1) {
        if (i < 10) core_rec_w(own,0xe) ^= (int16_t)(1 << (i-1));
        else if (i == 11 || i == 13) {
            int value = (int16_t)(mousex - (core_rec_w(core_option_entry(i),0) + (i == 11 ? 0x28 : 0x30))) / 8;
            if (value >= 0 && value <= 9) core_rec_w(own, i == 11 ? 0xc : 0x10) = (int16_t)value;
        }
    }
    int result = core_check_cancel(core_end_ok, core_end_cancel);
    if (!result) { core_options_refresh(); return; }
    if (result == 2) memcpy(own+3, core_req.saved_stats+3, 43);
    else {
        core_rec_w(own,0xc) = (int16_t)(10 - core_rec_w(own,0xc));
        core_rec_w(own,0x10) = (int16_t)(10 - core_rec_w(own,0x10));
    }
    core_req.kind = CORE_REQ_NONE; left_button = 2;  /* _return_to_game asm:13874-13885 */
}

/* _game_options asm:12801-12877: the saved values and the ticks. */
static void core_game_options_open(void)
{
    core_req.kind = CORE_REQ_GAME;
    core_req.saved_serial = serial_off; core_req.saved_paint = paint_map; core_req.saved_player = player;
    core_req.saved_human = core_rec_w(core_stats(player),6); core_req.saved_conquest = in_conquest;
    bring_up_two = 0;
    core_buttons_install(); core_menu_install(core_game_entry(0), core_game_template, 17);
    static const int ticks[] = {1,2,3,4,5,6,7,8,9,10};
    int16_t human = core_rec_w(core_stats(player),6);
    int16_t values[] = {serial_off, !serial_off, !paint_map, paint_map, !player, player, !human, human,
                        in_conquest != -1, in_conquest == -1};
    for (int k = 0; k < 10; ++k) core_rec_text(core_game_entry(ticks[k]))[0] = (char)(0x80 + values[k]);
    saved_flag = 0;
    core_requester_layout();
}
static void core_game_tick(int on, int off)
{
    core_rec_text(core_game_entry(on))[0] = (char)0x81; core_rec_text(core_game_entry(off))[0] = (char)0x80;
}
/* GOOD/EVIL asm:12974-13113: swap the _stats+6 human flags into the new
 * side; _set_tend_icons/_set_mode_icons and _pointer follow from state. */
static void core_game_side(int side)
{
    int16_t other = core_rec_w(core_stats(not_player),6);
    core_rec_w(core_stats(side),6) = core_rec_w(core_stats(player),6);
    core_rec_w(core_stats(!side),6) = other;
    player = (int16_t)side; not_player = player ? 0 : 1;
    core_set_pointer();
}
static void core_set_options_open(void);
/* _do_action's _new_map (asm:18917-18932) with in_conquest -1: 1 (MOVE
 * TO NEXT MAP, CONQUEST's number) keeps the live _seed, anything else
 * (RESTART, ground_in+2) reloads _start_seed; _setup_display(0, new_map-2). */
static int core_custom_new_map(int16_t new_map)
{
    return core_custom_start(new_map == 1 ? seed : start_seed, (int16_t)(new_map - 2));
}
/* Leaving _game_options: _return_to_game, then _do_action's _new_map
 * (asm:18917-18932): _setup_display restarts the map. A conquest start
 * restarts the current world (both 1 and ground_in+2 rebuild the same
 * conquest level); with in_conquest -1 it builds a custom landscape. */
static void core_game_options_close(void)
{
    core_req.kind = CORE_REQ_NONE; left_button = 2;
    int16_t new_map = options_new_map;
    if (new_map && !(in_conquest == -1 ? core_custom_new_map(new_map) : game_reset_start(current_start)))
        POP_LOG_ERROR("Game options: map restart failed; keeping the current game.");
}
/* _game_options click, asm:12878-13350. In a conquest game entries 0..8
 * are ignored. Inert here: TWO PLAYERS (_two_players, serial); SAVE A GAME / LOAD A GAME open
 * _save_load (core_file_open), LOAD only with _serial_off; ONE PLAYER only
 * ticks, as _serial_off is always 1.
 * CONQUEST (asm:13221-13239) opens _show_world with an empty _message. */
static void core_game_options_click(void)
{
    int i = 0;
    while (i < 17 && !core_menu_hit(core_game_entry(i),16,16)) ++i;
    if (i < 9 && in_conquest != -1) i = 0x13;
    switch (i) {
    case 1:
        core_game_tick(1,2);
        if (!serial_off) {
            core_rec_w(core_stats(player),6) = 1;  /* CLR then MOVE #1, asm:12930-12935 */
            core_game_tick(7,8); serial_off = 1;
        }
        break;
    case 3: core_game_tick(3,4); paint_map = 0; break;
    case 4: core_game_tick(4,3); paint_map = 1; break;
    case 5: core_game_tick(5,6); core_game_side(0); break;
    case 6: core_game_tick(6,5); core_game_side(1); break;
    case 7:
        core_game_tick(7,8); core_rec_w(core_stats(player),6) = 0;
        if (serial_off) core_rec_w(core_stats(!player),6) = 1;
        break;
    case 8:
        core_game_tick(8,7); core_rec_w(core_stats(player),6) = 1;
        if (serial_off) core_rec_w(core_stats(!player),6) = 1;
        break;
    case 9: core_game_tick(9,10); core_message[0] = 0; core_world_open(CORE_WORLD_OPTIONS); return;
    case 10: core_game_tick(10,9); in_conquest = -1; break;
    case 11: core_set_options_open(); return;
    case 12: core_file_open(1); return;                     /* asm:13259-13266 */
    case 13: if (serial_off) { core_file_open(0); return; } break;   /* asm:13267-13276 */
    case 14: options_new_map = 1; core_game_options_close(); return;
    case 15: options_new_map = (int16_t)(ground_in + 2); core_game_options_close(); return;
    case 16: surrender_side = player; core_game_options_close(); return;
    default: break;
    }
    int result = core_check_cancel(core_end_ok, core_end_cancel);
    if (!result) return;
    if (result == 2) {
        /* asm:13325-13350 */
        serial_off = core_req.saved_serial; paint_map = core_req.saved_paint;
        if (player != core_req.saved_player) player = core_req.saved_player;
        not_player = player ? 0 : 1;
        core_rec_w(core_stats(player),6) = core_req.saved_human;
        in_conquest = core_req.saved_conquest;
        core_set_pointer();
    }
    core_game_options_close();
}

/* _set_options asm:13362-13411. */
static void core_set_options_open(void)
{
    core_req.kind = CORE_REQ_SET; core_req.saved_mode = game_mode;
    core_menu_install(core_set_entry(0), core_set_template, 11);
    core_requester_layout();
}
/* _set_options click, asm:13412-13509: custom games only; entries 1,3,..9
 * toggle _game_mode bit i/2. Then back to _game_options (-4 = 1 redraws). */
static void core_set_options_click(void)
{
    if (in_conquest == -1)
        for (int i = 1; i < 10; i += 2)
            if (core_menu_hit(core_set_entry(i),0x20,0x18)) { game_mode ^= (int16_t)(1 << (i >> 1)); break; }
    int result = core_check_cancel(core_end_ok, core_end_cancel);
    if (!result) return;
    if (result == 2) game_mode = core_req.saved_mode;
    left_button = 2; core_req.kind = CORE_REQ_GAME; core_requester_layout();
}

/* ---- SAVE A GAME / LOAD A GAME: _save_game asm:17123-17385, _checksum
 * asm:17389-17407, _load_game asm:17412-17703.
 * The file is the raw A4 regions in _Write order, big-endian as the 68000
 * wrote them. The port's A4 holds host-endian words/longs, so each region
 * is converted per field; peep pointers (peep +14, _stats +$22/$26/$2a,
 * here slot+1) are written as Amiga _peeps addresses $53014+22*slot. */
#define CORE_SAVE_SIGNATURE 0xffffb615u   /* asm:17125, 17453 */
#define CORE_SAVE_PEEPS     0x53014u      /* _peeps (LAB_53018-4) in the IRA image */
enum { CORE_SAVE_BYTES, CORE_SAVE_WORDS, CORE_SAVE_LONG, CORE_SAVE_MAGNET, CORE_SAVE_PEEPLIST,
       CORE_SAVE_STATS, CORE_SAVE_CONQUEST };
static const struct { int offset, size, kind; } core_save_regions[] = {
    {0xae86,0x2102,CORE_SAVE_WORDS},     /* _alt, 65x65 words */
    {0xdf88,0x1000,CORE_SAVE_BYTES},     /* _map_alt */
    {0xcf88,0x1000,CORE_SAVE_BYTES},     /* _map_blk */
    {0xef88,0x1000,CORE_SAVE_BYTES},     /* _map_bk2 */
    {0x1f88,0x1000,CORE_SAVE_BYTES},     /* _map_who */
    {0x99f6,0x20,CORE_SAVE_MAGNET},      /* _magnet[2]: 4 words, good_pop/bad_pop, mana */
    {0x9c26,0x1222,CORE_SAVE_PEEPLIST},  /* _peeps, 211 22-byte records */
    {0x82b6,0x2e,CORE_SAVE_STATS},       /* _stats */
    {0x82e4,0x2e,CORE_SAVE_STATS},       /* strABC, the second _stats record */
    {0x8256,2,CORE_SAVE_WORDS},          /* _mode */
    {0x99e2,2,CORE_SAVE_WORDS},          /* _seed */
    {0x99e4,2,CORE_SAVE_WORDS},          /* _start_seed */
    {0x99f4,2,CORE_SAVE_WORDS},          /* _player */
    {0x99e6,2,CORE_SAVE_WORDS},          /* _ground_in */
    {0xae80,2,CORE_SAVE_WORDS},          /* _war */
    {0xae6a,2,CORE_SAVE_WORDS},          /* _game_turn */
    {0x84bc,10,CORE_SAVE_CONQUEST},      /* _conquest: 8 bytes, seed word */
    {0x84ba,2,CORE_SAVE_WORDS},          /* _in_conquest */
    {0x9c24,2,CORE_SAVE_WORDS},          /* _paint_map */
    {0xae7a,4,CORE_SAVE_WORDS},          /* _battle_won[2] */
    {0x84b8,2,CORE_SAVE_WORDS},          /* _game_mode */
    {0xae7e,2,CORE_SAVE_WORDS},          /* _level_number */
    {0xae72,4,CORE_SAVE_LONG},           /* _score */
};
/* 4 + regions + the checksum word: 29646 bytes. */
/* ---- .GAM save / load ------------------------------------------------- */

#define CORE_SAVE_SIZE (4 + 0x2102 + 4*0x1000 + 0x20 + 0x1222 + 2*0x2e + 7*2 + 10 + 2 + 2 + 4 + 2 + 2 + 4 + 2)
#define CORE_SAVE_MAGNET_AT (4 + 0x2102 + 4*0x1000)
#define CORE_SAVE_SCALARS_AT (CORE_SAVE_MAGNET_AT + 0x20 + 0x1222 + 2*0x2e)   /* _mode */

/* Field width at byte "at" of a region; *is_pointer marks a peep pointer. */
static int core_save_unit(int kind, int at, int *is_pointer)
{
    *is_pointer = 0;
    switch (kind) {
    case CORE_SAVE_WORDS: return 2;
    case CORE_SAVE_LONG: return 4;
    case CORE_SAVE_MAGNET: return at % 16 < 8 ? 2 : 4;
    case CORE_SAVE_PEEPLIST:
        at %= 22;   /* bytes 0-3, words 4-12, pointer 14, word 18, bytes 20-21 */
        if (at == 14) { *is_pointer = 1; return 4; }
        return at < 4 || at >= 20 ? 1 : 2;
    case CORE_SAVE_STATS:   /* bytes 0-3, words 4-$20, pointers $22/$26/$2a */
        if (at >= 0x22) { *is_pointer = 1; return 4; }
        return at < 4 ? 1 : 2;
    case CORE_SAVE_CONQUEST: return at < 8 ? 1 : 2;
    default: return 1;
    }
}
static void core_put_be(uint8_t *out, uint32_t value, int size)
{
    for (int i = size-1; i >= 0; --i, value >>= 8) out[i] = (uint8_t)value;
}
static uint32_t core_get_be(const uint8_t *in, int size)
{
    uint32_t value = 0;
    for (int i = 0; i < size; ++i) value = value << 8 | in[i];
    return value;
}
/* _checksum(_alt,$1081) + 4x _checksum(map,$800) + the low word of
 * good_pop+bad_pop (asm:17161-17189), over the big-endian file image:
 * _alt and the four maps are contiguous from offset 4. */
static uint16_t core_save_checksum(const uint8_t *file)
{
    uint16_t sum = 0;
    for (int at = 4; at < CORE_SAVE_MAGNET_AT; at += 2) sum = (uint16_t)(sum + core_get_be(file+at,2));
    return (uint16_t)(sum + core_get_be(file+CORE_SAVE_MAGNET_AT+8,4) + core_get_be(file+CORE_SAVE_MAGNET_AT+0x18,4));
}
/* The live A4 as the file image, checksum included. */
static uint16_t core_save_encode(uint8_t *file)
{
    int at = 4;
    core_put_be(file, CORE_SAVE_SIGNATURE, 4);
    for (size_t r = 0; r < sizeof core_save_regions / sizeof *core_save_regions; ++r) {
        const uint8_t *base = A4_DISP(core_save_regions[r].offset);
        for (int k = 0, size, is_pointer; k < core_save_regions[r].size; k += size, at += size) {
            size = core_save_unit(core_save_regions[r].kind, k, &is_pointer);
            uint32_t value = size == 1 ? base[k] : size == 2 ? (uint16_t)((const A4Word *)(base+k))->value
                                                             : ((const A4Long *)(base+k))->value;
            if (is_pointer) value = value && value <= 211 ? CORE_SAVE_PEEPS + 22*(value-1) : 0;
            core_put_be(file+at, value, size);
        }
    }
    uint16_t sum = core_save_checksum(file);
    core_put_be(file+at, sum, 2);
    return sum;
}
/* The file image into the (staged) A4. An address that is not a _peeps
 * record becomes 0, in a peep the peep itself so _get_heading still runs. */
static void core_save_decode(const uint8_t *file)
{
    int at = 4;
    for (size_t r = 0; r < sizeof core_save_regions / sizeof *core_save_regions; ++r) {
        uint8_t *base = A4_DISP(core_save_regions[r].offset);
        int kind = core_save_regions[r].kind;
        for (int k = 0, size, is_pointer; k < core_save_regions[r].size; k += size, at += size) {
            size = core_save_unit(kind, k, &is_pointer);
            uint32_t value = core_get_be(file+at, size);
            if (is_pointer && value) {
                uint32_t slot = (value - CORE_SAVE_PEEPS) / 22;
                if (value >= CORE_SAVE_PEEPS && (value - CORE_SAVE_PEEPS) % 22 == 0 && slot < 208) value = slot+1;
                else value = kind == CORE_SAVE_PEEPLIST && k/22 < 208 ? (uint32_t)(k/22+1) : 0;
            }
            if (size == 1) base[k] = (uint8_t)value;
            else if (size == 2) ((A4Word *)(base+k))->value = (int16_t)value;
            else ((A4Long *)(base+k))->value = value;
        }
    }
}

/* Port: save files live under POPULOUS_SAVE_DIR, else $XDG_DATA_HOME/populous/saves,
 * else ~/.local/share/populous/saves, else ./saves. The original's paths
 * were relative to the current AmigaDOS directory; here a relative drawer or
 * name is relative to that base, an absolute one ('/') is used as it is. */
static int core_save_base(char *out, size_t size)
{
    const char *env = getenv("POPULOUS_SAVE_DIR"), *home = getenv("HOME");
    int length;
    if (env && *env) length = snprintf(out, size, "%s", env);
    else if ((env = getenv("XDG_DATA_HOME")) && *env) length = snprintf(out, size, "%s/populous/saves", env);
    else if (home && *home) length = snprintf(out, size, "%s/.local/share/populous/saves", home);
    else length = snprintf(out, size, "saves");
    return length > 0 && (size_t)length < size;
}
static int core_save_path(char *out, size_t size, const char *name)
{
    char base[4096];
    if (name[0] == '/') return (size_t)snprintf(out, size, "%s", name) < size;
    if (!core_save_base(base, sizeof base)) return 0;
    return (size_t)snprintf(out, size, "%s/%s", base, name) < size;
}
static void core_save_mkdirs(void)
{
    char path[4096];
    if (!core_save_base(path, sizeof path)) return;
    for (char *slash = path + 1; ; ++slash) {
        if (*slash && *slash != '/') continue;
        char end = *slash; *slash = 0;
        mkdir(path, 0777);   /* existing components fail with EEXIST */
        if (!end) break;
        *slash = end;
    }
}
/* _save_game/_load_game name: ".GAM" unless it already ends in it
 * (asm:17128-17154, 17414-17440). */
static int core_save_file_name(char *out, size_t size, const char *name)
{
    size_t length = strlen(name);
    char full[4096];
    if ((size_t)snprintf(full, sizeof full, "%s%s", name,
                         length >= 4 && !strcmp(name+length-4, ".GAM") ? "" : ".GAM") >= sizeof full) return 0;
    return core_save_path(out, size, full);
}

/* _save_game: 1 on success (the original returns 0 then). Port: written to
 * a temporary file and renamed, so a failed save leaves no partial file. */
static int core_save_game(const char *name)
{
    char path[4160], temporary[4200];
    static uint8_t file[CORE_SAVE_SIZE];
    if (!core_save_file_name(path, sizeof path, name)) return 0;
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    core_save_mkdirs();
    FILE *out = fopen(temporary, "wb");
    if (!out) return 0;
    land_checksum = (int16_t)core_save_encode(file);   /* _land_checksum, asm:17189 */
    int ok = fwrite(file, 1, sizeof file, out) == sizeof file;
    if (fclose(out)) ok = 0;
    if (ok && rename(temporary, path)) ok = 0;
    if (!ok) remove(temporary);
    return ok;
}

/* _load_game: 1 if the game was loaded. The original only checks the
 * signature and then reads field by field. Port: the whole file is read
 * and checked first (exact size, signature, checksum, _ground_in 0..3,
 * _player 0..1, the LAND rules and graphics of _ground_in), then staged and
 * published at once, so a rejected file leaves the game untouched. */
static int core_load_game(const char *name)
{
    char path[4160], error[160];
    static uint8_t file[CORE_SAVE_SIZE + 1];
    if (!core_save_file_name(path, sizeof path, name)) return 0;
    FILE *in = fopen(path, "rb");
    if (!in) return 0;
    size_t count = fread(file, 1, sizeof file, in);
    fclose(in);
    if (count != CORE_SAVE_SIZE || core_get_be(file,4) != CORE_SAVE_SIGNATURE ||
        core_get_be(file+CORE_SAVE_SIZE-2,2) != core_save_checksum(file)) {
        POP_LOG_ERROR("Load game: %s is not a saved game.", path);
        return 0;
    }
    uint32_t loaded_player = core_get_be(file+CORE_SAVE_SCALARS_AT+6,2);
    uint32_t terrain = core_get_be(file+CORE_SAVE_SCALARS_AT+8,2);
    if (loaded_player > 1 || terrain > 3) {
        POP_LOG_ERROR("Load game: %s has an unsupported player or landscape.", path);
        return 0;
    }
    /* ___load_ground(_ground_in,0), asm:17586-17589. */
    const char *asset_directory = getenv("POPULOUS_ASSET_DIR");
    if (!asset_directory) asset_directory = POPULOUS_DEFAULT_ASSET_DIR;
    char default_data[4096];
    if ((size_t)snprintf(default_data, sizeof default_data, "%s/raw", asset_directory) >= sizeof default_data) return 0;
    const char *directory = getenv("POPULOUS_DATA_DIR");
    if (!directory) directory = default_data;
    LandRules rules;
    if (!land_rules_load(directory, terrain, &rules, error, sizeof error)) {
        POP_LOG_ERROR("Load game: LAND data: %s", error);
        return 0;
    }
    GraphicsAssets *assets = graphics_load(asset_directory, terrain, rules.sprite_set, error, sizeof error);
    if (!assets) { POP_LOG_ERROR("Load game: graphics: %s", error); return 0; }
    _Alignas(4) static uint8_t staged[sizeof prg_a4_area];
    memcpy(staged, prg_a4_area, sizeof staged);
    uint8_t *live = prg_a4;
    prg_a4 = staged + 32768;
    core_save_decode(file);
    land_checksum = (int16_t)core_get_be(file+CORE_SAVE_SIZE-2,2);   /* _land_checksum */
    core_install_land(&rules);
    /* asm:17590-17682 */
    not_player = player ? 0 : 1;
    god_magnet = core_target(0); devil_magnet = core_target(1);   /* _god_magnet/_devil_magnet */
    int last = 207;
    while (last >= 0 && !peep_word(last,4)) --last;
    no_peeps = (int16_t)(last + 1);
    for (int owner = 0; owner < 2; ++owner) memset(stats_bytes(owner), 0, 3);
    for (int person = 0; person < no_peeps; ++person) {
        uint32_t raw; memcpy(&raw, peeps+person*22+14, sizeof raw);
        if (raw) core_get_heading(person);
    }
    prg_a4 = live;
    memcpy(prg_a4_area, staged, sizeof prg_a4_area);
    graphics_free(game_assets);
    game_assets = assets;
    /* _mode 2 sets the terrain _pointer (asm:17671-17682); the port's
     * pointer follows its tool, reset to terrain. */
    core_result = core_tool = core_swamp_selected = 0; core_set_pointer();
    core_end_shown = 0; core_end_next = -1;
    core_custom_live = 1;
    /* RESTART THIS MAP rebuilds the loaded world (port bookkeeping; the
     * synthetic demo starts have no original level to point at). */
    if (in_conquest == -1) {
        current_start = (StartSelection){START_CUSTOM, 0};
        core_land = (CustomLand){start_seed, 0, ground_in};
    } else if (current_start.kind != START_SYNTHETIC && in_conquest >= 0 && in_conquest / 5 < LEVEL_COUNT)
        current_start = (StartSelection){START_ORIGINAL, in_conquest / 5};
    return 1;
}

/* _do_action y=13 asm:18983-18999: _ground_in = x, ___load_ground(x,1),
 * redraw. The port has LAND rules and graphics only for 0..3 (keys 1..4);
 * 4..8 ("custom", keys 5..9) keep the current landscape. */
static int core_load_ground(int terrain)
{
    if (terrain < 0 || terrain > 3) {
        POP_LOG_WARN("Paint map: landscape %d is not available.", terrain);
        return 0;
    }
    char error[256];
    const char *asset_directory = getenv("POPULOUS_ASSET_DIR");
    if (!asset_directory) asset_directory = POPULOUS_DEFAULT_ASSET_DIR;
    char default_data[4096];
    if ((size_t)snprintf(default_data, sizeof default_data, "%s/raw", asset_directory) >= sizeof default_data) return 0;
    const char *directory = getenv("POPULOUS_DATA_DIR");
    if (!directory) directory = default_data;
    LandRules rules;
    if (!land_rules_load(directory, (unsigned)terrain, &rules, error, sizeof error)) {
        POP_LOG_WARN("Paint map: LAND data: %s", error);
        return 0;
    }
    GraphicsAssets *assets = graphics_load(asset_directory, (unsigned)terrain, rules.sprite_set, error, sizeof error);
    if (!assets) { POP_LOG_WARN("Paint map: graphics: %s", error); return 0; }
    ground_in = (int16_t)terrain;
    core_install_land(&rules);
    graphics_free(game_assets);
    game_assets = assets;
    return 1;
}

/* _save_load(save) asm:10493-10972: _requester(d_screen,0x48,0x10,0xb0,0xb0)
 * with "SAVE"/"LOAD" and CANCEL, 12 rows of the _drawer listing at
 * 0x58,0x22+8*row, arrows 0x88/0x89 at 0xd8, then _drawer (0x8d) and
 * _filename (0xa0), each 16 boxes. _drawer/_filename (20 bytes each) persist
 * in the A4. The original loops until OK or CANCEL; here it is a requester. */
/* core_drawer / core_filename live in a4_state.h */
static char *core_file_field(void) { return core_file.field == 1 ? core_drawer : core_filename; }
static int core_file_compare(const void *a, const void *b) { return strcmp(a, b); }
/* _get_a_dir asm:16841-16915 + _sort asm:11016-11079: files only, names
 * upper-cased and cut at 29 characters; those ending ".GAM" without it. */
static void core_file_directory(void)
{
    char path[4160];
    core_file.count = core_file.top = 0;
    if (!core_save_path(path, sizeof path, core_drawer[0] ? core_drawer : ".")) return;
    DIR *directory = opendir(path);
    if (!directory) return;
    for (struct dirent *entry; core_file.count < CORE_FILE_MAX && (entry = readdir(directory));) {
        char full[8320]; struct stat info;
        snprintf(full, sizeof full, "%s/%s", path, entry->d_name);
        if (stat(full, &info) || S_ISDIR(info.st_mode)) continue;
        char *name = core_file.names[core_file.count];
        size_t length = 0;
        for (; length < 29 && entry->d_name[length]; ++length) {
            unsigned char c = (unsigned char)entry->d_name[length];
            name[length] = (char)(c >= 'a' && c <= 'z' ? c - 0x20 : c);
        }
        name[length] = 0;
        if (length < 4 || strcmp(name+length-4, ".GAM")) continue;
        name[length-4] = 0;
        ++core_file.count;
    }
    closedir(directory);
    qsort(core_file.names, (size_t)core_file.count, sizeof *core_file.names, core_file_compare);
}
static void core_file_layout(int draw)
{
    static const CoreMenuEntry buttons[] = {{0,0,0,"SAVE"},{0,0,0,"LOAD"}};
    core_menu_install(core_file_ok, buttons + !core_file.save, 1);
    core_requester(0x48,0x10,0xb0,0xb0,core_file_ok,core_end_cancel,draw);
}
static void core_file_open(int save)
{
    memset(&core_file, 0, sizeof core_file);
    core_file.save = save;
    core_req.kind = CORE_REQ_FILE; left_button = 2;
    core_file_directory();
    core_requester_layout();
}
/* _get_name(d_screen,field,0x58,y,16) entry as core_world_entry. */
static void core_file_entry(int field)
{
    core_file.field = field; core_file.blink = 0;
    char *text = core_file_field();
    core_file.length = (int)strnlen(text, 16);
    int end = 0;
    for (int i = 0; i < 16; ++i) {
        if (!text[i]) end = 1;
        if ((uint8_t)text[i] <= 0x20 || end) text[i] = (char)0x82;
    }
    text[16] = 0; left_button = 2;
}
/* LAB_45ABA: drop the trailing boxes; a new _drawer reloads (-144 = 3). */
static void core_file_entry_done(void)
{
    char *text = core_file_field();
    text[core_file.length] = (char)0x82;
    for (int i = 15; i >= 0 && (uint8_t)text[i] > 0x80; --i) text[i] = 0;
    if (core_file.field == 1) core_file_directory();
    core_file.field = 0;
}
/* _get_name keys as core_world_key. */
static void core_file_key(int key)
{
    if (!core_file.field) return;
    char *text = core_file_field();
    if (key >= 0x20 && key <= 0x7a) {
        if (core_file.length >= 15) return;
        text[core_file.length++] = (char)(key >= 'a' ? key - 0x20 : key);
    } else if ((key == 8 || key == 0x7f) && core_file.length) {
        text[core_file.length--] = (char)0x82;
        text[core_file.length] = (char)0x82;
    } else if (key == 13) core_file_entry_done();
}
/* _save_load returning 0: _game_options redraws (-4 = 1). */
static void core_file_back(void)
{
    core_req.kind = CORE_REQ_GAME; left_button = 2; core_requester_layout();
}
static void core_game_options_close(void);
/* The click loop LAB_45EAC..LAB_4615C (asm:10760-10972). */
static void core_file_click(void)
{
    int result;
    if (core_file.error) { core_file.error = 0; left_button = 2; return; }   /* asm:10907-10928 */
    if (core_file.field) {
        /* _get_name's _check_cancel (asm:10432-10446): OK ends the entry,
         * CANCEL empties the field; both then act on the requester. */
        if (!(result = core_check_cancel(core_file_ok, core_end_cancel))) return;
        if (result == 2) { core_file_field()[0] = 0; core_file.field = 0; }
        else core_file_entry_done();
    } else {
        if (mousex >= 0x58 && mousex <= 0xd8) {
            if (mousey >= 0x22 && mousey <= 0x82) {
                int index = ((mousey - 0x22) >> 3) + core_file.top;
                if (index < core_file.count) snprintf(core_filename, 20, "%s", core_file.names[index]);
            } else if (mousey >= 0x8d && mousey <= 0x95) { core_file_entry(1); return; }
            else if (mousey >= 0xa0 && mousey <= 0xa8) { core_file_entry(2); return; }
        } else if (mousex >= 0xd8 && mousex <= 0xe0) {
            if (mousey >= 0x22 && mousey <= 0x2a && core_file.top) --core_file.top;
            else if (mousey >= 0x7a && mousey <= 0x82 && core_file.top + 5 < core_file.count) ++core_file.top;
        }
        result = core_check_cancel(core_file_ok, core_end_cancel);
    }
    left_button = 2;
    if (!result) return;
    if (result == 2 || !core_filename[0]) { core_file_back(); return; }
    /* _drawer, '/' unless it ends in ':' or '/' (or is one character), _filename. */
    char name[48];
    snprintf(name, sizeof name, "%.19s", core_drawer);
    size_t last = strlen(name);
    if (last > 1 && name[last-1] != ':' && name[last-1] != '/') { name[last] = '/'; name[last+1] = 0; }
    snprintf(name + strlen(name), sizeof name - strlen(name), "%.19s", core_filename);
    if (core_file.save) {
        if (!core_save_game(name)) { core_file.error = 1; return; }   /* "ERROR IN SAVING" */
        saved_flag = 1;   /* _saved */
        /* LAB_46110..LAB_46144 (asm:10931-10944): reopen level.dat until it
         * exists, _do_message(strInsertDisk,0) after each failure. */
        FILE *check = core_level_dat_open();
        if (!check) { core_disk_message = CORE_DISK_SAVE; return; }
        fclose(check);
    } else if (!core_load_game(name)) fprintf(stderr, "Load game: %s could not be loaded.\n", name);
    core_game_options_close();   /* _save_load returns 1 either way */
}
static void core_draw_game_options(void);
static void core_draw_file(void)
{
    core_draw_game_options();
    core_file_layout(1);
    amiga_text(game_pixels, 0x58, 0x18, core_file.save ? "SAVE GAME" : "LOAD GAME");
    amiga_text(game_pixels, 0xd8, 0x22, "\x88"); amiga_text(game_pixels, 0xd8, 0x7a, "\x89");
    amiga_text(game_pixels, 0x58, 0x84, "DIRECTORY");
    amiga_text(game_pixels, 0x58, 0x97, core_file.error ? "ERROR IN SAVING" : "FILE NAME");
    char row[17];
    for (int i = 0; i < 12; ++i) {
        int index = core_file.top + i;
        const char *name = index < core_file.count ? core_file.names[index] : "";
        int length = (int)strlen(name);
        for (int k = 0; k < 16; ++k) row[k] = k < length ? name[k] : (char)0x82;
        row[16] = 0;
        amiga_text(game_pixels, 0x58, 0x22 + i*8, row);
    }
    for (int field = 1; field <= 2; ++field) {
        const char *text = field == 1 ? core_drawer : core_filename;
        int length = (int)strnlen(text, 16);
        for (int k = 0; k < 16; ++k) row[k] = k < length ? text[k] : (char)0x82;
        row[16] = 0;
        if (core_file.field == field && !(core_file.blink / 10 & 1)) row[core_file.length] = (char)0x83;
        amiga_text(game_pixels, 0x58, field == 1 ? 0x8d : 0xa0, row);
    }
}

static void core_requester_click(void)
{
    if (core_req.kind == CORE_REQ_OPTIONS) core_options_click();
    else if (core_req.kind == CORE_REQ_GAME) core_game_options_click();
    else if (core_req.kind == CORE_REQ_SET) core_set_options_click();
    else if (core_req.kind == CORE_REQ_WORLD) core_world_click();
    else if (core_req.kind == CORE_REQ_FILE) core_file_click();
}

static FILE *core_level_dat_open(void)
{
    char fallback[4096], path[4200];
    const char *directory = getenv("POPULOUS_DATA_DIR"), *assets = getenv("POPULOUS_ASSET_DIR");
    if (!assets) assets = POPULOUS_DEFAULT_ASSET_DIR;
    snprintf(fallback, sizeof fallback, "%s/raw", assets);
    if (!directory) directory = fallback;
    snprintf(path, sizeof path, "%s/level.dat", directory);
    return fopen(path, "rb");
}
/* _do_message(strInsertDisk,0) asm:11098-11141. */
static void core_disk_layout(int draw) { core_buttons_install(); core_requester(0x28,0x3c,0x100,0x30,core_end_ok,NULL,draw); }
static void core_draw_disk_message(void)
{
    core_disk_layout(1);
    amiga_text(game_pixels, 0x38, 0x48, "INSERT THE ORIGINAL\nPOPULOUS DISK IN DF0:");
}
static void core_disk_click(void)
{
    core_disk_layout(0);
    if (!core_check_cancel(core_end_ok, NULL)) return;
    int retry = core_disk_message;
    core_disk_message = CORE_DISK_NONE; left_button = 2;
    if (retry == CORE_DISK_WORLD) { core_requester_layout(); core_world_evaluate(); return; }
    FILE *check = core_level_dat_open();
    if (!check) { core_disk_message = CORE_DISK_SAVE; return; }
    fclose(check);
    core_game_options_close();
}

static void core_draw_game_options(void)
{
    core_requester(0x10,0x10,0x120,0xb0,core_end_ok,core_end_cancel,1);
    for (int i = 0; i < 17; ++i)   /* asm:12847-12872 */
        amiga_text(game_pixels, core_rec_w(core_game_entry(i),0)+16, core_rec_w(core_game_entry(i),2)+16,
                   core_rec_text(core_game_entry(i)));
}
/* Draw the open requester over the frame, as the original's d_screen. */
static void core_draw_requester(void)
{
    if (core_req.kind == CORE_REQ_OPTIONS) {
        core_requester(0x10,0x10,0x120,0xa6,core_end_ok,core_end_cancel,1);
        uint8_t *own = core_stats(core_req.side);
        static const char marker[] = "\x87";
        for (int i = 0; i < 14; ++i) {   /* asm:13671-13740 */
            uint8_t *entry = core_option_entry(i);
            amiga_text(game_pixels, core_rec_w(entry,0)+16, core_rec_w(entry,2)+16, core_rec_text(entry));
            if (i == 11) amiga_text(game_pixels, core_rec_w(own,0xc)*8 + core_rec_w(entry,0) + 0x28,
                                    core_rec_w(entry,2)+16, marker);
            if (i == 13) amiga_text(game_pixels, core_rec_w(own,0x10)*8 + core_rec_w(entry,0) + 0x30,
                                    core_rec_w(entry,2)+16, marker);
        }
    } else if (core_req.kind == CORE_REQ_GAME) core_draw_game_options();
    else if (core_req.kind == CORE_REQ_WORLD) core_draw_world();
    else if (core_req.kind == CORE_REQ_FILE) core_draw_file();
    if (core_req.kind && core_disk_message) core_draw_disk_message();
    else if (core_req.kind == CORE_REQ_SET) {
        /* _set_options draws over the game options screen. */
        core_draw_game_options();
        core_requester(0x20,0x18,0x100,0x70,core_end_ok,core_end_cancel,1);
        amiga_text(game_pixels, core_rec_w(core_set_entry(0),0)+0x20, core_rec_w(core_set_entry(0),2)+0x18,
                   core_rec_text(core_set_entry(0)));
        int shown = 0;
        for (int i = 0; i < 5; ++i) {   /* asm:13386-13410: the set choice of each pair */
            shown = i*2 + ((game_mode & (1 << i)) ? 1 : 0) + 1;
            uint8_t *entry = core_set_entry(shown);
            amiga_text(game_pixels, core_rec_w(entry,0)+0x20, core_rec_w(entry,2)+0x18, core_blank);
            amiga_text(game_pixels, core_rec_w(entry,0)+0x20, core_rec_w(entry,2)+0x18, core_rec_text(entry));
        }
        /* Landscape number, asm:13412-13437 (1993 addition): below the last pair. */
        char landscape[32]; snprintf(landscape, sizeof landscape, "LANDSCAPE NO %d", (int)start_seed);
        amiga_text(game_pixels, core_rec_w(core_set_entry(shown),0)+0x20, core_rec_w(core_set_entry(shown),2)+0x22, landscape);
    }
}

/* _end_game display asm:14317-14385: _requester(d_screen,16,16,0x120,0xa6,
 * button) with the -60(A5) button "TRY IT AGAIN" (lost conquest) or
 * "NEW GAME", the rows at _end_text+16, then wait for a click on the
 * button (_check_cancel(button,0)). */
static uint8_t core_end_button[46];
static void core_end_button_layout(int draw)
{
    memset(core_end_button, 0, sizeof core_end_button);
    strcpy(core_rec_text(core_end_button), core_result == 2 && core_conquest_game() ? "TRY IT AGAIN" : "NEW GAME");
    core_requester(16,16,0x120,0xa6,core_end_button,NULL,draw);
}
static void core_draw_end_screen(void)
{
    core_end_button_layout(1);
    for (int r = 0; r < 7; ++r)
        amiga_text(game_pixels, A4_WORD(0x8e7e + r*46) + 16, A4_WORD(0x8e80 + r*46) + 16, core_end_row(r));
}

/* _end_words asm:24783-24785, _words asm:24786-24788 (two per rank, -1 none). */
static const int16_t core_end_words[5] = {0,1,3,5,6};
static const int16_t core_words[20] = {9,-1,5,-1,3,-1,7,-1,1,8,2,-1,1,2,9,6,1,6,3,6};
/* The original's working directory: POPULOUS_DATA_DIR, else <assets>/raw. */
static FILE *core_open_raw(const char *name)
{
    const char *directory = getenv("POPULOUS_DATA_DIR"), *assets = getenv("POPULOUS_ASSET_DIR");
    char path[4160];
    if (directory) snprintf(path,sizeof path,"%s/%s",directory,name);
    else snprintf(path,sizeof path,"%s/raw/%s",assets ? assets : POPULOUS_DEFAULT_ASSET_DIR,name);
    return fopen(path,"rb");
}
/* _read_lord asm:16631-16663: 0x7d00 bytes of four plane-major planes
 * (8000 bytes, 40 per row, MSB left) into _w_screen, then 0x20 bytes =
 * 16 BE16 RGB12 words into _tcmap. The original fails only on Open; the
 * port also rejects a short file instead of keeping stale w_screen data. */
static int core_read_lord(void)
{
    FILE *file = core_open_raw("lord.pic");
    if (!file) return 0;
    static uint8_t data[0x7d00 + 0x20];
    int ok = fread(data,1,sizeof data,file) == sizeof data;
    fclose(file);
    if (!ok) return 0;
    for (int pixel = 0; pixel < 320*200; ++pixel) {
        int colour = 0;
        for (int plane = 0; plane < 4; ++plane)
            colour |= ((data[plane*8000 + pixel/8] >> (7 - pixel%8)) & 1) << plane;
        core_lord.screen[pixel] = (uint8_t)colour;
    }
    for (int i = 0; i < 16; ++i) core_lord.palette[i] = (uint16_t)(data[0x7d00+i*2] << 8 | data[0x7d01+i*2]);
    return 1;
}
/* _read_mouth asm:16668-16690: 0x13b0 bytes, no failure path. Without the
 * file the port skips _draw_mouth (the original would copy stale _map_steps). */
static void core_read_mouth(void)
{
    FILE *file = core_open_raw("mouths.pic");
    core_lord.mouths_loaded = file && fread(core_lord.mouths,1,sizeof core_lord.mouths,file) == sizeof core_lord.mouths;
    if (file) fclose(file);
}
/* _draw_mouth(screen,x,y,rows,frame) asm:19894-19922: x in bytes; frame
 * 0x348 bytes of rows of 6 bytes (48 pixels) per plane 0..3, each row
 * replacing all four planes (plane stride 0x1f40). */
static void core_draw_mouth(int x, int y, int rows, int frame)
{
    if (!core_lord.mouths_loaded || frame < 0 || (frame+1)*0x348 > (int)sizeof core_lord.mouths) return;
    const uint8_t *m = core_lord.mouths + frame*0x348;
    for (int r = 0; r < rows && y+r < 200; ++r) for (int px = 0; px < 48 && x*8+px < 320; ++px) {
        int colour = 0;
        for (int plane = 0; plane < 4; ++plane) colour |= ((m[r*24 + plane*6 + px/8] >> (7 - px%8)) & 1) << plane;
        core_lord.screen[(y+r)*320 + x*8 + px] = (uint8_t)colour;
    }
}
/* One pass of the loop before its four _waittof, LAB_48DCE-LAB_48EF2. */
static void core_lord_speak(void)
{
    if (core_lord.finished) {
        if (core_lord.c >= 10 && core_lord.c <= 14) {
            if (core_check_effect(2)) { core_PlaySound(core_end_words[core_lord.c-10],2); ++core_lord.c; }
            return;
        }
        if (core_lord.c > 200) core_lord.c = 0;
        if (core_lord.c == 16) core_draw_mouth(0x10,0x4b,0x23,5);
    } else if (core_lord.c >= 10 && core_lord.c <= 12) {
        if (core_lord.c == 10) { core_PlaySound(0,2); ++core_lord.c; }
        else if (core_check_effect(2)) {
            int16_t word = core_words[core_lord.rank*2 + core_lord.c - 11];
            if (word >= 0) core_PlaySound(word,2);
            ++core_lord.c;
        }
        return;
    } else if (core_lord.c > 250) core_lord.c = 0;
    ++core_lord.c; core_lord.f = 0; core_lord.d = 1;
}
/* Per vertical blank: after four _waittof the mouth frame -108(A5) at
 * byte 0x12, y 0x87, then while channel 2 talks it bounces 0..4
 * (asm:14740-14757), and the next pass begins. */
static void core_lord_vblank(void)
{
    if (!core_lord.shown || ++core_lord.wait < 4) return;
    core_lord.wait = 0;
    core_draw_mouth(0x12,0x87,0x23,core_lord.f);
    if (!core_check_effect(2)) {
        core_lord.f = (int16_t)(core_lord.f + core_lord.d);
        if (core_lord.f >= 4 || core_lord.f <= 0) core_lord.d = (int16_t)-core_lord.d;
    }
    core_lord_speak();
}
/* asm:14503-14594: sound off, _read_lord (0: _level_number back, empty
 * _message, asm:14762-14765), _read_mouth, gwords, the two text lines at
 * x 8, y 0xb2/0xba (core_conquest_text), _tcmap 16..31 = _palette for the
 * pointer (the port's prg_screen palette), _left_button 2. */
static int core_lord_open(int old_level, int level, int finished)
{
    core_lord.saved_music = music_off; music_off = 0;
    core_lord.saved_effect = effect_off; effect_off = 0;
    core_kill_effect(0,4);
    if (!core_read_lord()) {
        level_number = (int16_t)(finished ? 0 : old_level);
        core_message[0] = 0;
        music_off = core_lord.saved_music; effect_off = core_lord.saved_effect;
        return 0;
    }
    core_read_mouth();
    core_free_all_sounds();
    core_load_sound("gwords");
    if (finished) old_level = 0;
    seed = (uint16_t)old_level;   /* asm:14518 */
    char line[2][48];
    core_conquest_text(old_level, level, finished, line);
    if (!finished && seed) ___newrand();
    amiga_text(core_lord.screen, 8, 0xb2, line[0]);
    amiga_text(core_lord.screen, 8, 0xba, line[1]);
    core_lord.finished = finished; core_lord.rank = level / 250;
    core_lord.c = core_lord.f = 0; core_lord.d = 1; core_lord.wait = 0;
    core_lord.shown = 1; left_button = 2;
    core_lord_speak();
    return 1;
}
/* The click, LAB_48F4A asm:14740-14765: kill, gmusic1 back, the saved
 * _music_off/_effect_off, then _show_world. */
static void core_lord_leave(void)
{
    if (!core_lord.shown) return;
    core_lord.shown = 0; left_button = 2;
    core_kill_effect(0,4);
    core_free_all_sounds();
    core_load_sound("gmusic1");
    music_off = core_lord.saved_music; effect_off = core_lord.saved_effect;
    core_world_open(CORE_WORLD_WON);
}
/* _make_copper(4 planes)/_show_screen of w_screen with the lord _tcmap;
 * the 200-line screen leaves the port HUD rows black but for one line. */
static void core_draw_lord(void)
{
    uint32_t colours[16];
    for (int i = 0; i < 16; ++i) {
        uint16_t w = core_lord.palette[i];
        colours[i] = 0xff000000u | (uint32_t)((w >> 8) & 15) * 17 << 16 | (uint32_t)((w >> 4) & 15) * 17 << 8 | (uint32_t)(w & 15) * 17;
    }
    for (int pixel = 0; pixel < 320*200; ++pixel) prg_screen->chunky_buffer[pixel] = colours[core_lord.screen[pixel] & 15];
    static uint8_t hud[320*200];
    memset(hud,0,320*56);
    amiga_text(hud,0,0,"CLICK TO CONTINUE, ESC QUIT");
    for (int pixel = 0; pixel < 320*56; ++pixel) prg_screen->chunky_buffer[200*320+pixel] = prg_screen->palette[hud[pixel]];
    core_draw_pointer();
}

/* _show_world asm:12190-12773, run as a requester (CORE_REQ_WORLD): the
 * original loops inside the caller, so the game is paused while it is
 * open. _requester(d_screen,16,0x18,0x120,0xa0,"START GAME","NEW GAME")
 * (asm:12193-12216); the buttons are _show_world's locals -72/-118(A5).
 * Phases: shown (the parameter lines, waiting for START GAME/NEW GAME,
 * asm:12744-12761) or entry (_get_name, asm:12240-12260). */
/* _c_text asm:24621-24725 (A4 0x8afc): word x, y, then the put_in_string
 * offset (lines 9..14: the pixel offset of the YOU/HIM columns), text. */
static const CoreMenuEntry core_world_template[15] = {
    {0x10,0x0a,0x10,"WORLD TO CONQUER"},     {0x10,0x1e,0x11,"BATTLE NUMBER IS "},
    {0x10,0x26,0x14,"WORLDS LANDSCAPE IS "}, {0x10,0x2e,0x12,"HIS REACTIONS ARE "},
    {0x10,0x36,0x0e,"HIS RATING IS "},       {0x10,0x3e,0x05,"LAND "},
    {0x10,0x46,0x0f,"THE SWAMPS ARE "},      {0x10,0x4e,0x09,"WATER IS "},
    {0x10,0x58,0x48,"               YOU    HIM"}, {0x10,0x60,0x78,"POPULATION"},
    {0x10,0x68,0x78,"EARTHQUAKES"}, {0x10,0x70,0x78,"SWAMPS"}, {0x10,0x78,0x78,"KNIGHTS"},
    {0x10,0x80,0x78,"VOLCANOS"},    {0x10,0x88,0x78,"FLOODS"},
};
/* _con_text asm:24726-24750 (strings asm:21257-21330). */
static const char *const core_con_text[24] = {"GRASS PLANES","DESERT","SNOW AND ICE","ROCKY",
    "VERY SLOW","SLOW","MEDIUM","FAST","VERY FAST","VERY POOR","POOR","AVERAGE","GOOD","VERY GOOD",
    "CANNOT BE BUILT","BUILT JUST ON TOWNS","ONLY BUILT UP","BUILT ON PEOPLE",
    "SHALLOW","BOTTOMLESS","HARMFUL","FATAL","YES","NO"};
static const char *core_con(unsigned index) { return index < 24 ? core_con_text[index] : ""; }

static void core_world_layout(int draw)
{
    static const CoreMenuEntry buttons[] = {{0,0,0,"START GAME"},{0,0,0,"NEW GAME"}};
    core_menu_install(core_world_ok, buttons, 1); core_menu_install(core_world_cancel, buttons+1, 1);
    core_requester(0x10,0x18,0x120,0xa0,core_world_ok,core_world_cancel,draw);
}
/* Line i of the jump table asm:12380-12743 (put_in_string asm:12163-12186
 * truncates the text at the offset and appends). Lines 9..14 also give
 * the YOU/HIM columns (x+off+16, x+off+0x48). */
static void core_world_line(int i, char *line, char you[8], char him[8])
{
    const CoreMenuEntry *entry = &core_world_template[i];
    const char *value = NULL;
    snprintf(line, 48, "%s", entry->text); you[0] = him[0] = 0;
    switch (i) {
    case 1: { char number[12]; snprintf(number, sizeof number, "%d", core_world.level / 5); value = number;
              line[entry->w] = 0; strcat(line, value); return; }
    case 2: value = core_con(conq_05_terrain); break;
    case 3: value = core_con((uint16_t)(10 - conq_01_speed) / 2 + 4); break;   /* LSR.W asm:12441 */
    case 4: value = core_con((uint16_t)(10 - conquest) / 2 + 9); break;
    case 5: value = core_con(conq_04_mode & 4 ? 14 : conq_04_mode & 0x10 ? 15 : conq_04_mode & 8 ? 16 : 17); break;
    case 6: value = core_con(conq_04_mode & 2 ? 19 : 18); break;
    case 7: value = core_con(conq_04_mode & 1 ? 21 : 20); break;
    case 9: snprintf(you, 8, "%u", conq_06_your_count); snprintf(him, 8, "%u", conq_07_enemy_count); return;
    default:
        if (i >= 10) {
            strcpy(you, core_con(conq_03_yourpow & (1 << (i-10)) ? 22 : 23));
            strcpy(him, core_con(conq_02_enemypow & (1 << (i-10)) ? 22 : 23));
        }
        return;
    }
    line[entry->w] = 0; strncat(line, value, 47 - strlen(line));
}
/* level.dat record level/25 into _conquest 0x84bc..0x84c5 (asm:12315-12368). */
static int core_world_read(int record)
{
    FILE *file = core_level_dat_open();
    if (!file) return 0;
    uint8_t bytes[10];
    /* ___Seek/___Read: a short read past the end keeps the old bytes. */
    if (!fseek(file, (long)record * 10, SEEK_SET) && fread(bytes, 1, 10, file) == 10) {
        memcpy(A4_DISP(0x84bc), bytes, 8);
        conq_08_seed = (int16_t)(bytes[8] << 8 | bytes[9]);
    }
    fclose(file);
    return 1;
}
/* The callers' MOVE.W D0,_in_conquest: the conquest start (asm:543-551)
 * and _won_conquest (asm:14766-14772) loop while it is -1, then start
 * the world; _game_options (asm:13236-13239) sets _new_map, whose restart
 * with -1 (a number) builds that custom landscape from _seed. */
static void core_world_return(int result)
{
    in_conquest = (int16_t)result;
    if (result == -1 && core_world.origin != CORE_WORLD_OPTIONS) { core_world_open(core_world.origin); return; }
    if (core_world.origin == CORE_WORLD_OPTIONS) options_new_map = 1;
    if (result == -1 && !core_custom_new_map(1)) {
        POP_LOG_ERROR("World selection: landscape %d could not be started.", (int)seed);
        in_conquest = core_req.saved_conquest;
    }
    if (result != -1 && !game_reset_start((StartSelection){START_ORIGINAL, result / 5})) {
        POP_LOG_ERROR("World selection: world %d could not be started.", result);
        in_conquest = (int16_t)level_number;
        if (core_world.origin != CORE_WORLD_OPTIONS) return;   /* keep the screen */
    }
    core_req.kind = CORE_REQ_NONE; left_button = 2;
}
/* _get_name(d_screen,_message,0x20,0x2c,16) entry, asm:10307-10330:
 * from strlen, bytes <= 0x20 and past the end become 0x82 boxes. */
static void core_world_entry(void)
{
    core_world.shown = 0; core_world.entry = 1; core_world.blink = 0;
    core_world.length = (int)strlen(core_message);
    int end = 0;
    for (int i = 0; i < 16; ++i) {
        if (!core_message[i]) end = 1;
        if ((uint8_t)core_message[i] <= 0x20 || end) core_message[i] = (char)0x82;
    }
    core_message[16] = 0; left_button = 2;
}
/* LAB_4712A..LAB_47236, asm:12262-12378. */
static void core_world_evaluate(void)
{
    int16_t value = (int16_t)atoi(core_message);   /* ___atoi's low word, TST.W */
    if (value) {   /* a number: landscape seed for a custom game */
        start_seed = seed = (int16_t)value; core_message[0] = 0;
        core_world_return(-1); return;
    }
    if (core_message[0]) {
        core_world.level = core_decode(core_message);   /* GENESIS: 0 */
        if (core_world.level < 0) core_world.error = 1;   /* "NO SUCH WORLD" */
    }
    if (core_world.level < 0) { core_world_entry(); return; }
    /* diskerr_47222 (asm:12369-12375): _do_message(strInsertDisk,0); its
     * OK returns to LAB_47064 without _get_name, i.e. this evaluation. */
    if (!core_world_read(core_world.level / 25)) { core_disk_message = CORE_DISK_WORLD; left_button = 2; return; }
    core_world.shown = 1; core_world.entry = 0; left_button = 2;
}
static void core_world_open(int origin)
{
    core_req.kind = CORE_REQ_WORLD;
    memset(&core_world, 0, sizeof core_world); core_world.origin = origin;
    if (!core_message[0]) strcpy(core_message, "GENESIS");
    core_requester_layout();
    core_world_evaluate();
}
/* _get_name end (LAB_45ABA asm:10461-10488): drop the trailing boxes;
 * then _show_world blanks the "NO SUCH WORLD" line (asm:12251-12260). */
static void core_world_entry_done(void)
{
    core_message[core_world.length] = (char)0x82;
    for (int i = 15; i >= 0 && (uint8_t)core_message[i] > 0x80; --i) core_message[i] = 0;
    core_world.entry = 0; core_world.error = 0;
    core_world_evaluate();
}
/* _get_name keys, asm:10397-10431 (_asckey from ___keyboard): 0x20..0x7a
 * appended upper-cased while length < 15, 8/0x7f delete, 13 ends. */
static void core_world_key(int key)
{
    if (!core_world.entry) return;
    if (key >= 0x20 && key <= 0x7a) {
        if (core_world.length >= 15) return;
        core_message[core_world.length++] = (char)(key >= 'a' ? key - 0x20 : key);
    } else if ((key == 8 || key == 0x7f) && core_world.length) {
        core_message[core_world.length--] = (char)0x82;
        core_message[core_world.length] = (char)0x82;   /* the original waits for the cursor blink */
    } else if (key == 13) core_world_entry_done();
}
/* _check_cancel: in _get_name (asm:10432-10446) START GAME ends the
 * entry, NEW GAME empties it; on the shown world (asm:12749-12773) NEW
 * GAME clears _message and re-enters, START GAME starts it. */
static void core_world_click(void)
{
    int result = core_check_cancel(core_world_ok, core_world_cancel);
    if (core_world.entry) {
        if (result == 1) core_world_entry_done();
        else if (result == 2) { core_message[0] = 0; core_world.entry = core_world.error = 0; core_world_evaluate(); }
        return;
    }
    if (result == 2) { core_message[0] = 0; core_world_entry(); return; }
    if (result != 1) return;
    if (core_world.level / 5 >= LEVEL_COUNT) {   /* port: no level.dat record above 2470 */
        POP_LOG_ERROR("World selection: world %d has no level.dat record.", core_world.level);
        return;
    }
    /* asm:12762-12771 */
    player = 0; not_player = 1;
    stats_build_mode(1) = 1; stats_build_mode(0) = 0; stats_build_latch(0) = 1;
    stats_build_latch(1) = 0; serial_off = 1; paint_map = 0; war_flag = 0;
    level_number = (int16_t)core_world.level;
    core_world_return(core_world.level);
}
/* ---- World / lord screens -------------------------------------------- */

/* The caller clears the screen first (___clr_wsc asm:540, 14757), except
 * _game_options, which draws over its own setup screen. */
static void core_draw_world(void)
{
    if (core_world.origin == CORE_WORLD_OPTIONS) core_draw_game_options();
    else memset(game_pixels, 0, sizeof game_pixels);
    core_world_layout(1);
    const CoreMenuEntry *t = core_world_template;
    amiga_text(game_pixels, t[0].x+16, t[0].y+24, t[0].text);
    /* LAB_525C8: the 16-box field, then _message (asm:12228-12239). */
    char field[17], name[17];
    memset(field, 0x82, 16); field[16] = 0;
    snprintf(name, sizeof name, "%s", core_message);
    if (core_world.entry && !(core_world.blink / 10 & 1)) name[core_world.length] = (char)0x83;   /* cursor, asm:10351-10372 */
    amiga_text(game_pixels, 0x20, 0x2c, field);
    amiga_text(game_pixels, 0x20, 0x2c, name);
    if (core_world.error) amiga_text(game_pixels, t[1].x+16, t[1].y+24, "NO SUCH WORLD");
    if (!core_world.shown) return;
    for (int i = 1; i < 15; ++i) {
        char line[48], you[8], him[8];
        core_world_line(i, line, you, him);
        if (you[0]) amiga_text(game_pixels, t[i].x+t[i].w+16, t[i].y+24, you);
        if (him[0]) amiga_text(game_pixels, t[i].x+t[i].w+0x48, t[i].y+24, him);
        amiga_text(game_pixels, t[i].x+16, t[i].y+24, line);
    }
}

static int game_pick(int sx, int sy, int *column, int *row)
{
    int16_t heights[81];
    for (int v=0; v<=8; ++v) for (int u=0; u<=8; ++u)
        heights[v*9+u]=core_vertex(xoff+u,yoff+v);
    int u,v;
    if (!graphics_pick(heights,sx,sy,&u,&v)) return 0;
    *column=xoff+u; *row=yoff+v;
    return 1;
}

/* _toggle_icon asm:19493-19521: XOR bitplane 0 (palette bit 0) with the
 * LAB_4C750 diamond; a steps +8 rows/+16px, b +8 rows/-16px. */
static void core_xor_icon(int base, int a, int b)
{
    static const uint32_t mask[16] = {0,0x0000c000,0x0003f000,0x000ffc00,0x003fff00,
        0x00ffffc0,0x03fffff0,0x0ffffffc,0x3fffffff,0x0ffffffc,0x03fffff0,0x00ffffc0,
        0x003fff00,0x000ffc00,0x0003f000,0x0000c000};
    int x0 = 8*(base%40) + 16*a - 16*b, y0 = base/40 + 8*a + 8*b;
    for (int r = 1; r < 16; ++r) for (int bit = 0; bit < 32; ++bit) {
        int x = x0+bit, y = y0+r;
        if ((mask[r] >> (31-bit) & 1) && x >= 0 && x < 320 && y >= 0 && y < 200)
            game_pixels[y*320+x] ^= 1;
    }
}

/* Persistent highlights, derived from state rather than toggled: tendency
 * (_set_tend_icons asm:2455-2522), mode (_set_mode_icons asm:2526-2567,
 * magnet bit before swamp bit before _mode), sound (initial toggles
 * asm:513-536) and pause (_do_action y=6 asm:18591). The short press
 * flashes on the power and view icons are not reproduced. */
static struct { int16_t base, a, b; } core_flash[4];
static int core_flashes;
static void core_flash_icon(int base, int a, int b)
{
    if (core_flashes < 4) { core_flash[core_flashes].base = (int16_t)base; core_flash[core_flashes].a = (int16_t)a; core_flash[core_flashes].b = (int16_t)b; ++core_flashes; }
}
static void core_draw_icons(void)
{
    for (int index = 0; index < core_flashes; ++index) core_xor_icon(core_flash[index].base,core_flash[index].a,core_flash[index].b);
    core_flashes = 0;
    if (player < 0 || player > 1 || diagnostic_view) return;
    static const int8_t tend[4][2] = {{3,3},{4,3},{5,3},{4,4}};
    int t = core_tendency(player);
    if (t >= 0 && t <= 3) core_xor_icon(0x12c0,tend[t][0],tend[t][1]);
    if (core_tool == 1) core_xor_icon(0x12c0,6,2);
    else if (core_swamp_selected) core_xor_icon(0x12c0,2,2);
    else if (core_tool == 2) core_xor_icon(0x12c0,6,0);
    else core_xor_icon(0x12c0,6,1);
    /* Toggled at startup with both flags 1 (asm:513-536,24131-24134). */
    if (music_off) core_xor_icon(0x17e4,0,3);
    if (effect_off) core_xor_icon(0x17e4,0,4);
    if (pause) core_xor_icon(0x17e4,1,3);
}

/* _draw_bar asm:19213-19282: 4px column at byte x (pixels 2-5), growing
 * up from y; value rows in colour, the rest of height in colour 2. */
static void core_draw_bar(int x, int y, int height, int value, int colour)
{
    if (value < 0) value = 0;
    if (value > height) value = height;
    for (int k = 0; k < height; ++k) {
        int row = y-k;
        if (row < 0 || row >= 200) continue;
        for (int px = 8*x+2; px < 8*x+6; ++px) game_pixels[row*320+px] = (uint8_t)(k < value ? colour : 2);
    }
}

/* _show_the_shield state part asm:2769-2790: a dead view_who is dropped. */
static int core_shield_state(void)
{
    if (!view_who) return 1;
    if (view_who < 0 || view_who > no_peeps || peep_word(view_who-1,4) <= 0) { view_who = 0; return 0; }
    return 1;
}

/* _show_the_shield asm:2790-3147. The owner flag and weapon icons use
 * _icon_data, which the asset export lacks; they are not drawn. */
static void core_show_the_shield(GraphicsSheet small)
{
    if (!core_shield_state()) return;
    if (view_who) {
        int person = view_who-1, flags = peeps[person*22], owner = peeps[person*22+1]&1;
        int16_t pop = peep_word(person,4);
        uint32_t heading; memcpy(&heading,peeps+person*22+14,sizeof heading);
        core_draw_icon(0x11,4,peeps[person*22+1]);
        int weapon = 1;
        while (weapon < 11 && (uint16_t)A4_WORD(0x2f96+2*weapon) != peeps[person*22+3]) ++weapon;
        if (weapon < 11) core_draw_icon(0x12,4,weapon+1);
        if (flags & 8) {
            /* Battle: sprite +0xC and strength bars, blue left (asm:2840-2895). */
            int other = peep_word(person,6);
            if (other >= 0 && other < no_peeps) {
                int blue = owner ? other : person, red = owner ? person : other;
                int32_t sum = peep_word(blue,4) + peep_word(red,4);
                int frame = peep_word(person,12);
                if (frame >= 0 && frame < 143) graphics_blit(game_pixels,64000,320,200,small,frame,0x110,0x16);
                core_draw_bar(0x24,0x25,16,(int)___divs(16*peep_word(blue,4),sum),15);
                core_draw_bar(0x25,0x25,16,(int)___divs(16*peep_word(red,4),sum),8);
            }
        } else if (flags == 1) {
            /* Town: capacity and fill bars (asm:2896-2953). */
            graphics_blit(game_pixels,64000,320,200,small,0x40+owner*2+(toggle&1),0x10c,0x16);
            int capacity = core_check_life(owner,peep_word(person,8));
            if (!capacity) capacity = 1;
            core_draw_bar(0x24,0x25,16,capacity == 0xbea ? 16 : (uint16_t)(capacity*16)/0x131,10);
            int fill = (uint16_t)(pop*16)/(uint16_t)capacity;
            core_draw_bar(0x25,0x25,16,fill > 16 ? 16 : fill,12);
        } else {
            /* Walker sprite (asm:2954-3020) and population bars (asm:3021-3061). */
            int frame;
            if (flags & 0x60) frame = (heading ? 0x7d : 0x65) + owner*2 + (toggle&1);
            else if (flags & 0x10) frame = (heading ? 0x6d : 0x5d) + owner*4 + ((uint16_t)game_turn&3);
            else {
                static const int16_t dir_sprite[9] = {-65,-64,-63,1,65,64,63,-1,0};
                int d = 0;
                while (d < 9 && dir_sprite[d] != peep_word(person,10)) ++d;
                if (d > 7) d = 4;
                frame = d*2 + (toggle&1) + (heading ? 0x20 : 0) + owner*16;
            }
            if (frame < 143) graphics_blit(game_pixels,64000,320,200,small,frame,0x110,0x16);
            if (pop > 0x1000) { core_draw_bar(0x24,0x25,16,pop/0x400,10); core_draw_bar(0x25,0x25,16,pop%0x400,9); }
            else { core_draw_bar(0x24,0x25,16,pop/0x100,9); core_draw_bar(0x25,0x25,16,(pop%0x100)/16,9); }
        }
    }
    /* good_pop/bad_pop bars asm:3062-3147. */
    int32_t good = (int32_t)core_good_pop(0), bad = (int32_t)core_good_pop(1);
    core_draw_bar(0x20,0x1f,0x20,good ? (int)(31*good/50000+1) : 0,15);
    core_draw_bar(0x27,0x1f,0x20,bad ? (int)(31*bad/50000+1) : 0,8);
}

/* Funny occupant (map_who byte >= $D1), _move_sprite asm:15951-16258: the
 * walking (flags 2 / bit 4) branch at asm:16061-16191 interpolates by +$C
 * along +$A and hides the second half of moves leaving the 8x8 view; then
 * asm:16192-16197 take frame +6. The main loop (asm:630-641) draws entries
 * whose +6 is >= $D1 with _draw_s_32 at (x-8,y-16). The extra view_who
 * entry (frame $44, asm:16245-16254) is queued like the original, but $44 is
 * past the 13-frame sheet, so _draw_s_32 would read beyond _sprite_data_32;
 * the port draws nothing for it. A shaken screen never gets here. */
static const struct { int16_t step, hide_u, hide_v, dx, dy; } core_funny_moves[] = {
    {-65, 0, 0, 0,-2}, {-64,-1, 0, 2,-1}, {-63, 7, 0, 4, 0}, {  1, 7,-1, 2, 1},
    { 65, 7, 7, 0, 2}, { 64,-1, 7,-2, 1}, { 63, 0, 7,-4, 0}, { -1, 0,-1,-2,-1},
};
static void core_queue_sprite(int x, int y, int frame, int who)
{
    if (no_sprites >= 64) return;
    int entry = 0x9a22 + no_sprites*8;
    A4_WORD(entry) = (int16_t)x; A4_WORD(entry+2) = (int16_t)y;
    A4_WORD(entry+4) = (int16_t)frame; A4_WORD(entry+6) = (int16_t)who;
    ++no_sprites;
}
static void core_draw_funny(int u, int v, int cell)
{
    int who = map_who[cell], slot = who-1, flags = peeps[slot*22];
    if (who > CORE_FUNNY_END) return;   /* past the 211-record _peeps */
    if (flags != 2 && (flags == 1 || !(flags & 16))) return;   /* other branches never see funnies */
    int counter = peep_word(slot,12), step = peep_word(slot,10), left, top;
    if (counter >= 8 || !graphics_tile(u,v,map_alt[cell],&left,&top)) return;   /* asm:16063: _debug */
    int next = cell+step, sx = left+8, sy = top-8;
    sy += ((int8_t)map_alt[cell] - (next >= 0 && next < 4096 ? (int8_t)map_alt[next] : 0)) * counter;
    if (map_blk[cell] != 15) sy += 4;
    int dx = 0, dy = 0;
    for (size_t index = 0; index < sizeof core_funny_moves/sizeof *core_funny_moves; ++index) {
        if (core_funny_moves[index].step != step) continue;
        if (counter > 4 && (u == core_funny_moves[index].hide_u || v == core_funny_moves[index].hide_v)) return;
        dx = core_funny_moves[index].dx; dy = core_funny_moves[index].dy;
        break;
    }
    sx += dx*counter; sy += dy*counter;
    int frame = peep_word(slot,6);
    core_queue_sprite(sx,sy,frame,who);
    if (view_who == who) core_queue_sprite(sx+8,sy,0x44,who);
    graphics_draw_s_32(game_pixels,sizeof game_pixels,graphics_big_sprites(game_assets),frame,sx-8,sy-16);
}
/* Ordinary occupant (map_who byte < $D1), _move_sprite asm:15951-16258.
 * Dispatch (asm:15996-16016): ==2 walk, ==1 town, bit 4 walk (water frame),
 * bits 5/6 wait, bit 3 battle, bit 7 remnant, otherwise the +$C default.
 * The walking branch (asm:16061-16191) starts at the map_who cell - the
 * departure cell the core claims - and moves by +$C (0..7) along +$A with
 * the altitude difference, hiding the second half of moves that leave the
 * 8x8 view; +$C >= 8 is the _debug exit. Frames: asm:16192-16224 (_toggle,
 * heading, owner). Every entry is queued (SUB_49F72, asm:16225-16254) with
 * the view_who marker (frame $44 at x+8) and drawn by the main loop with
 * ___draw_sprite (asm:645-650). Host deviations: heading presence tests all
 * four host reference bytes (the original TST.W/TST.L a pointer); dead,
 * foreign-owner or out-of-pool slots and a map_who cell that is neither the
 * peep's +8 nor (walking) its departure cell are skipped; frames outside the
 * 143-frame sheet and invalid battle frames keep the render-only bounds. */
static void core_emit_sprite(GraphicsSheet small, int x, int y, int frame, int who)
{
    int queued = no_sprites < 64;
    core_queue_sprite(x,y,frame,who);
    if (queued && frame >= 0 && frame < 143) graphics_blit(game_pixels,64000,320,200,small,frame,x,y);
}
static void core_draw_peep(GraphicsSheet small, int u, int v, int cell)
{
    int person=(int)map_who[cell]-1;
    if (person<0 || person>=no_peeps || peep_word(person,4)<=0) return;
    int left,top,owner=peep_owner(person),flags=peeps[person*22];
    if (owner>1 || !graphics_tile(u,v,map_alt[cell],&left,&top)) return;
    int walking = flags==2 || (flags!=1 && (flags&0x10));
    int position=peep_word(person,8), step=peep_word(person,10);
    if (position!=cell && (!walking || (int16_t)(position-step)!=cell)) return;
    /* asm:15965-15982: build permission of every visible occupant. */
    int build = flags&0xf8;
    if (owner==(int16_t)player) { build=flags; if (peep_word(person,18)) build|=1; }
    ok_to_build = (int16_t)(ok_to_build|build);
    uint32_t raw_heading;
    memcpy(&raw_heading,peeps+person*22+14,sizeof raw_heading);
    int headed=raw_heading!=0, stored=peep_word(person,12), frame, sx=left+16, sy=top-8;
    if (walking) {
        int counter=stored;
        if (counter>=8) return;   /* asm:16061-16069: _debug */
        int next=cell+step, dx=0, dy=0;
        sy += ((int8_t)map_alt[cell]-(next>=0 && next<4096 ? (int8_t)map_alt[next] : 0))*counter;
        if (map_blk[cell]!=15) sy += 4;
        for (size_t index=0; index<sizeof core_funny_moves/sizeof *core_funny_moves; ++index) {
            if (core_funny_moves[index].step!=step) continue;
            if (counter>4 && (u==core_funny_moves[index].hide_u || v==core_funny_moves[index].hide_v)) return;
            dx=core_funny_moves[index].dx; dy=core_funny_moves[index].dy;
            break;
        }
        if (flags&0x10) frame=0x5d+((uint16_t)game_turn&3)+0x10*headed+4*owner;
        else frame=graphics_walker_frame(step,owner,toggle&1)+0x20*headed;
        sx=left+8+dx*counter; sy+=dy*counter;
    } else if (flags==1) frame=0x40+(toggle&1)+2*owner;
    else if (flags&0x60) frame=stored+0x18*headed+2*owner;
    else if (flags&8) {
        sound_effect |= 8;
        /* Preserve a render-only fallback for malformed or legacy frames. */
        frame=((stored>=70 && stored<=72) || (stored>=130 && stored<=132) ||
               (stored>=134 && stored<=136) || (stored>=138 && stored<=140)) ? stored : 138+(toggle&1);
    } else if (flags&0x80) {
        sound_effect |= 0x80;
        frame=0x69+((uint16_t)game_turn&3); sx=left+8;
    } else frame=stored+0x20*headed+4*owner;
    core_emit_sprite(small,sx,sy,frame,person+1);
    if (view_who==person+1) core_emit_sprite(small,sx+8,sy,0x44,person+1);
}

static void draw_original(int view_x, int view_y)
{
    if (!game_assets || view_x<0 || view_x>56 || view_y<0 || view_y>56) return;
    memset(game_pixels,0,sizeof game_pixels);
    memcpy(game_pixels,game_assets->background,sizeof game_assets->background);
    core_draw_icons();
    /* asm:615-622: terrain mode refreshes _pointer, then _move_mana. */
    if (!core_tool && !core_swamp_selected) core_set_pointer();
    _move_mana();
    int dy = core_shake_dy;
    GraphicsSheet land=graphics_land(game_assets), small=graphics_sprites(game_assets);
    int phase=((uint16_t)game_turn/4)&1;
    /* _draw_it, asm:15794-15949: row-major tiles, raised overlays, front faces. */
    for (int v=0; v<8; ++v) for (int u=0; u<8; ++u) {
        int cell=(view_y+v)*64+view_x+u, left,top;
        if (!graphics_tile(u,v,map_alt[cell],&left,&top)) continue;
        top+=dy;
        int frame=map_blk[cell];
        if (!frame && !phase) frame=16;
        graphics_blit(game_pixels,64000,320,200,land,frame,left,top);
        if (map_bk2[cell]) graphics_blit(game_pixels,64000,320,200,land,map_bk2[cell],left,top-8);
        for (int owner=0; owner<2; ++owner)
            if (core_tendency(owner)==0 && core_target(owner)==cell)
                graphics_blit(game_pixels,64000,320,200,land,45+owner,left,top-8);
    }
    for (int v=0; v<8; ++v)
        for (int level=0; level<map_alt[(view_y+v)*64+view_x+7]; ++level)
            graphics_blit(game_pixels,64000,320,200,small,77,304-16*v,128+8*v-8*level+dy);
    for (int u=7; u>=0; --u)
        for (int level=0; level<map_alt[(view_y+7)*64+view_x+u]; ++level)
            graphics_blit(game_pixels,64000,320,200,small,76,64+16*u,128+8*u-8*level+dy);
    /* Original queues occupants then draws them after terrain (asm:623-657).
     * A shaken screen_origin skips _move_sprite entirely (asm:15842-15849). */
    if (!dy) { no_sprites = 0; ok_to_build = 0; }   /* asm:612-613 */
    sound_effect = 0;   /* asm:614, before _draw_it */
    for (int v=0; v<8 && !dy; ++v) for (int u=0; u<8; ++u) {
        int cell=(view_y+v)*64+view_x+u;
        if (map_who[cell]>=0xd1) { core_draw_funny(u,v,cell); continue; }
        core_draw_peep(small,u,v,cell);
    }
    core_show_the_shield(small);
    /* Original overview projection (_a_putpixel, asm:16758-16777). */
    for (int row=0; row<64; ++row) for (int column=0; column<64; ++column) {
        int cell=row*64+column, mx=64+column-row, my=(column+row)/2;
        int colour=map_colour_data[map_blk[cell]&15];
        int person=(int)map_who[cell]-1;
        if (person>=0 && person<no_peeps && peep_word(person,4)>0) colour=peep_owner(person) ? 8 : 15;
        if (column>=view_x && column<=view_x+7 && row>=view_y && row<=view_y+7 &&
            (column==view_x || column==view_x+7 || row==view_y || row==view_y+7)) colour=5;
        game_pixels[my*320+mx]=(uint8_t)colour;
    }
    if (core_end_shown && core_result) core_draw_end_screen();
    core_draw_requester();
    for (size_t pixel=0; pixel<sizeof game_pixels; ++pixel)
        prg_screen->chunky_buffer[pixel]=prg_screen->palette[game_pixels[pixel]];
    /* Port HUD (not in the original, whose screen ends at y 200): 7 lines of
     * 40 columns in the original font (amiga_text) on y 200..255. */
    static uint8_t hud[320*200];
    char lines[7][48]={{0}};
    snprintf(lines[0],48,"%s BLUE %d RED %d MANA %d",core_result==1 ? "VICTORY" : core_result==2 ? "DEFEAT" : pause ? "PAUSED" : A4_WORD(0xae80) ? "WAR" : "PLAY",core_population(0),core_population(1),core_mana(0));
    snprintf(lines[1],48,"%s",core_end_shown && core_result ? "GAME OVER: CLICK/KEY CONTINUE, ESC QUIT" : A4_WORD(0xae80) ?
        "WAR: TERRAIN+MAGNET LOCKED, ARROWS/MAP" : core_swamp_selected ?
        "SWAMP ARMED 5000: LMB TARGET, RMB INERT" : "LMB RAISE, RMB LOWER, ARROWS/MAP SCROLL");
    snprintf(lines[2],48,"%s",core_swamp_selected ?
        (core_tool == 1 ? "AFTER TARGET: MAGNET / D VIEW" : core_tool == 2 ?
                          "AFTER TARGET: QUERY / D VIEW" : "AFTER TARGET: TERRAIN / D VIEW") :
        (core_tool == 1 ? "MAGNET TOOL, ICONS ACTIVE / D VIEW" : core_tool == 2 ?
                          "QUERY HOVER/LMB TEMP RMB KEEP / D VIEW" : "TERRAIN TOOL, ICONS ACTIVE / D VIEW"));
    snprintf(lines[3],48,"0 GATHER 1 SETTLE 2 JOIN 3 FIGHT R RESET");
    snprintf(lines[4],48,"S SWAMP 5000 Q QUAKE 2500 K KNIGHT 7500");
    snprintf(lines[5],48,"V VOLCANO 10000 F FLOOD 40000 M MAGNET");
    snprintf(lines[6],48,"A ARMAGEDDON 80000 SPC PAUSE ESC QUIT");
    /* Re-rasterise only when the text or the loaded font changes. */
    static char shown[7][48]; static uint8_t font[sizeof game_font]; static int valid;
    for (int line=0; line<7; ++line) lines[line][40]=0;
    if (!valid || memcmp(shown,lines,sizeof shown) || valid != game_font_loaded+1 || memcmp(font,game_font,sizeof font)) {
        memset(hud,0,320*56);
        for (int line=0; line<7; ++line) amiga_text(hud,0,line*8,lines[line]);
        memcpy(shown,lines,sizeof shown); memcpy(font,game_font,sizeof font); valid=game_font_loaded+1;
    }
    for (int pixel=0; pixel<320*56; ++pixel)
        prg_screen->chunky_buffer[200*320+pixel]=prg_screen->palette[hud[pixel]];
    core_draw_pointer();
}

/* The diagnostic view has no d_screen: the end screen and the requesters
 * are composed into game_pixels then copied onto the chunky buffer. */
static void core_draw_diagnostic_overlay(void)
{
    if (!(core_end_shown && core_result) && !core_req.kind) return;
    memset(game_pixels, 0xff, sizeof game_pixels);
    if (core_end_shown && core_result) core_draw_end_screen();
    core_draw_requester();
    for (size_t pixel=0; pixel<320*200; ++pixel)   /* d_screen's 200 lines */
        if (game_pixels[pixel] != 0xff) prg_screen->chunky_buffer[pixel]=prg_screen->palette[game_pixels[pixel]];
    core_draw_pointer();
}

static void _draw_it(int16_t y, int16_t x)
{
    ensure_prg_screen();
    if (!prg_screen) return;
    const int width = 320, height = 256;
    if (diagnostic_view) {
    for (int screen_y = 0; screen_y < height; ++screen_y) {
        for (int screen_x = 0; screen_x < width; ++screen_x) {
            int column = (screen_x+x)/4, row = (screen_y+y)/4;
            uint32_t colour = 0xff182028;
            if (column < 64 && row < 64 && screen_y < 240) {
                int cell = row*64+column, block = map_blk[cell];
                colour = !block ? 0xff285899 : 0xff427346 + map_alt[cell]*0x090909;
                if (block && block != 15 && block != 31 && block != 32) colour = 0xff70805a;
                if (block == 31) colour = 0xff568980;
                if (block == 32) colour = 0xff88544c;
                if (((screen_x+x)%4 == 0 || (screen_y+y)%4 == 0) && block) colour &= 0xffeeeeee;
            }
            prg_screen->chunky_buffer[screen_y*width+screen_x] = colour;
        }
    }
    for (int person = 0; person < no_peeps; ++person) {
        if (peep_word(person,4) <= 0) continue;
        int sx = peep_screen_x(person)-x, sy = peep_screen_y(person)-y;
        int town = peeps[person*22]&1, battle = peeps[person*22]&8;
        uint32_t colour = peep_owner(person) ? 0xffff795d : 0xff8cffff;
        int radius = paint_map ? 0 : town ? 3 : 1;
        if (battle) colour = 0xffffff00;
        for (int dy = -radius; dy <= radius; ++dy) for (int dx = -radius; dx <= radius; ++dx) {
            int xx = sx+dx, yy = sy+dy;
            if (xx < 0 || xx >= width || yy < 0 || yy >= 240) continue;
            if (town && abs(dx)<radius && abs(dy)<radius && !paint_map) continue;
            prg_screen->chunky_buffer[yy*width+xx] = colour;
        }
    }
    int population[2] = {core_population(0),core_population(1)};
    int total = population[0]+population[1];
    for (int owner = 0; owner < 2; ++owner) {
        int length = total ? population[owner]*300/total : 0;
        for (int pixel = 0; pixel < length; ++pixel)
            prg_screen->chunky_buffer[(244+owner*3)*width+10+pixel] = owner ? 0xffff795d : 0xff8cffff;
    }
    int mana_length = core_mana(0) > 0 ? core_mana(0)/10 : 0;
    if (mana_length > 300) mana_length = 300;
    for (int pixel = 0; pixel < mana_length; ++pixel)
        prg_screen->chunky_buffer[251*width+10+pixel] = 0xffd8c060;
    if (pause || core_result) {
        uint32_t colour = core_result == 1 ? 0xff8cffff : core_result ? 0xffff795d : 0xffffff80;
        for (int pixel=0; pixel<width; ++pixel) prg_screen->chunky_buffer[pixel] = colour;
    }
    core_draw_diagnostic_overlay();
    } else draw_original(x,y);
    char title[512];
    snprintf(title,sizeof title,"Populous | %s | cyan %d / red %d | mana %d | %s | 0 gather 1 settle 2 join 3 fight; M magnet; S swamp 5000; K knight 7500; Q quake 2500; F flood 40000; V volcano 10000; A Armageddon 80000; Space pause R reset Esc quit",
        core_result==1 ? "VICTORY" : core_result==2 ? "DEFEAT" : pause ? "PAUSED" : war_flag ? "WAR" : "RUNNING",
        core_population(0),core_population(1),core_mana(0),war_flag ?
        "TERRAIN AND MAGNET INPUT LOCKED / ARROWS SCROLL / MAP CLICK" : core_swamp_selected ?
        (core_tool == 1 ? "SWAMP ARMED (5000) LMB target then MAGNET / RMB inert" : core_tool == 2 ?
                          "SWAMP ARMED (5000) LMB target then QUERY / RMB inert" :
                          "SWAMP ARMED (5000) LMB target then TERRAIN / RMB inert") :
        (core_tool == 1 ? "MAGNET (200)" : core_tool == 2 ? "QUERY" : "TERRAIN (10+4/point)"));
    SDL_SetWindowTitle(prg_screen->window,title);
}

static void _do_knight(void) { core_knight(player); }
static void _do_war(void) { core_war(player); }
static void _do_flood(void) { core_flood(player); }
static void _do_quake(void) { core_quake(player,xoff,yoff,___newrand); }
static void _do_volcano(void) { core_volcano(player,xoff,yoff,___newrand); }
static void _do_funny(void) { core_do_funny(); }
/* Ordinary _set_frame branches, asm:5728-5743,5846-5861. */
