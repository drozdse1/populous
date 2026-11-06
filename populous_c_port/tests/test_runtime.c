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
    if (fail_window) { SDL_SetError("injected window failure"); return NULL; }
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
    if (fail_renderer) { SDL_SetError("injected renderer failure"); return NULL; }
    return SDL_CreateRenderer(window, index, flags);
}

static SDL_Texture *checked_texture(SDL_Renderer *renderer, Uint32 format, int access, int w, int h)
{
    if (fail_texture) { SDL_SetError("injected texture failure"); return NULL; }
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
int prg_exit_status(void) { return 0; }

/* The PRG is deliberately isolated until its unsafe A4 model is repaired. */
void prg_main(void)
{
    ++prg_calls;
    assert(SDL_WasInit(SDL_INIT_VIDEO));
    assert(live_windows == 0); /* No launcher window survives the handoff. */
    assert(screen == NULL);
    assert(g_argc >= 1 && strcmp(g_argv[0], "populous") == 0);
    assert(title_music_playing() == 0 && !SDL_WasInit(SDL_INIT_AUDIO));
    prg_argc = g_argc; prg_start = g_start_of_game;   /* checked per caller */
}

/* Launcher seams: no 20 ms waits; events are pushed at chosen frames. */
static void no_wait(void) {}
static int click_x, click_y, click_frame, quit_frame, hook_calls, music_seen;
static void push_click(int x, int y)
{
    /* Window coordinates: the 320x256 screen is shown at 2x. */
    SDL_Event motion = {.type = SDL_MOUSEMOTION};
    motion.motion.x = x * 2; motion.motion.y = y * 2;
    SDL_Event down = {.type = SDL_MOUSEBUTTONDOWN}, up = {.type = SDL_MOUSEBUTTONUP};
    down.button.button = up.button.button = SDL_BUTTON_LEFT;
    down.button.x = up.button.x = x * 2; down.button.y = up.button.y = y * 2;
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
    launch_wait = no_wait; launch_frame_hook = script_hook; launch_frames = 0;
    click_frame = frame; click_x = x; click_y = y; quit_frame = quit;
    hook_calls = music_seen = 0;
}

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
        assert(SDL_RenderReadPixels(created->renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                    pixels, 66 * sizeof(uint32_t)) == 0);
        assert(pixels[0] == 0xffff0000);
        amiga_destroy_screen(created);
        assert(live_allocations == 0 && live_windows == 0);
    }
    amiga_destroy_screen(NULL);
    SDL_Quit();
}

static void test_failures(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    const int invalid[][3] = {{0, 17, 4}, {-1, 17, 4}, {33, 0, 4},
        {33, -1, 4}, {33, 17, 0}, {33, 17, 9}, {INT_MAX, 17, 4}, {33, INT_MAX, 4}};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index)
        assert(amiga_create_screen(invalid[index][0], invalid[index][1], invalid[index][2]) == NULL);
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
    const int clicks[][3] = {{0xe7, 0x9c, 3}, {319, 0xa9, 3}, {0xe7, 0xaa, 2}, {300, 0xb7, 2},
                             {0xf1, 0xb8, 1}, {319, 199, 1}};
    for (size_t index = 0; index < sizeof clicks / sizeof clicks[0]; ++index) {
        script(3, clicks[index][0], clicks[index][1], -1);
        int before = prg_calls;
        assert(launcher_main(1, arguments) == 0);
        assert(prg_calls == before + 1 && prg_start == clicks[index][2]);
        assert(hook_calls == 4 && !music_seen);   /* frames 0..3: the click is latched on frame 3 */
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
    memset(&launch, 0, sizeof launch);
    /* LAB_5A0C row 8 of glyph 0 ($5a5c): mask $800f, planes $2000 $7ff0 $2010 $5fe0. */
    assert(intro_title_glyphs[0][8][0] == 0x800f && intro_title_glyphs[0][8][1] == 0x2000 &&
           intro_title_glyphs[0][8][4] == 0x5fe0 && intro_title_word[1] == 1 && intro_title_word[7] == 4);
    launch.state = LAUNCH_TITLE;                   /* drop 0 = 0: top $a8 */
    for (int i = 1; i < 10; ++i) launch.drop[i] = 0xa9;   /* others above the top: skipped */
    launch_compose(pixels);
    assert(pixels[176 * 320 + 0x61] == 0xa);       /* P: opaque, planes 1 and 3 */
    assert(pixels[176 * 320 + 0x60] == 5);         /* mask set, no planes: background */
    assert(pixels[167 * 320 + 0x61] == 5 && pixels[176 * 320 + 0x71] == 5);
    for (int row = 0; row < 32; ++row)             /* banner left piece, glyph 5 */
        for (int bit = 0; bit < 16; ++bit) {
            const uint16_t *w = intro_title_glyphs[5][row];
            int s = 15 - bit, c = ((w[1] >> s) & 1) | ((w[2] >> s) & 1) << 1 | ((w[3] >> s) & 1) << 2 | ((w[4] >> s) & 1) << 3;
            assert(pixels[(0xa8 + row) * 320 + 0x50 + bit] == (((w[0] >> s) & 1) ? 5 | c : c));
        }
    launch.drop[0] = 0xa9;                         /* top -1: SUB_1802 skips it */
    launch_compose(pixels);
    assert(pixels[176 * 320 + 0x61] == 5 && pixels[(0xa8 + 31) * 320 + 0x5f] == 5);
    launch.state = LAUNCH_RISE; launch.drop[0] = 0;   /* top = drop + 8 */
    launch_compose(pixels);
    assert(pixels[16 * 320 + 0x61] == 0xa && pixels[176 * 320 + 0x61] == 5);
    launch.state = LAUNCH_SCRIPT;                  /* letters gone: bare background */
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
            launch_font[g * 40 + row * 5 + 2] = 0xff;     /* plane 1 -> colour 1 */
            launch_font[g * 40 + row * 5 + 4] = 0xff;     /* plane 3 -> colour 4 */
        }
    launch_font_loaded = 1;
    memset(&launch, 0, sizeof launch);
    launch.credit = 0;                             /* "POPULOUS": 8 chars at byte 16 */
    launch_strip_text(1);
    assert(launch.credit == 1 && launch.strip[1][0][0] == 0xff && launch.strip[1][0][16] == 0x0f &&
           launch.strip[1][0][24] == 0xff && launch.strip[1][4][23] == 0xff && launch.strip[1][4][24] == 0);
    launch.state = LAUNCH_CREDITS;
    launch.strip_y[0] = 0x4b;                      /* slot 0 still empty: all keep */
    launch.strip_y[1] = 25; launch.strip_fade[1] = 25;   /* LAB_1F62 words 25..27 = 0 */
    launch_compose(pixels);
    for (int row = 0; row < 8; ++row)
        for (int x = 0; x < 320; ++x) {
            int inside = x >= 128 && x < 192 && x % 8 < 4;
            assert(pixels[(25 + row) * 320 + x] == (inside ? 0xa : 5));
        }
    assert(pixels[24 * 320 + 128] == 5 && pixels[33 * 320 + 128] == 5);
    launch.strip_fade[1] = 0;                      /* words 0..2 = $ffff: nothing */
    launch_compose(pixels);
    assert(pixels[25 * 320 + 128] == 5);
    launch.strip_fade[1] = 10;                     /* words 8, 9: fade + 0 = $5555, fade + 1 = $4aa9 */
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
    assert(launch_region(0xe7, 0xb8) == 0 && launch_region(0xf0, 0xb8) == 0 && launch_region(0xf1, 0xb8) == 1);
    /* Latch: armed 2 ignores a held button until it is released. */
    memset(&launch, 0, sizeof launch);
    launch.seed = 1; launch_load_begin();
    launch_mouse(1, 250, 190); assert(launch.button == 2);
    launch_mouse(0, 250, 190); assert(launch.button == 0x40);
    launch_mouse(1, 251, 191); assert(!launch.button && launch.x == 251 && launch.y == 191);
    launch_mouse(0, 10, 10); assert(!launch.button && launch.x == 251);
    assert(launch_step() == LAUNCH_START && launch.choice == 1 && launch.button == 2);

    memset(&launch, 0, sizeof launch);
    launch.seed = 1; launch_load_begin();
    for (int frame = 1; frame <= LAUNCH_LOAD_FRAMES; ++frame)
        assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_LOAD);
    assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_BLOCKS);   /* timeout, choice 0 */
    for (int i = 0; i < LAUNCH_DROPS; ++i)
        assert(launch.drop[i] >= 0xe8 && launch.drop[i] <= 0xf4 && launch.speed[i] >= 0 && launch.speed[i] <= 8);
    int blocks = 0, title = 0, rise = 0, setup = 0;
    while (launch.state == LAUNCH_BLOCKS) { assert(launch_step() == LAUNCH_RUN); ++blocks; }
    for (int i = 0; i < LAUNCH_DROPS; ++i) assert(launch.settled[i] >= 0 && launch.settled[i] < 8);
    while (launch.state == LAUNCH_TITLE) { assert(launch_step() == LAUNCH_RUN); ++title; }
    while (launch.state == LAUNCH_RISE) { assert(launch_step() == LAUNCH_RUN); ++rise; }
    /* SUB_536's 16-drop loop: one frame per SUB_110A, the zero one too. */
    assert(launch.state == LAUNCH_SCRIPT && blocks > 20 && blocks < 1000 && rise > 0);
    while (launch.state == LAUNCH_SCRIPT) { assert(launch_step() == LAUNCH_RUN); ++setup; }
    assert(setup > 1 && launch.state == LAUNCH_CREDITS);
    /* SUB_714 first frame: slot 1 (50) takes line 0 at row 50, fade 0;
     * slot 0 (75) waits 25 frames; each slot then repeats every 51. */
    assert(launch.strip_count[0] == 0x4a && launch.strip_count[1] == 49 && launch.credit == 1);
    assert(launch.strip_y[0] == 0x4b && launch.strip_fade[0] == 0 && launch.strip_y[1] == 50 && launch.strip_fade[1] == 0);
    for (int frame = 1; frame <= 25; ++frame) assert(launch_step() == LAUNCH_RUN);
    assert(launch.strip_y[0] == 50 && launch.strip_y[1] == 25 && launch.strip_fade[1] == 25 && launch.credit == 2);
    for (int frame = 26; frame <= 50; ++frame) assert(launch_step() == LAUNCH_RUN);
    assert(launch.strip_y[1] == 0 && launch.strip_fade[1] == 0 && launch.strip_count[1] == 50);
    assert(launch_step() == LAUNCH_RUN && launch.strip_y[1] == 50 && launch.credit == 3);
    /* The script never ends by itself: LAB_1F9E wraps after 23 lines. */
    for (int frame = 52; frame < 51 * 13; ++frame) assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_CREDITS);
    assert(LAUNCH_CREDIT_LINES == 23 && launch.credit == 26 - 23);
    printf("intro frames: blocks %d, title %d, rise %d, setup %d\n", blocks, title, rise, setup);
    /* Same seed, same drops: the sequence is deterministic. */
    int again = 0;
    memset(&launch, 0, sizeof launch);
    launch.seed = 1; launch_load_begin(); launch.frame = LAUNCH_LOAD_FRAMES;
    launch_step();
    while (launch.state == LAUNCH_BLOCKS) { launch_step(); ++again; }
    assert(again == blocks);
    /* A fresh click in any intro phase ends it (asm:1553-1555). */
    for (int phase = LAUNCH_BLOCKS; phase <= LAUNCH_CREDITS; ++phase) {
        if (phase == LAUNCH_SCRIPT) continue;   /* tested below */
        memset(&launch, 0, sizeof launch);
        launch.seed = 1; launch_load_begin(); launch.frame = LAUNCH_LOAD_FRAMES;
        launch_step();
        while (launch.state != phase) launch_step();
        launch_mouse(1, 250, 190);   /* armed: ignored while held */
        assert(launch_step() == LAUNCH_RUN && launch.state != LAUNCH_LOAD);
        launch_mouse(0, 250, 190); launch_mouse(1, 250, 190);
        assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_LOAD && launch.choice == 0);
    }
    /* SUB_536's loop has no latch test: the click ends the first SUB_714 frame. */
    memset(&launch, 0, sizeof launch);
    launch.seed = 1; launch_load_begin(); launch.frame = LAUNCH_LOAD_FRAMES;
    launch_step();
    while (launch.state != LAUNCH_SCRIPT) launch_step();
    launch_mouse(0, 250, 190); launch_mouse(1, 250, 190);
    while (!launch.script_done) { assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_SCRIPT); }
    assert(launch_step() == LAUNCH_RUN && launch.state == LAUNCH_LOAD);
    test_intro_frames();
    /* The full SDL loop: no click, the timeout runs the intro with music. */
    char *arguments[] = {"populous", NULL};
    assert(SDL_Init(SDL_INIT_VIDEO) == 0); SDL_Quit();
    script(-1, 0, 0, LAUNCH_LOAD_FRAMES + 30);
    assert(launcher_main(1, arguments) == 0);
    assert(music_seen && prg_calls == 0 && SDL_WasInit(0) == 0 && live_allocations == 0);
}

/* Any argument bypasses the launcher screens (SUB_4 asm:83-86). */
static void test_cli(void)
{
    const char *flags[][2] = {{"--conquest", NULL}, {"--level", "7"}, {"--code", "GENESIS"}, {"3", NULL}, {"--tutorial", NULL},
                              {"--host", NULL}, {"--host", "2611"}, {"--join", "127.0.0.1:2611"}};
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

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (strcmp(argv[1], "screen") == 0) test_screen();
    else if (strcmp(argv[1], "failures") == 0) test_failures();
    else if (strcmp(argv[1], "launcher") == 0) { test_launcher(); test_choices(); }
    else if (strcmp(argv[1], "intro") == 0) test_intro();
    else if (strcmp(argv[1], "cli") == 0) test_cli();
    else if (strcmp(argv[1], "invalid-driver") == 0) {
        char *arguments[] = {"populous", NULL};
        assert(launcher_main(1, arguments) != 0);
        assert(prg_calls == 0 && SDL_WasInit(0) == 0);
    } else assert(!"Unknown test");
    printf("runtime %s: PASS\n", argv[1]);
    return 0;
}
