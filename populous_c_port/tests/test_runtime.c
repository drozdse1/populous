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
static int prg_calls;
int prg_exit_status(void) { return 0; }

/* The PRG is deliberately isolated until its unsafe A4 model is repaired. */
void prg_main(void)
{
    ++prg_calls;
    assert(SDL_WasInit(SDL_INIT_VIDEO));
    assert(live_windows == 0); /* No launcher window survives the handoff. */
    assert(screen == NULL);
    assert(g_argc == 1 && strcmp(g_argv[0], "populous") == 0);
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
    assert(launcher_main(1, arguments) == 0);
    assert(prg_calls == 1);
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

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (strcmp(argv[1], "screen") == 0) test_screen();
    else if (strcmp(argv[1], "failures") == 0) test_failures();
    else if (strcmp(argv[1], "launcher") == 0) test_launcher();
    else if (strcmp(argv[1], "invalid-driver") == 0) {
        char *arguments[] = {"populous", NULL};
        assert(launcher_main(1, arguments) != 0);
        assert(prg_calls == 0 && SDL_WasInit(0) == 0);
    } else assert(!"Unknown test");
    printf("runtime %s: PASS\n", argv[1]);
    return 0;
}
