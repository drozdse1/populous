/*
 * test_runtime.c - Runtime tests: SDL window/renderer setup with fault injection.
 *
 * Run via `make test`. Each test_* function is self-contained and
 * aborts through assert() on the first mismatch.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

/* Fault injection stays in this translation unit; SDL rendering remains real. */
static int allocation_calls, fail_allocation, live_allocations;
static int fail_window, fail_renderer, fail_texture, live_windows;
static int renderer_calls;

static void *checked_calloc(size_t count, size_t size)
{
    if (++allocation_calls == fail_allocation) return NULL;
    void *memory = calloc(count, size);
    if (memory) ++live_allocations;
    return memory;
}

static void checked_free(void *memory)
{
    if (memory) --live_allocations;
    free(memory);
}

static SDL_Window *checked_window(const char *title, int x, int y, int w, int h, Uint32 flags)
{
    if (fail_window) {
        SDL_SetError("injected window failure");
        return NULL;
    }
    SDL_Window *window = SDL_CreateWindow(title, x, y, w, h, flags);
    if (window) ++live_windows;
    return window;
}

static void checked_destroy_window(SDL_Window *window)
{
    if (window) --live_windows;
    SDL_DestroyWindow(window);
}

static SDL_Renderer *checked_renderer(SDL_Window *window, int index, Uint32 flags)
{
    ++renderer_calls;
    if (fail_renderer) {
        SDL_SetError("injected renderer failure");
        return NULL;
    }
    return SDL_CreateRenderer(window, index, flags);
}

static SDL_Texture *checked_texture(SDL_Renderer *renderer, Uint32 format, int access, int w, int h)
{
    if (fail_texture) {
        SDL_SetError("injected texture failure");
        return NULL;
    }
    return SDL_CreateTexture(renderer, format, access, w, h);
}

#define calloc checked_calloc
#define free checked_free
#define SDL_CreateWindow checked_window
#define SDL_DestroyWindow checked_destroy_window
#define SDL_CreateRenderer checked_renderer
#define SDL_CreateTexture checked_texture
#include "../sdl_lowlevel.c"
#define main launcher_main
#include "../populous_init.c"
#undef main
#undef calloc
#undef free
#undef SDL_CreateWindow
#undef SDL_DestroyWindow
#undef SDL_CreateRenderer
#undef SDL_CreateTexture

int g_argc;
char **g_argv;
int g_start_of_game;
static int prg_calls, prg_argc, prg_start;
int prg_exit_status(void)
{ return 0; }

/* The PRG is deliberately isolated until its unsafe A4 model is repaired. */
void prg_main(void)
{
    ++prg_calls;
    assert(SDL_WasInit(SDL_INIT_VIDEO));
    assert(live_windows == 0); /* No launcher window survives the handoff. */
    assert(screen == NULL);
    assert(g_argc >= 1 && strcmp(g_argv[0], "populous") == 0);
    assert(title_music_playing() == 0 && !SDL_WasInit(SDL_INIT_AUDIO));
    prg_argc = g_argc;
    prg_start = g_start_of_game; /* checked per caller */
}

/* Launcher seams: no 20 ms waits; events are pushed at chosen frames. */
static void no_wait(void)
{
}
static int click_x, click_y, click_frame, quit_frame, hook_calls, music_seen;
static void push_click(int x, int y)
{
    /* Window coordinates: the 320x256 screen is shown at 2x. */
    SDL_Event motion = {.type = SDL_MOUSEMOTION};
    motion.motion.x = x * 2;
    motion.motion.y = y * 2;
    SDL_Event down = {.type = SDL_MOUSEBUTTONDOWN}, up = {.type = SDL_MOUSEBUTTONUP};
    down.button.button = up.button.button = SDL_BUTTON_LEFT;
    down.button.x = up.button.x = x * 2;
    down.button.y = up.button.y = y * 2;
    assert(SDL_PushEvent(&motion) == 1 && SDL_PushEvent(&down) == 1 && SDL_PushEvent(&up) == 1);
}
static void script_hook(int frame)
{
    ++hook_calls;
    if (title_music_playing()) music_seen = 1;
    if (frame == click_frame) push_click(click_x, click_y);
    if (frame == quit_frame) {
        SDL_Event escape = {.type = SDL_KEYDOWN};
        escape.key.keysym.sym = SDLK_ESCAPE;
        assert(SDL_PushEvent(&escape) == 1);
    }
}
static void script(int frame, int x, int y, int quit)
{
    launch_wait = no_wait;
    launch_frame_hook = script_hook;
    launch_frames = 0;
    click_frame = frame;
    click_x = x;
    click_y = y;
    quit_frame = quit;
    hook_calls = music_seen = 0;
}

/* Screen. */
static void test_screen(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    for (int repeat = 0; repeat < 3; ++repeat) {
        AmigaScreen *created = amiga_create_screen(33, 17, 4);
        assert(created && created->window && created->renderer && created->texture);
        assert(created->palette && created->chunky_buffer);
        SDL_RendererInfo info;
        assert(SDL_GetRendererInfo(created->renderer, &info) == 0);
        assert(info.flags & SDL_RENDERER_SOFTWARE);
        for (int plane = 0; plane < 4; ++plane) {
            assert(created->bitplanes[plane]);
            /* Each scanline needs ceil(width / 8) bytes, including its padding. */
            created->bitplanes[plane][5 * 17 - 1] = 255;
        }
        created->chunky_buffer[0] = 0xffff0000;
        amiga_present(created);
        uint32_t pixels[66 * 34];
        assert(SDL_RenderReadPixels(created->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels,
                                    66 * sizeof(uint32_t)) == 0);
        assert(pixels[0] == 0xffff0000);
        amiga_destroy_screen(created);
        assert(live_allocations == 0 && live_windows == 0);
    }
    amiga_destroy_screen(NULL);
    SDL_Quit();
}

/* Failures. */
static void test_failures(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    const int invalid[][3] = {{0, 17, 4},  {-1, 17, 4}, {33, 0, 4},       {33, -1, 4},
                              {33, 17, 0}, {33, 17, 9}, {INT_MAX, 17, 4}, {33, INT_MAX, 4}};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index)
        assert(amiga_create_screen(invalid[index][0], invalid[index][1], invalid[index][2]) ==
               NULL);
    for (int failed = 1; failed <= 7; ++failed) {
        allocation_calls = 0;
        fail_allocation = failed;
        assert(amiga_create_screen(33, 17, 4) == NULL);
        assert(live_allocations == 0 && live_windows == 0);
    }
    fail_allocation = 0;
    fail_window = 1;
    assert(amiga_create_screen(33, 17, 4) == NULL);
    assert(live_allocations == 0 && live_windows == 0);
    fail_window = 0;
    fail_renderer = 1;
    renderer_calls = 0;
    assert(amiga_create_screen(33, 17, 4) == NULL);
    assert(renderer_calls == 2);
    assert(live_allocations == 0 && live_windows == 0);
    fail_renderer = 0;
    fail_texture = 1;
    assert(amiga_create_screen(33, 17, 4) == NULL);
    assert(live_allocations == 0 && live_windows == 0);
    fail_texture = 0;
    SDL_Quit();
}

/* Launcher. */
static void test_launcher(void)
{
    char *arguments[] = {"populous", NULL};
    /* The load.pic screen now waits for its click: start a normal game. */
    script(1, 250, 190, -1);
    assert(launcher_main(1, arguments) == 0);
    assert(prg_calls == 1 && prg_argc == 1 && prg_start == 1);
    assert(SDL_WasInit(0) == 0);
    assert(live_allocations == 0 && live_windows == 0);
    fail_window = 1;
    assert(launcher_main(1, arguments) != 0);
    assert(prg_calls == 1);
    assert(SDL_WasInit(0) == 0);
    assert(live_allocations == 0 && live_windows == 0);
    fail_window = 0;
    /* Seven screen allocations, followed by seven launcher allocations. */
    for (int failed = 8; failed <= 14; ++failed) {
        allocation_calls = 0;
        fail_allocation = failed;
        assert(launcher_main(1, arguments) != 0);
        assert(prg_calls == 1);
        assert(SDL_WasInit(0) == 0);
        assert(live_allocations == 0 && live_windows == 0);
    }
    fail_allocation = 0;
}

/* SUB_456 regions (asm:408-434) through real SDL events: 3 tutorial,
 * 2 conquest, 1 normal game; Escape quits without the PRG. */
static void test_choices(void)
{
    char *arguments[] = {"populous", NULL};
    const int clicks[][3] = {{0xe7, 0x9c, 3}, {319, 0xa9, 3},  {0xe7, 0xaa, 2},
                             {300, 0xb7, 2},  {0xf1, 0xb8, 1}, {319, 199, 1}};
    for (size_t index = 0; index < sizeof clicks / sizeof clicks[0]; ++index) {
        script(3, clicks[index][0], clicks[index][1], -1);
        int before = prg_calls;
        assert(launcher_main(1, arguments) == 0);
        assert(prg_calls == before + 1 && prg_start == clicks[index][2]);
        /* frames 0..3: the click is latched on frame 3; LAB_426 then fades the
         * load.pic palette (max nibble $f: 15 frames) before populous.prg. */
        assert(hook_calls == 4 + 15 + 1 && !music_seen);
        assert(SDL_WasInit(0) == 0 && live_allocations == 0 && live_windows == 0);
    }
    int before = prg_calls;
    script(-1, 0, 0, 5);
    assert(launcher_main(1, arguments) == 0);
    assert(prg_calls == before && hook_calls == 6 && SDL_WasInit(0) == 0);
    /* A click outside the buttons (LAB_43E) starts the intro with music;
     * a second click ends it and returns to load.pic; then Escape. */
    script(2, 10, 10, 40);
    assert(launcher_main(1, arguments) == 0);
    assert(prg_calls == before && music_seen && launch.state == LAUNCH_LOAD);
    assert(SDL_WasInit(0) == 0 && live_allocations == 0 && live_windows == 0);
}

/* Composed frames on a flat background (no land0 sheet): SUB_1802 glyphs
 * and SUB_5026 strips at the listing's positions and colours. */
static void test_intro_frames(void)
{
    static uint8_t pixels[320 * 200];
    GraphicsAssets *assets = launch_assets;
    launch_assets = NULL;
    memset(launch_demo_pixels, 5, sizeof launch_demo_pixels);
    memset(launch_frame_bg, 5, sizeof launch_frame_bg); /* SUB_714's SUB_58CA copy */
    memset(&launch, 0, sizeof launch);
    /* LAB_5A0C row 8 of glyph 0 ($5a5c): mask $800f, planes $2000 $7ff0 $2010 $5fe0. */
    assert(intro_title_glyphs[0][8][0] == 0x800f && intro_title_glyphs[0][8][1] == 0x2000 &&
           intro_title_glyphs[0][8][4] == 0x5fe0 && intro_title_word[1] == 1 &&
           intro_title_word[7] == 4);
    launch.state = LAUNCH_TITLE; /* drop 0 = 0: top $a8 */
    for (int i = 1; i < 10; ++i)
        launch.drop[i] = 0xa9; /* others above the top: skipped */
    launch_compose(pixels);
    assert(pixels[176 * 320 + 0x61] == 0xa); /* P: opaque, planes 1 and 3 */
    assert(pixels[176 * 320 + 0x60] == 5);   /* mask set, no planes: background */
    assert(pixels[167 * 320 + 0x61] == 5 && pixels[176 * 320 + 0x71] == 5);
    for (int row = 0; row < 32; ++row) /* banner left piece, glyph 5 */
        for (int bit = 0; bit < 16; ++bit) {
            const uint16_t *w = intro_title_glyphs[5][row];
            int s = 15 - bit, c = ((w[1] >> s) & 1) | ((w[2] >> s) & 1) << 1 |
                                  ((w[3] >> s) & 1) << 2 | ((w[4] >> s) & 1) << 3;
            assert(pixels[(0xa8 + row) * 320 + 0x50 + bit] == (((w[0] >> s) & 1) ? 5 | c : c));
        }
    launch.drop[0] = 0xa9; /* top -1: SUB_1802 skips it */
    launch_compose(pixels);
    assert(pixels[176 * 320 + 0x61] == 5 && pixels[(0xa8 + 31) * 320 + 0x5f] == 5);
    launch.state = LAUNCH_RISE;
    launch.drop[0] = 0; /* top = drop + 8 */
    launch_compose(pixels);
    assert(pixels[16 * 320 + 0x61] == 0xa && pixels[176 * 320 + 0x61] == 5);
    launch.state = LAUNCH_SCRIPT; /* letters gone: bare background */
    launch_compose(pixels);
    assert(pixels[16 * 320 + 0x61] == 5);

    /* SUB_1D1A + SUB_5026 with a synthetic font: every glyph opaque on its
     * left half (byte 0 = $0f), strip planes 2 and 4 = screen planes 1, 3: $a. */
    uint8_t font[sizeof launch_font];
    memcpy(font, launch_font, sizeof font);
    int had_font = launch_font_loaded;
    memset(launch_font, 0, sizeof launch_font);
    for (int g = 0; g < (int)sizeof launch_font / 40; ++g)
        for (int row = 0; row < 8; ++row) {
            launch_font[g * 40 + row * 5] = 0x0f;
            launch_font[g * 40 + row * 5 + 2] = 0xff; /* plane 1 -> colour 1 */
            launch_font[g * 40 + row * 5 + 4] = 0xff; /* plane 3 -> colour 4 */
        }
    launch_font_loaded = 1;
    memset(&launch, 0, sizeof launch);
    launch.credit = 0; /* "POPULOUS": 8 chars at byte 16 */
    launch_strip_text(1);
    assert(launch.credit == 1 && launch.strip[1][0][0] == 0xff && launch.strip[1][0][16] == 0x0f &&
           launch.strip[1][0][24] == 0xff && launch.strip[1][4][23] == 0xff &&
           launch.strip[1][4][24] == 0);
    launch.state = LAUNCH_CREDITS;
    launch.strip_y[0] = 0x4b; /* slot 0 still empty: all keep */
    launch.strip_y[1] = 25;
    launch.strip_fade[1] = 25; /* LAB_1F62 words 25..27 = 0 */
    launch_compose(pixels);
    for (int row = 0; row < 8; ++row)
        for (int x = 0; x < 320; ++x) {
            int inside = x >= 128 && x < 192 && x % 8 < 4;
            assert(pixels[(25 + row) * 320 + x] == (inside ? 0xa : 5));
        }
    assert(pixels[24 * 320 + 128] == 5 && pixels[33 * 320 + 128] == 5);
    launch.strip_fade[1] = 0; /* words 0..2 = $ffff: nothing */
    launch_compose(pixels);
    assert(pixels[25 * 320 + 128] == 5);
    launch.strip_fade[1] = 10; /* words 8, 9: fade + 0 = $5555, fade + 1 = $4aa9 */
    launch_compose(pixels);
    for (int bit = 0; bit < 4; ++bit) {
        assert(pixels[25 * 320 + 128 + bit] == (((0x5555 >> (15 - bit)) & 1) ? 5 : 0xa));
        assert(pixels[25 * 320 + 144 + bit] == (((0x4aa9 >> (15 - bit)) & 1) ? 5 : 0xa));
    }
    memcpy(launch_font, font, sizeof font);
    launch_font_loaded = had_font;
    launch_assets = assets;
}

/* State progression on the injected VBI clock (launch_step per frame). */
static void test_intro(void)
{
    assert(launch_region(0xe6, 0x9c) == 0 && launch_region(0xe7, 0x9b) == 0);
    assert(launch_region(0xe7, 0x9c) == 3 && launch_region(0xe7, 0xaa) == 2);
    assert(launch_region(0xe7, 0xb8) == 0 && launch_region(0xf0, 0xb8) == 0 &&
           launch_region(0xf1, 0xb8) == 1);
    /* Latch: armed 2 ignores a held button until it is released. */
    memset(&launch, 0, sizeof launch);
    launch.seed = 1;
    launch_load_begin();
    launch_mouse(1, 250, 190);
    assert(launch.button == 2);
    launch_mouse(0, 250, 190);
    assert(launch.button == 0x40);
    launch_mouse(1, 251, 191);
    assert(!launch.button && launch.x == 251 && launch.y == 191);
    launch_mouse(0, 10, 10);
    assert(!launch.button && launch.x == 251);
    assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_FADE && launch.choice == 1 &&
           launch.button == 2);
    for (int pass = 1; pass <= launch.fade_steps; ++pass)
        assert(launch_step() == LAUNCH_RUN && launch.fade_step == pass);
    assert(launch_step() == LAUNCH_START && launch.choice == 1 && launch.button == 2);

    memset(&launch, 0, sizeof launch);
    launch.seed = 1;
    launch_load_begin();
    for (int frame = 1; frame <= LAUNCH_LOAD_FRAMES; ++frame)
        assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_LOAD);
    assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_BLOCKS); /* timeout, choice 0 */
    for (int i = 0; i < LAUNCH_DROPS; ++i)
        assert(launch.drop[i] >= 0xe8 && launch.drop[i] <= 0xf4 && launch.speed[i] >= 0 &&
               launch.speed[i] <= 8);
    int blocks = 0, title = 0, rise = 0, setup = 0;
    while (launch.state == LAUNCH_BLOCKS) {
        assert(launch_step() == LAUNCH_RUN);
        ++blocks;
    }
    for (int i = 0; i < LAUNCH_DROPS; ++i)
        assert(launch.settled[i] >= 0 && launch.settled[i] < 8);
    while (launch.state == LAUNCH_TITLE) {
        assert(launch_step() == LAUNCH_RUN);
        ++title;
    }
    while (launch.state == LAUNCH_RISE) {
        assert(launch_step() == LAUNCH_RUN);
        ++rise;
    }
    /* SUB_536's 16-drop loop: one frame per SUB_110A, the zero one too. */
    assert(launch.state == LAUNCH_SCRIPT && blocks > 20 && blocks < 1000 && rise > 0);
    while (launch.state == LAUNCH_SCRIPT) {
        assert(launch_step() == LAUNCH_RUN);
        ++setup;
    }
    assert(setup > 1 && launch.state == LAUNCH_CREDITS);
    /* SUB_714 first frame: slot 1 (50) takes line 0 at row 50, fade 0;
     * slot 0 (75) waits 25 frames; each slot then repeats every 51. */
    assert(launch.strip_count[0] == 0x4a && launch.strip_count[1] == 49 && launch.credit == 1);
    assert(launch.strip_y[0] == 0x4b && launch.strip_fade[0] == 0 && launch.strip_y[1] == 50 &&
           launch.strip_fade[1] == 0);
    for (int frame = 1; frame <= 25; ++frame)
        assert(launch_step() == LAUNCH_RUN);
    assert(launch.strip_y[0] == 50 && launch.strip_y[1] == 25 && launch.strip_fade[1] == 25 &&
           launch.credit == 2);
    for (int frame = 26; frame <= 50; ++frame)
        assert(launch_step() == LAUNCH_RUN);
    assert(launch.strip_y[1] == 0 && launch.strip_fade[1] == 0 && launch.strip_count[1] == 50);
    assert(launch_step() == LAUNCH_RUN && launch.strip_y[1] == 50 && launch.credit == 3);
    /* The script never ends by itself: LAB_1F9E wraps after 23 lines. */
    for (int frame = 52; frame < 51 * 13; ++frame)
        assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_CREDITS);
    assert(LAUNCH_CREDIT_LINES == 23 && launch.credit == 26 - 23);
    printf("intro frames: blocks %d, title %d, rise %d, setup %d\n", blocks, title, rise, setup);
    /* Same seed, same drops: the sequence is deterministic. */
    int again = 0;
    memset(&launch, 0, sizeof launch);
    launch.seed = 1;
    launch_load_begin();
    launch.frame = LAUNCH_LOAD_FRAMES;
    launch_step();
    while (launch.state == LAUNCH_BLOCKS) {
        launch_step();
        ++again;
    }
    assert(again == blocks);
    /* A fresh click in any intro phase ends it (asm:1553-1555). */
    for (int phase = LAUNCH_BLOCKS; phase <= LAUNCH_CREDITS; ++phase) {
        if (phase == LAUNCH_SCRIPT) continue; /* tested below */
        memset(&launch, 0, sizeof launch);
        launch.seed = 1;
        launch_load_begin();
        launch.frame = LAUNCH_LOAD_FRAMES;
        launch_step();
        while (launch.state != phase)
            launch_step();
        launch_mouse(1, 250, 190); /* armed: ignored while held */
        assert(launch_step() == LAUNCH_RUN && launch.state != LAUNCH_LOAD);
        launch_mouse(0, 250, 190);
        launch_mouse(1, 250, 190);
        /* LAB_1236: the demo palette fades out (12 frames) before load.pic;
         * SUB_714 tests before drawing, so its first pass is this frame. */
        assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_FADE && launch.choice == 0 &&
               launch.fade_steps == 12 && launch.fade_step == (phase == LAUNCH_CREDITS));
        for (int pass = launch.fade_step + 1; pass <= 12; ++pass)
            assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_FADE && launch.fade_step == pass);
        assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_LOAD && launch.choice == 0);
    }
    /* SUB_536's loop has no latch test: the click ends the first SUB_714 frame. */
    memset(&launch, 0, sizeof launch);
    launch.seed = 1;
    launch_load_begin();
    launch.frame = LAUNCH_LOAD_FRAMES;
    launch_step();
    while (launch.state != LAUNCH_SCRIPT)
        launch_step();
    launch_mouse(0, 250, 190);
    launch_mouse(1, 250, 190);
    while (!launch.script_done) {
        assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_SCRIPT);
    }
    /* That first SUB_714 frame aborts before drawing and fades (pass 1). */
    assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_FADE && launch.fade_step == 1);
    while (launch.state == LAUNCH_FADE)
        launch_step();
    assert(launch.state == LAUNCH_LOAD && launch.fade_step == 12);
    test_intro_frames();
    /* The full SDL loop: no click, the timeout runs the intro with music. */
    char *arguments[] = {"populous", NULL};
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_Quit();
    script(-1, 0, 0, LAUNCH_LOAD_FRAMES + 30);
    assert(launcher_main(1, arguments) == 0);
    assert(music_seen && prg_calls == 0 && SDL_WasInit(0) == 0 && live_allocations == 0);
}

/* SUB_714 block script (asm:586-1233), SUB_536 (asm:449-585), SUB_50B2,
 * SUB_3F92, SUB_1CBC, SUB_5792 and SUB_58FA. Oracles are derived by hand
 * from the listing; the milestone iterations come from an independent
 * Python model of the same opcodes over the contiguous $1272 block. */
static int script_index(int actor, uint8_t opcode)
{
    for (int pc = 0; intro_script_start[actor] + pc < INTRO_SCRIPT_BYTES; ++pc)
        if (intro_script_memory[intro_script_start[actor] + pc] == opcode) return pc;
    assert(!"opcode not in script");
    return -1;
}
static void script_reset(void)
{
    memset(&launch, 0, sizeof launch);
    launch.seed = 1;
    launch_actors_reset();
    launch_sub536_begin();
    memset(launch_bg, 5, sizeof launch_bg);
    memset(launch_clean, 5, sizeof launch_clean);
    launch.ops = 0;
    memset(launch.drop, 0, sizeof launch.drop); /* SUB_10A2 seeded them; landed */
    memset(launch.speed, 0, sizeof launch.speed);
}
static void test_intro_script(void)
{
    /* SUB_456's $55731 passes of 90 cycles in frames of 141876 - ~900 VBI cycles. */
    assert(LAUNCH_LOAD_FRAMES == (0x55731 * 90 + 140976 - 1) / 140976);
    script_reset();
    /* Data: actor 0 at cell 38, pc 0; SUB_536 sets step 8, the $6e08 cells. */
    assert(launch.actor[0][A_CELL] == 38 && launch.actor[0][A_STEP] == 8 &&
           launch.actor[7][A_BASE] == 16 && launch.actor[14][A_ANIM] == -1 &&
           launch.actor[15][A_EXTRA] == 1 && launch.actor[6][A_ANIM] == 1);
    /* GO 38 then 26: arrival reads the next byte; step sgn(-1)*10+sgn(-2) = -11,
     * dir 0. x = (8-3)*16+$98 = 232, y = 11*8+$1c-h[38]=8 -> 108. */
    launch.toggle = 0;
    launch_actor_step(0);
    assert(launch.actor[0][A_PC] == 1 && launch.actor[0][A_STEP] == -11 &&
           launch.actor[0][A_SUB] == 1 && launch.actor[0][A_CELL] == 38);
    assert(launch.ops == 1 && launch.op[0].kind == 0 && launch.op[0].frame == 0 &&
           launch.op[0].x == 232 && launch.op[0].y == 108);
    for (int i = 0; i < 7; ++i) launch_actor_step(0); /* sub 1..7 -> wraps on the 7th */
    assert(launch.actor[0][A_SUB] == 0 && launch.actor[0][A_CELL] == 27);
    /* SUB_50E6 with step 1 at cell 99, phase 3: h[100] is SUB_6538's $4e = 78,
     * (0-78)*3 = -234 >> 3 = -30; x = 152+2*3, y = 172-30+3; frame +6 +toggle. */
    launch.toggle = 1;
    launch.ops = 0;
    launch_draw(99, 1, 0, 3, 0, 0);
    assert(launch.op[0].x == 158 && launch.op[0].y == 145 && launch.op[0].frame == 7);
    launch_draw(99, 1, 0x65, 3, 4, 0); /* >= $40, extra 0: frame as is; drop lifts */
    assert(launch.op[1].frame == 0x65 && launch.op[1].y == 141);
    launch_draw(99, 1, 0x65, 3, 0, 1); /* extra: toggle only */
    assert(launch.op[2].frame == 0x66);
    launch_draw(38, 0, 16, 0, 0, 0); /* step 0: default dir 8 */
    assert(launch.op[3].frame == 16 + 8 + 1 && launch.op[3].x == 232);

    /* PUT: cell-sorted (type, cell) list, replace on the same cell. */
    script_reset();
    launch.actor[0][A_CELL] = 26;
    launch.actor[0][A_PC] = 2; /* put12 -> type $38 */
    launch_actor_step(0);
    assert(launch.list_cell[0] == 26 && launch.list_type[0] == 0x38 && launch.dirty &&
           launch.actor[0][A_PC] == 3 && launch.ops == 1 && launch.op[0].frame == 8);
    launch.actor[0][A_CELL] = 4;
    launch.actor[0][A_PC] = 2;
    launch_actor_step(0);
    launch.actor[0][A_CELL] = 30;
    launch_actor_step(0); /* pc 3 = frm157 -> no list change */
    launch.actor[0][A_PC] = 5; /* put0 -> $23 */
    launch.actor[0][A_CELL] = 26;
    launch_actor_step(0);
    assert(launch.list_cell[0] == 4 && launch.list_cell[1] == 26 && launch.list_type[1] == 0x23 &&
           launch.list_cell[2] == 0);
    /* The dirty rebuild of the next SUB_714 frame: the copied bg is the old
     * one; the new bg has land0 type $38 at cell 4 (SUB_1CBC asm:2258-2284):
     * x = 4*16+$90, y = 4*8-h[4](8)+$18. */
    launch.list_type[0] = 0x38;
    launch.list_cell[1] = 0;
    launch.state = LAUNCH_CREDITS;
    launch.button = 2;
    assert(launch_step() == LAUNCH_RUN && launch.vblanks == 2);
    assert(memcmp(launch_frame_bg, launch_clean, sizeof launch_clean) == 0);
    if (launch_assets) {
        GraphicsSheet land = graphics_land(launch_assets);
        static uint8_t tile[320 * 200];
        memset(tile, 5, sizeof tile);
        assert(graphics_blit(tile, sizeof tile, 320, 200, land, 0x38, 4 * 16 + 0x90, 4 * 8 - 8 + 0x18));
        assert(memcmp(tile, launch_bg, sizeof tile) == 0 && memcmp(tile, launch_clean, sizeof tile) != 0);
    }

    /* FADE_R: 19 screen-only steps, the 20th also into the bg, then pc+1. */
    script_reset();
    int fade = script_index(0, 0xfd);
    launch.actor[0][A_PC] = (int16_t)fade;
    for (int step = 0; step < 19; ++step) {
        launch.ops = 0;
        launch_actor_step(0);
        assert(launch.actor[0][A_PC] == fade && launch.fade_a == step + 1 && launch.ops == 1 &&
               launch.op[0].kind == 1 && launch.op[0].right && launch.op[0].frame == step);
    }
    static uint8_t before[320 * 200];
    memcpy(before, launch_bg, sizeof before);
    launch.ops = 0;
    launch_actor_step(0);
    assert(launch.actor[0][A_PC] == fade + 1 && launch.fade_a == 20 && launch.ops == 1);
    assert(memcmp(before, launch_bg, sizeof before) != 0);
    /* WAIT_R (A2 pc 0) holds below 20. */
    launch.fade_a = 19;
    launch_actor_step(2);
    assert(launch.actor[2][A_PC] == 0);
    launch.fade_a = 20;
    launch_actor_step(2);
    assert(launch.actor[2][A_PC] == 1);

    /* SUB_3F92 on a flat 5: word 0 keeps where LAB_1F62[step+1] is set,
     * word 1 uses step+2, word 2 step+0 (D0 2,4,0 across rows). */
    static uint8_t px[320 * 200];
    memset(px, 5, sizeof px);
    launch_corner(px, 1, 10);
    const uint16_t *right = intro_corner_words + 0x400;
    for (int word = 0; word < 3; ++word) {
        uint16_t mask = intro_fade_mask[10 + (word == 2 ? 0 : word + 1)];
        for (int bit = 0; bit < 16; ++bit) {
            int s = 15 - bit, c = 0;
            for (int p = 0; p < 4; ++p) c |= ((right[word * 4 + p] >> s) & 1) << p;
            assert(px[256 + word * 16 + bit] == ((mask >> s) & 1 ? 5 : c));
        }
    }
    /* Row 1 word 0 continues the cycle: global word 4 -> step + 2. */
    uint16_t mask1 = intro_fade_mask[12];
    for (int bit = 0; bit < 16; ++bit) {
        int s = 15 - bit, c = 0;
        for (int p = 0; p < 4; ++p) c |= ((right[16 + p] >> s) & 1) << p;
        assert(px[320 + 256 + bit] == ((mask1 >> s) & 1 ? 5 : c));
    }
    assert(px[64 * 320 + 256] == 5 && px[0 * 320 + 255] == 5);

    /* ADV_ALL ($f7, A13): pc+1 unless resting on $ff. */
    script_reset();
    int end0 = script_index(0, 0xff), adv = script_index(13, 0xf7);
    launch.actor[0][A_PC] = (int16_t)end0;
    launch.actor[13][A_PC] = (int16_t)adv;
    launch_actor_step(13);
    assert(launch.actor[0][A_PC] == end0 && launch.actor[13][A_PC] == adv + 1 &&
           launch.actor[2][A_PC] == 1 && launch.ops == 0);
    /* BOB ($f9, A6 base $20): counter $ef -> $f0 releases every pc, $ff too. */
    script_reset();
    int bob = script_index(6, 0xf9);
    launch.actor[6][A_PC] = (int16_t)bob;
    launch.actor[0][A_PC] = (int16_t)end0;
    launch.counter = 0xef;
    launch.toggle = 0;
    launch_actor_step(6);
    assert(launch.counter == 0xf0 && launch.actor[6][A_PC] == bob + 1 &&
           launch.actor[0][A_PC] == end0 + 1 && launch.actor[6][A_SUB] == 1 &&
           launch.op[0].frame == 0x8a);
    /* ANIM4 ($fa): -1 keeps the base (extra: +toggle), else 0..3 wrap. */
    script_reset();
    launch.actor[15][A_PC] = 2;
    launch.toggle = 1;
    launch_actor_step(15);
    assert(launch.actor[15][A_ANIM] == -1 && launch.op[0].frame == 67);
    launch.actor[15][A_ANIM] = 3;
    launch_actor_step(15);
    assert(launch.actor[15][A_ANIM] == 0 && launch.op[1].frame == 66);

    /* SUB_536: four actors (0, 1, 7, 8) at step 8 (dir 8), one vblank. */
    script_reset();
    launch.state = LAUNCH_SCRIPT;
    for (int i = 0; i < 16; ++i) launch.drop[i] = 0, launch.speed[i] = 0;
    launch.speed[0] = 3;
    assert(launch_step() == LAUNCH_RUN && launch.vblanks == 1 && launch.ops == 4 &&
           launch.toggle == 1);
    assert(launch.op[0].x == 232 && launch.op[0].frame == 0 + 8 + 1 && launch.op[2].frame == 16 + 9);
    /* SUB_1910 (asm:1951-1957): y < 0 skips the whole sprite. */
    if (launch_assets) {
        memset(px, 5, sizeof px);
        launch_sprite(px, 0, 100, -1);
        for (size_t i = 0; i < sizeof px; ++i) assert(px[i] == 5);
        /* The composed SUB_536 frame: bg copy, then each op's sprites0 frame,
         * keep bit -> background OR colour (SUB_1910 asm:1994-2002). */
        memset(launch_frame_bg, 5, sizeof launch_frame_bg);
        launch.ops = 1;
        launch.op[0] = (LaunchOp){0, 9, 100, 50, 0};
        launch_compose(px);
        GraphicsSheet sheet = graphics_sprites(launch_assets);
        int opaque = 0;
        for (int r = 0; r < 16; ++r)
            for (int c = 0; c < 16; ++c) {
                size_t src = 9 * 256 + (size_t)r * 16 + (size_t)c;
                opaque += !sheet.keep[src];
                assert(px[(50 + r) * 320 + 100 + c] == ((sheet.keep[src] ? 5 : 0) | sheet.indices[src]));
            }
        assert(opaque > 0 && px[49 * 320 + 100] == 5 && px[50 * 320 + 116] == 5);
    }

    /* The whole script from SUB_714's first iteration (Python cross-check). */
    script_reset();
    launch.state = LAUNCH_CREDITS;
    launch.button = 2;
    int fade_l = script_index(7, 0xfe), fade_r = script_index(0, 0xfd);
    for (int iteration = 0; iteration < 1100; ++iteration) {
        if (iteration == 559) assert(launch.actor[7][A_PC] == fade_l && launch.fade_b == 19);
        if (iteration == 560) assert(launch.actor[7][A_PC] == fade_l + 1 && launch.fade_b == 20);
        if (iteration == 607) assert(launch.actor[0][A_PC] == fade_r);
        if (iteration == 608) assert(launch.actor[0][A_PC] == fade_r + 1);
        if (iteration == 888) assert(launch.counter == 0xef);
        if (iteration == 889) assert(launch.counter == 0xf0 && launch.actor[6][A_PC] == bob + 1);
        if (iteration == 1021) assert(launch.actor[13][A_PC] == adv);
        if (iteration == 1022) assert(launch.actor[13][A_PC] == adv + 1);
        assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_CREDITS && launch.vblanks == 2);
        for (int i = 0; i < 20; ++i) assert(i < 15 || launch.list_cell[i] == 0);
    }

    /* SUB_5792: a new key-down aborts at the next call; held keys latch;
     * a press released before the call is lost (CIA SDR keeps the last byte). */
    script_reset();
    launch.state = LAUNCH_CREDITS;
    launch.button = 2;
    launch_key(1, 0);
    assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_FADE && launch.ops == 0 &&
           launch.fade_step == 1 && launch.vblanks == 1);
    /* LAB_1236: SUB_58FA on the demo palette, one nibble per frame, 12 frames. */
    for (int k = 1; k <= 12; ++k) {
        for (int c = 0; c < 16; ++c) {
            int r = (graphics_palette[c] >> 8) & 15, g = (graphics_palette[c] >> 4) & 15,
                b = graphics_palette[c] & 15;
            r = r > k ? r - k : 0, g = g > k ? g - k : 0, b = b > k ? b - k : 0;
            assert(launch.fade_pal[c] == (r << 8 | g << 4 | b));
        }
        assert(launch.state == LAUNCH_FADE && launch.fade_steps == 12);
        launch_step();
    }
    assert(launch.state == LAUNCH_LOAD && launch.frame == 0);
    launch.state = LAUNCH_BLOCKS; /* key still held: latched, no abort */
    for (int i = 0; i < LAUNCH_DROPS; ++i) launch.drop[i] = 200, launch.speed[i] = 0;
    launch_step();
    assert(launch.state != LAUNCH_FADE);
    launch_key(0, 0);
    launch_step();
    launch_key(1, 0);
    launch_key(0, 0); /* tap inside one frame: lost */
    launch_step();
    assert(launch.state != LAUNCH_FADE);
    launch_key(1, 0);
    launch_step(); /* end-of-loop test: fade starts the next frame */
    assert(launch.state == LAUNCH_FADE && launch.fade_step == 0);
    launch_key(0, 0);
    /* A modifier ($60..$67) never sets the latch: it aborts again. */
    script_reset();
    launch.state = LAUNCH_BLOCKS;
    launch.button = 2;
    launch_key(1, 1);
    launch_step();
    assert(launch.state == LAUNCH_FADE);
    launch.state = LAUNCH_BLOCKS;
    launch_step();
    assert(launch.state == LAUNCH_FADE);
    launch_key(0, 1);
    /* LAB_426: the load.pic palette fades (32 colours) before the game. */
    script_reset();
    for (int c = 0; c < 32; ++c) launch_load_palette[c] = (uint16_t)(c == 3 ? 0x0f20 : 0x0111);
    launch_load_begin();
    launch.button = 0;
    launch.x = 250;
    launch.y = 190;
    assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_FADE && launch.fade_steps == 15 &&
           launch.fade_colours == 32);
    for (int k = 1; k <= 15; ++k) {
        assert(launch_step() == LAUNCH_RUN && launch.fade_step == k);
        assert(launch.fade_pal[3] == (k < 15 ? (15 - k) << 8 | (k < 2 ? 2 - k : 0) << 4 : 0) &&
               launch.fade_pal[0] == 0);
    }
    assert(launch_step() == LAUNCH_START && launch.choice == 1);
}

/* The three load.pic buttons as rendered by the launcher: the whole frame
 * against an independent decode of raw/load.pic (5 planes, palette at
 * 40000), and the label ink (colours 21..25) against rows transcribed from
 * that bitmap: TUTORIAL ($9c.., choice 3), CONQUEST ($aa.., 2), CUSTOM ($b8.., 1). */
static void test_buttons(void)
{
    static const struct { int y; const char *ink; } labels[] = {
        {158, "            #######   ##    #   #######   #####   ########   #######     ###    ######"},
        {163, "              ###      ###  #   # ###     ####    # ##    #  #  ####   ##    ## # ######"},
        {172, "              #####    #####   ###   ###   #####    ##    # #########   ######    #######"},
        {177, "             #######   ####      #     #   #### #    ###  # # ##### #  ####  ##   # ###"},
        {186, "                        #####   ##    #   ######    #######   #####   ##     ##"},
        {191, "                       #######   ###  #  ####  ##   # ###     ####     ##    ##"},
    };
    const char *root = getenv("POPULOUS_ASSET_DIR");
    if (!root) root = POPULOUS_DEFAULT_ASSET_DIR;
    char path[4160];
    snprintf(path, sizeof path, "%s/raw/load.pic", root);
    FILE *file = fopen(path, "rb");
    assert(file);
    static uint8_t raw[40064];
    assert(fread(raw, 1, sizeof raw, file) == sizeof raw);
    fclose(file);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    screen = amiga_create_screen(320, 256, 4);
    assert(screen && launch_load_assets());
    memset(&launch, 0, sizeof launch);
    launch_load_begin();
    launch_render();
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320; ++x) {
            int index = 0;
            for (int plane = 0; plane < 5; ++plane)
                index |= ((raw[plane * 8000 + y * 40 + x / 8] >> (7 - x % 8)) & 1) << plane;
            uint16_t c = (uint16_t)(raw[40000 + 2 * index] << 8 | raw[40001 + 2 * index]);
            uint32_t argb = 0xff000000u | (uint32_t)((c >> 8) & 15) * 17 << 16 |
                            (uint32_t)((c >> 4) & 15) * 17 << 8 | (uint32_t)(c & 15) * 17;
            assert(screen->chunky_buffer[y * 320 + x] == argb);
            for (size_t l = 0; l < sizeof labels / sizeof labels[0]; ++l)
                if (labels[l].y == y && x >= 0xe0) {
                    size_t at = (size_t)(x - 0xe0);
                    int ink = at < strlen(labels[l].ink) && labels[l].ink[at] == '#';
                    assert(ink == (index >= 21 && index <= 25));
                }
        }
    launch_free_assets();
    amiga_destroy_screen(screen);
    screen = NULL;
    SDL_Quit();
}

/* Any argument bypasses the launcher screens (SUB_4 asm:83-86). */
static void test_cli(void)
{
    const char *flags[][2] = {{"--conquest", NULL},
                              {"--level", "7"},
                              {"--code", "GENESIS"},
                              {"3", NULL},
                              {"--tutorial", NULL},
                              {"--host", NULL},
                              {"--host", "2611"},
                              {"--join", "127.0.0.1:2611"}};
    for (size_t index = 0; index < sizeof flags / sizeof flags[0]; ++index) {
        char *arguments[] = {"populous", (char *)flags[index][0], (char *)flags[index][1], NULL};
        int count = flags[index][1] ? 3 : 2;
        script(0, 250, 190, 0);
        int before = prg_calls;
        assert(launcher_main(count, arguments) == 0);
        assert(prg_calls == before + 1 && prg_argc == count && hook_calls == 0);
        assert(SDL_WasInit(0) == 0 && live_allocations == 0 && live_windows == 0);
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    }
}

/* ========================================================================
 * Test runner (focused --flags first, then the full suite)
 * ======================================================================== */
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (strcmp(argv[1], "screen") == 0)
        test_screen();
    else if (strcmp(argv[1], "failures") == 0)
        test_failures();
    else if (strcmp(argv[1], "launcher") == 0) {
        test_launcher();
        test_choices();
    } else if (strcmp(argv[1], "intro") == 0)
        test_intro();
    else if (strcmp(argv[1], "buttons") == 0)
        test_buttons();
    else if (strcmp(argv[1], "intro-script") == 0) {
        if (!launch_load_assets()) assert(!"intro assets missing");
        test_intro_script();
        launch_free_assets();
    }
    else if (strcmp(argv[1], "cli") == 0)
        test_cli();
    else if (strcmp(argv[1], "invalid-driver") == 0) {
        char *arguments[] = {"populous", NULL};
        assert(launcher_main(1, arguments) != 0);
        assert(prg_calls == 0 && SDL_WasInit(0) == 0);
    } else
        assert(!"Unknown test");
    printf("runtime %s: PASS\n", argv[1]);
    return 0;
}
