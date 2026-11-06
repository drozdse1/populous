#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#define main launcher_main
#define amiga_alloc_mem launcher_alloc_mem
#define amiga_free_mem launcher_free_mem
#include "../populous_init.c"
#undef amiga_alloc_mem
#undef amiga_free_mem
#undef main

static int allocation_calls, fail_allocation, live_allocations;
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
#define calloc checked_calloc
#define free checked_free
#include "../populous_prg.c"
#undef calloc
#undef free

/* Renders own the original _sprite table (no_sprites A4 0x9a1c, entries
 * 0x9a22..0x9c21); everything else must be untouched. */
static int a4_same(const uint8_t *before)
{
    size_t lo=(size_t)(A4_DISP(0x9a1c)-prg_a4_area), hi=(size_t)(A4_DISP(0x9c22)-prg_a4_area);
    return !memcmp(before,prg_a4_area,lo) &&
           !memcmp(before+hi,prg_a4_area+hi,sizeof prg_a4_area-hi);
}

static void test_memory(void)
{
    assert((uintptr_t)A4_DISP(0x8000) >= (uintptr_t)prg_a4_area);
    assert((uintptr_t)A4_DISP(0x7fff) < (uintptr_t)(prg_a4_area + sizeof prg_a4_area));
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    ((uint8_t *)A4_DISP(0x8000))[0] = 19;
    ((uint8_t *)A4_DISP(0x7fff))[0] = 23;
    seed = 123;
    assert(___newrand() == 15930); /* (123 * 0x24a1 + 0x24df) & 0x7fff */
    assert(((uint8_t *)A4_DISP(0x8000))[0] == 19 && ((uint8_t *)A4_DISP(0x7fff))[0] == 23);
    memset(map_who, 7, 4096);
    demo_height_at(0, 0) = 11;
    assert(((uint8_t *)map_who)[0] == 7);
    A4_DISP(0x8002)[0] = 91;
    A4_DISP(0x8007)[0] = 92;
    A4_LONG(0x8003) = 0x12345678;
    assert(A4_LONG(0x8003) == 0x12345678);
    assert(A4_DISP(0x8002)[0] == 91 && A4_DISP(0x8007)[0] == 92);
    memset(map_blk, 17, 4096);
    memset(map_bk2, 19, 4096);
    demo_height_at(63, 63) = -12;
    assert(map_blk[4095] == 17 && map_bk2[0] == 19);
    memset(peeps, 23, 208 * 22);
    _clear_map();
    assert(peeps[0] == 23 && peeps[208 * 22 - 1] == 23);
}
static void test_initial_world(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    assert(game_reset(0));
    assert(num_demo_peeps == 22);
    for (int cell = 0; cell < 4096; ++cell) {
        assert(map_alt[cell] == 0);
        assert(map_blk[cell] == 15);
    }
    memset(prg_screen->chunky_buffer, 0, 320 * 256 * sizeof(uint32_t));
    ___draw_it(0, 0);
    assert(prg_screen->chunky_buffer[100] != 0);
    SDL_Event key = {.type = SDL_KEYDOWN};
    key.key.keysym.sym = SDLK_SPACE;
    assert(SDL_PushEvent(&key) == 1);
    ___keyboard();
    assert(pause == 1);
    SDL_Event quit = {.type = SDL_QUIT};
    assert(SDL_PushEvent(&quit) == 1);
    prg_main();
    assert(prg_screen == NULL && live_allocations == 0);
    SDL_Quit();
}
static void test_reset_and_input(void)
{
    static const int populations[8][2] = {{12,10},{8,14},{15,9},{10,12},{6,18},{20,5},{11,11},{9,13}};
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    for (int scenario = 0; scenario < 8; ++scenario) {
        assert(game_reset(scenario));
        int counts[2] = {0,0};
        for (int index = 0; index < no_peeps; ++index) {
            uint8_t *person = peeps + index * 22;
            assert(person[1] <= 1);
            ++counts[person[1]];
            int16_t position;
            memcpy(&position, person + 8, 2);
            assert(position >= 0 && position < 4096);
            assert(map_who[position] == index + 1);
            assert(map_blk[position]==15 && core_check_life(person[1],position)>0);
        }
        A4_WORD(0x2ff8)=A4_WORD(0x2fb2)=0;
        assert(counts[0] == populations[scenario][0]);
        assert(counts[1] == populations[scenario][1]);
        SDL_SetWindowSize(prg_screen->window, 640, 512);
        game_render();
        uint32_t pixels[640 * 512];
        assert(SDL_RenderReadPixels(prg_screen->renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                    pixels, 640 * sizeof(uint32_t)) == 0);
        assert(pixels[0] == prg_screen->chunky_buffer[0]);
        SDL_SetWindowSize(prg_screen->window, 640, 512);
        uint8_t before[sizeof prg_a4_area];
        memcpy(before, prg_a4_area, sizeof before);
        SDL_Event key = {.type = SDL_KEYDOWN};
        key.key.keysym.sym = SDLK_r;
        assert(SDL_PushEvent(&key) == 1);
        ___keyboard();
        assert(a4_same(before));
        key.key.keysym.sym = SDLK_SPACE;
        assert(SDL_PushEvent(&key) == 1);
        ___keyboard();
        assert(pause);
        memcpy(before, prg_a4_area, sizeof before);
        game_tick();
        game_render();
        assert(a4_same(before));
        for (int repeat = 0; repeat < 100; ++repeat) {
            key.key.keysym.sym = SDLK_RIGHT;
            game_handle_event(&key);
        }
        assert(xoff == 56);
        SDL_SetWindowSize(prg_screen->window, 960, 768);
        SDL_Event motion = {.type = SDL_MOUSEMOTION};
        motion.motion.x = 300; motion.motion.y = 150;
        game_handle_event(&motion);
        assert(mousex == 100 && mousey == 50);
        memcpy(before, prg_a4_area, sizeof before);
        /* Both viewport powers reject while paused, not because V is inert. */
        assert(pause);
        key.key.keysym.sym = SDLK_q; game_handle_event(&key);
        key.key.keysym.sym = SDLK_v; game_handle_event(&key);
        assert(memcmp(before, prg_a4_area, sizeof before) == 0);
    }
    _closeall();
    SDL_Quit();
}

static void test_cli(void)
{
    const char *invalid[] = {"-1", "x", "", "8x", "2147483648", "9999999999999999999999999", " 1", "+1"};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index) {
        char *arguments[] = {"populous", (char *)invalid[index], NULL};
        int selected;
        assert(!parse_scenario(2, arguments, &selected));
    }
    char *arguments[] = {"populous", "8", NULL};
    int selected;
    assert(parse_scenario(2, arguments, &selected) && selected == 0);
    arguments[1] = "7";
    assert(parse_scenario(2, arguments, &selected) && selected == 7);
    assert(parse_scenario(1, arguments, &selected) && selected == 0);
}
static void test_lifecycle(void)
{
    char *arguments[] = {"populous", "0", NULL};
    g_argc = 2; g_argv = arguments;
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    for (int failed = 1; failed <= 8; ++failed) {
        allocation_calls = 0; fail_allocation = failed;
        prg_main();
        assert(prg_exit_status() != 0);
        assert(prg_screen == NULL && live_allocations == 0);
    }
    fail_allocation = 0;
    for (int repeat = 0; repeat < 3; ++repeat) {
        ensure_prg_screen();
        SDL_Event quit = {.type = SDL_KEYDOWN};
        quit.key.keysym.sym = SDLK_ESCAPE;
        assert(SDL_PushEvent(&quit) == 1);
        prg_main();
        assert(prg_exit_status() == 0);
        assert(prg_screen == NULL && live_allocations == 0);
    }
    SDL_Quit();
    for (int run = 0; run < 3; ++run) {
        assert(SDL_Init(SDL_INIT_VIDEO) == 0);
        SDL_Event quit = {.type = SDL_QUIT};
        assert(SDL_PushEvent(&quit) == 1);
        arguments[1] = run == 1 ? "7" : "8";
        assert(launcher_main(run == 0 ? 1 : 2, arguments) == 0);
        assert(!SDL_WasInit(0) && !prg_screen && !screen);
    }
    arguments[1] = "-1";
    assert(launcher_main(2, arguments) != 0);
    assert(!SDL_WasInit(0) && !prg_screen && !screen);
    g_argc = 0; g_argv = NULL;
}

static void test_original_rules(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    assert(game_reset(0));
    /* Original _make_map: uniform vertex height 1 => flat tile 15, altitude 0. */
    assert(map_blk[10 * 64 + 10] == 15);
    assert(A4_WORD(0x842e) == -64);
    assert(A4_LONG(0x848a) == 10);
    assert(core_valid_move(63, 1) == 1);
    assert(core_valid_move(64, -1) == 1);
    assert(core_check_life(0, 10 * 64 + 10) == 3050);
    uint8_t initial[sizeof prg_a4_area];
    memcpy(initial, prg_a4_area, sizeof initial);
    for (int tick = 0; tick < 16; ++tick) game_tick();
    if (!getenv("POPULOUS_TEST_REAL_DATA")) {
        /* Synthetic increments 6/7, two eight-tick growth cycles, one walking loss. */
        assert(peep_word(0,4)==56);
        assert(A4_WORD(0x2fe2+20)==6 && A4_WORD(0x2fb6+20)==7);
    }
    int towns = 0;
    for (int person = 0; person < no_peeps; ++person)
        towns += peep_word(person, 4) > 0 && (peeps[person * 22] & 1);
    assert(towns > 0);
    assert(core_population(0) > current_start_pop_you * 45);
    assert(core_mana(0) > 0);
    assert(memcmp(initial, prg_a4_area, sizeof initial) != 0);
    memcpy(initial, prg_a4_area, sizeof initial);
    game_render(); _move_mana();
    assert(a4_same(initial));
    assert(game_reset(0));
    int32_t mana_before = core_mana(0);
    assert(core_sculpt(0, 10, 10, 1));
    assert(core_vertex(10, 10) == 2);
    assert(core_mana(0) == mana_before - 14);
    assert(!core_sculpt(0, -1, 10, 1));
    assert(game_reset(3));
    memcpy(initial, prg_a4_area, sizeof initial);
    assert(!core_sculpt(0, 10, 10, 1));
    assert(memcmp(initial, prg_a4_area, sizeof initial) == 0);
    assert(game_reset(4));
    assert(!core_sculpt(0, 10, 10, -1));
    _closeall();
    SDL_Quit();
}

static void test_sculpt_projection_and_rejection(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(game_reset(0));
    SDL_SetWindowSize(prg_screen->window,960,768);
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_RIGHT;
    assert(xoff==6 && yoff==6); /* Both first populations are in the 8x8 view. */
    for (int step=0;step<2;++step) { assert(SDL_PushEvent(&key)==1); }
    ___keyboard();
    game_render();
    uint32_t pixels[320*256]; memcpy(pixels,prg_screen->chunky_buffer,sizeof pixels);
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN};
    click.button.button=SDL_BUTTON_LEFT; click.button.x=480; click.button.y=336;
    assert(SDL_PushEvent(&click)==1); ___keyboard();
    assert(core_vertex(10,10)==2 && core_mana(0)==385);
    game_render(); assert(memcmp(pixels,prg_screen->chunky_buffer,sizeof pixels)!=0);
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    click.button.y=750;
    assert(SDL_PushEvent(&click)==1); ___keyboard();
    assert(a4_same(before)); /* HUD must not sculpt hidden terrain. */
    LAB_516B2=0; memcpy(before,prg_a4_area,sizeof before);
    assert(!core_sculpt(0,10,10,1));
    assert(a4_same(before));
    _closeall(); SDL_Quit();
}

static void test_load_failure_and_magnet(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(game_reset(0));
    char *directory=strdup(getenv("POPULOUS_DATA_DIR")); assert(directory);
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    assert(setenv("POPULOUS_DATA_DIR","/nonexistent-populous-data",1)==0);
    assert(!game_reset(0));
    assert(a4_same(before));
    assert(setenv("POPULOUS_DATA_DIR",directory,1)==0); free(directory);
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_0;
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    for (int tick=0;tick<1024 && !core_leader(0);++tick) game_tick();
    assert(core_leader(0)>0);
    key.key.keysym.sym=SDLK_m; assert(SDL_PushEvent(&key)==1); ___keyboard();
    SDL_SetWindowSize(prg_screen->window,640,512);
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN}; click.button.button=SDL_BUTTON_LEFT;
    click.button.x=384; click.button.y=256;
    int32_t mana_before=core_mana(0);
    assert(SDL_PushEvent(&click)==1); ___keyboard();
    assert(core_target(0)==650 && core_mana(0)==mana_before-200);
    _closeall(); SDL_Quit();
}

static void test_battle_casualties_and_rewards(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(game_reset(0));
    memset(peeps,0,208*22); memset(map_who,0,4096); no_peeps=2;
    peeps[0]=peeps[22]=8; peeps[23]=1;
    peeps[3]=2; peeps[25]=3;
    set_peep_word(0,4,1000); set_peep_word(1,4,400);
    set_peep_word(0,8,1000); set_peep_word(1,8,1000); map_who[1000]=2;
    int32_t mana_before[2]={core_mana(0),core_mana(1)};
    seed=123;
    core_battle(0,1);
    /* asm:6082-6158, rolls 15930%3+1=1 then 28505%3+1=3.
     * Lesser strength=400: casualties 4*3+10=22 and 4*2+10=18. */
    assert(peep_word(0,4)==978 && peep_word(1,4)==382);
    assert(core_mana(0)==mana_before[0] && core_mana(1)==mana_before[1]);
    for (int leader=0; leader<=1; ++leader) {
        peeps[0]=peeps[22]=8;
        set_peep_word(0,4,100); set_peep_word(1,4,10);
        map_who[1000]=2; core_leader(1)=leader ? 2 : 0;
        /* Distinct fixture values catch swapped battle_add2 indices, independently
         * of the selected LAND data (asm:6440-6453,6514-6531). */
        A4_WORD(0x3010)=23; A4_WORD(0x3014)=25;
        A4_LONG(0x9a02)=100; A4_LONG(0x9a12)=100;
        seed=123;
        core_battle(0,1);
        assert(peep_word(0,4)==90 && !peep_word(1,4));
        assert(map_who[1000]==1 && !core_leader(1));
        assert(core_mana(0)==(leader ? 125 : 123));
        assert(core_mana(1)==(leader ? 75 : 77));
    }
    _closeall(); SDL_Quit();
}

static void setup_colocated_walkers(int owner)
{
    assert(game_reset(0));
    memset(peeps,0,208*22); memset(map_who,0,4096); no_peeps=2;
    for (int person=0; person<2; ++person) {
        peeps[person*22]=2; peeps[person*22+1]=owner ^ person;
        peeps[person*22+2]=peeps[person*22+3]=1;
        set_peep_word(person,4,100); set_peep_word(person,8,50*64+50);
    }
    core_tendency(0)=core_tendency(1)=1;
    map_who[50*64+50]=2;
}

static void test_colocated_hostile_encounter(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for (int owner=0; owner<2; ++owner) for (int leaving=0; leaving<2; ++leaving) {
        setup_colocated_walkers(owner);
        int position=50*64+50;
        if (leaving) {
            core_tendency(owner)=0; core_leader(owner)=1; core_target(owner)=position+1;
        }
        core_move(0);
        assert(peeps[0]==8 && peeps[22]==10);
        assert(peep_word(0,6)==1 && peep_word(1,6)==0);
        assert(peep_word(0,8)==position && peep_word(1,8)==position);
        assert(map_who[position]==1 && !map_who[position+1]); /* _set_battle attacker */
        assert(peep_word(0,4)==100 && peep_word(1,4)==100);
        assert(core_battles==1 && core_settlements==0 && !map_bk2[position]);
        uint8_t saved[sizeof prg_a4_area]; memcpy(saved,prg_a4_area,sizeof saved);
        game_render(); assert(a4_same(saved));
    }
    /* asm:4148-4184 handshake: slot 0's incomplete first frame marks the
     * represented slot 1 (0x22, frame 101, counter 0); it waits until slot 0
     * completes its eighth frame and attacks the waiting resident. */
    setup_colocated_walkers(0);
    for (int tick=0; tick<7; ++tick) _move_peeps();
    assert(!core_battles && peeps[0]==2 && peeps[22]==0x22);
    assert(peep_word(0,12)==7 && peep_word(1,6)==7 && peep_word(1,12)==102);
    _move_peeps();
    assert(core_battles==1 && (peeps[0]&8) && (peeps[22]&8));
    assert(map_who[50*64+50]==1 && !core_settlements);
    /* No direction means no current-cell encounter yet (asm:4426). */
    setup_colocated_walkers(0);
    peeps[2]=0;
    core_move(0);
    assert(!core_battles && peeps[0]==0x42 && peep_word(0,6)==7);
    assert(map_who[50*64+50]==2);
    /* A defending town retains its town bit, then transfers through normal battle. */
    setup_colocated_walkers(0);
    int position=50*64+50;
    peeps[22]=1; set_peep_word(1,12,35); core_set_town(1,0);
    core_tendency(0)=0; core_leader(0)=1; core_target(0)=position+1;
    core_move(0);
    assert(peeps[0]==8 && peeps[22]==9 && map_who[position]==1);
    assert(peep_word(0,6)==1 && peep_word(1,6)==0 && map_bk2[position]==35);
    set_peep_word(1,4,10); seed=123; core_battle(0,1);
    assert(peeps[0]==5 && peep_word(0,12)==85 && !peep_word(1,4) && map_who[position]==1);
    assert(map_blk[position]==31 && map_bk2[position]==42);
    _closeall(); SDL_Quit();
}

static void test_no_hidden_town(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for (int owner=0; owner<2; ++owner) {
        setup_colocated_walkers(owner);
        peeps[23]=owner;
        core_move(0);
        assert(!(peeps[0]&1) && !core_settlements && !map_bk2[50*64+50]);
        assert(map_who[50*64+50]==2 && peep_word(1,4)>0);
        /* _where_do_i_go needs map_blk 15 for a zero heading (asm:4679-4692),
         * so this town state is forced. _move_explorer itself has no hidden-
         * town guard: the represented town absorbs the walker (_join_forces,
         * asm:4537-4547); otherwise it settles (asm:4564-4589), after
         * releasing its own claim (asm:4438-4449). */
        for (int representative=0; representative<=2; ++representative) {
            setup_colocated_walkers(owner);
            int position=50*64+50;
            peeps[23]=owner; peeps[22]=1; set_peep_word(1,12,35);
            core_set_town(1,0);
            map_who[position]=representative;
            core_tendency(owner)=1; core_leader(owner)=1; core_target(owner)=position;
            /* Exercise settlement explicitly; magnet center now departs north. */
            map_blk[position]=15;
            assert(core_check_life(owner,position)>0);
            assert(core_explore(0)==0);
            core_move(0);
            assert(peeps[22]==1 && map_who[position]==(representative==2 ? 2 : 1));
            if (representative==2) {
                assert(!peeps[0] && !peep_word(0,4) && peep_word(1,4)==200);
                assert(!core_settlements && map_bk2[position]==35);
            } else {
                assert(peeps[0]==1 && core_settlements==1 && peep_word(1,4)==100);
                assert(peep_word(0,6)==game_turn && map_bk2[position]==peep_word(0,12));
            }
        }
        /* A genuinely empty cell must publish the newly created town. */
        setup_colocated_walkers(owner);
        core_remove(1); map_who[50*64+50]=0;
        core_move(0);
        assert(peeps[0]==1 && core_settlements==1 && map_who[50*64+50]==1);
    }
    _closeall(); SDL_Quit();
}

static void test_colocated_battle_death(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for (int dead=0; dead<2; ++dead) {
        setup_colocated_walkers(0);
        core_move(0);
        assert((peeps[0]&8) && (peeps[22]&8));
        core_remove(dead);
        int survivor=1-dead, position=50*64+50;
        assert(peep_word(dead,4)==0 && !peeps[dead*22]);
        assert(peeps[survivor*22]==2 && peep_word(survivor,6)==0);
        assert(map_who[position]==survivor+1 && peep_word(survivor,4)==100);
        core_move(survivor);
        assert(peeps[survivor*22]==1 && map_who[position]==survivor+1);
    }
    /* Walking loss immediately after battle entry must not strand its partner. */
    setup_colocated_walkers(0);
    A4_WORD(0x2fb0)=1; set_peep_word(0,4,1); set_peep_word(0,12,7);
    _move_peeps();
    assert(!peep_word(0,4) && peeps[22]==2 && map_who[50*64+50]==2);
    for (int winner=0; winner<2; ++winner) {
        setup_colocated_walkers(0);
        core_move(0);
        set_peep_word(winner,4,100); set_peep_word(1-winner,4,10);
        seed=123; core_battle(0,1);
        assert(peep_word(winner,4)==90 && !peep_word(1-winner,4));
        assert(peeps[winner*22]==6 && peep_word(winner,12)==85 && map_who[50*64+50]==winner+1);
        for (int tick=0; tick<3; ++tick) {
            _move_peeps();
            assert(peeps[winner*22]==(tick==2 ? 2 : 6));
            assert(peep_word(winner,4)==90 && peep_word(winner,8)==50*64+50);
        }
        core_move(winner);
        assert(peeps[winner*22]==1 && core_settlements==1);
    }
    setup_colocated_walkers(0); core_move(0);
    set_peep_word(0,4,10); set_peep_word(1,4,10);
    core_battle(0,1);
    assert(!peep_word(0,4) && !peep_word(1,4) && !map_who[50*64+50]);
    _closeall(); SDL_Quit();
}

/* Every game start: _setup_display sets _surender = -1 (asm:1149) and
 * _two_players derives _not_player (asm:11779-11786). The end-of-pass
 * check (asm:4323-4347) compares both, so ticking fixtures need them. */
static void start_state(void) { A4_WORD(0x8258)=-1; not_player=player ? 0 : 1; }
/* _move_peeps asm:3296-3301,3459-3474: owner's signed pre-move word sum. */
static void expect_good_pop(uint8_t *expected, int owner, int32_t value)
{ memcpy(expected+(A4_DISP(0x99fe + owner*16)-prg_a4_area),&value,4); }
static void expect_good_pop_add(uint8_t *expected, int owner, int32_t delta)
{
    int32_t value; memcpy(&value,expected+(A4_DISP(0x99fe + owner*16)-prg_a4_area),4);
    expect_good_pop(expected,owner,value+delta);
}
/* Call just before a compared _move_peeps(): live A4 is the pre-pass state.
 * asm:3296-3301 resets owners 0/1, asm:3459-3474 adds each positive word by
 * owner byte. Explicit builder adjustments (mid-pass children) are kept. */
static void expect_pass_good_pop(uint8_t *expected)
{
    int32_t sums[256]={0};
    for (int person=0; person<no_peeps; ++person)
        if (peep_word(person,4)>0) sums[peeps[person*22+1]]+=peep_word(person,4);
    for (int owner=0; owner<256; ++owner) {
        int32_t built,live=(int32_t)A4_LONG(0x99fe + owner*16);
        memcpy(&built,expected+(A4_DISP(0x99fe + owner*16)-prg_a4_area),4);
        if (owner<2 || sums[owner]) expect_good_pop(expected,owner,(owner<2 ? 0 : live)+sums[owner]+built-live);
    }
}
static void setup_reinforcement(int newcomer, int owner, int colocated, int representative)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    memset(map_blk,1,4096);
    no_peeps=4; core_result=core_settlements=0; core_battles=17; prg_quit=0;
    int first=newcomer==0 ? 1 : 0, second=first+1;
    for (int side=0; side<2; ++side) {
        int person=first+side;
        peeps[person*22]=side ? 9 : 10; peeps[person*22+1]=side;
        peeps[person*22+2]=3; peeps[person*22+3]=7;
        set_peep_word(person,4,100); set_peep_word(person,6,side ? first : second);
        set_peep_word(person,8,650); set_peep_word(person,10,side ? -64 : 1);
        set_peep_word(person,12,side ? 35 : 102);
        for (int byte=14; byte<22; ++byte) peeps[person*22+byte]=byte+side;
    }
    peeps[newcomer*22]=2; peeps[newcomer*22+1]=owner;
    peeps[newcomer*22+2]=1; peeps[newcomer*22+3]=1;
    /* Not colocated: arrived at 650 from 649, whose claim it still holds
     * (_move_explorer asm:4590-4617); the join is its next departure. */
    set_peep_word(newcomer,4,45); set_peep_word(newcomer,8,650);
    if (!colocated) set_peep_word(newcomer,10,1);
    set_peep_word(newcomer,6,23); set_peep_word(newcomer,12,7);
    map_who[650]=first+representative+1;
    if (!colocated) map_who[649]=newcomer+1;
    core_leader(owner)=newcomer+1; core_leader(1-owner)=first+(1-owner)+1;
    core_target(owner)=651; view_who=newcomer+1;
    A4_LONG(0x9a02)=1234; A4_LONG(0x9a12)=5678;
    A4_WORD(0xae7a)=4; A4_WORD(0xae7c)=5; seed=123;
}

static void test_battle_reinforcement(void)
{
    /* Catch wrong-side joins, friendly-merge frame resets, signed weapons,
     * wide host-pointer copies, lost references and over-eager map cleanup. */
    for (int newcomer=0; newcomer<=2; newcomer+=2)
    for (int owner=0; owner<2; ++owner)
    for (int colocated=0; colocated<2; ++colocated)
    for (int representative=0; representative<2; ++representative)
    for (int variant=0; variant<3; ++variant) {
        setup_reinforcement(newcomer,owner,colocated,representative);
        int first=newcomer==0 ? 1 : 0, recipient=first+owner, opponent=first+1-owner;
        if (variant==1) {
            set_peep_word(recipient,4,31990); peeps[newcomer*22+3]=200;
            const uint8_t metadata[4]={0x81,0,0xff,0x37};
            memcpy(peeps+newcomer*22+14,metadata,sizeof metadata);
        } else if (variant==2) {
            peeps[recipient*22+3]=200;
            /* No leader keeps magnet navigation deterministic even when colocated. */
            core_leader(owner)=0; view_who=opponent+1;
            if (!colocated) {
                peeps[66]=2; peeps[67]=owner; set_peep_word(3,4,12);
                set_peep_word(3,8,649); map_who[649]=4;
            }
        }
        uint8_t expected[22], enemy[22], third[22], occupancy[4096];
        memcpy(expected,peeps+recipient*22,sizeof expected);
        memcpy(enemy,peeps+opponent*22,sizeof enemy); memcpy(third,peeps+66,sizeof third);
        memcpy(occupancy,map_who,sizeof occupancy);
        int16_t population=variant==1 ? 32000 : 145;
        memcpy(expected+4,&population,sizeof population);
        if (variant==1) {
            expected[3]=200; memcpy(expected+14,peeps+newcomer*22+14,4);
        }
        if (!colocated && variant!=2) occupancy[649]=0;
        core_move(newcomer);
        assert(!memcmp(expected,peeps+recipient*22,sizeof expected));
        assert(!memcmp(enemy,peeps+opponent*22,sizeof enemy));
        assert(!memcmp(third,peeps+66,sizeof third));
        assert(!memcmp(occupancy,map_who,sizeof occupancy));
        assert(!peeps[newcomer*22] && !peep_word(newcomer,4));
        assert(peep_word(newcomer,8)==650);
        assert(core_leader(owner)==(variant==2 ? 0 : recipient+1));
        assert(core_leader(1-owner)==opponent+1);
        assert(view_who==(variant==2 ? opponent+1 : recipient+1));
        assert(seed==123 && core_mana(0)==1234 && core_mana(1)==5678);
        assert(A4_WORD(0xae7a)==4 && A4_WORD(0xae7c)==5);
        assert(core_battles==17 && !core_settlements && !A4_WORD(0xff88+651*2));
    }
}

static void test_invalid_battle_blocks_reinforcement(void)
{
    for (int owner=0; owner<2; ++owner) for (int colocated=0; colocated<2; ++colocated)
    for (int invalid=0; invalid<7; ++invalid) {
        setup_reinforcement(2,owner,colocated,0);
        switch (invalid) {
        case 0: set_peep_word(0,6,-1); break;
        case 1: set_peep_word(0,6,no_peeps); break;
        case 2: set_peep_word(0,6,0); break;
        case 3: set_peep_word(1,6,2); break;
        case 4: set_peep_word(1,4,0); break;
        case 5: peeps[22]=2; break;
        case 6: peeps[23]=0; break;
        }
        peeps[2*22+21]=1; /* Navigation writes the already selected direction. */
        uint8_t before[sizeof prg_a4_area], saved[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        /* A rejected _join_battle falls through to the departure (asm:4536,
         * 4560-4617): release 649, count 650 (represented by the battle), step. */
        if (!colocated) map_who[649]=0;
        A4_WORD(0xff88+650*2)=1; set_peep_word(2,8,651); set_peep_word(2,10,1);
        memcpy(saved,prg_a4_area,sizeof saved); memcpy(prg_a4_area,before,sizeof before);
        core_move(2);
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
        assert(core_battles==17 && !core_settlements);
    }
}

static void test_reinforcement_helper_guards(void)
{
    /* Direct calls cover invalid arguments that movement itself filters out. */
    const int invalid_indices[]={-1,4,208,32767};
    for (unsigned index=0; index<sizeof invalid_indices/sizeof invalid_indices[0]; ++index) {
        setup_reinforcement(2,0,1,0);
        uint8_t saved[sizeof prg_a4_area]; memcpy(saved,prg_a4_area,sizeof saved);
        core_join_battle(invalid_indices[index],0);
        core_join_battle(2,invalid_indices[index]);
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
    }
    for (int invalid=0; invalid<10; ++invalid) {
        setup_reinforcement(2,0,1,0);
        switch (invalid) {
        case 0: set_peep_word(2,4,-1); break;
        case 1: set_peep_word(2,4,0); break;
        case 2: peeps[44]=8; set_peep_word(2,6,0); set_peep_word(0,6,2); break;
        case 3: peeps[44]=1; break;
        case 4: peeps[44]=0x42; break;
        case 5: peeps[45]=2; break;
        case 6: set_peep_word(0,4,0); break;
        case 7: peeps[0]=2; break;
        case 8: peeps[1]=2; break;
        case 9: peeps[23]=2; break;
        }
        uint8_t saved[sizeof prg_a4_area]; memcpy(saved,prg_a4_area,sizeof saved);
        core_join_battle(2,0);
        core_join_battle(0,0);
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
        assert(core_battles==17 && !core_settlements);
    }
    /* A retained leader reference need not point at the source, even colocated. */
    setup_reinforcement(2,0,1,0); core_leader(0)=1; view_who=2;
    core_join_battle(2,0);
    assert(core_leader(0)==1 && view_who==2 && seed==123);
    assert(peep_word(0,4)==145 && peep_word(1,4)==100 && !peep_word(2,4));
}

static void test_reinforcement_tick_and_death(void)
{
    for (int newcomer=0; newcomer<=2; newcomer+=2) for (int owner=0; owner<2; ++owner) {
        setup_reinforcement(newcomer,owner,1,0);
        int first=newcomer==0 ? 1 : 0, recipient=first+owner, opponent=first+1-owner;
        peeps[first*22]=peeps[(first+1)*22]=8;
        peeps[first*22+3]=peeps[(first+1)*22+3]=peeps[newcomer*22+3]=0;
        A4_WORD(0x2fb0)=1;
        game_tick();
        assert(!peeps[newcomer*22] && !peep_word(newcomer,4));
        assert(peep_word(recipient,4)==135 && peep_word(opponent,4)==90);
        /* Each slot counts at its visit (asm:3459-3474): the newcomer's 45,
         * minus 45 again when its recipient is a later slot (asm:6725-6740);
         * the low battle slot before the 10/10 loss (100, or 145 once joined),
         * the high slot after it. Owner 0 is low, owner 1 high. */
        assert(A4_LONG(0x99fe + owner*16)==(owner ? 135u : 145u));
        assert(A4_LONG(0x99fe + (1-owner)*16)==(owner ? 100u : 90u));
        assert(core_battles==17 && !core_result);
        set_peep_word(opponent,4,10);
        core_battle(first,first+1);
        assert(peep_word(recipient,4)==125 && !peep_word(opponent,4));
        assert(peeps[recipient*22]==6 && peep_word(recipient,12)==85 && map_who[650]==recipient+1);
        assert(core_leader(owner)==recipient+1 && view_who==recipient+1);
        assert(core_mana(0)==1234 && core_mana(1)==5678); /* Fixture reward is zero. */
        assert(A4_WORD(0xae7a+owner*2)==(owner ? 6 : 5));
    }
    /* Expired waiting attrition may go negative before a current-cell encounter. */
    for (int colocated=0; colocated<2; ++colocated) for (int population=1; population<=3; ++population) {
        setup_reinforcement(2,0,colocated,0);
        peeps[0]=peeps[22]=8; peeps[3]=peeps[25]=peeps[47]=0;
        peeps[44]=0x42; set_peep_word(2,6,15); set_peep_word(2,12,101);
        set_peep_word(2,4,population); A4_WORD(0x2fb0)=2;
        _move_peeps();
        assert(peep_word(0,4)==(population==3 ? 91 : 90) && peep_word(1,4)==90);
        assert(!peeps[44] && !peep_word(2,4));
        assert(peeps[0]==8 && peeps[22]==8 && peep_word(0,6)==1 && peep_word(1,6)==0);
        assert(map_who[650]==1 && !map_who[649] && core_battles==17);
        assert(core_leader(0)==(population==3 ? 1 : 0));
    }
}

static void test_blocked_reinforcement_retry(void)
{
    setup_reinforcement(0,0,1,0);
    peeps[22]=peeps[44]=8; peeps[3]=peeps[25]=peeps[47]=0;
    set_peep_word(1,4,1000); set_peep_word(2,4,1000);
    memset(map_blk,47,4096); map_blk[650]=1;
    uint8_t pair[44]; memcpy(pair,peeps+22,sizeof pair);
    core_move(0);
    assert(peeps[0]==0x42 && peep_word(0,6)==7 && peep_word(0,4)==45);
    assert(!memcmp(pair,peeps+22,sizeof pair) && map_who[650]==2 && seed==123);
    map_blk[651]=1; A4_WORD(0x2fb0)=1;
    for (int tick=1; tick<=8; ++tick) {
        _move_peeps();
        assert(peeps[0]==0x42 && peep_word(0,6)==7+tick && peep_word(0,4)==45-tick);
        assert(peep_word(1,4)==1000-10*tick && peep_word(2,4)==1000-10*tick);
        assert(map_who[650]==2 && core_leader(0)==1);
    }
    _move_peeps();
    assert(!peeps[0] && !peep_word(0,4));
    assert(peep_word(1,4)==946 && peep_word(2,4)==910);
    assert(peeps[22]==8 && peeps[44]==8 && peep_word(1,6)==2 && peep_word(2,6)==1);
    assert(map_who[650]==2 && !map_who[651] && core_leader(0)==2 && view_who==2);
    assert(core_battles==17 && !core_settlements);
}

static void setup_friendly_merge(int source, int owner)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    memset(map_blk,1,4096);
    no_peeps=3; core_result=core_battles=core_settlements=0; prg_quit=0;
    int target=1-source;
    for (int person=0; person<3; ++person) {
        peeps[person*22]=2; peeps[person*22+1]=owner;
        peeps[person*22+2]=3; peeps[person*22+3]=5;
        set_peep_word(person,4,200); set_peep_word(person,6,23);
        set_peep_word(person,8,person==source ? 649 : 650);
        set_peep_word(person,10,-64); set_peep_word(person,12,101);
        for (int byte=14; byte<22; ++byte) peeps[person*22+byte]=byte;
    }
    memset(peeps+source*22+14,0,4);
    set_peep_word(source,4,100); peeps[source*22+21]=1;
    set_peep_word(2,8,900); peeps[45]=1-owner;
    map_who[649]=source+1; map_who[650]=target+1; map_who[900]=3;
    core_leader(owner)=source+1; core_leader(1-owner)=3;
    core_target(owner)=650; view_who=source+1;
    A4_LONG(0x99fe)=12345; A4_LONG(0x9a0e)=23456;
    seed=123;
}

static void test_friendly_merge_state(void)
{
    /* Catch missing wait/frame resets, signed weapons, metadata overcopy,
     * the index-dependent good_pop correction, lost references and unrelated
     * occupancy cleanup. */
    const int flags[]={2,0x22,0x42,0x62,0x61,1};
    const int populations[][3]={{100,200,300},{16000,16000,32000},{32000,1,32000}};
    for (int source=0; source<2; ++source) for (int owner=0; owner<2; ++owner)
    for (unsigned state=0; state<sizeof flags/sizeof flags[0]; ++state)
    for (int variant=0; variant<3; ++variant) {
        setup_friendly_merge(source,owner);
        int target=1-source;
        /* Phased movement: the source arrived at 650 from 649 (claim kept
         * there); its next departure meets the target at its own cell. */
        set_peep_word(source,8,650); set_peep_word(source,10,1); core_target(owner)=651;
        peeps[target*22]=flags[state];
        set_peep_word(source,4,populations[variant][0]);
        set_peep_word(target,4,populations[variant][1]);
        peeps[(variant==1 ? source : target)*22+3]=200;
        if (variant==1 && flags[state]!=1) {
            const uint8_t metadata[4]={0x81,0,0xff,0x37};
            memcpy(peeps+source*22+14,metadata,4);
        }
        if (variant==2) {
            core_leader(owner)=target+1; view_who=3;
            peeps[45]=owner; set_peep_word(2,8,649); map_who[649]=3;
        }
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        /* Build the complete expected state without using merge logic. */
        set_peep_word(target,4,populations[variant][2]);
        peeps[target*22+3]=200;
        if (variant==1 && flags[state]!=1) memcpy(peeps+target*22+14,peeps+source*22+14,4);
        peeps[target*22]=(flags[state]&1) ? 1 : 2; set_peep_word(target,12,0);
        core_leader(owner)=target+1; if (variant!=2) view_who=target+1;
        set_peep_word(source,4,0); peeps[source*22]=0;
        if (variant!=2) map_who[649]=0;
        /* Variant 2 follows its co-located leader: magnet history north. */
        if (variant==2) peeps[source*22+21]=(uint8_t)-64;
        /* asm:5605-5620: an unvisited (higher) target will count the merged
         * word, so the source's word leaves its owner's running good_pop,
         * even past the $7d00 clamp. */
        if (target>source) A4_LONG(0x99fe + owner*16)-=(uint32_t)populations[variant][0];
        uint8_t wanted[sizeof prg_a4_area]; memcpy(wanted,prg_a4_area,sizeof wanted);
        memcpy(prg_a4_area,expected,sizeof expected);
        core_move(source);
        assert(!memcmp(wanted,prg_a4_area,sizeof wanted));
        assert(!core_battles && !core_settlements);
    }
}

/* The source arrived at 650 from 649. A rejected _join_forces falls through
 * to the ordinary departure (asm:4547,4560-4617): release 649, count 650
 * (target keeps it) and step east. */
static void friendly_merge_arrived(int source, int owner)
{
    set_peep_word(source,8,650); set_peep_word(source,10,1); core_target(owner)=651;
}
static void friendly_merge_rejected(uint8_t *saved, int source)
{
    memcpy(saved,prg_a4_area,sizeof prg_a4_area);
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    set_peep_word(source,8,651); map_who[649]=0;
    A4_WORD(0xff88+650*2)=1;
    memcpy(saved,prg_a4_area,sizeof prg_a4_area);
    memcpy(prg_a4_area,before,sizeof before);
}
static void test_friendly_merge_metadata_rejection(void)
{
    for (int source=0; source<2; ++source) for (int owner=0; owner<2; ++owner)
    for (int byte=14; byte<18; ++byte) {
        setup_friendly_merge(source,owner); friendly_merge_arrived(source,owner);
        peeps[(1-source)*22]=1;
        peeps[source*22+byte]=0x80; peeps[source*22+3]=200;
        uint8_t saved[sizeof prg_a4_area]; friendly_merge_rejected(saved,source);
        core_move(source);
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
        assert(!core_battles && !core_settlements);
    }
}

static void test_friendly_merge_dead_target(void)
{
    const int populations[]={0,-1};
    for (int source=0; source<2; ++source) for (int owner=0; owner<2; ++owner)
    for (unsigned index=0; index<sizeof populations/sizeof populations[0]; ++index) {
        setup_friendly_merge(source,owner); friendly_merge_arrived(source,owner);
        set_peep_word(1-source,4,populations[index]);
        uint8_t saved[sizeof prg_a4_area]; friendly_merge_rejected(saved,source);
        core_move(source);
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
    }
}

static void test_friendly_merge_helper_guards(void)
{
    const int invalid_indices[]={-1,3,208,32767};
    for (unsigned index=0; index<sizeof invalid_indices/sizeof invalid_indices[0]; ++index) {
        setup_friendly_merge(0,0);
        uint8_t saved[sizeof prg_a4_area]; memcpy(saved,prg_a4_area,sizeof saved);
        core_join_forces(invalid_indices[index],1);
        core_join_forces(0,invalid_indices[index]);
        core_join_forces(0,0);
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
    }
    for (int invalid=0; invalid<11; ++invalid) {
        setup_friendly_merge(0,0);
        switch (invalid) {
        case 0: set_peep_word(0,4,0); break;
        case 1: set_peep_word(0,4,-1); break;
        case 2: peeps[0]=1; break;
        case 3: peeps[0]=8; break;
        case 4: peeps[0]=0x22; break;
        case 5: peeps[0]=0x42; break;
        case 6: peeps[1]=peeps[23]=2; break;
        case 7: set_peep_word(1,4,0); break;
        case 8: set_peep_word(1,4,-1); break;
        case 9: peeps[23]=1; break;
        case 10: peeps[22]=10; break;
        }
        uint8_t saved[sizeof prg_a4_area]; memcpy(saved,prg_a4_area,sizeof saved);
        core_join_forces(0,1);
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
    }
    /* The helper does not navigate or replace unrelated leader/view references. */
    setup_friendly_merge(0,0); core_leader(0)=3; view_who=3; peeps[21]=0xab;
    core_join_forces(0,1);
    assert(core_leader(0)==3 && view_who==3 && peeps[21]==0xab);
    assert(peep_word(1,4)==300 && peep_word(1,12)==0 && !peep_word(0,4));
}

static void test_friendly_merge_tick_totals(void)
{
    for (int source=0; source<2; ++source) for (int owner=0; owner<2; ++owner) {
        setup_friendly_merge(source,owner); friendly_merge_arrived(source,owner);
        set_peep_word(source,12,7); set_peep_word(1-source,12,0);
        set_peep_word(2,12,0);
        game_tick();
        assert(!peep_word(source,4) && peep_word(1-source,4)==300);
        assert(A4_LONG(0x99fe + owner*16)==300 && A4_LONG(0x99fe + (1-owner)*16)==200);
        assert(!core_result && !core_battles && !core_settlements);
    }
}

static void test_friendly_merge_town_and_split(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for (int owner=0; owner<2; ++owner) {
        assert(game_reset(0));
        memset(peeps,0,208*22); memset(map_who,0,4096); no_peeps=3;
        int position=50*64+50;
        for (int person=0; person<3; ++person) {
            peeps[person*22]=2; peeps[person*22+1]=owner;
            peeps[person*22+2]=1;
            set_peep_word(person,4,100); set_peep_word(person,8,position);
        }
        peeps[22]=1; set_peep_word(1,12,42); core_set_town(1,0);
        /* The split hands the town's cell to the child (asm:3961-3983), so
         * it departs instead of meeting (and merging back into) its town. */
        map_who[position]=1; core_tendency(owner)=0; core_leader(owner)=1; core_target(owner)=position+1;
        core_move(0);
        assert(peep_word(0,4)==100 && peep_word(1,4)==100 && peeps[22]==1);
        assert(peep_word(0,8)==position+1 && map_bk2[position]==42 && map_who[position]==1);
        map_who[position]=2;
        /* A nearby return: re-entering the town cell is not an encounter;
         * the next departure meets the town at its own cell (asm:4510-4547). */
        core_target(owner)=position; peeps[22]=0x61; set_peep_word(1,12,101);
        uint8_t terrain[4096], buildings[4096], split[22];
        memcpy(terrain,map_blk,sizeof terrain); memcpy(buildings,map_bk2,sizeof buildings);
        memcpy(split,peeps+44,sizeof split);
        core_move(0);
        assert(peep_word(0,8)==position && peep_word(0,4)==100 && peeps[22]==0x61);
        assert(map_who[position]==2 && map_who[position+1]==1);
        core_move(0);
        assert(!peep_word(0,4) && peep_word(1,4)==200 && peeps[22]==1);
        assert(peep_word(1,12)==0 && map_who[position]==2 && !map_who[position+1]);
        assert(!memcmp(terrain,map_blk,sizeof terrain) && !memcmp(buildings,map_bk2,sizeof buildings));
        assert(!memcmp(split,peeps+44,sizeof split));
        /* Restoring the frame precedes all town mana/weapon/growth lookups. */
        game_turn=8; A4_WORD(0x2fb6+20)=17; A4_WORD(0x2fcc+20)=9; A4_WORD(0x2fe2+20)=11;
        int mana=core_mana(owner);
        core_settle_tick(1);
        assert(peep_word(1,12)==42 && peep_word(1,4)==211);
        assert(core_mana(owner)==mana+17 && peeps[25]==9 && map_bk2[position]==42);
        assert(!memcmp(split,peeps+44,sizeof split));
    }
    _closeall(); SDL_Quit();
}

static void setup_blocked_walker(void)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    memset(map_blk,47,4096);
    no_peeps=1; core_battles=core_settlements=0;
    peeps[0]=2; peeps[2]=peeps[3]=1;
    set_peep_word(0,4,100); set_peep_word(0,8,650);
    map_blk[650]=1; map_who[650]=1;
    core_tendency(0)=1; A4_WORD(0x2fb0)=1;
}

static void setup_magnet_walker(int owner)
{
    setup_blocked_walker();
    memset(map_blk,15,4096);
    peeps[1]=owner; core_tendency(owner)=0; core_target(owner)=651;
    core_result=0; seed=123; view_who=0;
}

static void test_magnet_signs_and_center(void)
{
    /* Literal x-major sign table catches center exploration and transposition. */
    const int targets[] = {585,649,713,586,650,714,587,651,715};
    const int deltas[] = {-65,-1,63,-64,-64,64,-63,1,65};
    for (int owner=0; owner<2; ++owner) for (int sign=0; sign<9; ++sign) {
        setup_magnet_walker(owner); core_target(owner)=targets[sign];
        peeps[21]=0x80;
        assert(core_magnet_direction(0)==deltas[sign]);
        assert((int8_t)peeps[21]==deltas[sign] && seed==123);
        assert(core_leader(owner)==(sign==4) && view_who==(sign==4));
        assert(peep_word(0,8)==650 && peeps[0]==2 && !core_settlements);
    }
    /* Center uses the same north move even on a settleable flat plot. */
    setup_magnet_walker(0); core_target(0)=650;
    core_move(0);
    assert(peep_word(0,8)==586 && peep_word(0,10)==-64 && peeps[0]==2);
    /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
    assert(map_who[650]==1 && !map_who[586] && !core_settlements && seed==123);
    assert(core_leader(0)==1 && view_who==1);
}

static void test_magnet_leaders(void)
{
    for (int owner=0; owner<2; ++owner) for (int person=0; person<2; ++person)
        for (int tracked=0; tracked<3; ++tracked) for (int claimed=0; claimed<2; ++claimed) {
            setup_magnet_walker(owner); no_peeps=2;
            memcpy(peeps+22,peeps,22); set_peep_word(1-person,8,651);
            set_peep_word(person,8,650); core_target(owner)=650;
            core_leader(owner)=claimed ? person+1 : 0;
            core_leader(1-owner)=77; view_who=tracked;
            uint8_t other[22]; memcpy(other,peeps+(1-person)*22,sizeof other);
            assert(core_magnet_direction(person)==-64);
            assert(core_leader(owner)==person+1 && core_leader(1-owner)==77);
            assert(view_who==(!claimed && !tracked ? person+1 : tracked));
            assert(!memcmp(other,peeps+(1-person)*22,sizeof other));
            assert(peep_owner(person)==owner && seed==123);
        }
    for (int owner=0; owner<2; ++owner) {
        setup_magnet_walker(owner); no_peeps=2;
        memcpy(peeps+22,peeps,22); core_leader(owner)=2; view_who=19;
        set_peep_word(1,8,714); core_target(owner)=649;
        assert(core_magnet_direction(0)==64);
        set_peep_word(1,8,586);
        assert(core_magnet_direction(0)==-64); /* Follow current, not cached position. */
        assert(core_leader(owner)==2 && view_who==19);
    }
    /* Invalid references use the magnet, without repairing refs or records. */
    const int refs[] = {-1,3,209,32767,2,2,2,2};
    for (int owner=0; owner<2; ++owner) for (int sample=0; sample<8; ++sample) {
        setup_magnet_walker(owner); no_peeps=2;
        memcpy(peeps+22,peeps,22); set_peep_word(1,8,649);
        if (sample==4) set_peep_word(1,4,0);
        if (sample==5) peeps[23]=1-owner;
        if (sample==6) set_peep_word(1,8,-1);
        if (sample==7) set_peep_word(1,8,4096);
        core_leader(owner)=refs[sample]; core_target(owner)=650; view_who=17;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==-64);
        assert((int8_t)peeps[21]==-64);
        peeps[21]=0;
        assert(a4_same(before));
    }
}

static void test_magnet_fallback(void)
{
    const int deltas[] = {-64,-63,1,65,64,63,-1,-65};
    const int opposites[] = {64,63,-1,-65,-64,-63,1,65};
    const int failed_avoidance[] = {65,-64,63,-1,-65,-64,-63,1};
    /* Each sole opening must be found, including the eighth attempt. */
    for (int direction=0; direction<8; ++direction) {
        setup_magnet_walker(0); memset(map_blk,47,4096); map_blk[650]=15;
        int primary=(direction+1)%8;
        core_target(0)=650+deltas[primary]; map_blk[650+deltas[direction]]=15;
        assert(core_magnet_direction(0)==deltas[direction]);
        assert((int8_t)peeps[21]==opposites[direction]);
        /* Signed old avoidance rejects the only opening, including negatives. */
        peeps[21]=(uint8_t)deltas[direction];
        assert(core_magnet_direction(0)==999);
        assert((int8_t)peeps[21]==failed_avoidance[primary]);
    }
    for (int primary=0; primary<8; ++primary) {
        setup_magnet_walker(0); memset(map_blk,47,4096); map_blk[650]=15;
        core_target(0)=650+deltas[primary]; peeps[21]=0x80;
        assert(core_magnet_direction(0)==999);
        assert((int8_t)peeps[21]==failed_avoidance[primary]);
    }
    setup_magnet_walker(0); map_blk[651]=47;
    assert(core_magnet_direction(0)==-63); /* East primary starts northeast. */
    assert((int8_t)peeps[21]==63);
    setup_magnet_walker(0); map_blk[651]=47; peeps[21]=(uint8_t)-63;
    assert(core_magnet_direction(0)==65); /* Skip NE then blocked E, choose SE. */
    assert((int8_t)peeps[21]==-65);
    setup_magnet_walker(0); memset(map_blk,47,4096); map_blk[650]=15;
    core_target(0)=651; map_blk[586]=15; /* N is the eighth candidate for E. */
    assert(core_magnet_direction(0)==-64 && (int8_t)peeps[21]==64);
}

static void test_magnet_war_water_and_bounds(void)
{
    const int wars[] = {0,1,0x8000,0xffff};
    for (int sample=0; sample<4; ++sample) {
        setup_magnet_walker(0); A4_WORD(0xae80)=wars[sample]; map_blk[651]=47;
        assert(core_valid_move(650,1)==2);
        assert(core_magnet_direction(0)==(wars[sample] ? 1 : -63));
        assert((int8_t)peeps[21]==(wars[sample] ? 1 : 63));
        core_move(0);
        assert(peep_word(0,8)==(wars[sample] ? 651 : 587));
        /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
        assert(!map_who[peep_word(0,8)] && map_who[650]==1);
        assert(core_valid_move(650,1)==2); /* Never weaken generic validity. */
    }
    for (int active=0; active<2; ++active) {
        setup_magnet_walker(0); A4_WORD(0xae80)=active;
        map_blk[651]=0; map_blk[587]=47;
        A4_WORD(0x82bc)=1; A4_WORD(0x82c4)=0xffff; /* Queue producer conditions. */
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==65); /* Water and fallback 47 rejected. */
        assert((int8_t)peeps[21]==-65 && peep_word(0,8)==650);
        peeps[21]=0;
        {
            /* War and nonwar controller-1 source producers, and nothing
             * else: retain the complete no-terrain/mana/RNG snapshot check. */
            assert(A4_DISP(0x82b6)[0]==1 && A4_DISP(0x82b7)[0]==10 && A4_DISP(0x82b8)[0]==10);
            assert(A4_WORD(0x82be)==1);
            memset(A4_DISP(0x82b6),0,3); A4_WORD(0x82be)=0;
        }
        assert(a4_same(before)); /* Only command/latch/history changed. */
        memset(map_blk,47,4096); map_blk[650]=15; map_blk[651]=0;
        assert(core_magnet_direction(0)==999); /* War does not admit fallback 47. */
    }
    const int positions[] = {0,63,4032,4095};
    const int targets[] = {0,63,4032,4095};
    const int expected[] = {1,64,-64,-64};
    for (int corner=0; corner<4; ++corner) {
        setup_magnet_walker(0); memset(map_who,0,4096);
        set_peep_word(0,8,positions[corner]); core_target(0)=targets[corner];
        if (corner==1) map_blk[positions[corner]-65+64]=47; /* Block west at top right. */
        core_move(0);
        assert(peep_word(0,8)==positions[corner]+expected[corner]);
        /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
        assert(map_who[positions[corner]]==1 && !map_who[positions[corner]+expected[corner]]);
    }
    /* Opaque metadata cannot be a host pointer: bounded magnet fallback only. */
    for (int byte=14; byte<18; ++byte) {
        setup_magnet_walker(0); peeps[byte]=0xff; A4_WORD(0xae80)=1;
        uint8_t record[22]; memcpy(record,peeps,sizeof record);
        assert(core_magnet_direction(0)==1 && peeps[byte]==0xff);
        peeps[21]=0; assert(!memcmp(record,peeps,sizeof record));
    }
}

static void test_magnet_dispatch_and_wait(void)
{
    for (int owner=0; owner<2; ++owner) for (int tendency=0; tendency<4; ++tendency) {
        setup_magnet_walker(owner); core_tendency(owner)=tendency; A4_WORD(0xae80)=1;
        core_target(owner)=586;
        core_move(0);
        assert(peep_word(0,8)==586 && peep_word(0,10)==-64 && (int8_t)peeps[21]==-64);
        /* _move_explorer asm:4590-4617: count/claim the departure cell. */
        assert(map_who[650]==1 && !map_who[586] && peeps[0]==2 && seed==123);
        assert(!core_settlements && A4_WORD(0xff88+650*2)==1 && !A4_WORD(0xff88+586*2));
    }
    setup_magnet_walker(0); memset(map_blk,47,4096); map_blk[650]=15;
    set_peep_word(0,12,7);
    _move_peeps();
    assert(peeps[0]==0x42 && peep_word(0,6)==7 && peep_word(0,12)==0);
    assert(peep_word(0,4)==99 && (int8_t)peeps[21]==63);
    map_blk[651]=15;
    for (int tick=1; tick<=8; ++tick) {
        _move_peeps();
        assert(peeps[0]==0x42 && peep_word(0,6)==7+tick && peep_word(0,8)==650);
        assert(peep_word(0,4)==99-tick && peep_word(0,12)==(tick%2 ? 101 : 102));
    }
    _move_peeps();
    assert(peeps[0]==2 && peep_word(0,6)==16 && peep_word(0,8)==651);
    assert(peep_word(0,10)==1 && peep_word(0,12)==0 && peep_word(0,4)==90);
    /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
    assert(map_who[650]==1 && !map_who[651] && (int8_t)peeps[21]==1);
    assert(!core_battles && !core_settlements);
}

static void test_magnet(void)
{
    test_magnet_signs_and_center(); test_magnet_leaders(); test_magnet_fallback();
    test_magnet_war_water_and_bounds(); test_magnet_dispatch_and_wait();
}

static void test_blocked_entry(void)
{
    setup_blocked_walker();
    set_peep_word(0,10,-64); set_peep_word(0,12,6);
    uint8_t occupancy[4096]; memcpy(occupancy,map_who,sizeof occupancy);
    assert(core_explore(0)==999);
    core_move(0);
    assert(peeps[0]==0x42 && peep_word(0,6)==7);
    assert(peep_word(0,8)==650 && peep_word(0,10)==-64 && peep_word(0,12)==6);
    assert(peep_word(0,4)==100 && !memcmp(occupancy,map_who,sizeof occupancy));
    assert(!core_battles && !core_settlements);
    /* Successful navigation clears only the blocked bit (asm:4437). */
    map_blk[651]=1; core_tendency(0)=0; core_leader(0)=1; core_target(0)=651;
    core_move(0);
    assert(peeps[0]==2 && peep_word(0,8)==651 && peep_word(0,10)==1);
    /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
    assert(map_who[650]==1 && !map_who[651] && peep_word(0,12)==6);
}

static void test_blocked_retry(void)
{
    setup_blocked_walker(); set_peep_word(0,12,7);
    _move_peeps();
    assert(peeps[0]==0x42 && peep_word(0,6)==7 && peep_word(0,12)==0);
    assert(peep_word(0,4)==99);
    for (int tick=1; tick<=8; ++tick) {
        _move_peeps();
        assert(peeps[0]==0x42 && peep_word(0,6)==7+tick);
        assert(peep_word(0,4)==99-tick && peep_word(0,12)==(tick%2 ? 101 : 102));
        assert(peep_word(0,8)==650 && map_who[650]==1);
    }
    _move_peeps();
    assert(peeps[0]==0x42 && peep_word(0,6)==7 && peep_word(0,12)==0);
    assert(peep_word(0,4)==90 && peep_word(0,8)==650 && map_who[650]==1);
    assert(!core_battles && !core_settlements);
}

static void test_wait_boundaries(void)
{
    /* Seed collision waits: generating 0x20 still requires phase occupancy. */
    const int flags[] = {0x22,0x42,0x62};
    const int counters[] = {0,14,15,0xffff};
    for (unsigned state=0; state<sizeof flags/sizeof flags[0]; ++state)
        for (unsigned count=0; count<sizeof counters/sizeof counters[0]; ++count) {
            setup_blocked_walker(); peeps[0]=flags[state];
            set_peep_word(0,6,counters[count]); set_peep_word(0,12,101);
            /* Opening a route must not bypass the unsigned postincrement timeout. */
            map_blk[651]=1; core_tendency(0)=0; core_leader(0)=1; core_target(0)=651;
            int ticks=counters[count]<=14 ? 16-counters[count] : 1;
            for (int tick=1; tick<ticks; ++tick) {
                _move_peeps();
                assert(peeps[0]==flags[state] && peep_word(0,6)==counters[count]+tick);
                assert(peep_word(0,8)==650 && map_who[650]==1 && !map_who[651]);
                assert(peep_word(0,12)==(tick%2 ? 102 : 101));
                assert(peep_word(0,4)==100-tick);
            }
            _move_peeps();
            assert(peeps[0]==2 && peep_word(0,6)==(counters[count]==0xffff ? 0 : 16));
            assert(peep_word(0,8)==651 && peep_word(0,10)==1 && peep_word(0,12)==0);
            /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
            assert(peep_word(0,4)==100-ticks && map_who[650]==1 && !map_who[651]);
        }
    setup_blocked_walker(); peeps[0]=0x60;
    set_peep_word(0,6,15); set_peep_word(0,12,101);
    _move_peeps(); /* Timeout retries movement only for restored exact flag 2. */
    assert(!peeps[0] && peep_word(0,6)==16 && peep_word(0,4)==99);
    assert(peep_word(0,8)==650 && peep_word(0,12)==102 && map_who[650]==1);
}

static void test_wait_water_and_death(void)
{
    for (int fatal=0; fatal<=1; ++fatal) {
        setup_blocked_walker(); peeps[0]=0x42; set_peep_word(0,6,14);
        map_blk[650]=0; game_mode=fatal;
        _move_peeps();
        assert(peep_word(0,6)==14); /* Water precedes the waiting timer. */
        assert(peep_word(0,4)==100);
        assert(peeps[0]==18 && map_who[650]==1);
    }
    setup_blocked_walker(); peeps[0]=0x42;
    set_peep_word(0,6,7); set_peep_word(0,4,1); core_leader(0)=1;
    _move_peeps();
    assert(!peep_word(0,4) && !peeps[0] && !map_who[650] && !core_leader(0));
    for (int population=1; population<=2; ++population) {
        setup_blocked_walker(); peeps[0]=0x42;
        set_peep_word(0,6,15); set_peep_word(0,4,population);
        set_peep_word(0,12,101); core_leader(0)=1;
        no_peeps=2; peeps[22]=2; peeps[23]=1; peeps[24]=peeps[25]=1;
        set_peep_word(1,4,100); set_peep_word(1,8,650); map_who[650]=2;
        map_blk[651]=1; core_tendency(0)=0; core_target(0)=651;
        _move_peeps();
        assert(core_battles==1); /* Expiry may enter battle before death cleanup. */
        if (population==1) {
            assert(!peeps[0] && !peep_word(0,4) && !core_leader(0));
            assert(peeps[22]==2 && peep_word(1,6)==0 && map_who[650]==2);
        } else {
            assert(peeps[0]==8 && peeps[22]==10 && peep_word(0,4)==1);
            assert(peep_word(0,6)==1 && peep_word(1,6)==0 && map_who[650]==1);
            core_remove(0);
            assert(peeps[22]==2 && peep_word(1,6)==0 && map_who[650]==2);
        }
        assert(peep_word(1,4)==100 && !map_who[651]);
    }
    /* asm:4560-4563: negative expiry population cannot join a destination. */
    setup_blocked_walker(); peeps[0]=0x42; peeps[3]=9;
    A4_WORD(0x2fb0)=2; set_peep_word(0,4,1); set_peep_word(0,6,15);
    set_peep_word(0,12,101); core_leader(0)=1;
    no_peeps=2; peeps[22]=2; peeps[24]=3; peeps[25]=2;
    set_peep_word(1,4,100); set_peep_word(1,6,23);
    set_peep_word(1,8,651); set_peep_word(1,10,-64);
    for (int byte=14; byte<22; ++byte) peeps[22+byte]=byte;
    uint8_t metadata[8]; memcpy(metadata,peeps+22+14,sizeof metadata);
    map_blk[651]=1; map_who[651]=2;
    core_tendency(0)=0; core_target(0)=651;
    _move_peeps();
    assert(peep_word(1,4)==100 && peeps[25]==2 && !core_leader(0));
    assert(peeps[22]==2 && peeps[23]==0 && peeps[24]==3);
    assert(peep_word(1,6)==23 && peep_word(1,8)==651 && peep_word(1,10)==-64);
    assert(peep_word(1,12)==1 && !memcmp(metadata,peeps+22+14,sizeof metadata));
    assert(!peeps[0] && !peep_word(0,4) && peep_word(0,8)==650);
    assert(!map_who[650] && map_who[651]==2 && !A4_WORD(0xff88+651*2));
    assert(!core_battles && !core_settlements);
    /* A battle partner index, even above 14, is not a waiting timer. */
    setup_blocked_walker(); no_peeps=17; peeps[0]=8;
    peeps[16*22]=10; peeps[16*22+1]=1; peeps[16*22+3]=1;
    set_peep_word(0,6,16); set_peep_word(16,6,0);
    set_peep_word(16,4,100); set_peep_word(16,8,650);
    seed=123;
    /* Owner 1's AI pick (tendency 0, asm:9612-9633) takes 15930 first; the
     * battle then rolls 28505%3+1=3 and 12248%3+1=3: lesser strength 300,
     * loss 3*1+10=13, not wait loss 1. */
    _move_peeps();
    assert(peeps[0]==8 && peeps[16*22]==10);
    assert(peep_word(0,6)==16 && peep_word(16,6)==0);
    assert(peep_word(0,4)==87 && peep_word(16,4)==87);
}

/* Seed only the simulation consumer; visible sprite-art parity remains deferred.
 * Nonzero metadata must not trigger a leader prepass. */
static void setup_celebration(int owner)
{
    setup_blocked_walker();
    peeps[0]=6; peeps[1]=owner;
    set_peep_word(0,6,123); set_peep_word(0,10,-64); set_peep_word(0,12,85);
    for (int byte=14; byte<22; ++byte) peeps[byte]=byte;
    core_leader(0)=core_leader(1)=0;
    core_target(0)=1234; core_target(1)=2345;
    A4_LONG(0x9a02)=12345; A4_LONG(0x9a12)=23456;
    seed=123; core_result=0; prg_quit=0;
}

static void celebration_expected(uint8_t *expected, int flags, int16_t frame)
{
    memcpy(expected,prg_a4_area,sizeof prg_a4_area);
    size_t offset=(size_t)(peeps-prg_a4_area);
    expected[offset]=(uint8_t)flags;
    memcpy(expected+offset+12,&frame,sizeof frame);
}

static void celebration_expected_word(uint8_t *expected, int displacement, int16_t value)
{
    memcpy(expected+(A4_DISP(displacement)-prg_a4_area),&value,sizeof value);
}
/* _set_devil_magnet asm:9612-9633 (called from _move_peeps asm:3257-3261):
 * an unlatched owner with tendency 0 and fewer than 15 towns queues 14/x/1
 * with x = newrand%3+1, latches, and draws once. Owner 0 acts first. From
 * seed 123: (123*0x24a1+0x24df)&0x7fff = 15930 -> x 1; then 28505 -> x 3. */
static void expect_ai_pick(uint8_t *expected, int side, int x, int16_t seed_after)
{
    /* A later producer of the same pass (an oracle that already changed this
     * side's command bytes from the pre-pass state) overwrites the pick. */
    uint8_t *mail=expected+(A4_DISP(0x82b6+side*46)-prg_a4_area);
    if (!memcmp(mail,A4_DISP(0x82b6+side*46),3)) { mail[0]=14; mail[1]=x; mail[2]=1; }
    celebration_expected_word(expected,0x82be + side*46,1);
    celebration_expected_word(expected,0x99e2,seed_after);
}
/* For fixtures with zero published towns: each owner, in order, whose latch
 * and tendency are both 0 before the pass picks once, x = r%3+1 with the
 * LCG r = (seed*0x24a1+0x24df)&0x7fff (from 123: 15930 x 1, 28505 x 3).
 * Returns the seed after the AI. */
static uint16_t ai_lcg(uint16_t value) { return (uint16_t)((value*0x24a1u+0x24dfu)&0x7fffu); }
static int16_t expect_ai_fixture_picks(uint8_t *expected)
{
    uint16_t state=(uint16_t)seed;
    for (int side=0; side<2; ++side)
        if (!A4_WORD(0x82be + side*46) && !core_tendency(side)) {
            state=ai_lcg(state);
            expect_ai_pick(expected,side,state%3+1,(int16_t)state);
        }
    return (int16_t)state;
}
/* The same pass inside a game_tick that ends the game: _clear_send
 * (asm:18234-18254) then zeroes both command triples. */
static void expect_ai_fixture_picks_cleared(uint8_t *expected)
{
    expect_ai_fixture_picks(expected);
    for (int side=0; side<2; ++side) memset(expected+(A4_DISP(0x82b6+side*46)-prg_a4_area),0,3);
}
/* A whole game_tick: the picks are then consumed by _get_message (14/x/1 ->
 * _do_action y=1, tendency = x, asm:18410-18431) unless the tick ends the
 * game; either way the command triples end cleared. */
static void expect_ai_fixture_tick(uint8_t *expected, int ends)
{
    uint16_t state=(uint16_t)seed; int picked[2]={0,0};
    for (int side=0; side<2; ++side)
        if (!A4_WORD(0x82be + side*46) && !core_tendency(side)) { state=ai_lcg(state); picked[side]=state%3+1; }
    expect_ai_fixture_picks_cleared(expected);
    if (!ends) for (int side=0; side<2; ++side)
        if (picked[side]) celebration_expected_word(expected,0x99fa+side*16,picked[side]);
}
/* Both owners unlatched with tendency 0: owner 0 then owner 1 (asm:3254-3270). */
static void expect_ai_both_picks(uint8_t *expected)
{
    expect_ai_pick(expected,0,1,15930); expect_ai_pick(expected,1,3,28505);
}
/* The common case: only owner 1 is unlatched, seed 123. */
static void expect_ai_tendency_pick(uint8_t *expected) { expect_ai_pick(expected,1,1,15930); }
/* Same pass inside game_tick ending the game: _clear_send (asm:18234-18254)
 * drops the command bytes, the latch and the draw remain. */
static void expect_ai_pick_cleared(uint8_t *expected)
{
    celebration_expected_word(expected,0x82ec,1);
    celebration_expected_word(expected,0x99e2,15930);
}

/* _stats+0x26 (0x82dc, enemy-indexed, host slot+1): a completed exact-2
 * walker stores itself if empty (asm:4092-4106); a peaceful town stores
 * itself when its age is the oldest so far (asm:3706-3737). */
static void expect_stats_pointer(uint8_t *expected, int person)
{
    uint32_t first=(uint32_t)(person+1);
    memcpy(expected+(A4_DISP(0x82dc+(peeps[person*22+1] ? 0 : 1)*46)-prg_a4_area),&first,4);
}
/* Peaceful town consumer bookkeeping: LAB_52DEA counts non-city frames
 * (asm:3658-3661), then the oldest-town pointer (asm:3706-3737). */
static void expect_town_bookkeeping(uint8_t *expected, int person, int frame)
{
    int owner=peeps[person*22+1];
    if (frame!=42) {
        uint8_t *count=expected+(A4_DISP(0x99fc+owner*16)-prg_a4_area);
        uint16_t value; memcpy(&value,count,2); ++value; memcpy(count,&value,2);
    }
    expect_stats_pointer(expected,person);
    /* asm:3684-3705: first town of the pass beats the zero maximum. */
    uint32_t best=(uint32_t)(person+1);
    memcpy(expected+(A4_DISP(0x82d8+(owner ? 0 : 1)*46)-prg_a4_area),&best,4);
    /* asm:3738-3765: first town younger than the $4e1f start is the owner's
     * own _stats+0x2a pointer. */
    if ((uint16_t)((uint16_t)game_turn-(uint16_t)peep_word(person,6)) < 0x4e1f)
        memcpy(expected+(A4_DISP(0x82e0+owner*46)-prg_a4_area),&best,4);
}
/* asm:4349-4370: a full _move_peeps pass (not a direct consumer call)
 * publishes good_towns (LAB_52DEA, non-city frames) and good_castles (city
 * frames, asm:3637-3645) into _stats+0x16/+0x14 for both owners. */
static void expect_town_publish(uint8_t *expected, int owner, int towns, int castles)
{
    celebration_expected_word(expected,0x82cc+owner*46,towns);
    celebration_expected_word(expected,0x82ca+owner*46,castles);
}

/* _make_level asm:9017-9031: with _stats+0xe bit 0 clear D0 still holds the
 * _stats element address (MAKE_LEVEL_EXIT_BYTE_OWNER0/1, load-address dependent), and the town
 * pass stores its low byte in peep +20 (asm:3793-3797). */
static void expect_make_level_exit(uint8_t *expected, int person)
{
    expected[peeps-prg_a4_area+person*22+20]=peeps[person*22+1] ? MAKE_LEVEL_EXIT_BYTE_OWNER1 : MAKE_LEVEL_EXIT_BYTE_OWNER0;
}

/* Collision-wait producer, _move_peeps asm:4058-4067,4145-4184: an exact-2
 * walker alive after framing (and any move) marks map_who[own cell]'s peep. */
static void setup_collision(int a, int owner_a, int b, int owner_b, int b_flags,
                            int b_life, int b_frame)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    memset(map_blk,47,4096);
    no_peeps=2; core_battles=core_settlements=0; core_result=0; prg_quit=0;
    A4_WORD(0x2fb0)=1; seed=123;
    core_tendency(0)=core_tendency(1)=1;
    peeps[a*22]=2; peeps[a*22+1]=owner_a; peeps[a*22+2]=peeps[a*22+3]=1;
    peeps[a*22+21]=0x77;
    set_peep_word(a,4,100); set_peep_word(a,6,0x4321); set_peep_word(a,8,650);
    peeps[b*22]=b_flags; peeps[b*22+1]=owner_b; peeps[b*22+2]=peeps[b*22+3]=1;
    for (int byte=14; byte<18; ++byte) peeps[b*22+byte]=0;
    peeps[b*22+21]=0x99;
    set_peep_word(b,4,b_life); set_peep_word(b,6,0x1234); set_peep_word(b,8,700);
    set_peep_word(b,10,-63); set_peep_word(b,12,b_frame);
    map_blk[650]=map_blk[700]=1; map_who[650]=b+1;
    core_leader(owner_a)=a+1; view_who=a+1;
}

static void collision_expect_flags(uint8_t *expected, int person, int flags)
{
    expected[peeps-prg_a4_area+person*22]=flags;
}

static void collision_check(const uint8_t *expected, const char *label, int index)
{
    if (memcmp(expected,prg_a4_area,65536)) {
        fprintf(stderr,"collision %s case=%d\n",label,index);
        for (size_t byte=0; byte<65536; ++byte)
            if (expected[byte]!=prg_a4_area[byte])
                fprintf(stderr,"A4 %04zx expected=%02x actual=%02x\n",(byte-32768)&0xffff,expected[byte],prg_a4_area[byte]);
    }
    assert(!memcmp(expected,prg_a4_area,65536));
}

/* Phased movement, _move_explorer asm:4438-4449,4510-4617, caller
 * asm:4100-4125 and _zero_population asm:5671-5680. A magnet walker (tendency
 * 0, target 654) at 650 completes its frame and steps east (+1). */
static void setup_phased(int pop, int word18)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    memset(map_blk,1,4096);
    no_peeps=2; core_battles=core_settlements=0; core_result=0; prg_quit=0;
    A4_WORD(0x2fb0)=1; seed=123;
    core_tendency(0)=core_tendency(1)=0; core_target(0)=core_target(1)=654;
    peeps[0]=2; peeps[1]=0; peeps[2]=peeps[3]=1; peeps[21]=0x77;
    set_peep_word(0,4,pop); set_peep_word(0,8,650); set_peep_word(0,10,1);
    set_peep_word(0,12,7); set_peep_word(0,18,word18);
    map_who[649]=1; /* claim left by the previous departure */
}

static void phased_byte(uint8_t *expected, uint8_t *address, uint8_t value)
{
    expected[address-prg_a4_area]=value;
}

static void test_phased_move(void)
{
    static uint8_t expected[65536]; int checked=0;
    /* Departure: clear 649, count and claim 650, advance; no destination
     * write. +0x12 clears unless it equals pos-(+10) (asm:4100-4115). */
    const int words18[]={0,5,650};
    for (unsigned w=0; w<3; ++w) {
        setup_phased(100,words18[w]);
        memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x9c26+12,0);
        celebration_expected_word(expected,0x9c26+8,651);
        celebration_expected_word(expected,0x9c26+4,99);
        celebration_expected_word(expected,0x9c26+18,words18[w]==650 ? 650 : 0);
        phased_byte(expected,peeps+21,1);
        phased_byte(expected,map_who+649,0);
        phased_byte(expected,map_who+650,1);
        celebration_expected_word(expected,0xff88+650*2,1);
        expect_stats_pointer(expected,0); /* asm:4092-4106 */
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"phased-depart",checked++);
    }
    /* Arrival on a cell another peep names: no encounter yet; the caller's
     * collision wait marks that peep (asm:4145-4184). B is dead, so skipped. */
    setup_phased(100,0);
    peeps[22]=2; peeps[23]=1; set_peep_word(1,8,651); map_who[651]=2;
    memcpy(expected,prg_a4_area,sizeof expected);
    celebration_expected_word(expected,0x9c26+12,0);
    celebration_expected_word(expected,0x9c26+8,651);
    celebration_expected_word(expected,0x9c26+4,99);
    phased_byte(expected,peeps+21,1);
    phased_byte(expected,map_who+649,0);
    phased_byte(expected,map_who+650,1);
    celebration_expected_word(expected,0xff88+650*2,1);
    phased_byte(expected,peeps+22,0x22);
    celebration_expected_word(expected,0x9c26+22+12,101);
    expect_stats_pointer(expected,0); /* asm:4092-4106 */
    expect_ai_fixture_picks(expected);
    expect_pass_good_pop(expected); _move_peeps();
    collision_check(expected,"phased-arrive",checked++);
    assert(!core_battles);
    /* Next departure meets the friendly occupant of its own cell:
     * _join_forces (asm:4537-4547) merges A into B; A is zeroed twice. */
    setup_phased(50,0);
    set_peep_word(0,8,651); map_who[649]=0; map_who[650]=1;
    peeps[22]=2; peeps[23]=0; peeps[24]=peeps[25]=1;
    set_peep_word(1,4,100); set_peep_word(1,8,651); set_peep_word(1,12,3); map_who[651]=2;
    memcpy(expected,prg_a4_area,sizeof expected);
    phased_byte(expected,peeps,0);
    celebration_expected_word(expected,0x9c26+4,0);
    celebration_expected_word(expected,0x9c26+12,0);
    phased_byte(expected,peeps+21,1);
    phased_byte(expected,map_who+650,0);
    celebration_expected_word(expected,0x9c26+22+4,150);
    celebration_expected_word(expected,0x9c26+22+12,1);
    expect_stats_pointer(expected,0); /* asm:4092-4106 */
    expect_ai_fixture_picks(expected);
    expect_pass_good_pop(expected); _move_peeps();
    collision_check(expected,"phased-join",checked++);
    /* Walk death after departure: _zero_population clears pos-(+10). */
    setup_phased(1,0);
    memcpy(expected,prg_a4_area,sizeof expected);
    phased_byte(expected,peeps,0);
    celebration_expected_word(expected,0x9c26+4,0);
    celebration_expected_word(expected,0x9c26+12,0);
    celebration_expected_word(expected,0x9c26+8,651);
    phased_byte(expected,peeps+21,1);
    phased_byte(expected,map_who+649,0);
    celebration_expected_word(expected,0xff88+650*2,1);
    expect_stats_pointer(expected,0); /* asm:4092-4106 */
    expect_ai_fixture_picks(expected);
    expect_pass_good_pop(expected); _move_peeps();
    collision_check(expected,"phased-death",checked++);
    /* Hostile B is one step ahead (arrived 651->652, claim at 651). A's
     * departure meets it at A's own cell: _set_battle (asm:6568-6656)
     * releases B's claims and pulls walker B back onto 651, which A now
     * represents; the caller still applies walk death (asm:4116-4117). */
    setup_phased(100,0);
    set_peep_word(0,8,651); map_who[649]=0; map_who[650]=1;
    peeps[22]=2; peeps[23]=1; peeps[24]=peeps[25]=1;
    set_peep_word(1,4,100); set_peep_word(1,8,652); set_peep_word(1,10,1);
    set_peep_word(1,12,3); map_who[651]=2;
    memcpy(expected,prg_a4_area,sizeof expected);
    phased_byte(expected,peeps,8); phased_byte(expected,peeps+21,1);
    celebration_expected_word(expected,0x9c26+4,99);
    celebration_expected_word(expected,0x9c26+6,1);
    celebration_expected_word(expected,0x9c26+12,70);
    phased_byte(expected,peeps+22,0x0a);
    celebration_expected_word(expected,0x9c26+22+8,651);
    celebration_expected_word(expected,0x9c26+22+12,70);
    phased_byte(expected,map_who+650,0);
    phased_byte(expected,map_who+651,1);
    expect_stats_pointer(expected,0); /* asm:4092-4106 */
    int16_t ai_seed=expect_ai_fixture_picks(expected);
    expect_pass_good_pop(expected); _move_peeps();
    collision_check(expected,"phased-battle",checked++);
    assert(core_battles==1 && seed==ai_seed);
    printf("phased-move A4 cases: %d\n",checked);
}

/* Walker-path AI extras. _move_peeps asm:3290-3321 clears per owner the
 * LAB_52DEA town count (0x99fc) and _stats+0x22/0x26/0x2a (0x82d8/dc/e0);
 * a completed exact-2 walker stores itself as the enemy-indexed first walker
 * (asm:4092-4106, host slot+1 encoding) and, with a clear latch, calls
 * _one_block_flat (asm:4108-4122 -> 9148-9310). */
static void walker_extras_expected_depart(uint8_t *expected, int owner)
{
    celebration_expected_word(expected,0x9c26+12,0);
    celebration_expected_word(expected,0x9c26+8,651);
    celebration_expected_word(expected,0x9c26+4,99);
    phased_byte(expected,peeps+21,1);
    phased_byte(expected,map_who+649,0);
    phased_byte(expected,map_who+650,1);
    celebration_expected_word(expected,0xff88+650*2,1);
    uint32_t first=1;
    memcpy(expected+(A4_DISP(0x82dc+(1-owner)*46)-prg_a4_area),&first,4);
}
static void setup_war_town(int owner, int position, int city);
static void terrain_mailbox_write(int owner, int opcode, int column, int row);
static void walker_extras_vertices(int base)
{
    for (int vertex=0; vertex<65*65; ++vertex) A4_WORD(0xae86+vertex*2)=base;
}
static void test_walker_extras(void)
{
    static uint8_t expected[65536]; int checked=0;
    /* Prepass clears and the first-walker store, both owners. */
    for (int owner=0; owner<2; ++owner) {
        setup_phased(100,0); peeps[1]=owner;
        for (int side=0; side<2; ++side) {
            A4_WORD(0x99fc+side*16)=7+side;
            A4_LONG(0x82d8+side*46)=0x11111111u; A4_LONG(0x82dc+side*46)=0x22222222u;
            A4_LONG(0x82e0+side*46)=0x33333333u;
        }
        memcpy(expected,prg_a4_area,sizeof expected);
        for (int side=0; side<2; ++side) {
            celebration_expected_word(expected,0x99fc+side*16,0);
            memset(expected+(A4_DISP(0x82d8+side*46)-prg_a4_area),0,12);
        }
        walker_extras_expected_depart(expected,owner);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"extras-first",checked++);
    }
    /* Only the first completed walker of the pass is stored. */
    setup_phased(100,0); no_peeps=3;
    memcpy(peeps+44,peeps,22); set_peep_word(2,8,900); map_who[900]=3;
    _move_peeps();
    assert(A4_LONG(0x82dc+46)==1 && !A4_LONG(0x82dc)); ++checked;
    /* Raise: corners 1,1,1,0 (sum 3, q 0, rem 3): x-major scan finds (11,11). */
    for (int owner=0; owner<2; ++owner) {
        setup_phased(100,0); peeps[1]=owner; walker_extras_vertices(1);
        core_set_vertex(11,11,0);
        A4_LONG(0x9a02+owner*16)=20; A4_WORD(0x82c4+owner*46)=1;
        core_one_block_flat(650,owner);
        assert(A4_DISP(0x82b6+owner*46)[0]==1 && A4_DISP(0x82b7+owner*46)[0]==11);
        assert(A4_DISP(0x82b8+owner*46)[0]==11 && A4_WORD(0x82be + owner*46)==1);
    }
    for (int owner=0; owner<2; ++owner) {
        setup_phased(100,0); peeps[1]=owner; walker_extras_vertices(1);
        core_set_vertex(11,11,0);
        A4_LONG(0x9a02+owner*16)=20; A4_WORD(0x82c4+owner*46)=1;
        memcpy(expected,prg_a4_area,sizeof expected);
        walker_extras_expected_depart(expected,owner);
        /* Both tendencies are 0 and the latches clear: _set_devil_magnet
         * (asm:3254-3261, 9612-9633) queues 14/x/1 and latches first, so the
         * walker's _one_block_flat (latch gate asm:9159-9163) queues nothing.
         * The raise itself is covered by direct core_one_block_flat calls. */
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"extras-raise",checked++);
    }
    /* Lower: corners 2,1,1,1 (sum 5, q 1, rem 1): (10,10) is above 1. */
    setup_phased(100,0); walker_extras_vertices(1); core_set_vertex(10,10,2);
    A4_LONG(0x9a02)=1000; A4_WORD(0x82c4)=1;
    memcpy(expected,prg_a4_area,sizeof expected);
    walker_extras_expected_depart(expected,0);
    /* As above: the AI pick latches owner 0 before _one_block_flat. */
    expect_ai_fixture_picks(expected);
    expect_pass_good_pop(expected); _move_peeps();
    collision_check(expected,"extras-lower",checked++);
    /* The same lower, queued by a direct call (asm:9148-9310). */
    setup_phased(100,0); walker_extras_vertices(1); core_set_vertex(10,10,2);
    A4_LONG(0x9a02)=1000; A4_WORD(0x82c4)=1;
    core_one_block_flat(650,0);
    assert(A4_DISP(0x82b6)[0]==2 && A4_DISP(0x82b7)[0]==10 && A4_DISP(0x82b8)[0]==10);
    assert(A4_WORD(0x82be)==1);
    /* Every gate leaves the mailbox untouched; the walker still departs. */
    for (int gate=0; gate<9; ++gate) {
        setup_phased(100,0); walker_extras_vertices(1); core_set_vertex(11,11,0);
        A4_LONG(0x9a02)=20; A4_WORD(0x82c4)=1;
        switch (gate) {
        case 0: A4_WORD(0x82be)=1; break;          /* caller latch */
        case 1: A4_LONG(0x9a02)=19; break;         /* mana < 20 */
        case 2: A4_WORD(0x82c4)=0xfffe; break;     /* no bit 0 */
        case 3: game_mode=4; break;
        case 4: core_set_vertex(11,11,1); break;   /* rem 0 */
        case 5: core_set_vertex(11,11,3); break;   /* rem 2 (sum 6) */
        case 6: walker_extras_vertices(0); core_set_vertex(10,10,1); break; /* sum 1 */
        case 7: core_set_vertex(11,11,1); core_set_vertex(10,10,2); game_mode=8; break;
        case 8: A4_LONG(0x9a02)=0xffffffffu; break; /* signed -1 */
        }
        memcpy(expected,prg_a4_area,sizeof expected);
        walker_extras_expected_depart(expected,0);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"extras-gate",checked++);
        game_mode=0;
    }
    /* The LAB_52DEA gate is the in-pass count: 50 passes, 51 does not. */
    for (int count=50; count<=51; ++count) {
        setup_phased(100,0); walker_extras_vertices(1); core_set_vertex(11,11,0);
        A4_LONG(0x9a02)=20; A4_WORD(0x82c4)=1; A4_WORD(0x99fc)=count;
        core_one_block_flat(650,0);
        assert(A4_DISP(0x82b6)[0]==(count==50) && A4_WORD(0x82be)==(count==50));
        ++checked;
    }
    /* Consumer opcode 2 is _do_lower_point (asm:7091-7180): bounds first, then
     * the mana gate, lower, charge build_count+10 and rebuild (x-1..x,y-1..y). */
    for (int poor=0; poor<2; ++poor) {
        setup_phased(100,0); walker_extras_vertices(1); core_set_vertex(10,10,2);
        A4_LONG(0x848a)=5; A4_LONG(0x9a02)=poor ? 4 : 1000;
        terrain_mailbox_write(0,2,10,10);
        for (int row=9; row<=10; ++row) for (int column=9; column<=10; ++column) {
            map_blk[row*64+column]=3; map_alt[row*64+column]=5;
            A4_WORD(0xff88+(row*64+column)*2)=77;
        }
        memcpy(expected,prg_a4_area,sizeof expected);
        memset(expected+(A4_DISP(0x82b6)-prg_a4_area),0,3);
        for (int word=0; word<4; ++word) celebration_expected_word(expected,0x2f8a+word*2,10);
        if (!poor) {
            celebration_expected_word(expected,0xae86+(10*65+10)*2,1);
            celebration_expected_word(expected,0x2f88,1);
            /* asm:7121-7126: ASL.W #2, ADD.W #10, EXT.L, SUB.L: 1*4+10. */
            uint32_t mana=1000-14; memcpy(expected+(A4_DISP(0x9a02)-prg_a4_area),&mana,4);
            for (int row=9; row<=10; ++row) for (int column=9; column<=10; ++column) {
                phased_byte(expected,map_blk+row*64+column,15);
                phased_byte(expected,map_alt+row*64+column,0);
                celebration_expected_word(expected,0xff88+(row*64+column)*2,0);
            }
        }
        core_consume_terrain_mailboxes();
        collision_check(expected,"extras-consume-lower",checked++);
    }
    /* Peaceful towns: non-city frames count into 0x99fc (asm:3658-3661); the
     * oldest-so-far town (unsigned age >= max) stores itself as the enemy's
     * pointer (asm:3706-3737), overwriting a walker's store. */
    const struct { int first, second, result; } ages[]={{10,40,2},{40,10,1},{20,20,2}};
    for (unsigned sample=0; sample<3; ++sample) for (int city=0; city<2; ++city) {
        setup_war_town(0,650,0); no_peeps=2;
        memcpy(peeps+22,peeps,22); set_peep_word(1,8,1300); map_who[1300]=2;
        if (!city) { memset(map_blk,1,4096); map_blk[650]=map_blk[1300]=15; map_blk[651]=map_blk[1301]=15; }
        game_turn=100;
        set_peep_word(0,6,100-ages[sample].first); set_peep_word(1,6,100-ages[sample].second);
        _move_peeps();
        int frame0=peep_word(0,12), frame1=peep_word(1,12);
        assert(peeps[0]==1 && peeps[22]==1);
        assert(city ? frame0==42 && frame1==42 : frame0>=32 && frame0<42 && frame1>=32 && frame1<42);
        assert(A4_WORD(0x99fc)==(city ? 0 : 2) && !A4_WORD(0x99fc+16));
        assert(A4_LONG(0x82dc+46)==(uint32_t)ages[sample].result && !A4_LONG(0x82dc));
        ++checked;
    }
    /* _move_explorer asm:4450-4509: after the claim release a computer
     * walker (0x82bc==1) on block 0x42 with a clear latch and game_mode&0xc
     * clear writes command 2 at its own cell; it does not set the latch. */
    for (int owner=0; owner<2; ++owner) for (int gate=0; gate<6; ++gate) {
        setup_phased(100,0); peeps[1]=owner;
        A4_WORD(0x82bc+owner*46)=gate==1 ? 2 : 1; map_blk[650]=gate==2 ? 0x41 : 0x42;
        if (gate==3) A4_WORD(0x82be + owner*46)=1;
        if (gate==4) game_mode=4;
        if (gate==5) game_mode=8;
        memcpy(expected,prg_a4_area,sizeof expected);
        walker_extras_expected_depart(expected,owner);
        /* Gate 0: the owner's AI pick (tendency 0, asm:9612-9633) latches
         * first, so the 0x42 request sees a set latch and is not written. */
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"extras-walker-lower",checked++);
    }
    /* Additional case: 15 published towns at turn 50 (50%90 >= 10) send
     * _set_devil_magnet to its leader path; with no leader and tendency 0 it
     * writes nothing (asm:9686-9703) and _devil_effect has no permission, so
     * the clear latch admits the request. The pass republishes 0 towns. */
    for (int owner=0; owner<2; ++owner) {
        setup_phased(100,0); peeps[1]=owner; A4_WORD(0x82cc+owner*46)=15; game_turn=50;
        A4_WORD(0x82bc+owner*46)=1; map_blk[650]=0x42;
        memcpy(expected,prg_a4_area,sizeof expected);
        walker_extras_expected_depart(expected,owner);
        celebration_expected_word(expected,0x82cc+owner*46,0);
        phased_byte(expected,A4_DISP(0x82b6+owner*46),2);
        phased_byte(expected,A4_DISP(0x82b7+owner*46),10);
        phased_byte(expected,A4_DISP(0x82b8+owner*46),10);
        expect_ai_pick(expected,1-owner,1,15930); /* the other owner still picks */
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"extras-walker-lower-idle-ai",checked++);
        game_mode=0;
    }
    /* asm:3684-3705: the first town whose capacity strictly exceeds the
     * pass maximum (signed, starts 0) becomes the enemy's _stats+0x22. */
    for (int layout=0; layout<3; ++layout) {
        setup_war_town(0,650,0); no_peeps=2;
        memcpy(peeps+22,peeps,22); set_peep_word(1,8,1300); map_who[1300]=2;
        game_turn=100;
        memset(map_blk,1,4096);
        for (int dy=-2; dy<=2; ++dy) for (int dx=-2; dx<=2; ++dx) {
            if (layout!=2) map_blk[650+dy*64+dx]=15;   /* rich 650 */
            if (layout!=1) map_blk[1300+dy*64+dx]=15;  /* rich 1300 */
        }
        map_blk[650]=map_blk[651]=map_blk[1300]=map_blk[1301]=15;
        _move_peeps();
        assert(peeps[0]==1 && peeps[22]==1);
        /* layout 0: equal (first wins); 1: 650 richer; 2: 1300 richer. */
        assert(A4_LONG(0x82d8+46)==(uint32_t)(layout==2 ? 2 : 1) && !A4_LONG(0x82d8));
        ++checked;
    }
    printf("walker-extras A4 cases: %d\n",checked);
}

static void test_collision_wait_mark(void)
{
    /* Dead B is skipped by its own slot, so only the producer can touch it.
     * Bits outside 0x10 survive; no owner, liveness or no_peeps predicate. */
    const int flags[]={0,2,0x40,0x80,0x20,0x8a};
    const int marked[]={0x20,0x22,0x60,0xa0,0x20,0xaa};
    static uint8_t expected[65536]; int checked=0;
    for (int owner=0; owner<2; ++owner) for (int foreign=0; foreign<2; ++foreign)
        for (int order=0; order<2; ++order) for (unsigned f=0; f<6; ++f) {
            int a=order, b=1-order;
            setup_collision(a,owner,b,foreign ? 1-owner : owner,flags[f],0,0);
            memcpy(expected,prg_a4_area,sizeof expected);
            celebration_expected_word(expected,0x9c26+a*22+12,1);
            collision_expect_flags(expected,b,marked[f]);
            celebration_expected_word(expected,0x9c26+b*22+12,101);
            celebration_expected_word(expected,0x9c26+b*22+6,0);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps();
            collision_check(expected,"mark",checked++);
            assert(!core_battles && !core_settlements);
        }
    /* Slot 207 is the last accepted index, even beyond no_peeps. */
    setup_collision(0,0,1,1,2,0,0); map_who[650]=208;
    memcpy(expected,prg_a4_area,sizeof expected);
    celebration_expected_word(expected,0x9c26+12,1);
    collision_expect_flags(expected,207,0x20);
    celebration_expected_word(expected,0x9c26+207*22+12,101);
    expect_ai_fixture_picks(expected);
    expect_pass_good_pop(expected); _move_peeps();
    collision_check(expected,"slot207",0);
    /* Frame completion: blocked move (0x42, counter 7, frame 0), loss, then mark. */
    for (int owner=0; owner<2; ++owner) {
        setup_collision(0,owner,1,1-owner,0,0,0); set_peep_word(0,12,7);
        memcpy(expected,prg_a4_area,sizeof expected);
        collision_expect_flags(expected,0,0x42);
        celebration_expected_word(expected,0x9c26+6,7);
        celebration_expected_word(expected,0x9c26+12,0);
        celebration_expected_word(expected,0x9c26+4,99);
        /* One exploration draw: (123*0x24a1+0x24df)&0x7fff = 0x3e3a. */
        celebration_expected_word(expected,0x99e2,0x3e3a);
        collision_expect_flags(expected,1,0x20);
        celebration_expected_word(expected,0x9c26+22+12,101);
        celebration_expected_word(expected,0x9c26+22+6,0);
        expect_stats_pointer(expected,0);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"complete",owner);
    }
    printf("collision-wait mark A4 cases: %d\n",checked+3);
}

static void test_collision_wait_gates(void)
{
    static uint8_t expected[65536]; int checked=0;
    /* map_who: empty, self, slot 208; B frame nonzero; B bit 0x10. */
    struct { int who, frame, flags; } cases[]={
        {0,0,2},{-1,0,2},{209,0,2},{2,1,2},{2,-1,2},{2,101,2},{2,0,0x10},{2,0,0x12},{2,0,0x32}};
    for (int owner=0; owner<2; ++owner) for (unsigned c=0; c<sizeof cases/sizeof cases[0]; ++c) {
        setup_collision(0,owner,1,1-owner,cases[c].flags,0,cases[c].frame);
        map_who[650]=cases[c].who<0 ? 1 : cases[c].who;
        memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x9c26+12,1);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"gate",checked++);
    }
    /* Only exact 2 produces: other ordinary flags never frame or mark. */
    const int walker[]={0,3,0x82};
    for (unsigned w=0; w<3; ++w) {
        setup_collision(0,0,1,1,2,0,0); peeps[0]=walker[w];
        memcpy(expected,prg_a4_area,sizeof expected);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        collision_check(expected,"walker",checked++);
    }
    /* Death after completion skips the producer (asm:4145-4147). */
    for (int owner=0; owner<2; ++owner) {
        setup_collision(0,owner,1,1-owner,2,0,0);
        set_peep_word(0,12,7); set_peep_word(0,4,1);
        uint8_t other[22]; memcpy(other,peeps+22,sizeof other);
        _move_peeps();
        assert(!peep_word(0,4) && !memcmp(other,peeps+22,sizeof other) && map_who[650]==2);
        ++checked;
    }
    /* A live earlier-slot B frames 0->1 before A looks: no mark. */
    setup_collision(1,0,0,1,2,100,0);
    memcpy(expected,prg_a4_area,sizeof expected);
    celebration_expected_word(expected,0x9c26+12,1);
    celebration_expected_word(expected,0x9c26+22+12,1);
    expect_ai_fixture_picks(expected);
    expect_pass_good_pop(expected); _move_peeps();
    collision_check(expected,"earlier",checked++);
    printf("collision-wait gate cases: %d\n",checked);
}

static void test_collision_wait_tick(void)
{
    static uint8_t expected[65536];
    for (int owner=0; owner<2; ++owner) {
        /* Stopped ticks change nothing. */
        for (int gate=0; gate<3; ++gate) {
            setup_collision(0,owner,1,1-owner,2,100,0);
            if (gate==0) pause=1; else if (gate==1) core_result=2; else prg_quit=1;
            memcpy(expected,prg_a4_area,sizeof expected);
            game_tick();
            collision_check(expected,"stopped",gate);
            prg_quit=0;
        }
        /* Later live B is marked then consumed in the same tick. */
        setup_collision(0,owner,1,1-owner,2,100,0);
        memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0xae6a,1);
        celebration_expected_word(expected,0x9c26+12,1);
        collision_expect_flags(expected,1,0x22);
        celebration_expected_word(expected,0x9c26+22+12,102);
        celebration_expected_word(expected,0x9c26+22+6,1);
        celebration_expected_word(expected,0x9c26+22+4,99);
        uint32_t friendly=100, enemy=100; /* asm:3459-3474: before the walking loss */
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        game_tick();
        collision_check(expected,"tick1",owner);
        for (int tick=2; tick<=16; ++tick) {
            game_tick();
            assert(game_turn==tick && !core_result && peep_word(1,8)==700);
            if (tick<16) {
                assert(peeps[22]==0x22 && peep_word(1,6)==tick);
                assert(peep_word(1,12)==(tick%2 ? 102 : 101));
            } else assert(peeps[22]==0x42 && peep_word(1,6)==7 && peep_word(1,12)==0);
            assert(peep_word(1,4)==100-tick);
            if (tick<8) assert(peeps[0]==2 && peep_word(0,12)==tick && peep_word(0,4)==100);
            else if (tick==8) assert(peeps[0]==0x42 && peep_word(0,6)==7 && !peep_word(0,12) && peep_word(0,4)==99);
            else {
                assert(peeps[0]==0x42 && peep_word(0,6)==tick-1 && peep_word(0,4)==107-tick);
                assert(peep_word(0,12)==(tick%2 ? 101 : 102));
            }
            assert(peep_word(0,8)==650 && map_who[650]==2 && peeps[21]==0x77 && peeps[43]==0x99);
            assert(core_leader(owner)==1 && view_who==1 && !core_battles);
        }
    }
    puts("collision-wait tick cases: 8");
}

static void setup_town_celebration(int owner, int fertile, int city)
{
    setup_celebration(owner); peeps[0]=5;
    memset(peeps+14,0,4); /* Ordinary town; opaque trailing metadata stays seeded. */
    memset(map_blk,1,4096);
    for (int index=0; index<fertile; ++index) map_blk[650+core_offsets[index]]=31+owner;
    if (fertile) map_blk[650]=15;
    map_bk2[650]=37;
    if (city) for (int index=0; index<9; ++index)
        map_bk2[650+core_offsets[index]]=core_city[index];
    for (int cell=0; cell<4096; ++cell) A4_WORD(0xff88+cell*2)=cell+17;
    A4_WORD(0x2ff8)=1234; A4_WORD(0x2fb2)=2345;
    game_turn=8; A4_WORD(0x2fb0)=30000;
    for (int level=0; level<11; ++level) {
        A4_WORD(0x2fb6+level*2)=17; A4_WORD(0x2fcc+level*2)=9;
        A4_WORD(0x2fe2+level*2)=11;
    }
}

static void test_celebration_frames(void)
{
    /* Literal signed postincrement boundaries catch reset-in-helper, unsigned
     * comparisons, missing word wrap, and testing the old rather than new frame. */
    const struct { int before, after, complete, consumed; } cases[] = {
        {-32768,-32767,1,-32766}, {-2,-1,1,0}, {-1,0,1,1},
        {0,1,1,2}, {6,7,1,0}, {7,8,1,0}, {83,84,1,0},
        {84,85,0,85}, {85,86,0,86}, {86,87,0,87}, {87,88,1,0},
        {88,89,1,0}, {32766,32767,1,0}, {32767,-32768,1,-32767}
    };
    const int populations[]={1,-1,-32768,0};
    for (int owner=0; owner<2; ++owner) for (int flags=5; flags<=6; ++flags)
        for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
            for (unsigned pop=0; pop<sizeof populations/sizeof populations[0]; ++pop) {
                if (flags==5) setup_town_celebration(owner,17,1); else setup_celebration(owner);
                set_peep_word(0,4,populations[pop]); set_peep_word(0,12,cases[sample].before);
                uint8_t expected[sizeof prg_a4_area];
                celebration_expected(expected,flags,populations[pop] ? cases[sample].after : cases[sample].before);
                assert(core_frame_walker(0)==(populations[pop] ? cases[sample].complete : 0));
                assert(!memcmp(expected,prg_a4_area,sizeof expected));
            }
            if (flags==5) setup_town_celebration(owner,17,1); else setup_celebration(owner);
            set_peep_word(0,12,cases[sample].before);
            uint8_t expected[sizeof prg_a4_area];
            int complete=cases[sample].complete;
            celebration_expected(expected,complete ? flags-4 : flags,
                                 flags==5 && complete ? 42 : cases[sample].consumed);
            if (flags==5 && complete) {
                celebration_expected_word(expected,0x2ff8,9);
                celebration_expected_word(expected,0x2fb2,1);
            }
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(!core_battles && !core_settlements && !core_result);
        }
    /* Direct tick retains its stricter nonpositive-population skip. */
    const int skipped[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int flags=5; flags<=6; ++flags)
        for (unsigned sample=0; sample<sizeof skipped/sizeof skipped[0]; ++sample) {
            setup_celebration(owner); peeps[0]=flags; set_peep_word(0,4,skipped[sample]);
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
}

static void test_celebration_ticks(void)
{
    for (int owner=0; owner<2; ++owner) {
        setup_celebration(owner); set_peep_word(0,4,1); A4_WORD(0x2fb0)=30000;
        const int frames[]={86,87,0};
        for (int tick=0; tick<3; ++tick) {
            uint8_t expected[sizeof prg_a4_area];
            celebration_expected(expected,tick==2 ? 2 : 6,frames[tick]);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps();
            /* Full A4 checks population, timers, metadata, mana, RNG, position,
             * occupancy and every other resident; completion never moves/loses. */
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(!core_battles && !core_settlements && !core_result);
        }
        /* Walking resumes only when its later ordinary eight-frame cycle ends. */
        A4_WORD(0x2fb0)=0; assert(set_peep_reference(0,-1));
        core_tendency(owner)=0; core_leader(owner)=1; core_target(owner)=651;
        map_blk[651]=1;
        for (int tick=1; tick<8; ++tick) {
            uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,tick);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        _move_peeps();
        assert(peeps[0]==2 && peep_word(0,12)==0 && peep_word(0,8)==651);
        /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
        assert(peep_word(0,4)==1 && peep_word(0,10)==1 && map_who[650]==1 && !map_who[651]);
    }
}

static void test_town_celebration_ticks(void)
{
    /* Literal capacities 0,65,185,290,3050 exercise zero, ordinary and city
     * reframing. An old city must not be dismantled on celebration expiry. */
    const struct { int fertile, frame; } cases[]={{0,32},{1,34},{9,38},{16,41},{17,42}};
    for (int owner=0; owner<2; ++owner) for (int city=0; city<=1; ++city)
        for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample)
            for (int trigger=0; trigger<6; ++trigger) for (int overflow=0; overflow<=1; ++overflow) {
                setup_town_celebration(owner,cases[sample].fertile,city);
                set_peep_word(0,4,overflow ? 3200 : 1);
                if (trigger==1) A4_WORD(0xae80)=1;
                else if (trigger>=2) peeps[14+trigger-2]=0x80;
                /* No occupancy reclamation until ordinary town maintenance. */
                map_who[650]=0;
                for (int tick=0; tick<3; ++tick) {
                    uint8_t expected[sizeof prg_a4_area];
                    celebration_expected(expected,tick==2 ? 1 : 5,tick==2 ? cases[sample].frame : 86+tick);
                    if (tick==2) {
                        celebration_expected_word(expected,0x2ff8,cases[sample].fertile && city ? 9 : 0);
                        celebration_expected_word(expected,0x2fb2,cases[sample].fertile ? 1 : 0);
                    }
                    if (trigger==1) {
                        celebration_expected_word(expected,0x99f8,0x820);
                        celebration_expected_word(expected,0x9a08,0x820);
                        celebration_expected_word(expected,0xae78,0x820);
                        celebration_expected_word(expected,0xae76,0x820);
                    }
                    expect_ai_fixture_picks(expected); /* first pass only */
                    expect_pass_good_pop(expected); _move_peeps();
                    /* Includes all records, footprint, map history, scratch,
                     * mana, growth/split tables, metadata, RNG and turn eight. */
                    assert(!memcmp(expected,prg_a4_area,sizeof expected));
                    assert(!core_battles && !core_settlements && !core_result);
                }
                game_turn=9; _move_peeps();
                assert(peep_word(0,4)==(overflow ? 3200 : 1) && no_peeps==1);
                assert(core_mana(owner)==(owner ? 23456 : 12345) && seed==15930);
                if (trigger || !cases[sample].fertile) {
                    assert(peeps[0]==2 && peep_word(0,12)==0 && peep_word(0,10)==0);
                    assert(!map_bk2[650] && !map_who[650]);
                } else {
                    assert(peeps[0]==1 && peep_word(0,12)==cases[sample].frame);
                    assert(map_bk2[650]==cases[sample].frame && map_who[650]==1);
                    /* The next eligible ordinary town tick really grows/pays. */
                    set_peep_word(0,4,1); game_turn=16; _move_peeps();
                    assert(peep_word(0,4)==12 && peeps[3]==9 && no_peeps==1);
                    assert(core_mana(owner)==(owner ? 23473 : 12362));
                }
                for (int cell=0; cell<4096; ++cell) assert(A4_WORD(0xff88+cell*2)==cell+17);
            }
}

static void test_town_celebration_water(void)
{
    for (int owner=0; owner<2; ++owner) for (int city=0; city<=1; ++city)
        for (int fatal=0; fatal<=1; ++fatal) for (int population=1; population<=3; population+=2) {
            setup_town_celebration(owner,17,city);
            set_peep_word(0,4,population); set_peep_word(0,12,87);
            map_blk[650]=0; game_mode=fatal; A4_WORD(0x2fb0)=1;
            uint8_t expected[sizeof prg_a4_area];
            celebration_expected(expected,18,0);
            /* Independent coordinates: ordinary omits the eight (2,1)/(1,2)
             * offsets, whereas a city covers the entire five-by-five square. */
            for (int row=-2; row<=2; ++row) for (int column=-2; column<=2; ++column) {
                if (!city && ((abs(row)==2 && abs(column)==1) ||
                              (abs(row)==1 && abs(column)==2))) continue;
                int cell=650+row*64+column;
                if (map_blk[cell]==31+owner) expected[map_blk-prg_a4_area+cell]=15;
                if (city || (abs(row)<=1 && abs(column)<=1)) expected[map_bk2-prg_a4_area+cell]=0;
            }
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(!core_battles && !core_settlements);
        }
}

static void test_celebration_tick_gates(void)
{
    for (int owner=0; owner<2; ++owner) {
        setup_celebration(owner); set_peep_word(0,4,1); A4_WORD(0x2fb0)=30000;
        no_peeps=2; peeps[22]=5; peeps[23]=1-owner;
        set_peep_word(1,4,1); set_peep_word(1,8,651); map_blk[651]=1; map_who[651]=2;
        for (int gate=0; gate<4; ++gate) {
            pause=gate==0; core_result=gate;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(core_result==gate && !core_battles && !core_settlements);
        }
        pause=0; core_result=0; prg_quit=1;
        uint8_t stopped[sizeof prg_a4_area]; memcpy(stopped,prg_a4_area,sizeof stopped);
        game_tick();
        assert(!memcmp(stopped,prg_a4_area,sizeof stopped) && !core_result);
        prg_quit=0;
        /* A later ordinary resident must still receive its own frame update. */
        peeps[22]=2; set_peep_word(1,12,0);
        const int frames[]={86,87,0};
        for (int tick=0; tick<3; ++tick) {
            game_tick();
            assert(game_turn==tick+1 && !core_result);
            assert(peeps[0]==(tick==2 ? 2 : 6) && peep_word(0,12)==frames[tick]);
            assert(peeps[22]==2 && peep_word(1,12)==tick+1);
            assert(peep_word(0,4)==1 && peep_word(1,4)==1);
            assert(A4_LONG(0x99fe)==1 && A4_LONG(0x9a0e)==1);
            assert(peep_word(0,8)==650 && map_who[650]==1 && map_who[651]==2);
        }
    }
}

static void test_celebration_dispatch(void)
{
    /* Mixed mask-0x04 states must not gain the assembly's broader interception. */
    const int mixed[]={7,0xc,0xe,0x26,0x46,0x66,0x84,0x86};
    for (int owner=0; owner<2; ++owner)
        for (unsigned sample=0; sample<sizeof mixed/sizeof mixed[0]; ++sample) {
            setup_celebration(owner); peeps[0]=mixed[sample];
            set_peep_word(0,6,7); set_peep_word(0,12,87);
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            assert(!core_frame_walker(0));
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            /* Existing waits still lose population and advance their timer. */
            if (mixed[sample]&0x60) {
                int16_t population=99, timer=8;
                size_t offset=(size_t)(peeps-prg_a4_area);
                memcpy(expected+offset+4,&population,2); memcpy(expected+offset+6,&timer,2);
            }
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(!core_battles && !core_settlements);
        }
    for (int owner=0; owner<2; ++owner)
        for (int fatal=0; fatal<=1; ++fatal)
            for (int old_water=0; old_water<=1; ++old_water)
                for (int population=1; population<=3; population+=2) {
                    setup_celebration(owner); peeps[0]=old_water ? 0x16 : 6;
                    set_peep_word(0,4,population); map_blk[650]=0; game_mode=fatal;
                    _move_peeps();
                    int survives=!old_water || (!fatal && population==3);
                    assert(peeps[0]==(survives ? (old_water ? 0x16 : 18) : 0));
                    assert(peep_word(0,4)==(old_water ? survives : population));
                    assert(peep_word(0,12)==(old_water ? 85 : 0) && peep_word(0,6)==123);
                    /* setup_celebration: owner 1's AI picks once (seed 15930). */
                    assert(map_who[650]==survives && peep_word(0,8)==650 && seed==15930);
                }
    for (int owner=0; owner<2; ++owner) {
        setup_celebration(owner); peeps[0]=0x16; set_peep_word(0,12,87);
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,0);
        int16_t population=98; memcpy(expected+(peeps-prg_a4_area)+4,&population,2);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); /* Loss, two celebration frames, then walker reframe only. */
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_celebration(void)
{
    test_celebration_frames(); test_celebration_ticks();
    test_town_celebration_ticks(); test_town_celebration_water();
    test_celebration_tick_gates(); test_celebration_dispatch();
}

/* Newly submerged noncombat entry: asm:3562-3580, not old-water parity.
 * Catch premature loss/removal/queueing, frame advancement, unsigned compares,
 * stale flag bits, and skipped later residents with independent A4 deltas. */
static void setup_water_entry(int owner, int flags, int frame, int city)
{
    setup_celebration(owner);
    peeps[0]=flags; set_peep_word(0,12,frame);
    memset(peeps+14,0,4); /* Keep the live leader out of the heading prepass. */
    core_leader(owner)=1; view_who=1;
    A4_WORD(0xae78)=321; A4_WORD(0xae76)=432;
    A4_WORD(0x2ff8)=1234; A4_WORD(0x2fb2)=2345;
    A4_WORD(0xae7a)=17; A4_WORD(0xae7c)=29;
    for (int cell=0; cell<4096; ++cell) A4_WORD(0xff88+cell*2)=cell+17;
    /* Mixed ownership and foreign overlays expose overbroad footprint removal.
     * The city is selected by its map overlay, not the resident's frame. */
    for (int row=-3; row<=3; ++row) for (int column=-3; column<=3; ++column) {
        int cell=650+row*64+column;
        map_blk[cell]=31+((row+column+6)%2);
        map_bk2[cell]=37;
    }
    map_blk[650]=0; map_bk2[650]=city ? 42 : 37;
}

static void water_entry_expected(uint8_t *expected, int owner, int flags, int city, int frame)
{
    celebration_expected(expected,18,frame);
    if (flags&1) {
        /* Coordinates deliberately do not use core_offsets/core_set_town. */
        for (int row=-2; row<=2; ++row) for (int column=-2; column<=2; ++column) {
            if (!city && ((abs(row)==2 && abs(column)==1) ||
                          (abs(row)==1 && abs(column)==2))) continue;
            int cell=650+row*64+column;
            if (map_blk[cell]==31+owner) expected[map_blk-prg_a4_area+cell]=15;
            if (city || (abs(row)<=1 && abs(column)<=1)) expected[map_bk2-prg_a4_area+cell]=0;
        }
    }
    if (A4_WORD(0xae80)) {
        celebration_expected_word(expected,0x99f8,0x820);
        celebration_expected_word(expected,0x9a08,0x820);
        celebration_expected_word(expected,0xae78,0x820);
        celebration_expected_word(expected,0xae76,0x820);
    }
}

static void test_water_entry_state(void)
{
    const int flags[]={0,1,2,5,6,7,0x20,0x22,0x42,0x60,0x66,0x80,0x81,0x86};
    const struct { int before, after; } frames[]={
        {-32768,-32768},{-2,-2},{-1,-1},{0,0},{7,7},{8,0},{87,0},{32767,0}
    };
    const int populations[]={1,3,32767};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int war=0; war<2; ++war) for (int city=0; city<2; ++city)
    for (unsigned state=0; state<sizeof flags/sizeof flags[0]; ++state)
    for (unsigned sample=0; sample<sizeof frames/sizeof frames[0]; ++sample)
    for (unsigned pop=0; pop<sizeof populations/sizeof populations[0]; ++pop) {
        setup_water_entry(owner,flags[state],frames[sample].before,city);
        set_peep_word(0,4,populations[pop]); game_mode=fatal; A4_WORD(0xae80)=war;
        /* Empty or foreign occupancy must not be claimed or cleared on entry. */
        map_who[650]=pop;
        uint8_t expected[sizeof prg_a4_area];
        water_entry_expected(expected,owner,flags[state],city,frames[sample].after);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,4)==populations[pop]);
        assert(peep_word(0,12)==frames[sample].after && peeps[0]==18);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && !prg_quit);
    }
    /* Nonpositive slots remain outside the consumer, including their footprint. */
    const int dead[]={0,-1,-32768};
    for (unsigned sample=0; sample<sizeof dead/sizeof dead[0]; ++sample) {
        setup_water_entry(0,5,87,1); set_peep_word(0,4,dead[sample]);
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_water_entry_frame_words(void)
{
    /* Efficient frame-only oracle for every stored word: signed negatives are
     * retained, not clamped or incremented. Full-state checks live above. */
    for (int owner=0; owner<2; ++owner) {
        setup_water_entry(owner,2,0,0);
        for (unsigned word=0; word<=UINT16_MAX; ++word) {
            peeps[0]=2; set_peep_word(0,12,word);
            _move_peeps();
            unsigned want=(word>=8 && word<0x8000) ? 0 : word;
            assert((uint16_t)peep_word(0,12)==want);
            assert(peeps[0]==18 && peep_word(0,4)==100);
        }
    }
}

static void test_water_entry_second_tick(void)
{
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int war=0; war<2; ++war) for (int population=1; population<=3; population+=2) {
        setup_water_entry(owner,0x80,87,0); core_leader(owner)=0; view_who=0;
        set_peep_word(0,4,population); game_mode=fatal; A4_WORD(0xae80)=war;
        uint8_t expected[sizeof prg_a4_area];
        water_entry_expected(expected,owner,0x80,0,0);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        int16_t remaining=(!fatal && population==3) ? 1 : 0;
        expected[peeps-prg_a4_area]=remaining ? 18 : 0;
        memcpy(expected+(peeps-prg_a4_area)+4,&remaining,2);
        if (!remaining) expected[map_who-prg_a4_area+650]=0;
        if (war && !fatal) {
            expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
            expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=10;
            expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=10;
            celebration_expected_word(expected,0x82be + owner*46,1);
        }
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==15930); /* first pass's pick */
    }
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal) {
        /* Exact-18 dry recovery still loses or dies; fatal preserves frame. */
        setup_water_entry(owner,18,87,0); map_blk[650]=1; game_mode=fatal;
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,fatal ? 0 : 2,fatal ? 87 : 1);
        int16_t population=fatal ? 0 : 98;
        memcpy(expected+(peeps-prg_a4_area)+4,&population,2);
        if (fatal) {
            expected[map_who-prg_a4_area+650]=0;
            celebration_expected_word(expected,0x99f6+owner*16,0);
            celebration_expected_word(expected,0x99f8+owner*16,650);
            celebration_expected_word(expected,owner ? 0xae76 : 0xae78,650);
            celebration_expected_word(expected,0xae68,0);
        }
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        /* Battle exemption: no valid partner, so no combat or water side effects. */
        const int combat[]={8,9,10,0x88};
        for (unsigned state=0; state<sizeof combat/sizeof combat[0]; ++state) {
            setup_water_entry(owner,combat[state],87,1); game_mode=fatal;
            memcpy(expected,prg_a4_area,sizeof expected);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
    }
}

static void test_water_entry_tick_gates(void)
{
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal) {
        setup_water_entry(owner,0x42,7,0); game_mode=fatal;
        set_peep_word(0,4,1); no_peeps=18;
        peeps[17*22]=2; peeps[17*22+1]=1-owner;
        set_peep_word(17,4,100); set_peep_word(17,8,900);
        map_blk[900]=1; map_who[900]=18;
        for (int gate=0; gate<5; ++gate) {
            pause=gate==0; core_result=gate>=1 && gate<=3 ? gate : 0; prg_quit=gate==4;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(core_result==(gate>=1 && gate<=3 ? gate : 0));
            assert(!core_battles && !core_settlements);
        }
        pause=core_result=prg_quit=0;
        uint8_t expected[sizeof prg_a4_area];
        water_entry_expected(expected,owner,0x42,0,7);
        int16_t frame=1; memcpy(expected+(peeps-prg_a4_area)+17*22+12,&frame,2);
        int16_t turn=1; memcpy(expected+((uint8_t *)&game_turn-prg_a4_area),&turn,2);
        int32_t friendly=owner ? 100 : 1, enemy=owner ? 1 : 100;
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        expect_ai_fixture_tick(expected,0); /* game continues: tendency = x */
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result && !core_battles && !core_settlements);
        game_tick();
        /* asm:3459-3474: tick 2 still counts the dying 1; asm:4323-4347
         * ends the game on tick 3's zero sum. */
        assert(game_turn==2 && !core_result && A4_LONG(0x99fe + owner*16)==1);
        assert(!peep_word(0,4) && peep_word(0,6)==123 && peep_word(0,12)==7);
        assert(peep_word(17,12)==2 && peep_word(17,4)==100 && map_who[900]==18);
        game_tick(); assert(game_turn==3 && core_result==(owner ? 1 : 2));
    }
}

static void test_water_entry_heading_payload(void)
{
    const int flags[]={1,2,5,6,0x42,0x80};
    const uint8_t payloads[][4]={
        {1,0,0,0},{0x80,0,0,0},{0,1,0,0},{0,0x80,0,0},
        {0,0,1,0},{0,0,0x80,0},{0,0,0,1},{0,0,0,0x80},
        {0x12,0x34,0x56,0x78},{0xff,0xff,0xff,0xff}
    };
    /* Clearing or interpreting any heading byte on entry is a regression.
     * No leader means the independent heading prepass cannot consume it. */
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (unsigned state=0; state<sizeof flags/sizeof flags[0]; ++state)
    for (unsigned sample=0; sample<sizeof payloads/sizeof payloads[0]; ++sample) {
        setup_water_entry(owner,flags[state],87,1); game_mode=fatal;
        core_leader(0)=core_leader(1)=0;
        memcpy(peeps+14,payloads[sample],4);
        uint8_t expected[sizeof prg_a4_area];
        water_entry_expected(expected,owner,flags[state],1,0);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!memcmp(peeps+14,payloads[sample],4));
    }
}

static void test_water_entry_death_cleanup(void)
{
    /* Death must clear its leader/view, but never erase a foreign map owner.
     * The later live resident has an inert battle link to exclude movement. */
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int representative=1; representative<=2; ++representative) {
        setup_water_entry(owner,0x80,87,0); game_mode=fatal;
        set_peep_word(0,4,1); no_peeps=2;
        peeps[22]=8; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,6,1); set_peep_word(1,8,650);
        core_leader(1-owner)=2;
        map_who[650]=representative;
        uint8_t expected[sizeof prg_a4_area];
        water_entry_expected(expected,owner,0x80,0,0);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_leader(owner)==1 && view_who==1);
        expected[peeps-prg_a4_area]=0;
        int16_t zero=0;
        memcpy(expected+(peeps-prg_a4_area)+4,&zero,2);
        if (representative==1) expected[map_who-prg_a4_area+650]=0;
        celebration_expected_word(expected,0x99f6+owner*16,0);
        celebration_expected_word(expected,0xae68,0);
        celebration_expected_word(expected,0x99f8+owner*16,650);
        celebration_expected_word(expected,owner ? 0xae76 : 0xae78,650);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_leader(owner) && !view_who && core_leader(1-owner)==2);
        assert(map_who[650]==(representative==2 ? 2 : 0));
        assert(peep_word(1,4)==100 && !core_battles && !core_settlements);
    }
}

static void test_water_entry_edge_footprints(void)
{
    const int positions[]={0,63,4032,4095,32,2048,2047,4064};
    for (int owner=0; owner<2; ++owner) for (int city=0; city<2; ++city)
    for (unsigned sample=0; sample<sizeof positions/sizeof positions[0]; ++sample) {
        setup_water_entry(owner,5,87,city);
        int position=positions[sample]; set_peep_word(0,8,position);
        memset(map_who,0,4096); map_who[position]=1;
        for (int row=0; row<64; ++row) for (int column=0; column<64; ++column) {
            map_blk[row*64+column]=31+(row+column)%2;
            map_bk2[row*64+column]=37;
        }
        map_blk[position]=0; map_bk2[position]=city ? 42 : 37;
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,18,0);
        /* Independent clipped Cartesian footprint: catches row wrapping and
         * edge truncation without core_valid_move or the production offsets. */
        for (int row=0; row<64; ++row) for (int column=0; column<64; ++column) {
            int dy=abs(row-position/64), dx=abs(column-position%64);
            if (dy>2 || dx>2 || (!city && ((dy==2 && dx==1) || (dy==1 && dx==2)))) continue;
            int cell=row*64+column;
            if (map_blk[cell]==31+owner) expected[map_blk-prg_a4_area+cell]=15;
            if (city || (dy<=1 && dx<=1)) expected[map_bk2-prg_a4_area+cell]=0;
        }
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements);
    }
}

static void test_water_entry(void)
{
    test_water_entry_state(); test_water_entry_frame_words();
    test_water_entry_second_tick(); test_water_entry_tick_gates();
    test_water_entry_heading_payload(); test_water_entry_death_cleanup();
    test_water_entry_edge_footprints();
}

/* Old wet loss: catch widened arithmetic, premature flag/footprint changes,
 * dispatch fallthrough, and fatal queueing. Oracles never call core_remove. */
static void water_loss_expected(uint8_t *expected, int owner, int position, int remaining)
{
    memcpy(expected,prg_a4_area,sizeof prg_a4_area);
    int16_t population=remaining>0 ? remaining : 0;
    memcpy(expected+(peeps-prg_a4_area)+4,&population,2);
    if (A4_WORD(0xae80)) {
        celebration_expected_word(expected,0x99f8,0x820);
        celebration_expected_word(expected,0x9a08,0x820);
        celebration_expected_word(expected,0xae78,0x820);
        celebration_expected_word(expected,0xae76,0x820);
    }
    if (!(game_mode&1) && (A4_WORD(0xae80) ||
        (A4_WORD(0x82bc+owner*46)==1 && !(game_mode&4)))) {
        expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
        expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=position%64;
        expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=position/64;
        celebration_expected_word(expected,0x82be + owner*46,1);
    }
    if (remaining>0) return;
    expected[peeps-prg_a4_area]=0;
    if (map_who[position]==1) expected[map_who-prg_a4_area+position]=0;
    if (core_leader(owner)==1) {
        celebration_expected_word(expected,0x99f6+owner*16,0);
        celebration_expected_word(expected,0x99f8+owner*16,position);
        celebration_expected_word(expected,owner ? 0xae76 : 0xae78,position);
    }
    if (view_who==1) celebration_expected_word(expected,0xae68,0);
    if (!(peeps[0]&1)) return;
    int city=map_bk2[position]==42;
    /* Clipped Cartesian coordinates, independent of the production offsets. */
    for (int row=0; row<64; ++row) for (int column=0; column<64; ++column) {
        int dy=abs(row-position/64), dx=abs(column-position%64);
        if (dy>2 || dx>2 || (!city && ((dy==2 && dx==1) || (dy==1 && dx==2)))) continue;
        int cell=row*64+column;
        if (map_blk[cell]==31+owner) expected[map_blk-prg_a4_area+cell]=15;
        if (city || (dy<=1 && dx<=1)) expected[map_bk2-prg_a4_area+cell]=0;
    }
}

static void test_water_loss_state(void)
{
    const struct { int population, loss, result; } arithmetic[]={
        {3,1,1},{2,1,0},{1,1,-1},{100,32767,102},{100,16384,-32668},
        {1,16385,32767},{1,-1,3},{32767,-1,-32767},
        {100,-32768,100},{100,30000,5636}
    };
    const int flags[]={18,0x10,0x11,0x15,0x16,0x17,0x30,0x52,0x76,0x90,0x91,0x96};
    const int frames[]={-32768,-1,0,7,87,32767};
    const int timers[]={32767,-32768,0,15,-1,123};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int war=0; war<2; ++war)
    for (unsigned state=0; state<sizeof flags/sizeof flags[0]; ++state)
    for (unsigned sample=0; sample<sizeof arithmetic/sizeof arithmetic[0]; ++sample) {
        setup_water_entry(owner,flags[state],frames[sample%6],state%2);
        game_mode=fatal; A4_WORD(0xae80)=war;
        set_peep_word(0,4,arithmetic[sample].population);
        set_peep_word(0,6,timers[sample%6]); A4_WORD(0x2fb0)=arithmetic[sample].loss;
        /* Alternate leader cleanup and opaque heading preservation; never let
         * the heading prepass consume the latter. */
        if (sample%2) { core_leader(owner)=0; memcpy(peeps+14,"\x12\x34\x56\x78",4); }
        no_peeps=2; peeps[22]=8; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,6,1); set_peep_word(1,8,650);
        map_who[650]=sample%3;
        /* Existing owner command must survive fatal removal, not be erased. */
        for (int command_owner=0; command_owner<2; ++command_owner) {
            A4_DISP(0x82b6+command_owner*46)[0]=2;
            A4_DISP(0x82b7+command_owner*46)[0]=27;
            A4_DISP(0x82b8+command_owner*46)[0]=31;
            A4_WORD(0x82be + command_owner*46)=7;
            A4_WORD(0x82bc+command_owner*46)=1; /* Nonwar rescue is admitted. */
        }
        uint8_t expected[sizeof prg_a4_area];
        int remaining=fatal ? 0 : arithmetic[sample].result;
        water_loss_expected(expected,owner,650,remaining);
        int16_t ai_seed=expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,4)==(remaining>0 ? remaining : 0));
        assert(peeps[0]==(remaining>0 ? flags[state] : 0));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==ai_seed);
    }
}

static void test_water_loss_words(void)
{
    /* Every walk_death word, one population: modular residue oracle uses
     * division/remainder rather than the production word-shift/subtract. */
    for (int owner=0; owner<2; ++owner) {
        setup_water_entry(owner,0x96,-32768,0); core_leader(owner)=0; view_who=0;
        for (unsigned word=0; word<=UINT16_MAX; ++word) {
            peeps[0]=0x96; set_peep_word(0,4,100); map_who[650]=1;
            A4_WORD(0x2fb0)=word;
            unsigned residue=(131172u-2u*word)%65536u;
            int want=residue>0 && residue<32768 ? (int)residue : 0;
            _move_peeps();
            assert(peep_word(0,4)==want && peeps[0]==(want ? 0x96 : 0));
            assert(map_who[650]==(want ? 1 : 0));
            assert(peep_word(0,12)==-32768 && peep_word(0,6)==123 && peep_word(0,10)==-64);
        }
    }
}

static void test_water_loss_edges(void)
{
    const int positions[]={0,63,4032,4095,32,2048,2047,4064};
    for (int owner=0; owner<2; ++owner) for (int city=0; city<2; ++city)
    for (int fatal=0; fatal<2; ++fatal) for (int population=1; population<=3; population+=2)
    for (unsigned sample=0; sample<sizeof positions/sizeof positions[0]; ++sample) {
        setup_water_entry(owner,0x95,-1,city); game_mode=fatal;
        int position=positions[sample]; set_peep_word(0,8,position); set_peep_word(0,4,population);
        memset(map_who,0,4096); map_who[position]=1;
        for (int row=0; row<64; ++row) for (int column=0; column<64; ++column) {
            map_blk[row*64+column]=31+(row+column)%2;
            map_bk2[row*64+column]=37;
        }
        map_blk[position]=0; map_bk2[position]=city ? 42 : 37;
        uint8_t expected[sizeof prg_a4_area];
        water_loss_expected(expected,owner,position,fatal ? 0 : population-2);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements);
    }
}

static void test_water_loss_gates(void)
{
    const int dead[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal) {
        for (unsigned sample=0; sample<sizeof dead/sizeof dead[0]; ++sample) {
            setup_water_entry(owner,0x95,32767,1); game_mode=fatal;
            set_peep_word(0,4,dead[sample]);
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        setup_water_entry(owner,0x96,-32768,0); game_mode=fatal;
        set_peep_word(0,4,3); no_peeps=18;
        peeps[17*22]=2; peeps[17*22+1]=1-owner;
        set_peep_word(17,4,100); set_peep_word(17,8,900);
        map_blk[900]=1; map_who[900]=18;
        for (int gate=0; gate<5; ++gate) {
            pause=gate==0; core_result=gate>=1 && gate<=3 ? gate : 0; prg_quit=gate==4;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(core_result==(gate>=1 && gate<=3 ? gate : 0));
        }
        pause=core_result=prg_quit=0;
        uint8_t expected[sizeof prg_a4_area]; water_loss_expected(expected,owner,650,fatal ? 0 : 1);
        int16_t frame=1; memcpy(expected+(peeps-prg_a4_area)+17*22+12,&frame,2);
        int16_t turn=game_turn+1; memcpy(expected+((uint8_t *)&game_turn-prg_a4_area),&turn,2);
        /* asm:3459-3474: the pre-loss word counts, fatal or not; the fatal
         * end (asm:4323-4347) therefore follows on the next pass. */
        int32_t before=peep_word(0,4), friendly=owner ? 100 : before, enemy=owner ? before : 100;
        assert(before>0);
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        expect_ai_fixture_tick(expected,0);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result);
        assert(peep_word(17,12)==1 && peep_word(17,4)==100 && map_who[900]==18);
        if (fatal) { game_tick(); assert(core_result==(owner ? 1 : 2)); }
    }
    /* Mixed combat+water intentionally retains host recovery, not wet loss. */
    const int combat[]={0x18,0x19,0x1e,0x98};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (unsigned sample=0; sample<sizeof combat/sizeof combat[0]; ++sample) {
        setup_water_entry(owner,combat[sample],32767,1); game_mode=fatal;
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,1);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

/* Dry exact-18 recovery: catch missing doubled loss, resetting instead of
 * framing twice, removing before the first frame, and dead navigation/battle.
 * Literal outcomes come from asm:3544-3557,4056-4067,5728-5743. */
static void test_water_recovery_state(void)
{
    const struct { int population, loss, frame, remaining, after; } cases[]={
        {100,1,7,98,1},{100,1,93,98,1},{100,1,-1,98,1},
        {100,1,-32768,98,-32766},{100,1,0,98,2},
        {2,1,6,0,6},{1,1,6,0,7},{1,1,7,0,0},
        {2,1,93,0,93},{1,1,-32768,0,-32767},
        {100,32767,7,102,1},{100,16384,6,0,7},
        {1,16385,7,32767,1},{32767,-1,6,0,7},
        {100,-32768,7,100,1},{100,30000,7,5636,1}
    };
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int war=0; war<2; ++war) for (int swamp=0; swamp<2; ++swamp)
    for (int occupancy=0; occupancy<3; ++occupancy)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        setup_water_entry(owner,18,cases[sample].frame,0);
        map_blk[650]=swamp ? 53 : 1; map_who[650]=occupancy;
        game_mode=fatal; A4_WORD(0xae80)=war; A4_WORD(0x2fb0)=cases[sample].loss;
        set_peep_word(0,4,cases[sample].population);
        if (sample%2) { core_leader(owner)=0; memcpy(peeps+14,"\x12\x34\x56\x78",4); }
        /* Preserve a co-located battle resident and its foreign occupancy.
         * Noncombat encounter ordering is tested separately below. */
        no_peeps=2; peeps[22]=8; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,6,1); set_peep_word(1,8,650);
        for (int side=0; side<2; ++side) {
            A4_DISP(0x82b6+side*46)[0]=2;
            A4_DISP(0x82b7+side*46)[0]=27; A4_DISP(0x82b8+side*46)[0]=31;
            A4_WORD(0x82be + side*46)=7; A4_WORD(0x82bc+side*46)=1;
        }
        uint8_t expected[sizeof prg_a4_area];
        int remaining=fatal ? 0 : cases[sample].remaining;
        water_loss_expected(expected,owner,650,remaining);
        expected[peeps-prg_a4_area]=remaining ? 2 : 0;
        int16_t frame=fatal ? cases[sample].frame : cases[sample].after;
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        /* asm:4148-4184: the surviving exact-2 walker (second frame never
         * completes) sees map_who name battle peep 1 at frame 0: bit 8 is no
         * gate, so 0x28/101/0; its battle slot then has no later partner. */
        if (remaining && occupancy==2) {
            expected[peeps-prg_a4_area+22]=0x28;
            celebration_expected_word(expected,0x9c26+22+12,101);
            celebration_expected_word(expected,0x9c26+22+6,0);
        }
        int16_t ai_seed=expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,4)==remaining && peep_word(0,12)==frame);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==ai_seed);
    }
}

static void test_water_recovery_dead_noncombat_encounter(void)
{
    /* Missing the local signed-population guard would frame 7->0 and call
     * core_move, which creates a noncombat encounter before its late guard.
     * A battle-flag occupant takes core_join_battle instead and cannot prove
     * this ordering. Keep a real ordinary enemy's independent 0->1 tick. */
    for (int owner=0; owner<2; ++owner) {
        setup_water_entry(owner,18,6,0);
        memset(map_blk,1,4096); map_who[650]=2;
        core_tendency(owner)=0; core_target(owner)=715;
        A4_WORD(0x2fb0)=1; set_peep_word(0,4,1);
        no_peeps=2; peeps[22]=2; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,6,29);
        set_peep_word(1,8,650); set_peep_word(1,10,-63); set_peep_word(1,12,0);
        uint8_t expected[sizeof prg_a4_area];
        water_loss_expected(expected,owner,650,0);
        int16_t frame=7; memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        frame=1; memcpy(expected+(peeps-prg_a4_area)+22+12,&frame,2);
        int16_t after=expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(!peeps[0] && !peep_word(0,4) && peep_word(0,12)==7);
        assert(peeps[22]==2 && peep_word(1,4)==100 && peep_word(1,12)==1);
        assert(peep_word(0,6)==123 && peep_word(1,6)==29);
        assert(peep_word(0,10)==-64 && peep_word(1,10)==-63 && map_who[650]==2);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==after);
    }
}

static void test_water_recovery_movement(void)
{
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int war=0; war<2; ++war) for (int swamp=0; swamp<2; ++swamp)
    for (int deep=0; deep<2; ++deep) for (int low=0; low<2; ++low) {
        setup_water_entry(owner,18,6,0);
        memset(map_blk,1,4096); map_blk[650]=swamp ? 53 : 1;
        core_tendency(owner)=0; core_target(owner)=0x820;
        game_mode=fatal | (deep ? 2 : 0); A4_WORD(0xae80)=war;
        set_peep_word(0,4,low ? 3 : 100);
        int dead=fatal || swamp || low;
        uint8_t expected[sizeof prg_a4_area];
        water_loss_expected(expected,owner,650,dead ? 0 : 97);
        expected[peeps-prg_a4_area]=dead ? 0 : 2;
        int16_t frame=fatal ? 6 : 0;
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        if (!fatal && swamp) {
            celebration_expected_word(expected,0x2fac,66);
            if (!deep) expected[map_blk-prg_a4_area+650]=15;
        } else if (!fatal) {
            /* First frame 6->7 is ignored; ordinary 7->0 moves southeast,
             * then loses once. Population 3 dies only AFTER that move. */
            int16_t position=715, direction=65;
            memcpy(expected+(peeps-prg_a4_area)+8,&position,2);
            memcpy(expected+(peeps-prg_a4_area)+10,&direction,2);
            expected[peeps-prg_a4_area+21]=65;
            /* _move_explorer asm:4590-4617 counts/claims departure 650;
             * death clears that claim via _zero_population asm:5671-5680. */
            expected[map_who-prg_a4_area+650]=low ? 0 : 1;
            expected[map_who-prg_a4_area+715]=0;
            celebration_expected_word(expected,0xff88+650*2,668);
            /* asm:4100-4115: +0x12 (0x1312) differs from 715-65, so clears. */
            celebration_expected_word(expected,0x9c26+18,0);
            expect_stats_pointer(expected,0);
            if (low) {
                celebration_expected_word(expected,0x99f8+owner*16,715);
                celebration_expected_word(expected,owner ? 0xae76 : 0xae78,715);
            }
        }
        int16_t ai_seed=expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,4)==(dead ? 0 : 97));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==ai_seed);
    }
}

static void test_water_recovery_gates(void)
{
    const int dead[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal) {
        for (unsigned sample=0; sample<sizeof dead/sizeof dead[0]; ++sample) {
            setup_water_entry(owner,18,6,0); map_blk[650]=1; game_mode=fatal;
            set_peep_word(0,4,dead[sample]);
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        setup_water_entry(owner,18,6,0); map_blk[650]=1; game_mode=fatal;
        set_peep_word(0,4,1); no_peeps=18;
        peeps[17*22]=2; peeps[17*22+1]=1-owner;
        set_peep_word(17,4,100); set_peep_word(17,8,900);
        map_blk[900]=1; map_who[900]=18;
        for (int gate=0; gate<5; ++gate) {
            pause=gate==0; core_result=gate>=1 && gate<=3 ? gate : 0; prg_quit=gate==4;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        pause=core_result=prg_quit=0;
        uint8_t expected[sizeof prg_a4_area]; water_loss_expected(expected,owner,650,0);
        int16_t frame=fatal ? 6 : 7;
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        frame=1; memcpy(expected+(peeps-prg_a4_area)+17*22+12,&frame,2);
        int16_t turn=game_turn+1; memcpy(expected+((uint8_t *)&game_turn-prg_a4_area),&turn,2);
        /* asm:3459-3474: the dying peep's pre-loss word counts this pass, so
         * the tick continues (picks consumed) and asm:4323-4347 ends next. */
        int32_t dying=peep_word(0,4), friendly=owner ? 100 : dying, enemy=owner ? dying : 100;
        assert(dying>0);
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        expect_ai_fixture_tick(expected,0);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result && peep_word(17,12)==1);
        assert(!core_battles && !core_settlements);
        game_tick(); assert(core_result==(owner ? 1 : 2) && !A4_LONG(0x99fe + owner*16));
    }
    /* General mixed flags, including combat recovery, deliberately retain the
     * current host no-loss/reset-frame behavior, on both land and swamp. */
    const int mixed[]={0x10,0x96,0x18,0x19,0x1e,0x98};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int swamp=0; swamp<2; ++swamp)
    for (unsigned sample=0; sample<sizeof mixed/sizeof mixed[0]; ++sample) {
        setup_water_entry(owner,mixed[sample],-32768,0); game_mode=fatal;
        map_blk[650]=swamp ? 53 : 1;
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,1);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_water_recovery(void)
{
    test_water_recovery_state(); test_water_recovery_dead_noncombat_encounter();
    test_water_recovery_movement(); test_water_recovery_gates();
}

static void test_water_loss(void)
{
    test_water_loss_state(); test_water_loss_words(); test_water_loss_edges(); test_water_loss_gates();
}

/* Dry 0x15/0x16: catch early negative cleanup, a consumed first completion,
 * missing second/third frame, clamping before framing, and ordinary fallthrough.
 * Population/frames are literal assembly witnesses, not production helpers. */
static void test_water_celebration_state(void)
{
    const struct { int before, after, complete; } frames[]={
        {83,85,0},{84,86,0},{85,87,0},{86,0,1},{87,0,1},
        {32766,-32767,1},{32767,-32766,1},{-32768,-32765,1},
        {-3,0,1},{-2,1,1},{-1,2,1},{0,3,1},{4,7,1},{5,0,1}
    };
    const struct { int population, loss, remaining; } arithmetic[]={
        {3,1,1},{2,1,0},{1,1,-1},{100,32767,102},{100,16384,-32668},
        {1,16385,32767},{1,-1,3},{32767,-1,-32767},
        {100,-32768,100},{100,30000,5636}
    };
    const int terrain[]={1,53,15};
    for (int owner=0; owner<2; ++owner) for (int state=0x15; state<=0x16; ++state)
    for (int fatal=0; fatal<2; ++fatal) for (int war=0; war<2; ++war)
    for (unsigned sample=0; sample<sizeof frames/sizeof frames[0]; ++sample)
    for (unsigned pop=0; pop<sizeof arithmetic/sizeof arithmetic[0]; ++pop)
    for (unsigned land=0; land<sizeof terrain/sizeof terrain[0]; ++land) {
        setup_water_entry(owner,state,frames[sample].before,sample%2);
        map_blk[650]=terrain[land]; map_who[650]=pop%3;
        game_mode=fatal; A4_WORD(0xae80)=war;
        set_peep_word(0,4,arithmetic[pop].population); A4_WORD(0x2fb0)=arithmetic[pop].loss;
        if (sample%2) { core_leader(owner)=0; memcpy(peeps+14,"\x12\x34\x56\x78",4); }
        /* Ordinary co-located enemy, not battle-exempt: frame-5 completion
         * must not encounter it. It still receives its own later 0->1 frame. */
        no_peeps=2; peeps[22]=2; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,6,29); set_peep_word(1,8,650);
        set_peep_word(1,10,-63); set_peep_word(1,12,0);
        game_turn=8;
        for (int side=0; side<2; ++side) {
            A4_DISP(0x82b6+side*46)[0]=2;
            A4_DISP(0x82b7+side*46)[0]=27; A4_DISP(0x82b8+side*46)[0]=31;
            A4_WORD(0x82be + side*46)=7; A4_WORD(0x82bc+side*46)=1;
        }
        uint8_t expected[sizeof prg_a4_area];
        int remaining=fatal ? 0 : arithmetic[pop].remaining;
        water_loss_expected(expected,owner,650,remaining);
        int complete=!fatal && remaining!=0 && frames[sample].complete;
        expected[peeps-prg_a4_area]=remaining>0 ? (complete ? state-20 : state-16) : 0;
        int16_t frame=fatal || !remaining ? frames[sample].before : frames[sample].after;
        if (state==0x15 && complete) {
            /* Foreign neighbour overlay 37 rejects life after the centre.
             * Centre 15 records fertile scratch before that rejection. */
            frame=32;
            celebration_expected_word(expected,0x2ff8,land==2 && sample%2);
            celebration_expected_word(expected,0x2fb2,land==2);
        }
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        int16_t later=1; memcpy(expected+(peeps-prg_a4_area)+22+12,&later,2);
        int16_t ai_seed=expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,4)==(remaining>0 ? remaining : 0));
        assert(peep_word(0,12)==frame && peeps[0]==expected[peeps-prg_a4_area]);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==ai_seed);
    }
}

static void test_water_celebration_towns(void)
{
    /* Independent coordinate fixtures yield capacities 0,65,185,290,3050.
     * No core_offsets/core_city/check_life calls build expected results. */
    const struct { int fertile, frame; } capacities[]={{0,32},{1,34},{9,38},{16,41},{17,42}};
    const int positions[]={650,0,63,4032,4095,32,2048,2047,4064};
    for (int owner=0; owner<2; ++owner) for (int city=0; city<2; ++city)
    for (int population=1; population<=3; ++population) for (int fatal=0; fatal<2; ++fatal)
    for (unsigned sample=0; sample<sizeof capacities/sizeof capacities[0]; ++sample)
    for (unsigned place=0; place<sizeof positions/sizeof positions[0]; ++place) {
        /* Edge fixtures use full fertility: corners have 7 cells (155),
         * straight edges 11 (215); only the interior reaches 3050. */
        if (place && capacities[sample].fertile!=17) continue;
        setup_water_entry(owner,0x15,87,city);
        int position=positions[place], fertile=capacities[sample].fertile;
        set_peep_word(0,8,position); set_peep_word(0,4,population);
        memset(map_blk,1,4096); memset(map_bk2,0,4096); memset(map_who,0,4096);
        map_who[position]=1; game_mode=fatal; A4_WORD(0x2fb0)=1;
        game_turn=8;
        for (int level=0; level<11; ++level) {
            A4_WORD(0x2fb6+level*2)=17; A4_WORD(0x2fcc+level*2)=9;
            A4_WORD(0x2fe2+level*2)=11;
        }
        for (int row=0; row<64; ++row) for (int column=0; column<64; ++column) {
            int dy=abs(row-position/64), dx=abs(column-position%64), cell=row*64+column;
            int inner=dy<=1 && dx<=1;
            int outer=dy<=2 && dx<=2 && !((dy==2 && dx==1) || (dy==1 && dx==2));
            int planted=(fertile>=9 && inner) || (fertile>=16 && outer) || (fertile && !dx && !dy);
            if (fertile==16 && row==position/64-2 && column==position%64-2) planted=0;
            if (planted) map_blk[cell]=31+owner;
            if (dy<=2 && dx<=2) map_bk2[cell]=inner ? (city ? 41 : 0) : 30;
        }
        map_blk[position]=fertile ? 15 : 1; map_bk2[position]=city ? 42 : 37;
        uint8_t expected[sizeof prg_a4_area];
        int remaining=fatal ? 0 : population-2;
        water_loss_expected(expected,owner,position,remaining);
        expected[peeps-prg_a4_area]=remaining>0 ? 1 : 0;
        int16_t frame=87;
        if (!fatal && remaining) {
            frame=place ? (place<=4 ? 37 : 39) : capacities[sample].frame;
            celebration_expected_word(expected,0x2ff8,fertile && city ? (place ? (place<=4 ? 4 : 6) : 9) : 0);
            celebration_expected_word(expected,0x2fb2,fertile!=0);
        }
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        int16_t after=expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,12)==frame);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==after);
    }
}

static void test_water_celebration_gates(void)
{
    const int dead[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int state=0x15; state<=0x16; ++state)
    for (int fatal=0; fatal<2; ++fatal) {
        for (unsigned sample=0; sample<sizeof dead/sizeof dead[0]; ++sample) {
            setup_water_entry(owner,state,87,1); map_blk[650]=1; game_mode=fatal;
            set_peep_word(0,4,dead[sample]);
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        setup_water_entry(owner,state,87,1); map_blk[650]=1; game_mode=fatal;
        set_peep_word(0,4,1); no_peeps=18;
        peeps[17*22]=2; peeps[17*22+1]=1-owner;
        set_peep_word(17,4,100); set_peep_word(17,8,900);
        map_blk[900]=1; map_who[900]=18;
        for (int gate=0; gate<5; ++gate) {
            pause=gate==0; core_result=gate>=1 && gate<=3 ? gate : 0; prg_quit=gate==4;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        pause=core_result=prg_quit=0;
        uint8_t expected[sizeof prg_a4_area]; water_loss_expected(expected,owner,650,0);
        int16_t frame=fatal ? 87 : state==0x15 ? 32 : 0;
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        if (!fatal && state==0x15) {
            celebration_expected_word(expected,0x2ff8,0);
            celebration_expected_word(expected,0x2fb2,0);
        }
        frame=1; memcpy(expected+(peeps-prg_a4_area)+17*22+12,&frame,2);
        int16_t turn=game_turn+1; memcpy(expected+((uint8_t *)&game_turn-prg_a4_area),&turn,2);
        /* asm:3459-3474: the dying peep's pre-loss word counts this pass, so
         * the tick continues (picks consumed) and asm:4323-4347 ends next. */
        int32_t dying=peep_word(0,4), friendly=owner ? 100 : dying, enemy=owner ? dying : 100;
        assert(dying>0);
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        expect_ai_fixture_tick(expected,0);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result && peep_word(17,12)==1);
        assert(!core_battles && !core_settlements);
        game_tick(); assert(core_result==(owner ? 1 : 2) && !A4_LONG(0x99fe + owner*16));
    }
}

static void test_water_celebration(void)
{
    test_water_celebration_state(); test_water_celebration_towns(); test_water_celebration_gates();
}

/* Dry waiting recovery: independent SUB.W witnesses catch clamped triple loss,
 * early death, missing double framing, and retry gated on frame completion. */
static void test_water_wait_state(void)
{
    const struct { int population, loss, water, waiting; } arithmetic[]={
        {100,1,98,97},{4,1,2,1},{3,1,1,0},{2,1,0,-1},{1,1,-1,-2},
        {2,-32767,0,32767},{100,32767,102,-32665},{100,16384,-32668,16484},
        {1,16385,32767,16382},{32767,-1,-32767,-32766},{100,-32768,100,-32668}
    };
    const struct { int before, after; } frames[]={
        {100,102},{101,101},{102,102},{32767,102},{-32768,102},{5,102}
    };
    const int states[]={0x32,0x52,0x72}, terrain[]={1,53,15};
    for (int owner=0; owner<2; ++owner) for (unsigned state=0; state<3; ++state)
    for (int fatal=0; fatal<2; ++fatal) for (int war=0; war<2; ++war)
    for (unsigned land=0; land<3; ++land) for (unsigned sample=0; sample<6; ++sample)
    for (unsigned pop=0; pop<11; ++pop) for (int timer=0; timer<=14; timer+=14) {
        setup_water_entry(owner,states[state],frames[sample].before,sample%2);
        map_blk[650]=terrain[land]; map_who[650]=pop%3;
        set_peep_word(0,4,arithmetic[pop].population); set_peep_word(0,6,timer);
        A4_WORD(0x2fb0)=arithmetic[pop].loss; game_mode=fatal; A4_WORD(0xae80)=war;
        if (sample%2) { core_leader(owner)=0; memcpy(peeps+14,"\x12\x34\x56\x78",4); }
        no_peeps=2; peeps[22]=2; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,6,29); set_peep_word(1,8,650);
        set_peep_word(1,10,-63); set_peep_word(1,12,0);
        for (int side=0; side<2; ++side) {
            A4_DISP(0x82b6+side*46)[0]=2;
            A4_DISP(0x82b7+side*46)[0]=27; A4_DISP(0x82b8+side*46)[0]=31;
            A4_WORD(0x82be + side*46)=7; A4_WORD(0x82bc+side*46)=1;
        }
        uint8_t expected[sizeof prg_a4_area];
        int remaining=fatal ? 0 : arithmetic[pop].waiting;
        water_loss_expected(expected,owner,650,remaining);
        expected[peeps-prg_a4_area]=remaining>0 ? states[state]-16 : 0;
        int16_t frame=fatal || !arithmetic[pop].water ? frames[sample].before : frames[sample].after;
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        int16_t counter=fatal ? timer : timer+1;
        memcpy(expected+(peeps-prg_a4_area)+6,&counter,2);
        int16_t later=1; memcpy(expected+(peeps-prg_a4_area)+22+12,&later,2);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,4)==(remaining>0 ? remaining : 0));
        assert(peep_word(0,12)==frame && peep_word(0,6)==counter);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==123);
    }
}

static void test_water_wait_retry(void)
{
    const struct { int population, loss, remaining, frame5, frame101; } arithmetic[]={
        {100,1,97,0,0},{4,1,1,0,0},{3,1,0,102,101},{2,1,-1,6,0},{1,1,-2,0,0},
        {2,-32767,32767,6,0},{100,32767,-32665,0,0},{100,16384,16484,0,0},
        {1,16385,16382,0,0},{32767,-1,-32766,0,0},{100,-32768,-32668,0,0}
    };
    const struct { int before, after; } timers[]={{15,16},{32767,-32768},{-1,0}};
    const int states[]={0x32,0x52,0x72};
    for (int owner=0; owner<2; ++owner) for (unsigned state=0; state<3; ++state)
    for (int blocked=0; blocked<2; ++blocked) for (int swamp=0; swamp<2; ++swamp)
    for (unsigned timer=0; timer<3; ++timer) for (unsigned pop=0; pop<11; ++pop)
    for (int entry=0; entry<2; ++entry) {
        setup_water_entry(owner,states[state],entry ? 101 : 5,1);
        memset(map_blk,0,4096); map_blk[650]=swamp ? 53 : 1;
        core_tendency(owner)=blocked ? 1 : 0; core_target(owner)=651;
        if (!blocked) map_blk[651]=1;
        set_peep_word(0,4,arithmetic[pop].population); A4_WORD(0x2fb0)=arithmetic[pop].loss;
        set_peep_word(0,6,timers[timer].before);
        uint8_t expected[sizeof prg_a4_area];
        int remaining=arithmetic[pop].remaining;
        water_loss_expected(expected,owner,650,remaining);
        expected[peeps-prg_a4_area]=remaining>0 ? (blocked ? 0x42 : 2) : 0;
        int16_t frame=entry ? arithmetic[pop].frame101 : arithmetic[pop].frame5;
        int16_t counter=blocked ? 7 : timers[timer].after;
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        memcpy(expected+(peeps-prg_a4_area)+6,&counter,2);
        if (!blocked) {
            expected[peeps-prg_a4_area+21]=1;
            if (remaining>0) {
                int16_t position=651, direction=1;
                memcpy(expected+(peeps-prg_a4_area)+8,&position,2);
                memcpy(expected+(peeps-prg_a4_area)+10,&direction,2);
                /* _move_explorer asm:4590-4617: count and keep the
                 * departure claim at 650; 651 waits for the next departure. */
                expected[map_who-prg_a4_area+650]=1; expected[map_who-prg_a4_area+651]=0;
                celebration_expected_word(expected,0xff88+650*2,668);
            }
        }
        /* AI first (asm:3254-3270): a tendency-0 owner picks (owner 1 keeps
         * tendency 0 unless it is the blocked walker's owner). Then blocked
         * exploration visits the random ray once. Seeds 123, 15930, 28505, 12248. */
        int picks0=!owner && !blocked, picks1=!owner || !blocked;
        if (picks0) expect_ai_pick(expected,0,1,15930);
        if (picks1) expect_ai_pick(expected,1,picks0 ? 3 : 1,picks0 ? 28505 : 15930);
        const int16_t seeds[]={123,15930,28505,12248};
        int16_t final_seed=seeds[picks0+picks1+blocked];
        celebration_expected_word(expected,0x99e2,final_seed);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,4)==(remaining>0 ? remaining : 0) && peep_word(0,12)==frame);
        assert(peep_word(0,6)==counter && peeps[0]==expected[peeps-prg_a4_area]);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==final_seed);
    }
}

static void test_water_wait_dead_encounter(void)
{
    const int states[]={0x32,0x52,0x72};
    /* Real NONCOMBAT enemies distinguish current-cell encounter-before-death
     * from forbidden dead departure; reciprocal cleanup must release them. */
    for (int owner=0; owner<2; ++owner) for (unsigned state=0; state<3; ++state)
    for (int colocated=0; colocated<2; ++colocated) for (int population=1; population<=3; ++population) {
        setup_water_entry(owner,states[state],5,1);
        memset(map_blk,1,4096); core_tendency(owner)=0; core_target(owner)=651;
        set_peep_word(0,4,population); set_peep_word(0,6,15);
        no_peeps=2; peeps[22]=2; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,6,29); set_peep_word(1,8,colocated ? 650 : 651);
        set_peep_word(1,10,-63); set_peep_word(1,12,0);
        map_who[colocated ? 650 : 651]=2;
        uint8_t expected[sizeof prg_a4_area]; water_loss_expected(expected,owner,650,0);
        int16_t frame=colocated ? (population==3 ? 102 : 70) : (population==3 ? 102 : population==2 ? 6 : 0);
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        int16_t counter=colocated ? 0 : 16;
        memcpy(expected+(peeps-prg_a4_area)+6,&counter,2);
        expected[peeps-prg_a4_area+21]=1;
        int16_t later=1; memcpy(expected+(peeps-prg_a4_area)+22+12,&later,2);
        if (colocated) {
            int16_t zero=0;
            memcpy(expected+(peeps-prg_a4_area)+22+6,&zero,2);
            memcpy(expected+(peeps-prg_a4_area)+22+10,&zero,2);
        }
        int16_t after=expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==colocated && !core_settlements && !core_result && seed==after);
    }
}

static void test_water_wait_gates(void)
{
    const int states[]={0x32,0x52,0x72}, dead[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (unsigned state=0; state<3; ++state)
    for (int fatal=0; fatal<2; ++fatal) {
        for (unsigned sample=0; sample<3; ++sample) {
            setup_water_entry(owner,states[state],5,1); map_blk[650]=1; game_mode=fatal;
            set_peep_word(0,4,dead[sample]);
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            expect_ai_fixture_picks(expected);
            expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        setup_water_entry(owner,states[state],5,1); map_blk[650]=1; game_mode=fatal;
        set_peep_word(0,4,1); set_peep_word(0,6,14); no_peeps=18;
        peeps[17*22]=2; peeps[17*22+1]=1-owner;
        set_peep_word(17,4,100); set_peep_word(17,8,900); map_blk[900]=1; map_who[900]=18;
        for (int gate=0; gate<5; ++gate) {
            pause=gate==0; core_result=gate>=1 && gate<=3 ? gate : 0; prg_quit=gate==4;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        pause=core_result=prg_quit=0;
        uint8_t expected[sizeof prg_a4_area]; water_loss_expected(expected,owner,650,0);
        int16_t frame=fatal ? 5 : 102, counter=fatal ? 14 : 15;
        memcpy(expected+(peeps-prg_a4_area)+12,&frame,2);
        memcpy(expected+(peeps-prg_a4_area)+6,&counter,2);
        frame=1; memcpy(expected+(peeps-prg_a4_area)+17*22+12,&frame,2);
        int16_t turn=game_turn+1; memcpy(expected+((uint8_t *)&game_turn-prg_a4_area),&turn,2);
        /* asm:3459-3474: the waiter's pre-loss 1 counts, so this tick does
         * not end the game and the picks are consumed (asm:18410-18431). */
        int32_t friendly=owner ? 100 : 1, enemy=owner ? 1 : 100;
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        expect_ai_fixture_tick(expected,0);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result && !core_battles && !core_settlements);
        game_tick(); assert(core_result==(owner ? 1 : 2) && !A4_LONG(0x99fe + owner*16));
    }
    const int mixed[]={0x30,0x31,0x33,0x36,0x50,0x51,0x53,0x56,0x70,0x71,0x73,0x76,0x92,0xb2,0xd2,0xf2};
    for (unsigned sample=0; sample<sizeof mixed/sizeof mixed[0]; ++sample)
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal) {
        setup_water_entry(owner,mixed[sample],5,1); map_blk[650]=53; game_mode=fatal;
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,1);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_water_wait(void)
{
    test_water_wait_state(); test_water_wait_retry(); test_water_wait_dead_encounter(); test_water_wait_gates();
}

/* Exact dry 0x90: catch missing loss/bit clear, early death before timer,
 * signed timer expiry, unwanted framing/movement and town/map side effects. */
static void water_remnant_expected(uint8_t *expected, int owner, int position,
                                   int remaining, int16_t timer, int dry)
{
    water_loss_expected(expected,owner,position,remaining);
    if (!dry || (game_mode&1)) return;
    expected[peeps-prg_a4_area]=remaining>0 ? 0x80 : 0;
    memcpy(expected+(peeps-prg_a4_area)+6,&timer,2);
    if (remaining>0 && !map_who[position]) expected[map_who-prg_a4_area+position]=1;
}

static void test_water_remnant_state(void)
{
    const struct { int population, loss, result; } arithmetic[]={
        {3,1,1},{2,1,0},{1,1,-1},{1,0x4000,-32767},
        {1,0x4001,32767},{1,0x8000,1},{1,0xffff,3},{100,30000,5636}
    };
    const struct { int before, after; } timers[]={{40,39},{0,0xffff},{0xffff,0xfffe},{1,0}};
    const int frames[]={0,7,42,32767,-1}, terrain[]={1,53,15,31,255};
    for (int dry=1; dry>=0; --dry) for (int owner=0; owner<2; ++owner)
    for (int fatal=0; fatal<2; ++fatal) for (int war=0; war<2; ++war)
    for (int heading=0; heading<2; ++heading) for (int occupancy=0; occupancy<3; ++occupancy)
    for (unsigned sample=0; sample<8; ++sample) for (unsigned timer=0; timer<4; ++timer)
    for (unsigned frame=0; frame<5; ++frame) {
        setup_water_entry(owner,0x90,frames[frame],1);
        map_blk[650]=dry ? terrain[frame] : 0; map_who[650]=occupancy;
        set_peep_word(0,4,arithmetic[sample].population); set_peep_word(0,6,timers[timer].before);
        game_mode=fatal; A4_WORD(0xae80)=war; A4_WORD(0x2fb0)=arithmetic[sample].loss;
        if (heading) { core_leader(owner)=0; memcpy(peeps+14,"\x12\x34\x56\x78",4); }
        /* _zero_population asm:5671-5680 clears the previous-cell claim
         * (650-(-64)=714) on death. Host death clears flags; original
         * retains the flag byte. */
        map_who[714]=1;
        no_peeps=2; peeps[22]=2; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,6,29); set_peep_word(1,8,900);
        map_blk[900]=1; map_who[900]=2;
        for (int side=0; side<2; ++side) {
            A4_DISP(0x82b6+side*46)[0]=2;
            A4_DISP(0x82b7+side*46)[0]=27; A4_DISP(0x82b8+side*46)[0]=31;
            A4_WORD(0x82be + side*46)=7; A4_WORD(0x82bc+side*46)=1;
        }
        int remaining=fatal || (dry && !timers[timer].before) ? 0 : arithmetic[sample].result;
        int16_t counter=dry && !fatal ? timers[timer].after : timers[timer].before;
        uint8_t expected[sizeof prg_a4_area];
        water_remnant_expected(expected,owner,650,remaining,counter,dry);
        if (remaining<=0) expected[map_who-prg_a4_area+714]=0;
        int16_t later=1; memcpy(expected+(peeps-prg_a4_area)+22+12,&later,2);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(0,6)==counter);
        assert(peeps[0]==(remaining>0 ? (dry ? 0x80 : 0x90) : 0));
        assert(peep_word(0,4)==(remaining>0 ? remaining : 0));
        assert(peep_word(0,12)==frames[frame]);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && !prg_quit && seed==123);
    }
}

static void test_water_remnant_frames_and_expiry(void)
{
    const int populations[]={1,0,-1,-32768}, frames[]={0,7,42,32767,-1};
    for (int owner=0; owner<2; ++owner) for (unsigned pop=0; pop<4; ++pop)
    for (unsigned frame=0; frame<5; ++frame) {
        setup_water_entry(owner,0x80,frames[frame],1); map_blk[650]=53;
        set_peep_word(0,4,populations[pop]);
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        assert(!core_frame_walker(0)); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    for (int owner=0; owner<2; ++owner) for (int occupancy=0; occupancy<3; ++occupancy) {
        setup_water_entry(owner,0x90,42,1); map_blk[650]=53; map_who[650]=occupancy;
        set_peep_word(0,4,3); set_peep_word(0,6,1);
        uint8_t expected[sizeof prg_a4_area]; water_remnant_expected(expected,owner,650,1,0,1);
        expect_ai_tendency_pick(expected); /* first pass only: then latched */
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        /* Ordinary positive 0x80 expires only when the NEXT old timer is zero,
         * with no second water loss, frame change or swamp consumption. */
        water_remnant_expected(expected,owner,650,0,-1,1);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_water_remnant_edges(void)
{
    const int positions[]={0,63,4032,4095};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int population=1; population<=3; ++population) for (unsigned place=0; place<4; ++place)
    for (int occupancy=0; occupancy<3; ++occupancy) {
        setup_water_entry(owner,0x90,42,1); game_mode=fatal;
        int position=positions[place]; set_peep_word(0,8,position);
        set_peep_word(0,4,population); set_peep_word(0,6,40);
        memset(map_who,0,4096); map_who[position]=occupancy;
        for (int cell=0; cell<4096; ++cell) { map_blk[cell]=31+cell%2; map_bk2[cell]=42; }
        uint8_t expected[sizeof prg_a4_area];
        water_remnant_expected(expected,owner,position,fatal ? 0 : population-2,fatal ? 40 : 39,1);
        expect_ai_tendency_pick(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==15930);
    }
}

static void test_water_remnant_gates(void)
{
    const int dead[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int dry=0; dry<2; ++dry) {
        for (int state=0x80; state<=0x90; state+=0x10) for (unsigned sample=0; sample<3; ++sample) {
            setup_water_entry(owner,state,42,1); map_blk[650]=dry; game_mode=fatal;
            set_peep_word(0,4,dead[sample]);
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            expect_ai_tendency_pick(expected);
            expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        setup_water_entry(owner,0x90,42,1); map_blk[650]=dry; game_mode=fatal;
        set_peep_word(0,4,1); set_peep_word(0,6,40); no_peeps=18;
        peeps[17*22]=2; peeps[17*22+1]=1-owner;
        set_peep_word(17,4,100); set_peep_word(17,8,900); map_blk[900]=1; map_who[900]=18;
        for (int gate=0; gate<5; ++gate) {
            pause=gate==0; core_result=gate>=1 && gate<=3 ? gate : 0; prg_quit=gate==4;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        pause=core_result=prg_quit=0;
        uint8_t expected[sizeof prg_a4_area];
        water_remnant_expected(expected,owner,650,0,dry && !fatal ? 39 : 40,dry);
        int16_t frame=1; memcpy(expected+(peeps-prg_a4_area)+17*22+12,&frame,2);
        int16_t turn=game_turn+1; memcpy(expected+((uint8_t *)&game_turn-prg_a4_area),&turn,2);
        /* asm:3459-3474: the remnant's pre-loss 1 counts this pass. */
        int32_t friendly=owner ? 100 : 1, enemy=owner ? 1 : 100;
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        /* Not ending, game_tick consumes side 1's pick instead of clearing
         * it: _do_action y=1 (asm:18410-18431) stores tendency x=1. */
        expect_ai_pick_cleared(expected); celebration_expected_word(expected,0x99fa+16,1);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result && !core_battles && !core_settlements);
        /* asm:4323-4347 then ends on the next pass's zero sum. */
        game_tick(); assert(core_result==(owner ? 1 : 2) && !A4_LONG(0x99fe + owner*16));
    }
    /* All other unported dry water combinations retain reset/no-loss behavior.
     * Exact 0x90 and 0x11 have dedicated full-state coverage, never this oracle. */
    const int mixed[]={0x10,0x13,0x14,0x17,0x18,0x19,0x1e,0x30,0x31,
                       0x33,0x36,0x50,0x51,0x53,0x56,0x70,0x71,0x73,0x76,
                       0x91,0x92,0x94,0x95,0x96,0x97,0x98,0xb2,0xd2,0xf2};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (unsigned sample=0; sample<sizeof mixed/sizeof mixed[0]; ++sample) {
        setup_water_entry(owner,mixed[sample],42,1); map_blk[650]=53; game_mode=fatal;
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,1);
        expect_ai_tendency_pick(expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_water_remnant(void)
{
    test_water_remnant_state(); test_water_remnant_frames_and_expiry();
    test_water_remnant_edges(); test_water_remnant_gates();
}

/* Synthetic consumer fixtures; production integration is in --heading-town. */
static void setup_remnant(int owner, int counter)
{
    setup_blocked_walker();
    peeps[0]=0x80; peeps[1]=owner;
    set_peep_word(0,4,1); set_peep_word(0,6,counter);
    set_peep_word(0,10,-64); set_peep_word(0,12,42);
    for (int byte=14; byte<22; ++byte) peeps[byte]=byte;
    seed=123; core_result=0; prg_quit=0;
}

static void test_remnant_timer(void)
{
    const unsigned counters[] = {40,0,1,0x7fff,0x8000,0xffff};
    for (int owner=0; owner<2; ++owner)
        for (unsigned sample=0; sample<sizeof counters/sizeof counters[0]; ++sample) {
            unsigned counter=counters[sample];
            setup_remnant(owner,counter);
            core_leader(owner)=1; core_leader(1-owner)=2; view_who=1;
            core_target(owner)=1234; core_target(1-owner)=2345;
            /* Exercise all 65536 ticks for 0xffff, not a signed expiry shortcut. */
            for (unsigned tick=0; tick<counter; ++tick) {
                _move_peeps();
                assert(peeps[0]==0x80 && peep_word(0,4)==1);
                assert((uint16_t)peep_word(0,6)==counter-tick-1);
                assert(core_population(owner)==1 && map_who[650]==1);
                assert(!core_leader(owner) && view_who==1 && core_target(owner)==650);
            }
            _move_peeps();
            assert(!peeps[0] && !peep_word(0,4) && !core_population(owner));
            assert((uint16_t)peep_word(0,6)==0xffff && !map_who[650]);
            assert(!core_leader(owner) && !view_who && core_target(owner)==650);
            assert(A4_WORD(owner ? 0xae76 : 0xae78)==650);
            assert(core_leader(1-owner)==2 && core_target(1-owner)==2345);
            /* Owner 1 (tendency 0, latch clear) picks once on the first pass. */
            assert(seed==15930 && A4_DISP(0x82e4)[0]==14 && A4_WORD(0x82ec)==1);
            assert(peep_word(0,8)==650 && peep_word(0,10)==-64);
            assert(peep_word(0,12)==42 && !core_battles && !core_settlements);
            for (int byte=14; byte<22; ++byte) assert(peeps[byte]==byte);
        }
}

static void test_remnant_occupancy_and_state(void)
{
    for (int representative=0; representative<=2; ++representative) {
        setup_remnant(0,1); map_who[650]=representative;
        for (int cell=0; cell<4096; ++cell) A4_WORD(0xff88+cell*2)=cell;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        expect_ai_tendency_pick(before); /* the only other first-pass write */
        expect_pass_good_pop(before); /* and the pass's good_pop */
        _move_peeps();
        assert(map_who[650]==(representative ? representative : 1));
        assert(peep_word(0,6)==0);
        /* Apart from timer and empty-slot claim, consumption is inert. */
        set_peep_word(0,6,1); map_who[650]=representative;
        assert(a4_same(before));
        set_peep_word(0,6,0); _move_peeps();
        assert(!peep_word(0,4) && map_who[650]==(representative==2 ? 2 : 0));
    }
}

static void test_remnant_tick_and_water(void)
{
    for (int owner=0; owner<2; ++owner) {
        setup_remnant(owner,40);
        no_peeps=2; peeps[22]=0x80; peeps[23]=1-owner;
        set_peep_word(1,4,1); set_peep_word(1,6,100); set_peep_word(1,8,651);
        map_blk[651]=1; map_who[651]=2;
        for (int gate=0; gate<4; ++gate) {
            pause=gate==0; core_result=gate;
            uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
            game_tick();
            assert(a4_same(before));
        }
        pause=0; core_result=0;
        for (int tick=1; tick<=40; ++tick) {
            game_tick();
            assert(game_turn==tick && peep_word(0,6)==40-tick && !core_result);
            assert(A4_LONG(0x99fe)==1 && A4_LONG(0x9a0e)==1);
        }
        game_tick();
        assert(game_turn==41 && !peep_word(0,4) && !map_who[650]);
        /* asm:3459-3474: the expiring remnant's 1 is counted before expiry,
         * so asm:4323-4347 ends on pass 42. */
        assert(A4_LONG(0x99fe)==1 && A4_LONG(0x9a0e)==1 && !core_result);
        game_tick();
        assert(game_turn==42 && A4_LONG(owner ? 0x9a0e : 0x99fe)==0);
        assert(A4_LONG(owner ? 0x99fe : 0x9a0e)==1 && core_result==(owner ? 1 : 2));
    }
    /* New water entry precedes the remnant timer and defers loss/fatal removal. */
    for (int fatal=0; fatal<=1; ++fatal)
        for (int population=1; population<=3; population+=2) {
            setup_remnant(0,40); set_peep_word(0,4,population);
            map_blk[650]=0; game_mode=fatal;
            _move_peeps();
            assert(peeps[0]==0x12 && peep_word(0,4)==population);
            assert(peep_word(0,6)==40 && map_who[650]==1 && peep_word(0,12)==0);
        }
    /* Mixed states retain existing dispatch; only exact 0x80 gains a timer. */
    setup_remnant(0,5); peeps[0]=0xa0; set_peep_word(0,4,100);
    _move_peeps();
    assert(peeps[0]==0xa0 && peep_word(0,6)==6 && peep_word(0,4)==99);
    setup_remnant(0,5); peeps[0]=0x88;
    _move_peeps();
    assert(peeps[0]==0x88 && peep_word(0,6)==5 && peep_word(0,4)==1);
    setup_remnant(0,5); peeps[0]=0x82;
    _move_peeps();
    assert(peeps[0]==0x82 && peep_word(0,6)==5 && peep_word(0,4)==1);
}

static void test_remnant_collision(void)
{
    const int remnant_flags[] = {0x80,0x88,0xa0};
    for (int owner=0; owner<2; ++owner)
        for (int friendly=0; friendly<2; ++friendly)
            for (int destination=0; destination<2; ++destination)
                for (unsigned state=0; state<sizeof remnant_flags/sizeof remnant_flags[0]; ++state) {
                    setup_blocked_walker(); peeps[1]=owner; no_peeps=2;
                    int position=destination ? 651 : 650;
                    peeps[22]=remnant_flags[state]; peeps[23]=friendly ? owner : 1-owner;
                    set_peep_word(1,4,1); set_peep_word(1,6,40);
                    set_peep_word(1,8,position); set_peep_word(1,12,42);
                    map_blk[651]=1; map_who[position]=2;
                    core_tendency(owner)=0; core_leader(owner)=1; core_target(owner)=651;
                    uint8_t resident[22]; memcpy(resident,peeps+22,sizeof resident);
                    core_move(0);
                    assert(peeps[0]==2 && peep_word(0,4)==100 && peep_word(0,8)==651);
                    assert(!core_battles && !core_settlements && !memcmp(resident,peeps+22,sizeof resident));
                    /* _move_explorer asm:4438-4449,4530-4534,4590-4617: the
                     * walker re-claims empty 650 after releasing its own
                     * claim there, but never overwrites a bit-7 resident. */
                    assert(map_who[650]==(destination ? 1 : 2));
                    assert(map_who[651]==(destination ? 2 : 0));
                    assert(A4_WORD(0xff88+650*2)==1 && !A4_WORD(0xff88+651*2));
                    /* Remnant death clears only its own claim. */
                    if (remnant_flags[state]==0x80) {
                        _move_peeps();
                        assert(peep_word(1,6)==39 && peep_word(1,4)==1);
                        assert(map_who[651]==(destination ? 2 : 0));
                        set_peep_word(1,6,0); _move_peeps();
                        assert(!peep_word(1,4) && map_who[650]==(destination ? 1 : 0) && !map_who[651]);
                    }
                }
    /* Reverse slot order: consume/claim the resident before the walker moves. */
    for (int counter=0; counter<=1; ++counter)
        for (int destination=0; destination<=1; ++destination)
            for (int friendly=0; friendly<=1; ++friendly) {
                setup_remnant(friendly ? 0 : 1,counter); no_peeps=2;
                int position=destination ? 651 : 650;
                set_peep_word(0,8,position);
                peeps[22]=2; peeps[23]=0;
                set_peep_word(1,4,100); set_peep_word(1,8,650); set_peep_word(1,12,7);
                map_blk[651]=1; map_who[650]=destination ? 2 : 0; map_who[651]=0;
                core_tendency(0)=0; core_leader(0)=2; core_target(0)=651;
                A4_WORD(0x2fb0)=0; /* Isolate collision loss from walking attrition. */
                _move_peeps();
                assert(peeps[0]==(counter ? 0x80 : 0) && peep_word(0,4)==counter);
                assert((uint16_t)peep_word(0,6)==(counter ? 0 : 0xffff));
                assert(peeps[22]==2 && peep_word(1,4)==100 && peep_word(1,8)==651);
                /* _move_explorer asm:4590-4617: the walker claims 650 unless
                 * the surviving remnant already holds it. */
                assert(peep_word(1,12)==0 && map_who[651]==(destination && counter ? 1 : 0));
                assert(map_who[650]==(!destination && counter ? 1 : 2));
                assert(core_population(0)==100+(friendly ? counter : 0));
                assert(core_population(1)==(friendly ? 0 : counter));
                assert(!core_battles && !core_settlements && A4_WORD(0xff88+650*2)==1);
                assert(!A4_WORD(0xff88+651*2));
                _move_peeps();
                assert(!peeps[0] && !peep_word(0,4) && !map_who[651]);
                assert(map_who[650]==(!destination && counter ? 0 : 2));
                assert(peeps[22]==2 && peep_word(1,4)==100 && peep_word(1,8)==651);
                assert(core_population(0)==100 && !core_population(1));
                assert(!core_battles && !core_settlements);
            }
    /* _move_explorer asm:4530-4534,4564-4589: a bit-7 resident is ignored
     * and settlement has no occupancy guard; the claim is only if empty. */
    for (int friendly=0; friendly<2; ++friendly) {
        setup_blocked_walker(); memset(map_blk,15,4096); no_peeps=2;
        peeps[22]=0x80; peeps[23]=friendly ? 0 : 1;
        set_peep_word(1,4,1); set_peep_word(1,6,40); set_peep_word(1,8,650);
        map_who[650]=2;
        assert(core_explore(0)==0 && core_check_life(0,650)>0);
        core_move(0);
        assert(peeps[0]==1 && peeps[22]==0x80 && peep_word(1,4)==1);
        assert(map_blk[650]==31 && map_bk2[650]==peep_word(0,12));
        assert(map_who[650]==2 && !core_battles && core_settlements==1);
    }
}

/* Consumer-only fixtures: no Armageddon producer or war navigation enabled. */
static void setup_war_town(int owner, int position, int city)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    memset(map_blk,15,4096);
    no_peeps=1; core_result=prg_quit=core_battles=core_settlements=0;
    /* Direct consumer calls model the first town of a pass (asm:3331-3335). */
    core_oldest_town_age[0]=core_oldest_town_age[1]=0;
    core_best_town_capacity[0]=core_best_town_capacity[1]=0;
    core_youngest_town_age[0]=core_youngest_town_age[1]=0x4e1f;
    core_city_count[0]=core_city_count[1]=0;
    peeps[0]=1; peeps[1]=owner; peeps[2]=3; peeps[3]=7;
    set_peep_word(0,4,3100); set_peep_word(0,6,1234);
    set_peep_word(0,8,position); set_peep_word(0,10,-65);
    set_peep_word(0,12,city ? 42 : 37);
    for (int byte=18; byte<22; ++byte) peeps[byte]=0x80+byte;
    core_set_town(0,0); map_who[position]=1;
    core_leader(owner)=1; core_leader(1-owner)=2; view_who=1;
    core_tendency(0)=3; core_tendency(1)=2;
    A4_LONG(0x9a02)=12345; A4_LONG(0x9a12)=23456;
    seed=123; game_turn=8; A4_WORD(0x2fb0)=1;
    for (int level=0; level<11; ++level) {
        A4_WORD(0x2fb6+level*2)=17; A4_WORD(0x2fcc+level*2)=9;
        A4_WORD(0x2fe2+level*2)=11;
    }
}

static void test_war_target_reset(void)
{
    const int wars[] = {0,1,0x8000,0xffff};
    for (unsigned sample=0; sample<sizeof wars/sizeof wars[0]; ++sample)
        for (int records=0; records<=2; ++records) {
            setup_war_town(0,650,1); no_peeps=records;
            set_peep_word(0,4,0); set_peep_word(1,4,-1);
            peeps[14]=0x80; peeps[22+17]=0xff;
            A4_WORD(0xae80)=wars[sample];
            for (int repeat=0; repeat<3; ++repeat) {
                core_target(0)=101+repeat; core_target(1)=201+repeat;
                A4_WORD(0xae78)=301+repeat; A4_WORD(0xae76)=401+repeat;
                uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
                _move_peeps();
                int released0=records>=1 && !repeat, released1=records>=2 && !repeat;
                assert(core_target(0)==(wars[sample] ? 0x820 : released0 ? 650 : 101+repeat));
                assert(core_target(1)==(wars[sample] ? 0x820 : released1 ? 0 : 201+repeat));
                assert(A4_WORD(0xae78)==(wars[sample] ? 0x820 : released0 ? 650 : 301+repeat));
                assert(A4_WORD(0xae76)==(wars[sample] ? 0x820 : released1 ? 0 : 401+repeat));
                assert(core_leader(0)==(records>=1 ? 0 : 1));
                assert(core_leader(1)==(records>=2 ? 0 : 2));
                core_target(0)=101+repeat; core_target(1)=201+repeat;
                A4_WORD(0xae78)=301+repeat; A4_WORD(0xae76)=401+repeat;
                if (released0) core_leader(0)=1;
                if (released1) core_leader(1)=2;
                assert(a4_same(before));
                if (released0) core_leader(0)=0;
                if (released1) core_leader(1)=0;
            }
        }
    for (int gate=0; gate<5; ++gate) {
        setup_war_town(0,650,1); A4_WORD(0xae80)=1;
        core_target(0)=123; core_target(1)=456;
        A4_WORD(0xae78)=789; A4_WORD(0xae76)=987;
        pause=gate==0; prg_quit=gate==1; core_result=gate>=2 ? gate-1 : 0;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        game_tick();
        assert(a4_same(before));
    }
}

static void test_war_evacuation(void)
{
    /* Catch missing war/zero-capacity/any-byte metadata predicates, early
     * growth/splitting, lost metadata, and occupancy reclamation on evacuation. */
    for (int owner=0; owner<2; ++owner) for (int trigger=0; trigger<10; ++trigger)
        for (int occupant=0; occupant<=2; ++occupant) for (int direct=0; direct<=1; ++direct) {
            setup_war_town(owner,650,1);
            if (trigger==0) A4_WORD(0xae80)=1;
            else if (trigger==1) map_blk[650]=1;
            else peeps[14+(trigger-2)/2]=(trigger&1) ? 0x80 : 1;
            map_who[650]=occupant;
            assert(core_check_life(owner,650)==(trigger==1 ? 0 : 3050));
            A4_WORD(0x2ff8)=A4_WORD(0x2fb2)=-1;
            uint8_t records[208*22], occupancy[4096];
            memcpy(records,peeps,sizeof records); memcpy(occupancy,map_who,sizeof occupancy);
            records[0]=2; memset(records+10,0,4);
            if (direct) core_settle_tick(0); else _move_peeps();
            assert(!memcmp(records,peeps,sizeof records));
            assert(!memcmp(occupancy,map_who,sizeof occupancy) && no_peeps==1);
            assert(!map_bk2[650] && map_blk[650]==(trigger==1 ? 1 : 15));
            assert(core_leader(owner)==(!direct && trigger>=2 ? 0 : 1));
            assert(core_leader(1-owner)==2 && view_who==1);
            assert(core_tendency(0)==3 && core_tendency(1)==2 && seed==123);
            assert(core_mana(0)==12345 && core_mana(1)==23456);
            assert(!core_battles && !core_settlements && game_turn==8);
            assert(A4_WORD(0x2ff8)!=(int16_t)-1 && A4_WORD(0x2fb2)!=(int16_t)-1);
        }
    /* Peace without metadata still grows, pays mana, and splits at tick eight. */
    for (int split=0; split<=1; ++split) {
        setup_war_town(0,650,1); set_peep_word(0,4,split ? 3100 : 100);
        core_settle_tick(0);
        assert(peeps[0]==1 && peep_word(0,12)==42 && map_bk2[650]==42);
        assert(peep_word(0,4)==(split ? 1536 : 111) && core_mana(0)==12362);
        assert(peeps[3]==9 && no_peeps==1+split);
        if (split) assert(peeps[22]==2 && peep_word(1,4)==1575 && core_leader(0)==2);
    }
}

static void test_war_town_footprint(void)
{
    const int positions[] = {650,0,63,4032,4095,32,2048,2111,4064};
    /* Independent coordinate sets for the original 17-cell ordinary footprint
     * and 25-cell city; never derive expected removal with core_set_town. */
    for (int owner=0; owner<2; ++owner) for (int city=0; city<=1; ++city)
        for (unsigned sample=0; sample<sizeof positions/sizeof positions[0]; ++sample) {
            int position=positions[sample];
            setup_war_town(owner,position,city); A4_WORD(0xae80)=1;
            const uint8_t terrain[] = {31,32,53,47,0,1,15};
            uint8_t blocks[4096], buildings[4096], altitude[4096], history[8192], heights[65*65*2];
            for (int cell=0; cell<4096; ++cell) {
                map_blk[cell]=terrain[cell%7]; map_bk2[cell]=55;
                map_alt[cell]=cell%8; A4_WORD(0xff88+cell*2)=cell+123;
            }
            map_blk[position]=31+owner; map_bk2[position]=city ? 42 : 37;
            for (int row=0; row<=64; ++row) for (int column=0; column<=64; ++column)
                core_set_vertex(column,row,(column+row)%9);
            memcpy(blocks,map_blk,sizeof blocks); memcpy(buildings,map_bk2,sizeof buildings);
            memcpy(altitude,map_alt,sizeof altitude); memcpy(history,map_steps,sizeof history);
            memcpy(heights,A4_DISP(0xae86),sizeof heights);
            for (int cell=0; cell<4096; ++cell) {
                int dx=abs(cell%64-position%64), dy=abs(cell/64-position/64);
                int inner=dx<=1 && dy<=1;
                int ordinary=inner || (dx==2 && (dy==0 || dy==2)) || (dy==2 && dx==0);
                int footprint=city ? dx<=2 && dy<=2 : ordinary;
                if (footprint && blocks[cell]==31+owner) blocks[cell]=15;
                if ((city && footprint) || inner) buildings[cell]=0;
            }
            _move_peeps();
            assert(peeps[0]==2 && !peep_word(0,10) && !peep_word(0,12));
            assert(!memcmp(blocks,map_blk,sizeof blocks) && !memcmp(buildings,map_bk2,sizeof buildings));
            assert(!memcmp(altitude,map_alt,sizeof altitude) && !memcmp(history,map_steps,sizeof history));
            assert(!memcmp(heights,A4_DISP(0xae86),sizeof heights));
        }
}

static void test_war_dispatch(void)
{
    const int mixed[] = {3,5,7,0x81,0x83};
    for (unsigned sample=0; sample<sizeof mixed/sizeof mixed[0]; ++sample) {
        setup_war_town(0,650,1); peeps[0]=mixed[sample]; A4_WORD(0xae80)=1;
        peeps[17]=0x80;
        uint8_t record[22], blocks[4096], buildings[4096];
        memcpy(record,peeps,sizeof record); memcpy(blocks,map_blk,sizeof blocks);
        memcpy(buildings,map_bk2,sizeof buildings);
        /* Exact 5 expires from frame 42 and reframes to 42 despite war/heading;
         * unsupported mixed towns remain inert. Neither maintains the map. */
        if (mixed[sample]==5) record[0]=1;
        _move_peeps();
        assert(!memcmp(record,peeps,sizeof record));
        assert(!memcmp(blocks,map_blk,sizeof blocks) && !memcmp(buildings,map_bk2,sizeof buildings));
        assert(core_mana(0)==12345 && seed==123);
    }
    for (int flags=0x21; flags<=0x61; flags+=0x20) {
        setup_war_town(0,650,1); peeps[0]=flags; A4_WORD(0xae80)=1;
        set_peep_word(0,6,7);
        _move_peeps();
        assert(peeps[0]==flags && peep_word(0,4)==3099 && peep_word(0,6)==8);
        assert(peep_word(0,12)==101 && peep_word(0,10)==-65 && map_bk2[650]==42);
    }
    for (int fatal=0; fatal<=1; ++fatal) {
        setup_war_town(0,650,1); A4_WORD(0xae80)=1;
        map_blk[650]=0; game_mode=fatal;
        _move_peeps();
        assert(peeps[0]==18 && peep_word(0,4)==3100);
        assert(peep_word(0,12)==0 && peep_word(0,10)==-65 && !map_bk2[650]);
        assert(map_who[650]==1 && core_mana(0)==12345 && seed==123);
    }
    /* Reciprocal town battle still fights (even on water), never evacuates. */
    for (int water=0; water<=1; ++water) {
        setup_war_town(0,650,1); A4_WORD(0xae80)=1; no_peeps=2;
        peeps[0]=9; peeps[22]=8; peeps[23]=1; peeps[25]=1;
        set_peep_word(0,4,100); set_peep_word(1,4,100);
        set_peep_word(0,6,1); set_peep_word(1,6,0); set_peep_word(1,8,650);
        if (water) map_blk[650]=0;
        _move_peeps();
        assert(peeps[0]==9 && peeps[22]==8 && peep_word(0,4)<100 && peep_word(1,4)<100);
        assert(peep_word(0,12)==70 && peep_word(1,12)==70 && peep_word(0,10)==-65 && map_bk2[650]==42);
        assert(core_mana(0)==12345 && core_mana(1)==23456 && seed!=123);
    }
}

static void test_war_tick_totals(void)
{
    setup_war_town(0,650,1); no_peeps=2; A4_WORD(0xae80)=1; game_turn=7;
    memcpy(peeps+22,peeps,22); peeps[23]=1;
    set_peep_word(1,8,3250); set_peep_word(1,4,2100);
    core_set_town(1,0); map_who[3250]=2;
    game_tick();
    assert(game_turn==8 && !core_result && no_peeps==2);
    assert(peeps[0]==2 && peeps[22]==2 && !peep_word(0,12) && !peep_word(1,12));
    assert(A4_LONG(0x99fe)==3100 && A4_LONG(0x9a0e)==2100);
    assert(core_mana(0)==12345 && core_mana(1)==23456 && seed==123);
    assert(core_target(0)==0x820 && core_target(1)==0x820);
    assert(A4_WORD(0xae78)==0x820 && A4_WORD(0xae76)==0x820);
    assert(core_leader(0)==1 && core_leader(1)==2 && view_who==1);
    assert(map_who[650]==1 && map_who[3250]==2 && !core_battles && !core_settlements);
}

static void test_war_state(void)
{
    test_war_target_reset(); test_war_evacuation(); test_war_town_footprint();
    test_war_dispatch(); test_war_tick_totals();
}

/* Ordinary exact-1 economy fixtures keep controllers neutral. */
static void setup_town_economy(int owner, int population)
{
    setup_war_town(owner,650,1);
    set_peep_word(0,4,population); set_peep_word(0,12,37);
    set_peep_word(0,18,500); map_who[500]=1;
    A4_WORD(0x2ff8)=1234; A4_WORD(0x2fb2)=2345;
    A4_LONG(0x8486)=123456; /* Active minimum must not clamp this ADD.L. */
    for (int cell=0; cell<4096; ++cell) A4_WORD(0xff88+cell*2)=cell+17;
    for (int level=0; level<11; ++level) {
        A4_WORD(0x2fb6+level*2)=100+level;
        A4_WORD(0x2fcc+level*2)=0x1200+level;
        A4_WORD(0x2fe2+level*2)=200+level;
    }
    A4_WORD(0x2fca)=17; A4_WORD(0x2fe0)=0xab09; A4_WORD(0x2ff6)=11;
}

static void town_expected_long(uint8_t *expected, int displacement, uint32_t value)
{
    memcpy(expected+(A4_DISP(displacement)-prg_a4_area),&value,sizeof value);
}

static void town_expected_clear_footprint(uint8_t *expected, int owner)
{
    /* Literal city square centered at (10,10), independent of core_set_town. */
    for (int row=8; row<=12; ++row) for (int column=8; column<=12; ++column) {
        int cell=row*64+column;
        if (map_blk[cell]==31+owner) expected[map_blk-prg_a4_area+cell]=15;
        expected[map_bk2-prg_a4_area+cell]=0;
    }
}

static void town_expected_completion(uint8_t *expected, int owner, int population,
                                     int economy, uint32_t mana)
{
    memcpy(expected,prg_a4_area,sizeof prg_a4_area);
    celebration_expected_word(expected,0x2ff8,9);
    celebration_expected_word(expected,0x2fb2,0);
    celebration_expected_word(expected,0x9c26+12,42);
    celebration_expected_word(expected,0x9c26+4,population>0 ? population : 0);
    if (!map_who[650]) expected[map_who-prg_a4_area+650]=1;
    expect_town_bookkeeping(expected,0,42);
    /* asm:3766-3797: a dry town's reframe to 42 (or a zero +20) reaches
     * _make_level when the owner's latch is clear before the split check
     * (asm:3818+) sets it; a_flat_block is 0 here. */
    if (((!(peeps[0]&0x10) && peep_word(0,12)!=42) || !peeps[20]) &&
        !A4_WORD(0x82be + owner*46) && !map_alt[650]) expect_make_level_exit(expected,0);
    if (economy) {
        expected[peeps-prg_a4_area+3]=9;
        town_expected_long(expected,0x9a02+owner*16,mana);
    }
    if (population>0) return;
    expected[peeps-prg_a4_area]=0;
    if (map_who[650]<=1) expected[map_who-prg_a4_area+650]=0;
    celebration_expected_word(expected,0x99f6+owner*16,0);
    celebration_expected_word(expected,0x99f8+owner*16,650);
    celebration_expected_word(expected,owner ? 0xae76 : 0xae78,650);
    celebration_expected_word(expected,0xae68,0);
    town_expected_clear_footprint(expected,owner);
}

static void test_town_economy_words(void)
{
    /* Catches the 32000 cap, widened ADD.W, early death before revival, missing
     * completion cleanup, wrong cadence/index, and accidental timestamp writes. */
    const struct { int population, growth, result; } cases[]={
        {100,32000,32100},{1,32767,-32768},{1,-1,0},{1,-2,-1},{100,11,111},
        {0,11,11},{-1,11,10},{-32768,-1,32767},{-1,-32768,32767},
        {0,0,0},{-1,0,-1},{100,-32768,-32668}
    };
    const struct { int turn, economy; } turns[]={
        {7,0},{8,1},{9,0},{0xffff,0},{0,1},{0x8000,1},{0x8001,0}
    };
    for (int owner=0; owner<2; ++owner)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample)
    for (unsigned tick=0; tick<sizeof turns/sizeof turns[0]; ++tick)
    for (int occupant=0; occupant<=2; ++occupant)
    for (int direct=0; direct<=1; ++direct) {
        setup_town_economy(owner,cases[sample].population);
        game_turn=turns[tick].turn; map_who[650]=occupant;
        A4_WORD(0x2ff6)=cases[sample].growth;
        A4_WORD(0x2fe0)=sample&1 ? 0x5509 : 0xab09;
        uint8_t expected[sizeof prg_a4_area];
        int remaining=turns[tick].economy ? cases[sample].result : cases[sample].population;
        if (!direct && cases[sample].population<=0)
            memcpy(expected,prg_a4_area,sizeof expected); /* Resident-loop skip, not revival. */
        else town_expected_completion(expected,owner,remaining,turns[tick].economy,owner ? 23473 : 12362);
        if (!direct && cases[sample].population>0) expect_town_publish(expected,owner,0,1);
        if (direct) core_settle_tick(0); else { expect_pass_good_pop(expected); _move_peeps(); }
        if (direct || cases[sample].population>0)
            assert(peep_word(0,4)==(remaining>0 ? remaining : 0));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && !prg_quit);
    }
    /* Raw owner ADD.L, signed-word EXT.L: neither host bounds nor active minimum. */
    const struct { uint32_t before; int increment; uint32_t after; } mana[]={
        {1999999,17,2000016},{0xffffff06u,-1,0xffffff05u},
        {0x7fffffffu,1,0x80000000u},{0xffffffffu,1,0},
        {0,-32768,0xffff8000u},{0x80000000u,-32768,0x7fff8000u}
    };
    for (int owner=0; owner<2; ++owner)
    for (unsigned sample=0; sample<sizeof mana/sizeof mana[0]; ++sample) {
        setup_town_economy(owner,100);
        A4_LONG(0x9a02+owner*16)=mana[sample].before; A4_WORD(0x2fca)=mana[sample].increment;
        uint8_t expected[sizeof prg_a4_area];
        town_expected_completion(expected,owner,111,1,mana[sample].after);
        core_settle_tick(0);
        assert(A4_LONG(0x9a02+owner*16)==mana[sample].after);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        /* The shared helper's existing bounds remain unchanged for other users. */
        A4_LONG(0x9a02+owner*16)=1999999; core_add_mana(owner,17);
        assert(core_mana(owner)==1999999);
        A4_LONG(0x9a02+owner*16)=0xffffff06u; core_add_mana(owner,-1);
        assert(core_mana(owner)==-250);
    }
}

static void test_town_economy_evacuation(void)
{
    const int populations[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int trigger=0; trigger<3; ++trigger)
    for (int sample=0; sample<3; ++sample) for (int occupant=0; occupant<=2; ++occupant)
    for (int turn=7; turn<=9; ++turn) {
        setup_town_economy(owner,populations[sample]);
        game_turn=turn; map_who[650]=occupant;
        if (!trigger) map_blk[650]=1;
        else if (trigger==1) peeps[17]=0x80;
        else A4_WORD(0xae80)=1;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x2ff8,trigger ? 9 : 0);
        celebration_expected_word(expected,0x2fb2,0);
        expected[peeps-prg_a4_area]=2;
        celebration_expected_word(expected,0x9c26+10,0);
        celebration_expected_word(expected,0x9c26+12,0);
        town_expected_clear_footprint(expected,owner);
        core_settle_tick(0);
        assert(peep_word(0,4)==populations[sample] && peeps[0]==2);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_leader(owner)==1 && view_who==1 && map_who[650]==occupant);
        assert(!core_battles && !core_settlements && !core_result);
        /* No war/heading prepass in this case: next resident pass skips the raw slot. */
        if (!trigger) { expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected)); }
    }
}

static void test_town_economy_split(void)
{
    for (int owner=0; owner<2; ++owner) for (int full=0; full<=1; ++full) {
        setup_town_economy(owner,3100);
        if (full) {
            no_peeps=208; funny_done=1;   /* full-pool funny: test_funny_objects */
            for (int person=1; person<208; ++person) {
                peeps[person*22]=6; peeps[person*22+1]=1-owner;
                set_peep_word(person,4,77); set_peep_word(person,12,85);
            }
            A4_WORD(0x2ff6)=30000; /* 33100 wraps negative: no child can be allocated. */
        }
        uint8_t expected[sizeof prg_a4_area];
        town_expected_completion(expected,owner,full ? -32436 : 1536,1,owner ? 23473 : 12362);
        if (!full) {
            uint8_t *child=expected+(peeps-prg_a4_area)+22;
            child[0]=2; child[1]=owner; child[2]=3; child[3]=9;
            memset(child+10,0,8); child[20]=0;
            celebration_expected_word(expected,0x9c26+22+4,1575);
            celebration_expected_word(expected,0x9c26+22+8,650);
            celebration_expected_word(expected,0x9c26+22+18,650);
            celebration_expected_word(expected,0xae5e,2);
            celebration_expected_word(expected,0x99f6+owner*16,2);
            expected[peeps-prg_a4_area+2]=4; expected[map_who-prg_a4_area+650]=2;
            celebration_expected_word(expected,0xae68,2);
        }
        core_settle_tick(0);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result);
    }
}

/* Reused records retain only their own counter and navigation history. */
static void setup_town_split(int owner, int parent, int child, int count)
{
    setup_town_economy(owner,3100);
    uint8_t father[22]; memcpy(father,peeps,22);
    for (int person=0; person<208; ++person) {
        for (int byte=0; byte<22; ++byte) peeps[person*22+byte]=0x81+byte;
        set_peep_word(person,4,person<child ? 77 : -1);
    }
    memcpy(peeps+parent*22,father,22);
    set_peep_word(parent,12,42);
    no_peeps=count; map_who[650]=parent+1; map_who[500]=parent+1;
    core_leader(owner)=parent+1; core_leader(1-owner)=206; view_who=parent+1;
    /* Allocated A4 storage immediately beyond the 208-record pool is a guard,
     * not a 209th record. Keep it dead so an off-by-one search would select it. */
    memset(peeps+208*22,0xb7,22);
    /* A full pool also places a funny once (asm:3996-4006); that rule is
     * test_funny_objects', so these layouts start with it already done. */
    funny_done=child>=208;
    int16_t dead=-1; memcpy(peeps+208*22+4,&dead,sizeof dead);
}

static void test_town_split_view(void)
{
    /* These state pairs catch coupling the view transfer to leader transfer. */
    for (int owner=0; owner<2; ++owner) for (int leader=0; leader<=1; ++leader)
    for (int view=0; view<3; ++view) {
        setup_town_split(owner,2,0,4);
        core_leader(owner)=leader ? 3 : 206;
        view_who=view==1 ? 3 : view==2 ? 206 : 0;
        core_settle_tick(2);
        assert(view_who==(view==1 ? 1 : view==2 ? 206 : 0));
        assert(core_leader(owner)==(leader ? 1 : 206));
        assert(core_leader(1-owner)==206);
    }
}

static void test_town_split_records(void)
{
    /* Catches whole-record clearing/copying, unsigned-pop allocation, searching
     * only no_peeps, parent self-allocation, radius signedness and stale fields. */
    const struct { int parent, child, count, result_count; } layouts[]={
        {2,0,4,4},{0,2,4,4},{0,2,208,208},{0,4,1,5},
        {0,207,1,208},{207,1,208,208},{0,208,208,208}
    };
    const struct { int before, after; } radii[]={{0,1},{3,4},{4,4},{127,127},{128,128},{255,255}};
    const struct { int population, timer, history; } stale[]={
        {0,0x0180,1},{-1,0x8001,193},{-32768,0xffff,255}
    };
    for (int owner=0; owner<2; ++owner)
    for (unsigned layout=0; layout<sizeof layouts/sizeof layouts[0]; ++layout)
    for (unsigned radius=0; radius<sizeof radii/sizeof radii[0]; ++radius)
    for (unsigned poison=0; poison<sizeof stale/sizeof stale[0]; ++poison)
    for (int leader=0; leader<=1; ++leader) for (int view=0; view<3; ++view)
    for (int occupant=0; occupant<4; ++occupant) {
        int parent=layouts[layout].parent, child=layouts[layout].child, full=child==208;
        setup_town_split(owner,parent,child,layouts[layout].count);
        peeps[parent*22+2]=radii[radius].before;
        core_leader(owner)=leader ? parent+1 : 206;
        core_leader(1-owner)=poison==2 ? parent+1 : 206;
        view_who=view==1 ? parent+1 : view==2 ? 206 : 0;
        map_who[650]=occupant==0 ? 0 : occupant==1 ? parent+1 : 206;
        peeps[205*22]=8; peeps[205*22+1]=occupant==3 ? 1-owner : owner;
        set_peep_word(205,4,77); set_peep_word(205,8,650);
        if (!full) {
            set_peep_word(child,4,stale[poison].population);
            set_peep_word(child,6,stale[poison].timer);
            peeps[child*22+21]=stale[poison].history;
            peeps[child*22]=poison==0 ? 0 : poison==1 ? 0xff : 0x80;
            peeps[child*22+1]=poison==0 ? owner : poison==1 ? 1-owner : 255;
        }
        A4_WORD(0x2fe0)=poison==0 ? 0x5509 : 0xab09;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x2ff8,9); celebration_expected_word(expected,0x2fb2,0);
        celebration_expected_word(expected,0x9c26+parent*22+4,full ? 3111 : 1536);
        expected[peeps-prg_a4_area+parent*22+3]=9;
        town_expected_long(expected,0x9a02+owner*16,owner ? 23473 : 12362);
        if (!occupant) expected[map_who-prg_a4_area+650]=parent+1;
        expect_town_bookkeeping(expected,parent,42);
        if (!full) {
            uint8_t *record=expected+(peeps-prg_a4_area)+child*22;
            record[0]=2; record[1]=owner; record[2]=radii[radius].before; record[3]=9;
            celebration_expected_word(expected,0x9c26+child*22+4,1575);
            celebration_expected_word(expected,0x9c26+child*22+8,650);
            celebration_expected_word(expected,0x9c26+child*22+10,0);
            celebration_expected_word(expected,0x9c26+child*22+12,0);
            record[14]=record[15]=record[16]=record[17]=record[20]=0;
            celebration_expected_word(expected,0x9c26+child*22+18,650);
            celebration_expected_word(expected,0xae5e,layouts[layout].result_count);
            if (leader) celebration_expected_word(expected,0x99f6+owner*16,child+1);
            if (view==1) celebration_expected_word(expected,0xae68,child+1);
            expected[peeps-prg_a4_area+parent*22+2]=radii[radius].after;
            if (occupant<2) expected[map_who-prg_a4_area+650]=child+1;
        }
        core_settle_tick(parent);
        if (!full) {
            assert((uint16_t)peep_word(child,6)==stale[poison].timer);
            assert(peeps[child*22+21]==stale[poison].history);
        }
        if (memcmp(expected,prg_a4_area,sizeof expected)) {
            fprintf(stderr,"split owner=%d layout=%u radius=%u poison=%u leader=%d view=%d occupant=%d\n",
                    owner,layout,radius,poison,leader,view,occupant);
            for (size_t byte=0; byte<sizeof expected; ++byte)
                if (expected[byte]!=prg_a4_area[byte])
                    fprintf(stderr,"A4 %04zx expected=%02x actual=%02x\n",(byte-32768)&0xffff,expected[byte],prg_a4_area[byte]);
        }
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(no_peeps==layouts[layout].result_count);
        assert(!core_result && !core_battles && !core_settlements && !prg_quit);
    }
}

static void test_town_split_passes(void)
{
    /* Birth before/after the loop cursor must not move or lose population.
     * The following prepass must see cleared heading bytes and retain leader.
     * A blocked east step then distinguishes inherited NE history from zero. */
    const struct { int parent, child, count; } layouts[]={{1,0,2},{0,1,2},{0,1,1}};
    for (int owner=0; owner<2; ++owner) for (int layout=0; layout<3; ++layout)
    for (int history=0; history<2; ++history) {
        int parent=layouts[layout].parent, child=layouts[layout].child;
        setup_town_split(owner,parent,child,layouts[layout].count);
        core_leader(1-owner)=0; core_tendency(owner)=0; core_target(owner)=655;
        set_peep_word(child,6,0x8001); peeps[child*22+21]=history ? 193 : 63;
        /* Mountain is impassable but footprint maintenance leaves it alone.
         * Capacity is 275, so the town keeps 137 and releases 2963. */
        map_blk[651]=47;
        A4_WORD(0x2ff4)=11; A4_WORD(0x2fde)=0xab09;
        _move_peeps();
        int birth_frame=child>parent ? 1 : 0;
        assert(peeps[child*22]==2 && peep_word(child,4)==2963);
        assert(peep_word(child,12)==birth_frame && peep_word(child,8)==650);
        assert(peep_word(child,10)==0 && peep_word(child,18)==650);
        assert((uint16_t)peep_word(child,6)==0x8001 && peeps[child*22+21]==(history ? 193 : 63));
        assert(peeps[child*22+3]==9 && !peeps[child*22+20]);
        for (int byte=14; byte<18; ++byte) assert(!peeps[child*22+byte]);
        assert(core_leader(owner)==child+1 && view_who==child+1);
        for (int frame=birth_frame+1; frame<=7; ++frame) {
            ++game_turn; _move_peeps();
            assert(core_leader(owner)==child+1 && view_who==child+1);
            assert(peep_word(child,12)==frame && peep_word(child,8)==650);
            assert(peep_word(child,4)==2963 && (uint16_t)peep_word(child,6)==0x8001);
            assert(peeps[child*22+21]==(history ? 193 : 63));
        }
        ++game_turn; _move_peeps();
        assert(peeps[child*22]==2 && peep_word(child,12)==0 && peep_word(child,4)==2962);
        assert(peep_word(child,8)==(history ? 715 : 587));
        assert(peep_word(child,10)==(history ? 65 : -63));
        assert(peeps[child*22+21]==(history ? 191 : 63));
        assert((uint16_t)peep_word(child,6)==0x8001 && !peeps[child*22+20]);
        assert(core_leader(owner)==child+1 && view_who==child+1 && !core_leader(1-owner));
        /* _move_explorer asm:4590-4617: the child claims its departure
         * cell, so no parent turn can reclaim it; the destination waits for
         * the child's next departure. */
        assert(map_who[650]==child+1);
        assert(!map_who[history ? 715 : 587]);
        assert(!core_battles && !core_settlements && !core_result);
    }
}

static void test_town_split(void)
{
    test_town_split_records(); test_town_split_view(); test_town_split_passes();
}

static void test_town_economy_ticks(void)
{
    for (int owner=0; owner<2; ++owner) for (int gate=0; gate<6; ++gate) {
        setup_town_economy(owner,1); game_turn=7; A4_WORD(0x2ff6)=32767;
        no_peeps=2; peeps[22]=6; peeps[23]=1-owner;
        set_peep_word(1,4,77); set_peep_word(1,8,3250); set_peep_word(1,12,85);
        map_who[3250]=2;
        pause=gate==1; prg_quit=gate==2; core_result=gate>=3 ? gate-2 : 0;
        uint8_t expected[sizeof prg_a4_area];
        if (gate) memcpy(expected,prg_a4_area,sizeof expected);
        else {
            town_expected_completion(expected,owner,-32768,1,owner ? 23473 : 12362);
            expect_town_publish(expected,owner,0,1);
            celebration_expected_word(expected,0xae6a,8);
            celebration_expected_word(expected,0x9c26+22+12,86);
            /* asm:3459-3474: the town's pre-overflow 1 counts this pass. */
            town_expected_long(expected,0x99fe + owner*16,1);
            town_expected_long(expected,0x99fe + (1-owner)*16,77);
        }
        game_tick();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_result==(gate>=3 ? gate-2 : 0));
        assert(!core_battles && !core_settlements);
        if (!gate) {
            assert(!peeps[0] && !peep_word(0,4) && peep_word(1,12)==86);
            /* So asm:4323-4347 waits: turn 9 is an ordinary pass over the
             * dead town (pass resets, nothing counted), then ends on its 0. */
            memcpy(expected,prg_a4_area,sizeof expected);
            celebration_expected_word(expected,0xae6a,9);
            celebration_expected_word(expected,0x9c26+22+12,87);
            town_expected_long(expected,0x82dc+(owner ? 0 : 1)*46,0);
            town_expected_long(expected,0x82d8+(owner ? 0 : 1)*46,0);
            town_expected_long(expected,0x82e0+owner*46,0);
            expect_town_publish(expected,owner,0,0);
            town_expected_long(expected,0x99fe + owner*16,0);
            game_tick();
            if (memcmp(expected,prg_a4_area,sizeof expected))
                for (size_t byte=0; byte<sizeof expected; ++byte)
                    if (expected[byte]!=prg_a4_area[byte])
                        fprintf(stderr,"economy tick owner=%d A4 %04zx expected=%02x actual=%02x\n",
                                owner,(byte-32768)&0xffff,expected[byte],prg_a4_area[byte]);
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(core_result==(owner ? 1 : 2));
            memcpy(expected,prg_a4_area,sizeof expected); game_tick();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
    }
}

static void test_town_economy(void)
{
    test_town_economy_words(); test_town_economy_evacuation();
    test_town_economy_split(); test_town_economy_ticks();
}

/* Coordinate-only oracle: the caller supplies the expected refresh decision.
 * No capacity, frame, city-count or footprint helper from production is used. */
static void town_footprint_map(uint8_t *expected, int owner, int position,
                               int frame, int refresh, int dead)
{
    uint8_t *blocks=expected+(map_blk-prg_a4_area);
    uint8_t *buildings=expected+(map_bk2-prg_a4_area);
    for (int phase=0; phase<2; ++phase) {
        if ((!phase && !refresh) || (phase && !dead)) continue;
        int old_city=buildings[position]==42;
        for (int cell=0; cell<4096; ++cell) {
            int dx=abs(cell%64-position%64), dy=abs(cell/64-position/64);
            int inner=dx<=1 && dy<=1;
            int ordinary=inner || (dx==2 && (dy==0 || dy==2)) || (dy==2 && dx==0);
            int square=dx<=2 && dy<=2;
            if ((phase || (old_city && frame!=42)) && (old_city ? square : ordinary)) {
                if (old_city || inner) buildings[cell]=0;
                if (blocks[cell]==31+owner) blocks[cell]=15;
            }
            if (!phase && (frame==42 ? square : ordinary)) {
                if (inner) buildings[cell]=frame==42 ? (dx && dy ? 41 : dx ? 43 : dy ? 44 : 42) : 0;
                if (blocks[cell]==15) blocks[cell]=31+owner;
            }
        }
        if (phase) buildings[position]=0;
        else if (frame!=42 && blocks[position]==31+owner) buildings[position]=frame;
    }
}

static void setup_town_footprint(int owner, int position, int city, int flat)
{
    setup_town_economy(owner,100);
    memset(map_blk,1,4096); memset(map_bk2,55,4096); memset(map_who,0,4096);
    set_peep_word(0,8,position);
    for (int cell=0; cell<4096; ++cell) {
        int dx=abs(cell%64-position%64), dy=abs(cell/64-position/64);
        int inner=dx<=1 && dy<=1;
        int ordinary=inner || (dx==2 && (dy==0 || dy==2)) || (dy==2 && dx==0);
        if (dx<=2 && dy<=2) map_blk[cell]=31+owner;
        if (!city && position==650 && cell==780) map_blk[cell]=32-owner;
        if (city && inner) map_bk2[cell]=41;
        if (ordinary && flat && cell==position) map_blk[cell]=15;
        map_alt[cell]=cell%8;
    }
    map_bk2[position]=city ? 42 : 55;
}

static void test_town_footprint_predicate(void)
{
    /* Sentinel overlays expose unnecessary refreshes even with positive life
     * and no flat tiles. Edges exercise clipping without allowing row wrap. */
    const struct { int position, frame; } positions[]={
        {650,41},{0,37},{63,37},{4032,37},{4095,37},
        {32,39},{2048,39},{2111,39},{4064,39}
    };
    const struct { int city, flat, old_frame, frame, all_city, refresh; } cases[]={
        {0,0,41,41,0,0}, {0,0,-32768,41,0,1}, {0,1,41,41,0,1},
        {1,0,42,42,9,0}, {1,0,42,42,8,1}, {1,1,42,42,9,1},
        {1,0,-1,42,9,1}, {2,0,42,41,9,1}, {2,0,41,41,9,0}
    };
    unsigned checked=0;
    for (int owner=0; owner<2; ++owner)
    for (unsigned place=0; place<sizeof positions/sizeof positions[0]; ++place)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        if (place && cases[sample].city) continue;
        for (int turn=7; turn<=9; ++turn) for (int occupant=0; occupant<=2; ++occupant)
        for (int direct=0; direct<=1; ++direct) {
            int position=positions[place].position;
            int frame=place ? positions[place].frame : cases[sample].frame;
            int old_frame=cases[sample].old_frame==41 ? frame : cases[sample].old_frame;
            setup_town_footprint(owner,position,cases[sample].city!=0,cases[sample].flat);
            if (cases[sample].city==2) map_blk[780]=32-owner;
            if (cases[sample].all_city==8) map_bk2[585]=55;
            set_peep_word(0,12,old_frame); game_turn=turn; map_who[position]=occupant;
            for (int level=0; level<11; ++level) {
                A4_WORD(0x2fb6+level*2)=-17;
                A4_WORD(0x2fcc+level*2)=0xab09;
                A4_WORD(0x2fe2+level*2)=11;
            }
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            celebration_expected_word(expected,0x2ff8,cases[sample].all_city);
            celebration_expected_word(expected,0x2fb2,cases[sample].flat);
            celebration_expected_word(expected,0x9c26+12,frame);
            if (!occupant) expected[map_who-prg_a4_area+position]=1;
            expect_town_bookkeeping(expected,0,frame);
            /* asm:3766-3797 with the latch clear: a reframe, a_flat_block or
             * a zero +20 on a map_alt 0 cell reaches _make_level's exit. */
            if (!map_alt[position] && (frame!=old_frame || cases[sample].flat || !peeps[20]))
                expect_make_level_exit(expected,0);
            if (turn==8) {
                celebration_expected_word(expected,0x9c26+4,111);
                expected[peeps-prg_a4_area+3]=9;
                town_expected_long(expected,0x9a02+owner*16,owner ? 23439 : 12328);
            }
            town_footprint_map(expected,owner,position,frame,cases[sample].refresh,0);
            if (!direct) expect_town_publish(expected,owner,frame!=42,frame==42);
            if (direct) core_settle_tick(0); else { expect_pass_good_pop(expected); _move_peeps(); }
            assert(peep_word(0,12)==frame);
            assert(A4_WORD(0x2fb2)==cases[sample].flat && A4_WORD(0x2ff8)==cases[sample].all_city);
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(!core_result && !prg_quit && !core_battles && !core_settlements);
            ++checked;
        }
    }
    assert(checked==1188);
}

static void test_town_footprint_completion(void)
{
    /* Source framing/claim, economy and split retain the pre-footprint capacity;
     * growth death must still perform the footprint before signed cleanup. */
    const struct { int population, growth, result; } cases[]={
        {100,11,111},{1,32767,-32768},{0,11,11},{-1,11,10},
        {0,0,0},{-1,0,-1},{-32768,-1,32767}
    };
    for (int owner=0; owner<2; ++owner) for (int downgrade=0; downgrade<=1; ++downgrade)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample)
    for (int turn=7; turn<=9; ++turn) for (int occupant=0; occupant<=2; ++occupant) {
        setup_town_footprint(owner,650,1,0);
        if (downgrade) map_blk[780]=32-owner;
        else map_bk2[585]=55;
        set_peep_word(0,12,42); set_peep_word(0,4,cases[sample].population);
        int frame=downgrade ? 41 : 42;
        A4_WORD(0x2fe2+(frame-32)*2)=cases[sample].growth;
        game_turn=turn; map_who[650]=occupant;
        int population=turn==8 ? cases[sample].result : cases[sample].population;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x2ff8,downgrade ? 9 : 8);
        celebration_expected_word(expected,0x2fb2,0);
        celebration_expected_word(expected,0x9c26+12,frame);
        celebration_expected_word(expected,0x9c26+4,population>0 ? population : 0);
        if (!occupant) expected[map_who-prg_a4_area+650]=1;
        expect_town_bookkeeping(expected,0,frame);
        if (turn==8) {
            expected[peeps-prg_a4_area+3]=9;
            town_expected_long(expected,0x9a02+owner*16,(owner ? 23456u : 12345u)+(downgrade ? 109 : 17));
        }
        if (population<=0) {
            expected[peeps-prg_a4_area]=0;
            if (occupant<=1) expected[map_who-prg_a4_area+650]=0;
            celebration_expected_word(expected,0x99f6+owner*16,0);
            celebration_expected_word(expected,0x99f8+owner*16,650);
            celebration_expected_word(expected,owner ? 0xae76 : 0xae78,650);
            celebration_expected_word(expected,0xae68,0);
        }
        town_footprint_map(expected,owner,650,frame,1,population<=0);
        core_settle_tick(0);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result && !core_battles && !core_settlements);
    }
    for (int owner=0; owner<2; ++owner) for (int full=0; full<=1; ++full)
    for (int occupant=0; occupant<=2; ++occupant) for (int turn=7; turn<=9; ++turn) {
        setup_town_footprint(owner,650,1,0); map_blk[780]=32-owner;
        set_peep_word(0,12,42); set_peep_word(0,4,3100); game_turn=turn;
        A4_WORD(0x2ff4)=11; map_who[650]=occupant;
        no_peeps=full ? 208 : 2; funny_done=full;   /* full-pool funny: test_funny_objects */
        for (int person=1; person<no_peeps; ++person) {
            peeps[person*22]=0x80; peeps[person*22+1]=1-owner;
            set_peep_word(person,4,77); set_peep_word(person,8,650);
        }
        if (!full) { memset(peeps+2*22,0xa5,22); set_peep_word(2,4,-1); }
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x2ff8,9); celebration_expected_word(expected,0x2fb2,0);
        celebration_expected_word(expected,0x9c26+12,41);
        if (!occupant) expected[map_who-prg_a4_area+650]=1;
        expect_town_bookkeeping(expected,0,41);
        if (turn==8) {
            celebration_expected_word(expected,0x9c26+4,full ? 3111 : 156);
            expected[peeps-prg_a4_area+3]=9;
            town_expected_long(expected,0x9a02+owner*16,owner ? 23565 : 12454);
            if (!full) {
                uint8_t *child=expected+(peeps-prg_a4_area)+44;
                child[0]=2; child[1]=owner; child[2]=3; child[3]=9;
                memset(child+10,0,8); child[20]=0;
                celebration_expected_word(expected,0xae68,3);
                celebration_expected_word(expected,0x9c26+44+4,2955);
                celebration_expected_word(expected,0x9c26+44+8,650);
                celebration_expected_word(expected,0x9c26+44+18,650);
                celebration_expected_word(expected,0xae5e,3);
                celebration_expected_word(expected,0x99f6+owner*16,3);
                expected[peeps-prg_a4_area+2]=4;
                if (occupant<=1) expected[map_who-prg_a4_area+650]=3;
            }
        }
        town_footprint_map(expected,owner,650,41,1,0);
        core_settle_tick(0);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result && !core_battles && !core_settlements);
    }
}

static void test_town_footprint_pass_and_clock(void)
{
    /* The next resident's life scan observes the footprint from this pass,
     * not a queued end-of-pass update; its scratch then replaces the first's. */
    for (int owner=0; owner<2; ++owner) {
        setup_town_footprint(owner,650,0,0); game_turn=7;
        set_peep_word(0,12,-1); no_peeps=2;
        memcpy(peeps+22,peeps,22); set_peep_word(1,8,651);
        core_leader(1-owner)=0; map_who[650]=1; map_who[651]=2;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x9c26+12,41);
        celebration_expected_word(expected,0x2fb2,0); celebration_expected_word(expected,0x2ff8,0);
        town_footprint_map(expected,owner,650,41,1,0);
        expected[peeps-prg_a4_area+22]=2;
        celebration_expected_word(expected,0x9c26+22+10,0);
        celebration_expected_word(expected,0x9c26+22+12,0);
        town_footprint_map(expected,owner,651,0,0,1);
        expect_town_bookkeeping(expected,0,41); /* 651 evacuates: neither */
        expect_town_publish(expected,owner,1,0); /* map_alt 2: no _make_level */
        expect_pass_good_pop(expected); _move_peeps();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(peeps[0]==1 && peeps[22]==2 && seed==123);
    }
    for (int owner=0; owner<2; ++owner) for (int gate=0; gate<6; ++gate)
    for (int fatal=0; fatal<=1; ++fatal) {
        setup_town_footprint(owner,650,1,0); map_blk[780]=32-owner;
        set_peep_word(0,12,42); set_peep_word(0,4,1);
        A4_WORD(0x2ff4)=fatal ? 32767 : 11; game_turn=6;
        no_peeps=2; peeps[22]=6; peeps[23]=1-owner;
        set_peep_word(1,4,77); set_peep_word(1,8,3250); set_peep_word(1,12,85);
        map_who[650]=1; map_who[3250]=2;
        pause=gate==1; prg_quit=gate==2; core_result=gate>=3 ? gate-2 : 0;
        for (int turn=7; turn<=9; ++turn) {
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            if (!gate && !(fatal && turn==9)) {
                celebration_expected_word(expected,0xae6a,turn);
                celebration_expected_word(expected,0x2fb2,0);
                celebration_expected_word(expected,0x2ff8,turn==7 ? 9 : 0);
                celebration_expected_word(expected,0x9c26+12,41);
                celebration_expected_word(expected,0x9c26+22+12,turn==9 ? 0 : turn+79);
                /* Each pass resets then recounts this one non-city town. */
                celebration_expected_word(expected,0x99fc+owner*16,1);
                expect_stats_pointer(expected,0);
                town_expected_long(expected,0x82d8+(owner ? 0 : 1)*46,1); /* asm:3684-3705 */
                /* asm:4349-4370; age turn-1234 exceeds $4e1f, so no +0x2a,
                 * and map_alt 2 keeps _make_level off before turn 251. */
                expect_town_publish(expected,owner,1,0);
                if (turn==9) expected[peeps-prg_a4_area+22]=2;
                /* asm:3459-3474: pre-move words (turn 8 reads the 1 before
                 * growth or the fatal overflow). */
                town_expected_long(expected,0x99fe + owner*16,turn<9 ? 1 : 12);
                town_expected_long(expected,0x99fe + (1-owner)*16,77);
                if (turn==8) {
                    celebration_expected_word(expected,0x9c26+4,fatal ? 0 : 12);
                    expected[peeps-prg_a4_area+3]=9;
                    town_expected_long(expected,0x9a02+owner*16,owner ? 23565 : 12454);
                    if (fatal) {
                        expected[peeps-prg_a4_area]=0; expected[map_who-prg_a4_area+650]=0;
                        celebration_expected_word(expected,0x99f6+owner*16,0);
                        celebration_expected_word(expected,0x99f8+owner*16,650);
                        celebration_expected_word(expected,owner ? 0xae76 : 0xae78,650);
                        celebration_expected_word(expected,0xae68,0);
                    }
                }
                town_footprint_map(expected,owner,650,41,turn==7,fatal && turn==8);
            } else if (!gate) {
                /* The fatal turn 8 counted the town's 1, so asm:4323-4347 did
                 * not end the game: turn 9 is an ordinary pass over the dead
                 * town (pass resets, no town counted), then ends on its 0. */
                celebration_expected_word(expected,0xae6a,9);
                celebration_expected_word(expected,0x9c26+22+12,0);
                expected[peeps-prg_a4_area+22]=2;
                celebration_expected_word(expected,0x99fc+owner*16,0);
                town_expected_long(expected,0x82dc+(owner ? 0 : 1)*46,0);
                town_expected_long(expected,0x82d8+(owner ? 0 : 1)*46,0);
                expect_town_publish(expected,owner,0,0);
                town_expected_long(expected,0x99fe + owner*16,0);
                town_expected_long(expected,0x99fe + (1-owner)*16,77);
            }
            game_tick();
            if (!gate && fatal) assert(core_result==(turn==9 ? (owner ? 1 : 2) : 0));
            if (memcmp(expected,prg_a4_area,sizeof expected))
                for (size_t byte=0; byte<sizeof expected; ++byte)
                    if (expected[byte]!=prg_a4_area[byte])
                        fprintf(stderr,"footprint owner=%d gate=%d fatal=%d turn=%d A4 %04zx expected=%02x actual=%02x\n",
                                owner,gate,fatal,turn,(byte-32768)&0xffff,expected[byte],prg_a4_area[byte]);
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(!core_battles && !core_settlements);
        }
    }
}

static void test_town_footprint(void)
{
    test_town_footprint_predicate(); test_town_footprint_completion();
    test_town_footprint_pass_and_clock();
}

/* Literal outcomes catch missing/early overrides, Boolean controller tests,
 * unsigned population comparisons, cheat-owner truncation and lost latch order. */
static void town_capacity_expected(uint8_t *expected, int owner, int mother,
                                   int child_population, int latch, int economy,
                                   uint32_t mana)
{
    town_expected_completion(expected,owner,mother,economy,mana);
    celebration_expected_word(expected,0x82be + owner*46,latch);
    if (!child_population) return;
    uint8_t *child=expected+(peeps-prg_a4_area)+22;
    child[0]=2; child[1]=owner; child[2]=3; child[3]=9;
    memset(child+10,0,8); child[20]=0;
    celebration_expected_word(expected,0x9c26+22+4,child_population);
    celebration_expected_word(expected,0x9c26+22+8,650);
    celebration_expected_word(expected,0x9c26+22+18,650);
    celebration_expected_word(expected,0xae5e,2);
    celebration_expected_word(expected,0x99f6+owner*16,2);
    celebration_expected_word(expected,0xae68,2);
    expected[peeps-prg_a4_area+2]=4;
    expected[map_who-prg_a4_area+650]=2;
}

static void setup_town_capacity(int owner, int population)
{
    setup_town_economy(owner,population);
    A4_WORD(0x82bc+owner*46)=1;
    /* Permissions, funds and no-build must not gate a local capacity change. */
    A4_WORD(0x82c4)=A4_WORD(0x82c4+46)=0;
    game_mode=GAME_MODE_NO_BUILD;
    for (int side=0; side<2; ++side) {
        A4_DISP(0x82b6+side*46)[0]=1;
        A4_DISP(0x82b6+side*46)[1]=20+side;
        A4_DISP(0x82b6+side*46)[2]=30+side;
    }
}

static void test_town_capacity_words(void)
{
    const struct { int controller,latch,population,mother,child,result_latch; } controls[]={
        {1,0,306,163,154,1},{1,0,305,316,0,0},
        {0,0,306,317,0,0},{2,0,306,317,0,0},{-1,0,306,317,0,0},
        {1,1,306,317,0,1},{1,-1,306,317,0,-1},
        {0,1,305,316,0,1},{2,-1,305,316,0,-1},{-1,0,305,316,0,0},
        {0,0,305,316,0,0},{0,-1,305,316,0,-1},
        {0,1,306,317,0,1},{0,-1,306,317,0,-1},
        {1,1,305,316,0,1},{1,-1,305,316,0,-1},
        {2,0,305,316,0,0},{2,1,305,316,0,1},
        {2,1,306,317,0,1},{2,-1,306,317,0,-1},
        {-1,1,305,316,0,1},{-1,-1,305,316,0,-1},
        {-1,1,306,317,0,1},{-1,-1,306,317,0,-1}
    };
    const struct { int turn,eligible; } turns[]={
        {0,1},{7,0},{8,1},{9,0},{0x7fff,0},{0x8000,1},
        {0x8001,0},{0xfff8,1},{0xffff,0}
    };
    unsigned checked=0;
    for (int owner=0; owner<2; ++owner)
    for (unsigned sample=0; sample<sizeof controls/sizeof controls[0]; ++sample)
    for (unsigned tick=0; tick<sizeof turns/sizeof turns[0]; ++tick) {
        setup_town_capacity(owner,controls[sample].population);
        A4_WORD(0x82bc+owner*46)=controls[sample].controller;
        A4_WORD(0x82be + owner*46)=controls[sample].latch;
        game_turn=turns[tick].turn;
        uint8_t expected[sizeof prg_a4_area];
        town_capacity_expected(expected,owner,turns[tick].eligible ? controls[sample].mother : controls[sample].population,
                               turns[tick].eligible ? controls[sample].child : 0,
                               turns[tick].eligible ? controls[sample].result_latch : controls[sample].latch,
                               turns[tick].eligible,owner ? 23473 : 12362);
        core_settle_tick(0);
        assert(peep_word(0,4)==(turns[tick].eligible ? controls[sample].mother : controls[sample].population));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        ++checked;
    }
    const struct { int cheat,child0,child1; } cheats[]={
        {0,0,0},{1,26,0},{2,0,26},{-1,0,0},{32767,0,0},{-32768,0,0}
    };
    for (int owner=0; owner<2; ++owner) for (int controller=0; controller<=1; ++controller)
    for (unsigned sample=0; sample<sizeof cheats/sizeof cheats[0]; ++sample)
    for (int population=50; population<=51; ++population)
    for (unsigned tick=0; tick<sizeof turns/sizeof turns[0]; ++tick) {
        setup_town_capacity(owner,population); A4_WORD(0x82bc+owner*46)=controller;
        A4_WORD(0x2fb4)=cheats[sample].cheat; game_turn=turns[tick].turn;
        int child=population==51 && turns[tick].eligible ? (owner ? cheats[sample].child1 : cheats[sample].child0) : 0;
        uint8_t expected[sizeof prg_a4_area];
        town_capacity_expected(expected,owner,child ? 36 : population+(turns[tick].eligible ? 11 : 0),
                               child,0,turns[tick].eligible,owner ? 23473 : 12362);
        core_settle_tick(0);
        assert(!memcmp(expected,prg_a4_area,sizeof expected)); ++checked;
    }
    const struct { int population,growth,result; } signed_cases[]={
        {0,11,11},{-1,11,10},{-32768,-1,32767}
    };
    for (int owner=0; owner<2; ++owner) for (int cheat=0; cheat<=1; ++cheat)
    for (unsigned sample=0; sample<sizeof signed_cases/sizeof signed_cases[0]; ++sample)
    for (int direct=0; direct<=1; ++direct) {
        setup_town_capacity(owner,signed_cases[sample].population);
        A4_WORD(0x2fb4)=cheat ? owner+1 : 0; A4_WORD(0x2ff6)=signed_cases[sample].growth;
        /* Do not let the leader prepass obscure the resident initial skip. */
        core_leader(0)=core_leader(1)=view_who=0;
        uint8_t expected[sizeof prg_a4_area];
        if (direct) town_capacity_expected(expected,owner,signed_cases[sample].result,0,0,1,owner ? 23473 : 12362);
        else memcpy(expected,prg_a4_area,sizeof expected);
        if (direct) core_settle_tick(0); else { expect_pass_good_pop(expected); _move_peeps(); }
        assert(!memcmp(expected,prg_a4_area,sizeof expected)); ++checked;
    }
    printf("town capacity literal A4 cases: %u\n",checked);
}

static void test_town_capacity_order_and_economy(void)
{
    const struct { uint32_t before; int increment; uint32_t after; } mana[]={
        {0,17,17},{0,-1,0xffffffffu},{0xffffffffu,1,0},
        {0x7fffffffu,1,0x80000000u},{0x80000000u,-32768,0x7fff8000u}
    };
    for (int owner=0; owner<2; ++owner) for (int cheat=0; cheat<=1; ++cheat)
    for (unsigned sample=0; sample<sizeof mana/sizeof mana[0]; ++sample) {
        setup_town_capacity(owner,3100); A4_WORD(0x2fb4)=cheat ? owner+1 : 0;
        A4_LONG(0x9a02+owner*16)=mana[sample].before; A4_WORD(0x2fca)=mana[sample].increment;
        uint8_t expected[sizeof prg_a4_area];
        town_capacity_expected(expected,owner,cheat ? 36 : 163,cheat ? 3075 : 2948,1,1,mana[sample].after);
        core_settle_tick(0);
        assert(peep_word(0,12)==42 && peeps[3]==9 && peeps[25]==9);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    /* Growth crossing 305 is not a second predicate evaluation this turn. */
    setup_town_capacity(0,305); core_settle_tick(0);
    assert(peep_word(0,4)==316 && !A4_WORD(0x82be) && no_peeps==1);
    game_turn=9; core_settle_tick(0);
    assert(peep_word(0,4)==316 && !A4_WORD(0x82be) && no_peeps==1);
    game_turn=16; core_settle_tick(0);
    assert(peep_word(0,4)==163 && peep_word(1,4)==164 && A4_WORD(0x82be)==1);
    /* A stored city can downgrade: predicate sees the newly computed frame. */
    for (int owner=0; owner<2; ++owner) for (int cheat=0; cheat<=1; ++cheat) {
        setup_town_capacity(owner,306); set_peep_word(0,12,42);
        map_blk[780]=32-owner; A4_WORD(0x2fb4)=cheat ? owner+1 : 0;
        A4_WORD(0x2ff4)=11; A4_WORD(0x2fde)=0xab09;
        uint8_t expected[sizeof prg_a4_area];
        town_capacity_expected(expected,owner,cheat ? 36 : 156,cheat ? 281 : 161,0,1,owner ? 23565 : 12454);
        celebration_expected_word(expected,0x9c26+12,41);
        expect_town_bookkeeping(expected,0,41); /* non-city frame counts */
        expect_make_level_exit(expected,0); /* 42 -> 41 reframe, latch clear */
        town_footprint_map(expected,owner,650,41,1,0);
        core_settle_tick(0);
        assert(peep_word(0,12)==41 && !A4_WORD(0x82be + owner*46));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    /* ADD.W death after a successful override split must preserve its child. */
    for (int owner=0; owner<2; ++owner) for (int cheat=0; cheat<=1; ++cheat) {
        setup_town_capacity(owner,306); A4_WORD(0x2fb4)=cheat ? owner+1 : 0;
        A4_WORD(0x2ff6)=32767;
        uint8_t expected[sizeof prg_a4_area];
        town_capacity_expected(expected,owner,cheat ? -32744 : -32617,cheat ? 281 : 154,1,1,owner ? 23473 : 12362);
        /* Split transfers references BEFORE mother cleanup, so targets survive. */
        celebration_expected_word(expected,0x99f8+owner*16,core_target(owner));
        celebration_expected_word(expected,owner ? 0xae76 : 0xae78,A4_WORD(owner ? 0xae76 : 0xae78));
        core_settle_tick(0);
        assert(!peeps[0] && !peep_word(0,4));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_town_capacity_records(void)
{
    const struct { int parent,child,count,result_count; } layouts[]={
        {2,0,4,4},{0,2,4,4},{0,207,1,208},{207,1,208,208},{0,208,208,208}
    };
    for (int owner=0; owner<2; ++owner) for (int cheat=0; cheat<=1; ++cheat)
    for (unsigned layout=0; layout<sizeof layouts/sizeof layouts[0]; ++layout)
    for (int occupant=0; occupant<3; ++occupant) for (int leader=0; leader<=1; ++leader)
    for (int view=0; view<=1; ++view) {
        int parent=layouts[layout].parent, child=layouts[layout].child, full=child==208;
        setup_town_split(owner,parent,child,layouts[layout].count);
        A4_WORD(0x82bc+owner*46)=1; A4_WORD(0x2fb4)=cheat ? owner+1 : 0;
        core_leader(owner)=leader ? parent+1 : 206; view_who=view ? parent+1 : 206;
        map_who[650]=occupant==0 ? 0 : occupant==1 ? parent+1 : 206;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x2ff8,9); celebration_expected_word(expected,0x2fb2,0);
        celebration_expected_word(expected,0x82be + owner*46,1);
        celebration_expected_word(expected,0x9c26+parent*22+4,full ? 3111 : cheat ? 36 : 163);
        expected[peeps-prg_a4_area+parent*22+3]=9;
        town_expected_long(expected,0x9a02+owner*16,owner ? 23473 : 12362);
        if (!occupant) expected[map_who-prg_a4_area+650]=parent+1;
        expect_town_bookkeeping(expected,parent,42);
        if (!full) {
            uint8_t *record=expected+(peeps-prg_a4_area)+child*22;
            record[0]=2; record[1]=owner; record[2]=3; record[3]=9;
            celebration_expected_word(expected,0x9c26+child*22+4,cheat ? 3075 : 2948);
            celebration_expected_word(expected,0x9c26+child*22+8,650);
            memset(record+10,0,8); record[20]=0;
            celebration_expected_word(expected,0x9c26+child*22+18,650);
            celebration_expected_word(expected,0xae5e,layouts[layout].result_count);
            if (leader) celebration_expected_word(expected,0x99f6+owner*16,child+1);
            if (view) celebration_expected_word(expected,0xae68,child+1);
            expected[peeps-prg_a4_area+parent*22+2]=4;
            if (occupant<2) expected[map_who-prg_a4_area+650]=child+1;
        }
        core_settle_tick(parent);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(no_peeps==layouts[layout].result_count);
    }
    for (int owner=0; owner<2; ++owner) for (int cheat=0; cheat<=1; ++cheat) {
        setup_town_split(owner,0,208,208);
        A4_WORD(0x82bc+owner*46)=1; A4_WORD(0x2fb4)=cheat ? owner+1 : 0;
        A4_WORD(0x2ff6)=30000;
        uint8_t expected[sizeof prg_a4_area];
        town_capacity_expected(expected,owner,-32436,0,1,1,owner ? 23473 : 12362);
        core_settle_tick(0);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(no_peeps==208 && A4_WORD(0x82be + owner*46)==1);
    }
}

static void test_town_capacity_evacuation(void)
{
    for (int owner=0; owner<2; ++owner) for (int trigger=0; trigger<10; ++trigger) {
        setup_town_capacity(owner,306); A4_WORD(0x2fb4)=owner+1;
        if (!trigger) map_blk[650]=1;
        else if (trigger==1) A4_WORD(0xae80)=1;
        else peeps[14+(trigger-2)/2]=(trigger&1) ? 0x80 : 1;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x2ff8,trigger ? 9 : 0);
        celebration_expected_word(expected,0x2fb2,0);
        expected[peeps-prg_a4_area]=2;
        celebration_expected_word(expected,0x9c26+10,0);
        celebration_expected_word(expected,0x9c26+12,0);
        town_expected_clear_footprint(expected,owner);
        core_settle_tick(0);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_town_capacity_mailbox(void)
{
    /* Residents share only their owner's latch; clearing occurs after ALL of
     * them, even when the mailbox is empty. Pending terrain is a separate act. */
    const struct { int turn,divisor,latch; } clocks[]={
        {8,8,0},{8,3,1},{8,0,1},{0x8000,0x8000,0},{0,3,0}
    };
    for (int owner=0; owner<2; ++owner) for (unsigned clock=0; clock<sizeof clocks/sizeof clocks[0]; ++clock)
    for (int pending=0; pending<=1; ++pending) for (int terminal=0; terminal<=1; ++terminal) {
        setup_town_capacity(owner,306); no_peeps=3;
        for (int person=1; person<3; ++person) {
            memcpy(peeps+person*22,peeps,22);
            peeps[person*22+1]=person==1 ? owner : 1-owner;
            set_peep_word(person,8,person==1 ? 1300 : 2600);
            set_peep_word(person,12,42); core_set_town(person,0);
            map_who[person==1 ? 1300 : 2600]=person+1;
        }
        if (terminal) set_peep_word(2,4,0);
        core_leader(0)=core_leader(1)=view_who=0;
        game_mode=0; A4_LONG(0x848a)=10;
        for (int row=0; row<=64; ++row) for (int column=0; column<=64; ++column) core_set_vertex(column,row,1);
        for (int side=0; side<2; ++side) {
            A4_WORD(0x82bc+side*46)=1; A4_WORD(0x82c6+side*46)=clocks[clock].divisor;
            memset(A4_DISP(0x82b6+side*46),0,3);
        }
        if (pending) { A4_DISP(0x82b6+owner*46)[0]=1; A4_DISP(0x82b7+owner*46)[0]=50; A4_DISP(0x82b8+owner*46)[0]=50; }
        game_turn=(uint16_t)(clocks[clock].turn-1u);
        game_tick();
        assert(peep_word(0,4)==163 && peep_word(1,4)==317);
        assert(peep_word(terminal ? 2 : 3,4)==154);
        if (!terminal) assert(peep_word(2,4)==163 && peep_word(4,4)==154);
        assert(A4_WORD(0x82be + owner*46)==(terminal ? 1 : clocks[clock].latch));
        assert(A4_WORD(0x82be + (1-owner)*46)==(terminal ? 0 : clocks[clock].latch));
        int raised=pending && !terminal && clocks[clock].divisor!=0;
        assert(core_vertex(50,50)==1+raised);
        assert(A4_LONG(0x9a02+owner*16)==(owner ? 23490u : 12379u)-14u*raised);
        for (int side=0; side<2; ++side) for (int byte=0; byte<3; ++byte) assert(!A4_DISP(0x82b6+side*46)[byte]);
        assert(core_result==(terminal ? (owner ? 2 : 1) : 0));
    }
}

static void test_town_capacity(void)
{
    test_town_capacity_words(); test_town_capacity_order_and_economy();
    test_town_capacity_records(); test_town_capacity_evacuation(); test_town_capacity_mailbox();
}

static void test_near_town_sculpt(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(game_reset(0));
    game_mode=GAME_MODE_BUILD_NEAR_TOWNS;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    /* asm:7430-7438: above-water near-town mode rejects without permission. */
    assert(!core_sculpt(0,8,8,1));
    assert(a4_same(before));
    for (int tick=0; tick<16; ++tick) game_tick();
    int position=peep_word(0,8);
    assert(peeps[0]==1);
    int32_t mana_before=core_mana(0);
    assert(core_sculpt(0,position%64,position/64,1));
    assert(core_vertex(position%64,position/64)==2 && core_mana(0)==mana_before-14);
    memcpy(before,prg_a4_area,sizeof before);
    assert(!core_sculpt(0,60,60,1));
    assert(a4_same(before));
    _closeall(); SDL_Quit();
}

static void test_split_leader_and_occupancy(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for (int owner=0; owner<2; ++owner) for (int occupant=0; occupant<=2; ++occupant) {
        assert(game_reset(0));
        memset(peeps,0,208*22); memset(map_who,0,4096); no_peeps=2;
        int position=50*64+50;
        peeps[0]=1; peeps[1]=owner; peeps[2]=1;
        set_peep_word(0,4,3100); set_peep_word(0,8,position);
        peeps[22]=8; peeps[23]=1-owner;
        set_peep_word(1,4,45); set_peep_word(1,8,position);
        map_who[position]=occupant;
        core_leader(owner)=1; core_leader(1-owner)=2;
        game_turn=8;
        /* Flat capacity is 3050; reset's owner-1 controller instead uses 305.
         * Both transfer only that owner's leader (asm:3818-3929). */
        core_settle_tick(0);
        assert(no_peeps==3 && peep_word(2,4)==(owner ? 2948 : 1575) && peeps[44]==2);
        assert(A4_WORD(0x82be + owner*46)==1);
        assert(core_leader(owner)==3 && core_leader(1-owner)==2);
        assert(map_who[position]==(occupant==2 ? 2 : 3)); /* asm:3964-3983 */
        game_turn=9;
        core_settle_tick(0);
        assert(map_who[position]==(occupant==2 ? 2 : 3));
    }
    _closeall(); SDL_Quit();
}

static void test_sculpt_navigation_locality(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(game_reset(0));
    for (int tick=0; tick<500; ++tick) game_tick();
    uint8_t history[8192], blocks[4096], buildings[4096];
    memcpy(history,map_steps,sizeof history);
    memcpy(blocks,map_blk,sizeof blocks);
    memcpy(buildings,map_bk2,sizeof buildings);
    int visited = 0;
    for (int cell=0; cell<4096; ++cell) visited += A4_WORD(0xff88+cell*2)!=0;
    assert(visited>0);
    assert(core_sculpt(0,60,60,1));
    for (int cell=0; cell<4096; ++cell) {
        int column=cell%64, row=cell/64;
        if (column>=59 && column<=60 && row>=59 && row<=60) {
            assert(!A4_WORD(0xff88+cell*2));
        } else {
            assert(memcmp(history+cell*2,map_steps+cell*2,2)==0);
            assert(map_blk[cell]==blocks[cell] && map_bk2[cell]==buildings[cell]);
        }
    }
    /* The four adjacent cells really are cleared; remote history survives. */
    for (int row=59; row<=60; ++row) for (int column=59; column<=60; ++column)
        A4_WORD(0xff88+(row*64+column)*2)=9;
    assert(core_sculpt(0,60,60,-1));
    for (int row=59; row<=60; ++row) for (int column=59; column<=60; ++column)
        assert(!A4_WORD(0xff88+(row*64+column)*2));
    _closeall(); SDL_Quit();
}

static void test_settlement_occupancy(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(game_reset(0));
    int empty_last_tick[208] = {0}, towns = 0;
    /* A departing split walker can leave a tile empty until its town's next
     * turn. asm:3667-3677 restores it then, not indefinitely later. */
    for (int tick=0; tick<500; ++tick) {
        game_tick();
        for (int person=0; person<no_peeps; ++person) {
            int settled = peep_word(person,4)>0 && peeps[person*22]==1;
            int empty = settled && !map_who[peep_word(person,8)];
            assert(!empty || !empty_last_tick[person]);
            empty_last_tick[person] = empty;
            towns += settled;
        }
    }
    assert(towns>0);
    _closeall(); SDL_Quit();
}

/* One scripted match: rescue clicks lower land under every enemy walker each
 * `interval` ticks from tick `start`. Real-data outcomes are exact per script. */
typedef struct { int result; uint16_t turns; int battles, towns, own, enemy; } MatchOutcome;
static void assert_match_outcome(const MatchOutcome *expected)
{
    if (!expected || !getenv("POPULOUS_TEST_REAL_DATA")) return;
    assert(core_result==expected->result && (uint16_t)game_turn==expected->turns);
    assert(core_battles==expected->battles && core_settlements==expected->towns);
    assert(core_population(0)==expected->own && core_population(1)==expected->enemy);
}

static void test_match_events_cadence(int interval, int start, const MatchOutcome *expected)
{
    assert(interval==1 || interval==8);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(game_reset(0));
    SDL_Event key = {.type=SDL_KEYDOWN};
    key.key.keysym.sym = SDLK_d; /* Explicit legacy diagnostic view for this regression. */
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    key.key.keysym.sym = SDLK_3;
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    assert(core_tendency(0)==3);
    SDL_SetWindowSize(prg_screen->window,640,512);
    for (int tick=0; tick<12000 && !core_result; ++tick) {
        game_tick();
        if (tick >= start && tick%interval == 0) {
            for (int person=0; person<no_peeps; ++person) {
                if (!peep_owner(person) || peep_word(person,4)<=0) continue;
                int position=peep_word(person,8);
                for (int dy=0; dy<2; ++dy) for (int dx=0; dx<2; ++dx) {
                    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN};
                    click.button.button=SDL_BUTTON_RIGHT;
                    click.button.x=((position%64+dx)*4+1)*2;
                    click.button.y=((position/64+dy)*4+1)*2;
                    assert(SDL_PushEvent(&click)==1);
                }
            }
            ___keyboard();
        }
    }
    fprintf(stderr,"match result=%d turns=%u battles=%d towns=%d pop=%d/%d\n",core_result,(uint16_t)game_turn,core_battles,core_settlements,core_population(0),core_population(1));
    assert_match_outcome(expected);
    if (expected && expected->result!=1) { _closeall(); SDL_Quit(); return; }
    assert(core_battles>0 && core_settlements>0 && core_result);
    if (getenv("POPULOUS_TEST_REAL_DATA")) assert(core_result==1);
    uint8_t final_state[sizeof prg_a4_area]; memcpy(final_state,prg_a4_area,sizeof final_state);
    game_tick(); game_render();
    assert(memcmp(final_state,prg_a4_area,sizeof final_state)==0);
    key.key.keysym.sym=SDLK_r;
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    assert(!core_result && !game_turn && core_population(0)==540);
    key.key.keysym.sym=SDLK_d; assert(SDL_PushEvent(&key)==1); ___keyboard();
    /* Defeat is also reached via legal input, not an outcome flag or population patch. */
    SDL_SetWindowSize(prg_screen->window,640,512);
    for (int tick=0;tick<512;++tick) game_tick();
    for (int tick=0;tick<2000 && !core_result;++tick) {
        if (tick%8==0) {
            for (int person=0;person<no_peeps;++person) {
                if (peep_owner(person) || peep_word(person,4)<=0) continue;
                int position=peep_word(person,8);
                for (int dy=0;dy<2;++dy) for (int dx=0;dx<2;++dx) {
                    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN}; click.button.button=SDL_BUTTON_RIGHT;
                    click.button.x=((position%64+dx)*4+1)*2; click.button.y=((position/64+dy)*4+1)*2;
                    assert(SDL_PushEvent(&click)==1);
                }
            }
            ___keyboard();
        }
        game_tick();
    }
    fprintf(stderr,"defeat result=%d turns=%u battles=%d towns=%d pop=%d/%d\n",core_result,(uint16_t)game_turn,core_battles,core_settlements,core_population(0),core_population(1));
    assert(core_result==2 && core_population(0)==0 && core_population(1)>0);
    /* Measured deterministic defeat phase with the computer-player AI. */
    if (getenv("POPULOUS_TEST_REAL_DATA"))
        assert((uint16_t)game_turn==831 && core_battles==0 && core_settlements==43 && core_population(1)==6323);
    key.key.keysym.sym=SDLK_ESCAPE;
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    assert(prg_quit);
    _closeall(); SDL_Quit();
}

/* Winning script: from tick 3000 every enemy walker's footprint is lowered
 * each tick, drowning red before its walkers besiege blue's last towns. */
static const MatchOutcome events_victory = {1,4697,32,78,1434,0};
static void test_match_events(void)
{
    test_match_events_cadence(1,3000,&events_victory);
}

/* Synthetic rule files exercise mechanics, never pretend to be commercial LAND. */
static char test_data_directory[] = "/tmp/populous-game-test-XXXXXX";
static void fixture_pixels(const char *name,size_t size,int keep)
{
    char path[512]; snprintf(path,sizeof path,"%s/pixels/%s",test_data_directory,name);
    FILE *file=fopen(path,"wb"); assert(file);
    for (size_t pixel=0;pixel<size;++pixel) assert(fputc(keep ? 1 : (pixel%15)+1,file)!=EOF);
    assert(!fclose(file));
}
/* Exactly the loader minimum, with frame-identifying pixels and both mask modes. */
static void fixture_sprite_pixels(const char *name, int keep)
{
    char path[512]; snprintf(path,sizeof path,"%s/pixels/%s",test_data_directory,name);
    FILE *file=fopen(path,"wb"); assert(file);
    for (int frame=0;frame<143;++frame) for (int pixel=0;pixel<256;++pixel) {
        int value=keep ? pixel%3==0 : (pixel%3==0 ? 0 :
            pixel%3==1 ? frame%16 : frame/16);
        assert(fputc(value,file)!=EOF);
    }
    assert(!fclose(file));
}
/* The HUD draws in the original font: copy raw/font.dat (_font_data). */
static void copy_real_font(void)
{
    char path[4160]; snprintf(path,sizeof path,"%s/raw/font.dat",POPULOUS_DEFAULT_ASSET_DIR);
    FILE *in=fopen(path,"rb"); assert(in);
    uint8_t bytes[8192]; size_t count=fread(bytes,1,sizeof bytes,in); fclose(in); assert(count>0);
    snprintf(path,sizeof path,"%s/font.dat",test_data_directory);
    FILE *out=fopen(path,"wb"); assert(out && fwrite(bytes,1,count,out)==count && !fclose(out));
}
static void prepare_rule_data(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    assert(mkdtemp(test_data_directory));
    uint8_t data[LAND_FILE_SIZE] = {0};
    data[1] = 1;
    for (int index=0; index<11; ++index) {
        data[2+index*2+1] = 6;
        data[24+index*2+1] = 7;
        data[46+index*2+1] = 2;
        data[68+index*2+1] = 21;
    }
    data[91]=23; data[93]=24; data[95]=25;
    for (int index=0; index<16; ++index) data[96+index]=index;
    for (int land=0; land<4; ++land) {
        char path[256]; snprintf(path,sizeof path,"%s/land%d",test_data_directory,land);
        FILE *file=fopen(path,"wb"); assert(file);
        assert(fwrite(data,1,sizeof data,file)==sizeof data && fclose(file)==0);
    }
    assert(setenv("POPULOUS_DATA_DIR",test_data_directory,1)==0);
    char path[512]; snprintf(path,sizeof path,"%s/pixels",test_data_directory); assert(!mkdir(path,0700));
    fixture_pixels("qaz.pic.indices.u8",64000,0);
    fixture_sprite_pixels("sprites0.dat.indices.u8",0);
    fixture_sprite_pixels("sprites0.dat.keep.u8",1);
    for (int land=0;land<4;++land) {
        char name[64]; snprintf(name,sizeof name,"land%d.indices.u8",land); fixture_pixels(name,GFX_LAND_PIXELS,0);
        snprintf(name,sizeof name,"land%d.keep.u8",land); fixture_pixels(name,GFX_LAND_PIXELS,1);
    }
    assert(!setenv("POPULOUS_ASSET_DIR",test_data_directory,1));
    copy_real_font();
}
static void cleanup_rule_data(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    for (int land=0; land<4; ++land) {
        char path[256]; snprintf(path,sizeof path,"%s/land%d",test_data_directory,land);
        assert(unlink(path)==0);
    }
    char path[512];
    const char *names[]={"qaz.pic.indices.u8","sprites0.dat.indices.u8","sprites0.dat.keep.u8"};
    for (int n=0;n<3;++n) { snprintf(path,sizeof path,"%s/pixels/%s",test_data_directory,names[n]); assert(!unlink(path)); }
    for (int land=0;land<4;++land) {
        snprintf(path,sizeof path,"%s/pixels/land%d.indices.u8",test_data_directory,land); assert(!unlink(path));
        snprintf(path,sizeof path,"%s/pixels/land%d.keep.u8",test_data_directory,land); assert(!unlink(path));
    }
    snprintf(path,sizeof path,"%s/pixels",test_data_directory); assert(!rmdir(path));
    snprintf(path,sizeof path,"%s/font.dat",test_data_directory); assert(!unlink(path));
    assert(rmdir(test_data_directory)==0);
}

/* Real isometric SDL inputs: navigation first, then projected vertex click.
 * No camera/world/result mutation: this exercises exactly the exposed controls. */
static void iso_click_vertex(int column,int row,int button)
{
    int center_x=column>63 ? 63 : column, center_y=row>63 ? 63 : row;
    SDL_Event event={.type=SDL_MOUSEBUTTONDOWN};
    event.button.button=SDL_BUTTON_LEFT;
    event.button.x=2*(64+center_x-center_y); event.button.y=2*((center_x+center_y)/2);
    game_handle_event(&event);
    int u=column-xoff,v=row-yoff;
    assert(u>=0 && u<=8 && v>=0 && v<=8);
    event.button.button=button;
    event.button.x=2*(192+16*(u-v));
    event.button.y=2*(72+8*(u+v)-8*core_vertex(column,row));
    game_handle_event(&event);
}

static void test_isometric_match_cadence(int interval, int start, const MatchOutcome *expected)
{
    assert(interval==1 || interval==8);
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    assert(!diagnostic_view);
    SDL_SetWindowSize(prg_screen->window,640,512);
    SDL_Event event={.type=SDL_KEYDOWN}; event.key.keysym.sym=SDLK_SPACE;
    game_handle_event(&event);
    int32_t mana=core_mana(0);
    iso_click_vertex(63,63,SDL_BUTTON_LEFT);
    assert(xoff==56 && yoff==56 && core_vertex(63,63)==1 && core_mana(0)==mana);
    game_handle_event(&event);
    iso_click_vertex(63,63,SDL_BUTTON_LEFT);
    assert(core_vertex(63,63)==2 && core_mana(0)==mana-14);
    iso_click_vertex(63,63,SDL_BUTTON_RIGHT);
    assert(core_vertex(63,63)==1);
    event.key.keysym.sym=SDLK_r; game_handle_event(&event);
    for (int tick=0;tick<512;++tick) game_tick();
    for (int tick=0;tick<2000 && !core_result;++tick) {
        if (tick%8==0) for (int person=0;person<no_peeps && !core_result;++person) {
            if (peep_owner(person) || peep_word(person,4)<=0) continue;
            int position=peep_word(person,8);
            for (int dy=0;dy<2;++dy) for (int dx=0;dx<2;++dx)
                iso_click_vertex(position%64+dx,position/64+dy,SDL_BUTTON_RIGHT);
        }
        game_tick();
    }
    fprintf(stderr,"isometric defeat turns=%u pop=%d/%d\n",(uint16_t)game_turn,core_population(0),core_population(1));
    assert(core_result==2 && core_population(0)==0);
    /* Measured deterministic defeat phase with the computer-player AI. */
    if (getenv("POPULOUS_TEST_REAL_DATA")) assert((uint16_t)game_turn==831 && core_population(1)==6323);
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    game_tick(); game_render(); assert(a4_same(before));
    game_handle_event(&event); assert(!core_result && xoff==6 && yoff==6);
    event.key.keysym.sym=SDLK_3; game_handle_event(&event);
    for (int tick=0;tick<12000 && !core_result;++tick) {
        game_tick();
        if (tick>=start && tick%interval==0) for (int person=0;person<no_peeps;++person) {
            if (!peep_owner(person) || peep_word(person,4)<=0) continue;
            int position=peep_word(person,8);
            for (int dy=0;dy<2;++dy) for (int dx=0;dx<2;++dx)
                iso_click_vertex(position%64+dx,position/64+dy,SDL_BUTTON_RIGHT);
        }
    }
    fprintf(stderr,"isometric fight result=%d turns=%u battles=%d towns=%d pop=%d/%d\n",core_result,(uint16_t)game_turn,core_battles,core_settlements,core_population(0),core_population(1));
    assert_match_outcome(expected);
    if (expected && expected->result!=1) { _closeall(); SDL_Quit(); return; }
    assert(core_result && core_battles>0 && core_settlements>0);
    if (getenv("POPULOUS_TEST_REAL_DATA")) assert(core_result==1);
    event.key.keysym.sym=SDLK_ESCAPE; game_handle_event(&event); assert(prg_quit);
    _closeall(); SDL_Quit();
}

static const MatchOutcome isometric_victory = {1,4697,32,78,1434,0};
static void test_isometric_match(void)
{
    test_isometric_match_cadence(1,3000,&isometric_victory);
}

/* Historical scripts that started rescuing only after tick 6000. With the
 * computer-player AI active, red takes blue's last town at turn 3104, before
 * any of their input fires, so they pin that deterministic defeat. */
static const MatchOutcome late_rescue_defeat = {2,3104,33,79,0,30639};

/* Icon centre (x0+15,y0+8) per _toggle_icon geometry. */
static int icon_lit(int base, int a, int b)
{
    int x=8*(base%40)+16*a-16*b+15, y=base/40+8*a+8*b+8;
    return game_pixels[y*320+x]==(game_assets->background[y*320+x]^1);
}

/* _show_the_shield icons asm:2790-2826 and _icon_data asm:21656: first row
 * plane words of icons 0..11 copied from the DC.L lines. */
static int asm_icon_colour(const uint16_t planes[4], int bit)
{
    int colour=0;
    for (int plane=0;plane<4;++plane) colour|=((planes[plane]>>(15-bit))&1)<<plane;
    return colour;
}
static void test_shield_icons(void)
{
    static const uint16_t row0[12][4]={
        {0xffbe,0xff7f,0x0080,0x0000},{0xfffe,0xffff,0x0000,0x0000},{0x8000,0x0000,0x0000,0xffff},
        {0x8000,0x0000,0x0000,0xffff},{0x8000,0x0000,0x0000,0xffff},{0x8010,0x0008,0x0008,0xfff7},
        {0x8003,0x0001,0x0001,0xfffe},{0x9100,0x0080,0x1c80,0xe37f},{0x0007,0x6000,0x8000,0x1fff},
        {0x8000,0x0000,0x0000,0xffff},{0x8000,0x0000,0x0000,0xffff},{0x8001,0x0001,0x0002,0xfffc}};
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    assert(no_peeps>0 && peep_word(0,4)>0);
    view_who=0; game_render();
    static uint8_t empty[320*200]; memcpy(empty,game_pixels,sizeof empty);
    uint8_t saved_owner=peeps[1], saved_weapon=peeps[3];
    for (int owner=0;owner<2;++owner) for (int weapon=0;weapon<=11;++weapon) {
        view_who=1; peeps[1]=(uint8_t)owner;
        /* Slot 0 is a byte no slot 1..10 holds; slot 11 is read but never drawn. */
        int value=0;
        if (weapon) value=(uint16_t)A4_WORD(0x2f96+2*weapon);
        else for (int clash=1; clash; ) { clash=0; for (int d=1;d<=10;++d) if ((uint16_t)A4_WORD(0x2f96+2*d)==value) { clash=1; ++value; } }
        peeps[3]=(uint8_t)value;
        int expect=0;
        for (int d=1;d<=10;++d) if ((uint16_t)A4_WORD(0x2f96+2*d)==peeps[3]) { expect=d; break; }
        assert(weapon==0 ? !expect : weapon==11 || expect);
        game_render();
        for (int bit=0;bit<16;++bit) {
            assert(game_pixels[4*320+0x110+bit]==asm_icon_colour(row0[owner],bit));
            int at=4*320+0x120+bit;
            assert(game_pixels[at]==(expect ? asm_icon_colour(row0[expect+1],bit) : empty[at]));
        }
    }
    peeps[1]=saved_owner; peeps[3]=saved_weapon;
    view_who=0; game_render(); assert(!memcmp(empty,game_pixels,sizeof empty));
}

/* _zoom_map flashes: _toggle_icon(d_screen,...) shows for one frame, the
 * next _clr_wsc restores _back_scr (asm:611,762,989). Diamond of 226 px. */
static int frame_diff(const uint8_t *a, int *first)
{
    int count=0; *first=-1;
    for (int pixel=0;pixel<320*200;++pixel) if (a[pixel]!=game_pixels[pixel]) { if (*first<0) *first=pixel; ++count; }
    return count;
}
static void test_icon_flashes(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    view_who=0; core_tool=0; game_render();
    static uint8_t base[320*200]; memcpy(base,game_pixels,sizeof base);
    int first;
    /* Scroll pad centre (icon 4, line 1) at screen (64,168): base 0x12c0,
     * x0 = 16*4-16 = 48, y0 = 120+32+8 = 160; mask row 1 is bits 16,17. */
    assert(core_toolbar(64,168,1,0));
    game_render(); assert(frame_diff(base,&first)==226 && first==161*320+48+16);
    assert(game_pixels[168*320+50]==(base[168*320+50]^1) && game_pixels[168*320+49]==base[168*320+49]);
    game_render(); assert(frame_diff(base,&first)==0);
    /* Tendency (3,3) at (16,176) is a persistent state icon: no flash. */
    assert(core_toolbar(16,176,1,0));
    game_render(); assert(frame_diff(base,&first)==0);
    /* Message icon (system 2,2) at (304,192): base 0x17e4 = row 152, byte 36,
     * x0 = 8*36+32-32 = 288, y0 = 152+16+16 = 184. Posts 14/2. */
    uint8_t *mailbox=A4_DISP(0x82b6+player*46);
    mailbox[0]=0; mailbox[2]=0;
    assert(core_toolbar(304,192,1,1));
    assert(mailbox[0]==14 && mailbox[2]==2);
    game_render(); assert(frame_diff(base,&first)==226 && first==185*320+288+16);
    game_render(); assert(frame_diff(base,&first)==0);
    /* _do_action y=2 (asm:18404-18406): _serial_off set, nothing happens. */
    assert(serial_off==1);
    static uint8_t a4_before[0x10000];
    uint8_t *a4=prg_a4-32768; size_t span=sizeof a4_before;
    memcpy(a4_before,a4,span);
    int kind=core_req.kind;
    A4_DISP(0x82b6+(1-player)*46)[0]=0;
    core_consume_terrain_mailboxes();
    assert(core_req.kind==kind && serial_off==1);
    for (size_t at=0; at<span; ++at)
        if (at<32768+(int16_t)0x82b6 || at>=32768+(int16_t)0x82b6+92) assert(a4[at]==a4_before[at]);
    game_render(); assert(frame_diff(base,&first)==0);
}

static void test_query_and_shield(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    /* _interogate asm:2719-2765 on a hand-built sprite table. */
    no_sprites=2;
    A4_WORD(0x9a22)=100; A4_WORD(0x9a24)=50; A4_WORD(0x9a28)=3;
    A4_WORD(0x9a2a)=100; A4_WORD(0x9a2c)=50; A4_WORD(0x9a30)=4;
    view_who=1; view_timer=0; old_view_who=0; core_tool=0;
    core_interogate(106,54,0); assert(view_who==1 && view_timer==0); /* not query */
    core_tool=2;
    core_interogate(113,54,0); assert(view_who==1 && view_timer==0); /* miss x */
    core_interogate(106,59,0); assert(view_who==1 && view_timer==0); /* miss y */
    core_interogate(112,58,0); /* inclusive edges, first entry wins */
    assert(view_who==3 && view_timer==10 && old_view_who==1);
    core_interogate(100,50,1); assert(view_who==3 && view_timer==0);
    /* Shield state asm:2769-2790: a dead view_who is dropped. */
    view_who=1; set_peep_word(0,4,0); assert(!core_shield_state() && !view_who);
    assert(game_reset(0));
    /* Query clicks neither sculpt nor spend mana; the table follows the blit. */
    memset(game_assets->background,1,sizeof game_assets->background);
    memset(game_assets->sprite_keep,1,sizeof game_assets->sprite_keep);
    set_peep_word(0,12,0);   /* synthetic start +$C 255 hits the _debug exit */
    game_render();
    assert(no_sprites>0 && A4_WORD(0x9a22)==184 && A4_WORD(0x9a24)==88 && A4_WORD(0x9a28)==1);
    core_tool=2; SDL_SetWindowSize(prg_screen->window,640,512);
    static int16_t heights[65*65];
    for (int v=0;v<65*65;++v) heights[v]=core_vertex(v%65,v/65);
    int32_t mana=core_mana(0); int16_t target=core_target(0);
    for (int button=SDL_BUTTON_LEFT;button<=SDL_BUTTON_RIGHT;button+=2) {
        SDL_Event click={.type=SDL_MOUSEBUTTONDOWN}; click.button.button=(uint8_t)button;
        click.button.x=224; click.button.y=96; game_handle_event(&click);
    }
    assert(core_mana(0)==mana && core_target(0)==target);
    for (int v=0;v<65*65;++v) assert(heights[v]==core_vertex(v%65,v/65));
    /* Hover over the first sprite selects it temporarily. */
    view_who=0; view_timer=0;
    SDL_Event motion={.type=SDL_MOUSEMOTION}; motion.motion.x=(184+4)*2; motion.motion.y=(88+4)*2;
    game_handle_event(&motion); assert(view_who==1 && view_timer==10);
    /* Shield panel: pixels change only when view_who is set. */
    memset(game_assets->sprite_indices,7,sizeof game_assets->sprite_indices);
    view_who=0; game_render();
    static uint8_t empty[320*200]; memcpy(empty,game_pixels,sizeof empty);
    view_who=1; game_render();
    int changed=0;
    for (int y=0;y<40;++y) for (int x=256;x<320;++x) changed+=empty[y*320+x]!=game_pixels[y*320+x];
    assert(changed>0);
    view_who=0; game_render(); assert(!memcmp(empty,game_pixels,sizeof empty));
    core_tool=0;
}

static void test_toolbar_icon_highlight(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    int16_t saved_tend=core_tendency(player);
    /* Defaults asm:513-536: music, effects (both flags 1), terrain, plus current tendency. */
    A4_WORD(0x8250)=A4_WORD(0x8252)=1; pause=0; core_tool=0; core_swamp_selected=0;
    core_tendency(player)=1; game_render();
    assert(icon_lit(0x17e4,0,3) && icon_lit(0x17e4,0,4) && !icon_lit(0x17e4,1,3));
    assert(icon_lit(0x12c0,6,1) && icon_lit(0x12c0,4,3) && !icon_lit(0x12c0,6,2));
    /* Toggling music changes exactly the (0,3) diamond, centre included. */
    static uint8_t on[320*200]; memcpy(on,game_pixels,sizeof on);
    A4_WORD(0x8250)=0; game_render(); A4_WORD(0x8250)=1;
    int changed=0, x0=8*(0x17e4%40)-48, y0=0x17e4/40+24;
    for (int i=0;i<320*200;++i) if (on[i]!=game_pixels[i]) {
        int x=i%320-x0, y=i/320-y0;
        assert((on[i]^game_pixels[i])==1 && x>=0 && x<32 && y>=1 && y<16);
        ++changed;
    }
    assert(changed==2*(2+6+10+14+18+22+26)+30);
    const int tend[4][2]={{3,3},{4,3},{5,3},{4,4}};
    for (int t=0;t<4;++t) {
        core_tendency(player)=(int16_t)t; game_render();
        for (int o=0;o<4;++o) assert(icon_lit(0x12c0,tend[o][0],tend[o][1])==(o==t));
    }
    core_tool=1; game_render(); assert(icon_lit(0x12c0,6,2) && !icon_lit(0x12c0,6,1));
    core_tool=0; core_swamp_selected=1; game_render();
    assert(icon_lit(0x12c0,2,2) && !icon_lit(0x12c0,6,1));
    core_swamp_selected=0; core_tool=2; game_render();
    assert(icon_lit(0x12c0,6,0) && !icon_lit(0x12c0,6,1) && !icon_lit(0x12c0,6,2));
    core_tool=0;
    A4_WORD(0x8250)=0; A4_WORD(0x8252)=0; pause=1; game_render();
    assert(!icon_lit(0x17e4,0,3) && !icon_lit(0x17e4,0,4) && icon_lit(0x17e4,1,3));
    A4_WORD(0x8250)=A4_WORD(0x8252)=0; pause=0;
    core_tendency(player)=saved_tend;
}

static void test_original_graphics_output(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    GraphicsAssets *assets_before=game_assets;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    char *directory=strdup(getenv("POPULOUS_ASSET_DIR") ? getenv("POPULOUS_ASSET_DIR") : POPULOUS_DEFAULT_ASSET_DIR);
    assert(directory && !setenv("POPULOUS_ASSET_DIR","/nonexistent-populous-assets",1));
    assert(!game_reset(0) && game_assets==assets_before && a4_same(before));
    assert(!setenv("POPULOUS_ASSET_DIR",directory,1)); free(directory);
    game_render();
    assert(prg_screen->chunky_buffer[190*320+20]==prg_screen->palette[game_assets->background[190*320+20]]);
    assert(prg_screen->palette[15]==0xff2266cc);
    /* Sparse synthetic assets make frame, placement and layering expectations literal. */
    memset(game_assets->background,1,sizeof game_assets->background);
    memset(game_assets->land_keep,1,sizeof game_assets->land_keep);
    memset(game_assets->land_indices,0,sizeof game_assets->land_indices);
    memset(game_assets->sprite_keep,1,sizeof game_assets->sprite_keep);
    memset(game_assets->sprite_indices,0,sizeof game_assets->sprite_indices);
    game_assets->land_indices[15*768+8*32+16]=2;
    game_assets->land_indices[42*768+8*32+16]=4;
    game_assets->sprite_indices[8*256]=8;
    /* +$C 0 (the synthetic start's 255 is _move_sprite's _debug exit) and _toggle 0. */
    set_peep_word(0,12,0); toggle=0; memcpy(before,prg_a4_area,sizeof before);
    game_render();
    assert(game_pixels[72*320+192]==3); /* first flat tile */
    assert(game_pixels[88*320+184]==9); /* first walker at (8,8), counter 0: no offset */
    assert(a4_same(before));
    set_peep_word(0,12,255);   /* restore the start state for the ticks below */
    for (int tick=0;tick<16;++tick) game_tick();
    int settlements=0;
    for (int cell=0;cell<4096;++cell) settlements+=map_bk2[cell]==42;
    assert(settlements>0);
    game_assets->sprite_indices[64*256]=8;
    toggle=0; game_render();
    assert(game_pixels[96*320+192]==5); /* town overlay, tile_top-8 */
    assert(game_pixels[88*320+192]==9); /* settled occupant uses frame 64 */
    /* _move_sprite compares flags to exactly 1: flags 9 is a town defending
     * a battle, not a peaceful occupant. Distinct frame pixels catch precedence. */
    assert(peep_word(0,8)==8*64+8 && map_who[8*64+8]==1);
    for (int owner=0;owner<2;++owner) for (int phase=0;phase<2;++phase) {
        peeps[0]=9; peeps[1]=owner; game_turn=16+phase*4; toggle=phase;
        game_assets->sprite_indices[(64+phase+owner*2)*256]=8;
        game_assets->sprite_indices[(138+phase)*256]=2+phase*2;
        memcpy(before,prg_a4_area,sizeof before);
        game_render();
        assert(game_pixels[88*320+192]==(phase ? 5 : 3));
        assert(prg_screen->chunky_buffer[88*320+192]==prg_screen->palette[phase ? 5 : 3]);
        assert(a4_same(before));
    }
    /* High terrain projects beside (not inside) the minimap diamond. */
    for (int row=0;row<=64;++row) for (int column=0;column<=64;++column) core_set_vertex(column,row,8);
    _make_map();
    SDL_SetWindowSize(prg_screen->window,640,512);
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN}; click.button.button=SDL_BUTTON_RIGHT;
    click.button.x=224; click.button.y=96; /* vertex (6,11), local (0,5), h=8 */
    game_handle_event(&click);
    assert(core_vertex(6,11)==7);
    _closeall(); SDL_Quit();
}

/* This oracle uses raw asset bytes, never the production selector, projection
 * or blitter. The ROI excludes the overview (x<=127); picking is disabled. */
static void sprite_mask_oracle(uint8_t *expected, int frame, int left, int top)
{
    if (frame<0) return; /* Literal -1 expectations mean no occupant. */
    assert(frame<143 && (size_t)(frame+1)*256<=game_assets->sprite_size);
    for (int row=0;row<16;++row) for (int column=0;column<16;++column) {
        int x=left+column,y=top+row;
        if (x<0 || x>=320 || y<0 || y>=200) continue;
        int source=frame*256+row*16+column, destination=y*320+x;
        if (!game_assets->sprite_keep[source]) expected[destination]=0;
        expected[destination]|=game_assets->sprite_indices[source];
    }
}
static void assert_sprite_pixels(const uint8_t *expected)
{
    for (int y=0;y<200;++y) for (int x=128;x<320;++x) {
        int pixel=y*320+x;
        if (game_pixels[pixel]!=expected[pixel]) {
            fprintf(stderr,"sprite mismatch flags=%02x owner=%u stored=%d turn=%d at %d,%d: got %u want %u\n",
                peeps[0],peeps[1],peep_word(0,12),game_turn,x,y,game_pixels[pixel],expected[pixel]);
            assert(0);
        }
        assert(prg_screen->chunky_buffer[pixel]==prg_screen->palette[expected[pixel]]);
    }
}
static void sprite_scene(void)
{
    memset(map_who,0,4096); memset(map_bk2,0,4096);
    memset(map_blk,15,4096); memset(map_alt,0,4096);
    memset(peeps,0,208*22); no_peeps=2;
    xoff=16; yoff=16; mousex=-100; mousey=-100; view_who=0;
    core_tendency(0)=core_tendency(1)=1;
}
static void sprite_case(int flags, int owner, int stored, uint32_t heading,
                        int turn, int delta, int block, int u, int v, int altitude,
                        int want_frame, int want_x, int want_y)
{
    sprite_scene();
    int cell=(16+v)*64+16+u;
    peeps[0]=flags; peeps[1]=owner;
    set_peep_word(0,4,100); set_peep_word(0,8,cell);
    set_peep_word(0,10,delta); set_peep_word(0,12,stored);
    set_peep_word(1,4,100); set_peep_word(1,8,4000);
    memcpy(peeps+14,&heading,sizeof heading);
    /* _toggle (asm:606-612) drives walker/town phases; follow turn/4 here. */
    game_turn=turn; toggle=((uint16_t)turn/4)&1; map_blk[cell]=block; map_alt[cell]=altitude;
    uint8_t expected[64000],before[sizeof prg_a4_area];
    memcpy(before,prg_a4_area,sizeof before);
    draw_original(16,16);
    assert(a4_same(before));
    memcpy(expected,game_pixels,sizeof expected);
    sprite_mask_oracle(expected,want_frame,want_x,want_y);
    map_who[cell]=1;
    memcpy(before,prg_a4_area,sizeof before);
    draw_original(16,16);
    assert(a4_same(before));
    assert_sprite_pixels(expected);
}
static void test_special_sprite_frames(void)
{
    /* Each physical byte, valid target (2), self (1), and opaque values all
     * mean present. In particular, the high half or pointer validity is not a gate. */
    uint32_t headings[]={0,1,2,UINT32_MAX,0,0,0,0};
    for (int byte=0;byte<4;++byte) ((uint8_t *)&headings[4+byte])[byte]=0x80;
    const int celebration[2][2][3]={{{85,86,87},{117,118,119}},{{89,90,91},{121,122,123}}};
    const int waiting[2][2][2]={{{101,102},{125,126}},{{103,104},{127,128}}};
    const int water[2][2][4]={{{93,94,95,96},{109,110,111,112}},{{97,98,99,100},{113,114,115,116}}};
    const int remnant[]={105,106,107,108};
    const int town[2][2]={{64,65},{66,67}};
    const int directions[]={-65,-64,-63,1,65,64,63,-1};
    const int walkers[2][2][2][8]={
        {{{0,2,4,6,8,10,12,14},{1,3,5,7,9,11,13,15}},
         {{32,34,36,38,40,42,44,46},{33,35,37,39,41,43,45,47}}},
        {{{16,18,20,22,24,26,28,30},{17,19,21,23,25,27,29,31}},
         {{48,50,52,54,56,58,60,62},{49,51,53,55,57,59,61,63}}}
    };
    const int wait_flags[]={0x22,0x42,0x62};
    for (int owner=0;owner<2;++owner) for (int raw=0;raw<8;++raw)
    for (int phase=0;phase<2;++phase) {
        int headed=raw!=0;
        for (int stored=0;stored<3;++stored) for (int flags=5;flags<=6;++flags)
            sprite_case(flags,owner,85+stored,headings[raw],phase*4,1,15,0,0,0,
                celebration[owner][headed][stored],192,56);
        for (int state=0;state<3;++state) for (int stored=0;stored<2;++stored)
            sprite_case(wait_flags[state],owner,101+stored,headings[raw],phase*4,1,15,0,0,0,
                waiting[owner][headed][stored],192,56);
        sprite_case(1,owner,32767,headings[raw],phase*4,1,15,0,0,0,town[owner][phase],192,56);
        sprite_case(9,owner,-1,headings[raw],phase*4,1,15,0,0,0,phase ? 139 : 138,192,56);
        for (int turn=0;turn<4;++turn) {
            sprite_case(0x80,owner,32767,headings[raw],phase*4+turn,1,15,0,0,0,remnant[turn],184,56);
            for (int block=0;block<2;++block) for (int direction=0;direction<8;++direction)
                sprite_case(18,owner,0,headings[raw],phase*4+turn,directions[direction],block ? 0 : 15,0,0,0,
                    water[owner][headed][turn],184,block ? 60 : 56);
        }
        for (int direction=0;direction<8;++direction) for (int block=0;block<2;++block)
            sprite_case(2,owner,0,headings[raw],phase*4,directions[direction],block ? 0 : 15,0,0,0,
                walkers[owner][headed][phase][direction],184,block ? 60 : 56);
    }
    sprite_case(5,1,42,1,0,1,15,0,0,0,78,192,56);
    /* Malformed stored frames are not repaired or narrowed before arithmetic. */
    const int stored[]={0,142,143,-1,32767};
    const int bounds[4][5]={{0,142,-1,-1,-1},{4,-1,-1,3,-1},
                           {32,-1,-1,31,-1},{36,-1,-1,35,-1}};
    const int wait_bounds[4][5]={{0,142,-1,-1,-1},{2,-1,-1,1,-1},
                                {24,-1,-1,23,-1},{26,-1,-1,25,-1}};
    for (int owner=0;owner<2;++owner) for (int headed=0;headed<2;++headed)
    for (int sample=0;sample<5;++sample) {
        for (int flags=5;flags<=6;++flags)
            sprite_case(flags,owner,stored[sample],headed,0,1,15,0,0,0,bounds[headed*2+owner][sample],192,56);
        for (int state=0;state<3;++state)
            sprite_case(wait_flags[state],owner,stored[sample],headed,0,1,15,0,0,0,wait_bounds[headed*2+owner][sample],192,56);
    }
    /* Mixed flags follow the _move_sprite dispatch (asm:15996-16016):
     * ==2, ==1, bit 4, bits 5/6, bit 3, bit 7, then the +$C default. Stored
     * 3 (counter 3 for the walking branches), delta 1 (dx 2, dy 1), owner 1,
     * heading present, turn 7 (toggle 1), block 0 (+4 only when walking). */
    const int mixed[][4]={
        {0,3+32+4,192,56},{4,3+32+4,192,56},{0x0c,139,192,56},{0x86,105+3,184,56},
        {0x88,139,192,56},{0x16,93+3+16+4,190,63},{0x18,93+3+16+4,190,63},
        {0x28,3+24+2,192,56},{0x63,3+24+2,192,56},{0x90,93+3+16+4,190,63},
        {0xff,93+3+16+4,190,63},{0xc4,3+24+2,192,56},{0x84,105+3,184,56},
        {0x03,3+32+4,192,56},{0x0b,139,192,56}};
    for (unsigned sample=0;sample<sizeof mixed/sizeof mixed[0];++sample)
        sprite_case(mixed[sample][0],1,3,UINT32_MAX,7,1,0,0,0,0,
            mixed[sample][1],mixed[sample][2],mixed[sample][3]);
    /* Current-cell coordinates, height lift, top clipping and far visible corners. */
    sprite_case(6,1,42,1,0,1,15,0,0,8,78,192,-8);
    sprite_case(2,1,0,1,4,1,0,0,0,8,55,184,-4);
    sprite_case(0x80,0,0,0,3,1,15,7,0,0,108,296,112);
    sprite_case(5,0,85,0,0,1,15,7,7,0,85,192,168);
    sprite_case(0x22,0,101,0,0,1,15,3,2,3,101,208,72);
    sprite_case(5,1,42,1,0,1,0,0,0,0,78,192,56);
    sprite_case(0x80,1,0,1,3,1,0,0,0,0,108,184,56);
    sprite_case(0x62,1,102,1,4,1,0,0,0,0,128,192,56);
    /* Walking counters >= 8 take the _debug exit (asm:16061-16069); a
     * negative counter is multiplied as is (MULS). */
    for (int sample=0;sample<5;++sample) {
        int counter=stored[sample], shown=counter<8;
        sprite_case(2,1,counter,1,4,1,0,0,0,0,shown ? 55 : -1,184+2*counter,60+counter);
        sprite_case(18,1,counter,1,7,1,0,0,0,0,shown ? 116 : -1,184+2*counter,60+counter);
    }
}
static void test_special_sprite_order_and_guards(void)
{
    sprite_scene(); game_turn=4; toggle=1;
    uint8_t expected[64000],before[sizeof prg_a4_area];
    draw_original(16,16); memcpy(expected,game_pixels,sizeof expected);
    /* Reverse slot order: the earlier cell's celebration overlaps the later
     * cell's walker. Hidden colocated slot 2 must not be drawn. */
    no_peeps=3;
    for (int person=0;person<3;++person) {
        peeps[person*22]=person ? 5 : 2;
        set_peep_word(person,4,100); set_peep_word(person,8,16*64+16+(person==0));
        set_peep_word(person,10,1); set_peep_word(person,12,person==2 ? 86 : person ? 85 : 0);
    }
    map_who[16*64+16]=2; map_who[16*64+17]=1;
    sprite_mask_oracle(expected,85,192,56);
    sprite_mask_oracle(expected,7,200,64);
    memcpy(before,prg_a4_area,sizeof before); draw_original(16,16);
    assert(a4_same(before)); assert_sprite_pixels(expected);
    for (int guard=0;guard<5;++guard) {
        sprite_scene(); game_turn=0;
        draw_original(16,16); memcpy(expected,game_pixels,sizeof expected);
        map_who[16*64+16]=1; peeps[0]=5; peeps[1]=0;
        set_peep_word(0,4,100); set_peep_word(0,8,16*64+16); set_peep_word(0,12,85);
        if (guard==0) no_peeps=0;
        if (guard==1) set_peep_word(0,4,0);
        if (guard==2) set_peep_word(0,8,16*64+17);
        if (guard==3) peeps[1]=2;
        if (guard==4) map_who[16*64+16]=255;
        memcpy(before,prg_a4_area,sizeof before); draw_original(16,16);
        assert(a4_same(before)); assert_sprite_pixels(expected);
    }
}
/* 32x32 "funny" objects: _do_place_funny asm:8862-9013, _do_funny
 * asm:8630-8858, _move_sprite asm:16061-16197, main loop asm:630-641 and
 * _draw_s_32 asm:19365-19504. */
static void funny_clear(void)
{
    for (int slot=0xd1;slot<0xd3;++slot) memset(peeps+slot*22,0,22);
    memset(map_who,0,4096);
}
static void test_funny_objects(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    /* Real export: spr_320.dat is 13 frames (Read $2080, asm:16443-16449),
     * loaded byte for byte; the fixture export has none (Open fails, the
     * original skips the Read) and loads without a sheet. */
    char error[256];
    GraphicsAssets *real=graphics_load(POPULOUS_DEFAULT_ASSET_DIR,0,0,error,sizeof error);
    assert(real);
    GraphicsSheet big=graphics_big_sprites(real);
    assert(big.width==32 && big.height==32 && big.size==13*1024);
    static uint8_t file_bytes[13*1024+1];
    FILE *file=fopen(POPULOUS_DEFAULT_ASSET_DIR "/pixels/spr_320.dat.indices.u8","rb"); assert(file);
    assert(fread(file_bytes,1,sizeof file_bytes,file)==13*1024 && !fclose(file));
    assert(!memcmp(file_bytes,real->big_indices,13*1024));
    int opaque=0; for (int i=0;i<13*1024;++i) opaque+=!real->big_keep[i];
    assert(opaque>0 && opaque<13*1024);
    graphics_free(real);
    assert(!graphics_big_sprites(game_assets).indices && game_assets->big_size==0);
    assert(funny_done==0 && !peep_word(0xd1,4) && !peep_word(0xd2,4));
    /* _draw_s_32 placement, mask and clip rules on a synthetic frame. */
    static uint8_t screen[320*200], indices[2*1024], keep[2*1024];
    memset(keep,1,sizeof keep); memset(indices,0,sizeof indices);
    indices[1024+0]=9; keep[1024+0]=0;           /* frame 1 (0,0): replace */
    indices[1024+31*32+31]=6;                    /* frame 1 (31,31): OR over keep */
    indices[1024+1*32+0]=4; keep[1024+1*32+0]=0; /* frame 1 (0,1) */
    GraphicsSheet sheet={indices,keep,sizeof indices,32,32};
    memset(screen,3,sizeof screen);
    assert(graphics_draw_s_32(screen,sizeof screen,sheet,1,100,50));
    assert(screen[50*320+100]==9 && screen[81*320+131]==7 && screen[51*320+100]==4);
    assert(screen[50*320+101]==3 && screen[49*320+100]==3);
    assert(!graphics_draw_s_32(screen,sizeof screen,sheet,2,100,50));   /* past the sheet */
    memset(screen,3,sizeof screen);
    assert(!graphics_draw_s_32(screen,sizeof screen,sheet,1,100,-1));  /* TST D1/BMI */
    assert(!graphics_draw_s_32(screen,sizeof screen,sheet,1,100,199)); /* $C7-199 = 0: BLE */
    for (int i=0;i<320*200;++i) assert(screen[i]==3);
    assert(graphics_draw_s_32(screen,sizeof screen,sheet,1,100,198));  /* DBF 1: two rows */
    assert(screen[198*320+100]==9 && screen[199*320+100]==4);
    assert(graphics_draw_s_32(screen,sizeof screen,sheet,1,300,10));   /* D7 $28: one word */
    assert(screen[10*320+300]==9 && screen[11*320+0]==3);
    /* Placement: first free slot, table fields, edge formulas. */
    funny_clear(); seed=1234;
    uint16_t saved=seed; int expect=(int16_t)___newrand()%125/2+0xfc0; seed=saved;
    core_place_funny(0,0);
    assert(peep_word(0xd1,4)==1 && peeps[0xd1*22]==2 && peep_word(0xd1,8)==expect);
    assert(map_who[expect]==0xd2 && peep_word(0xd1,10)==-64 && peep_word(0xd1,6)==0);
    assert(peeps[0xd1*22+2]==4 && peeps[0xd1*22+3]==0 && peeps[0xd1*22+20]==0 && peeps[0xd1*22+21]==1);
    saved=seed; int odd=___newrand()&1, value=(int16_t)___newrand()%43; seed=saved;
    core_place_funny(2,2);
    assert(peep_word(0xd2,8)==(odd ? value*64 : value) && peep_word(0xd2,10)==65);
    assert(peeps[0xd2*22+2]==12 && peeps[0xd2*22+3]==9 && peep_word(0xd2,6)==9);
    saved=seed; core_place_funny(1,0); assert(seed==saved);           /* both slots busy */
    funny_clear(); saved=seed; core_place_funny(3,0); assert(seed==saved && !peep_word(0xd1,4));
    for (int trial=0;trial<8;++trial) {
        funny_clear(); saved=seed; odd=___newrand()&1; value=(int16_t)___newrand()%43; seed=saved;
        core_place_funny(1,1);
        int position=(value+20)*64+(odd ? 63 : 0xfc0);
        assert(peep_word(0xd1,8)==position && peep_word(0xd1,10)==-65 && peeps[0xd1*22+2]==8 && peeps[0xd1*22+3]==5);
        int marked=0; for (int i=0;i<4096;++i) marked+=map_who[i]!=0;
        assert(marked==(position<4096));   /* out-of-map edge cell is not written */
    }
    /* _do_funny: frame bounce every turn, a step every 8th, then trails. */
    funny_clear();
    memset(map_blk,15,4096); memset(map_bk2,0,4096);
    int start=40*64+20;
    map_blk[start-64-1]=0; map_blk[start-64+1]=16;
    core_place_funny(0,3); set_peep_word(0xd1,8,start); map_who[start]=0xd2;
    static const int frames[16]={1,2,3,4,3,2,1,0,1,2,3,4,3,2,1,0};
    for (int turn=0;turn<16;++turn) {
        core_do_funny();
        assert(peep_word(0xd1,6)==frames[turn]);
        if (turn==6) assert(peep_word(0xd1,12)==7 && peep_word(0xd1,8)==start && map_who[start]==0xd2);
        if (turn==7) {
            int now=start-64;
            assert(peep_word(0xd1,12)==0 && peep_word(0xd1,8)==now && map_who[now]==0xd2 && !map_who[start]);
            assert(map_bk2[now]==50 && map_bk2[now-1]==0 && map_bk2[now+1]==0);   /* sea and 16 skipped */
        }
    }
    assert(map_bk2[start-128]==50 && map_bk2[start-129]==51 && map_bk2[start-127]==52);
    /* Type 1 converts 15/31/32/66 to 53 and kills the occupant; type 2 puts 47+i. */
    funny_clear(); assert(game_reset(0)); funny_clear(); memset(map_blk,15,4096);
    start=30*64+30; map_blk[start-65]=31; map_blk[start-65-64]=40; map_blk[start-65-1]=66;
    int victim=0; int victim_cell=start-65-1; assert(peep_word(victim,4)>0);
    map_who[peep_word(victim,8)]=0; set_peep_word(victim,8,victim_cell); map_who[victim_cell]=victim+1;
    core_place_funny(1,3); set_peep_word(0xd1,8,start); map_who[start]=0xd2;
    for (int turn=0;turn<8;++turn) core_do_funny();
    assert(map_blk[start-65]==53 && map_blk[start-65-64]==40 && map_blk[start-65-1]==53);
    assert(peep_word(victim,4)==0 && peeps[victim*22]==0 && map_who[victim_cell]==0 && map_who[start-65]==0xd2);
    funny_clear(); memset(map_blk,15,4096); map_blk[start+65+1]=0;
    core_place_funny(2,3); set_peep_word(0xd1,8,start); map_who[start]=0xd2;
    for (int turn=0;turn<8;++turn) core_do_funny();
    assert(map_blk[start+65]==47 && map_blk[start+65+64]==48 && map_blk[start+65+1]==0);
    /* Leaving the map ends it (valid_move 1); the cell was already cleared. */
    funny_clear(); core_place_funny(0,3); set_peep_word(0xd1,8,5); map_who[5]=0xd2;
    for (int turn=0;turn<8;++turn) core_do_funny();
    assert(!peep_word(0xd1,4) && !map_who[5] && peep_word(0xd1,8)==5);
    /* Triggers: game turn 4096 (start_seed&3, asm:752-760) and once on a full pool. */
    assert(game_reset(0)); funny_clear();
    start_seed=6; game_turn=0x0fff; game_tick();
    assert(game_turn==0x1000 && peep_word(0xd1,4)==1 && peeps[0xd1*22+20]==2);
    game_tick(); assert(!peep_word(0xd2,4));
    assert(game_reset(0)); funny_clear(); start_seed=3; game_turn=0x0fff; game_tick();
    assert(!peep_word(0xd1,4) && !peep_word(0xd2,4));
    for (int owner=0;owner<2;++owner) {
        setup_town_split(owner,0,208,208); funny_clear(); funny_done=0;
        A4_WORD(0x82bc+owner*46)=1; A4_WORD(0x2ff6)=30000;
        core_settle_tick(0);
        assert(funny_done==1 && peep_word(0xd1,4)==1 && peeps[0xd1*22+20]==1 && peep_word(0xd1,10)==-65);
        funny_clear(); core_settle_tick(0); assert(!peep_word(0xd1,4));
    }
    /* Render: _move_sprite position, interpolation by +$C, edge hiding,
     * main-loop offset (-8,-16) and the queued _sprite entries. */
    assert(game_reset(0));
    memset(game_assets->background,1,sizeof game_assets->background);
    memset(game_assets->land_keep,1,sizeof game_assets->land_keep);
    memset(game_assets->land_indices,0,sizeof game_assets->land_indices);
    memset(game_assets->big_keep,1,sizeof game_assets->big_keep);
    memset(game_assets->big_indices,0,sizeof game_assets->big_indices);
    /* Without a sheet the object is still queued but nothing is drawn. */
    funny_clear(); draw_original(16,16);
    static uint8_t plain[320*200]; memcpy(plain,game_pixels,sizeof plain);
    map_who[17*64+17]=0xd2; peeps[0xd1*22]=2; set_peep_word(0xd1,10,-64);
    draw_original(16,16);
    assert(!memcmp(plain,game_pixels,sizeof plain) && no_sprites==1 && A4_WORD(0x9a28)==0xd2);
    game_assets->big_size=13*1024;
    for (int frame=0;frame<13;++frame) { game_assets->big_indices[frame*1024]=2+frame%13; game_assets->big_keep[frame*1024]=0; }
    static const struct { int step,dx,dy,hide_u,hide_v; } moves[]={
        {-65,0,-2,0,0},{-64,2,-1,-1,0},{-63,4,0,7,0},{1,2,1,7,-1},
        {65,0,2,7,7},{64,-2,1,-1,7},{63,-4,0,0,7},{-1,-2,-1,0,-1},{5,0,0,-1,-1}};
    for (unsigned m=0;m<sizeof moves/sizeof *moves;++m)
    for (int u=0;u<8;u+=3) for (int v=0;v<8;v+=7) for (int counter=0;counter<9;counter+=2) for (int flat=0;flat<2;++flat) {
        funny_clear(); memset(map_alt,0,4096); memset(map_blk,15,4096); view_who=0;
        int cell=(16+v)*64+16+u, frame=(int)(m+counter)%13;
        map_alt[cell]=2; if (cell+moves[m].step>=0) map_alt[cell+moves[m].step]=1;
        if (!flat) map_blk[cell]=20;
        static uint8_t base[320*200]; draw_original(16,16); memcpy(base,game_pixels,sizeof base);
        map_who[cell]=0xd2; peeps[0xd1*22]=2;
        set_peep_word(0xd1,6,frame); set_peep_word(0xd1,10,moves[m].step); set_peep_word(0xd1,12,counter);
        draw_original(16,16);
        int sx=176+16*(u-v)+8+moves[m].dx*counter, sy=64+8*(u+v)-16-8+(flat ? 0 : 4)+1*counter+moves[m].dy*counter;
        int hidden=counter>=8 || (counter>4 && (u==moves[m].hide_u || v==moves[m].hide_v));
        int colour=game_pixels[(sy-16)*320+sx-8];
        if (hidden) { assert(!memcmp(base,game_pixels,sizeof base) && no_sprites==0); continue; }
        /* Only the frame's single opaque pixel (its origin) changes. */
        int changed=0; for (int i=0;i<320*200;++i) changed+=base[i]!=game_pixels[i];
        assert(colour==2+frame && changed==(base[(sy-16)*320+sx-8]!=colour));
        assert(no_sprites==1 && A4_WORD(0x9a22)==sx && A4_WORD(0x9a24)==sy && A4_WORD(0x9a26)==frame && A4_WORD(0x9a28)==0xd2);
    }
    /* The view_who marker (frame $44) is queued but past the 13-frame sheet. */
    funny_clear(); memset(map_alt,0,4096); memset(map_blk,15,4096);
    map_who[17*64+17]=0xd2; peeps[0xd1*22]=2; set_peep_word(0xd1,6,3); set_peep_word(0xd1,10,-64);
    view_who=0xd2; draw_original(16,16);
    assert(no_sprites==2 && A4_WORD(0x9a2a)==A4_WORD(0x9a22)+8 && A4_WORD(0x9a2e)==0x44 && A4_WORD(0x9a30)==0xd2);
    /* A shaken screen skips _move_sprite (asm:15842-15849). */
    view_who=0; core_shake_dy=2; no_sprites=5; draw_original(16,16); assert(no_sprites==5); core_shake_dy=0;
    funny_clear(); view_who=0;
}

static void test_special_sprites(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    if (!getenv("POPULOUS_TEST_REAL_DATA")) assert(game_assets->sprite_size==143*256);
    test_special_sprite_frames(); test_special_sprite_order_and_guards();
    /* Even on a larger valid sheet, selected specials are bounded to 143. */
    size_t original_size=game_assets->sprite_size;
    game_assets->sprite_size=GFX_SPRITE_PIXELS;
    memset(game_assets->sprite_indices+143*256,14,4*256);
    memset(game_assets->sprite_keep+143*256,0,4*256);
    sprite_case(5,0,143,0,0,1,15,0,0,0,-1,192,56);
    sprite_case(0x42,0,143,0,0,1,15,0,0,0,-1,192,56);
    game_assets->sprite_size=original_size;
    _closeall(); SDL_Quit();
}

static unsigned scripted_calls;
static uint16_t scripted_value;
static uint16_t scripted_random(void)
{
    ++scripted_calls;
    return scripted_value;
}

static const uint16_t *scripted_sequence;
static size_t scripted_length, scripted_position;
static uint16_t sequence_random(void)
{
    assert(scripted_position < scripted_length);
    return scripted_sequence[scripted_position++];
}

static void test_original_generator(void)
{
    static const uint16_t clear_values[] = {5, 9, 7, 8};
    scripted_sequence = clear_values; scripted_length = 4; scripted_position = 0;
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    original_clear(sequence_random, 1235);
    assert(scripted_position == 4);
    assert(A4_WORD(0x82d0) == 2 && A4_WORD(0x82ce) == 7);
    assert(A4_WORD(0x82fe) == 1 && A4_WORD(0x82fc) == 5);
    assert(core_mana(0) == 399 && core_mana(1) == 399);
    scripted_position = 0;
    original_clear(sequence_random, 1236);
    assert(core_mana(1) == 2700);
    static const uint16_t edge_values[] = {63,63,4,8,2,4,2,4,2,4,2,4,2,4};
    scripted_sequence = edge_values; scripted_length = 14; scripted_position = 0;
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    original_make_thing(2, 4, sequence_random);
    assert(scripted_position == 14 && core_vertex(64,64) == 6);
    assert(core_vertex(63,63) == 5);
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    assert(original_raise_point(-1, 0) == 0);
    for (int repeat = 1; repeat <= 8; ++repeat)
        assert(original_raise_point(64, 64) == repeat);
    assert(original_raise_point(64, 64) == 8);
    assert(core_vertex(63, 63) == 7 && core_vertex(62, 64) == 6);
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    for (int repeat = 0; repeat < 5; ++repeat) original_raise_point(0, 0);
    scripted_calls = scripted_value = 0;
    original_make_thing(2, 4, scripted_random);
    assert(scripted_calls == 2 && core_vertex(0, 0) == 6);
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    scripted_calls = 0;
    original_make_thing(2, 4, scripted_random);
    assert(scripted_calls == 12 && core_vertex(0, 0) == 6);
    memset(map_blk, 0, 4096);
    map_blk[0] = 47; map_bk2[0] = 51;
    _make_map();
    assert(map_blk[0] == 47 && map_bk2[0] == 51);
    assert(map_alt[0] == 5); /* protected block does not lower flat altitude */
    memset(A4_DISP(0xae86), 0, 65 * 65 * 2);
    _make_map();
    assert(map_blk[0] == 0 && map_bk2[0] == 0);
    for (int row = 0; row <= 64; ++row)
        for (int column = 0; column <= 64; ++column) core_set_vertex(column, row, 2);
    map_blk[0] = 47;
    _make_map();
    assert(map_blk[0] == 47 && map_alt[0] == 2);
    memset(map_blk, 0, 4096);
    scripted_calls = 0;
    original_woods(scripted_random);
    assert(scripted_calls == 1364);
    map_blk[0] = 15;
    scripted_calls = 0;
    original_woods(scripted_random);
    assert(scripted_calls == 1365 && map_blk[0] == 47);
    memset(map_blk, 15, 4096);
    scripted_value = 1; scripted_calls = 0;
    original_woods(scripted_random);
    assert(scripted_calls == 2024 && map_blk[130] == 48 && map_bk2[130] == 51);
}

static void test_original_placement(void)
{
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    map_blk[128] = map_blk[129] = map_blk[3968] = map_blk[3967] = 15;
    assert(original_place_people(2, 2));
    assert(no_peeps == 4 && core_leader(0) == 2 && core_leader(1) == 4);
    const int positions[] = {128, 129, 3968, 3967};
    for (int index = 0; index < 4; ++index) {
        assert(peep_word(index, 8) == positions[index]);
        assert(peep_word(index, 4) == 45 && peep_word(index, 12) == 0);
        assert(map_who[positions[index]] == index + 1);
    }
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    map_blk[0] = map_blk[1] = map_blk[4095] = map_blk[4094] = 1;
    assert(original_place_people(2, 2));
    assert(peep_word(0, 8) == 0 && peep_word(1, 8) == 1);
    assert(peep_word(2, 8) == 4095 && peep_word(3, 8) == 4094);
    assert(core_leader(0) == 2 && core_leader(1) == 0); /* original fallback order */
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    map_blk[128] = 15;
    assert(original_place_people(1, 1)); /* asm preferred scans do not filter occupancy */
    assert(no_peeps == 2 && core_leader(0) == 1 && core_leader(1) == 2);
    assert(peep_word(0,8) == 128 && peep_word(1,8) == 128 && map_who[128] == 2);
    assert(peep_word(0,4) == 45 && peep_word(1,4) == 45);
    assert(peep_owner(0) == 0 && peep_owner(1) == 1);
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    map_blk[128] = map_blk[129] = 15;
    assert(original_place_people(2, 2));
    const int overlapping_positions[] = {128,129,129,128};
    for (int index = 0; index < 4; ++index) {
        assert(peep_word(index,8) == overlapping_positions[index]);
        assert(peep_word(index,4) == 45 && peep_owner(index) == (index >= 2));
    }
    assert(map_who[128] == 4 && map_who[129] == 3);
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    map_blk[128] = 15; map_blk[0] = map_blk[4095] = 1;
    assert(original_place_people(2, 2)); /* fallback alone skips occupied cells */
    assert(peep_word(0,8) == 128 && peep_word(1,8) == 0);
    assert(peep_word(2,8) == 128 && peep_word(3,8) == 4095);
    assert(map_who[128] == 3 && map_who[0] == 2 && map_who[4095] == 4);
    assert(core_leader(0) == 2 && core_leader(1) == 0);
    assert(!original_place_person(0,-1,0) && !original_place_person(0,4096,0));
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    memset(map_blk, 15, 4096);
    assert(!original_place_people(209, 1));
    assert(!original_place_people(0, 1)); /* unsupported zero-count original loop */
    memset(map_blk, 0, 4096);
    assert(!original_place_people(1, 1));
}

static void test_original_selection(void)
{
    StartSelection selection;
    char *arguments[] = {"populous", "--level", "494", NULL};
    assert(parse_start(3, arguments, &selection));
    assert(selection.kind == START_ORIGINAL && selection.number == 494);
    arguments[2] = "495"; assert(!parse_start(3, arguments, &selection));
    arguments[2] = "-1"; assert(!parse_start(3, arguments, &selection));
    arguments[2] = "0"; assert(parse_start(3, arguments, &selection));
    assert(!parse_start(2, arguments, &selection));
    assert(parse_start(1, arguments, &selection));
    assert(selection.kind == START_SYNTHETIC && selection.number == 0);
}

static void write_level_fixture(unsigned friendly, unsigned enemy)
{
    assert(!getenv("POPULOUS_TEST_REAL_DATA"));
    uint8_t bytes[LEVEL_FILE_SIZE];
    for (int index = 0; index < LEVEL_RECORD_COUNT; ++index) {
        uint8_t record[] = {7, 9, 135, 63, 19, 2, friendly, enemy, 0xab, 0xcd};
        memcpy(bytes + 10 * index, record, 10);
    }
    char path[512]; snprintf(path, sizeof path, "%s/level.dat", test_data_directory);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes && !fclose(file));
}

static void assert_original_world(void)
{
    for (int row = 0; row <= 64; ++row)
        for (int column = 0; column <= 64; ++column) {
            int height = core_vertex(column, row);
            assert(height >= 0 && height <= 8);
            if (column < 64) assert(abs(height - core_vertex(column + 1, row)) <= 1);
            if (row < 64) assert(abs(height - core_vertex(column, row + 1)) <= 1);
        }
    /* Independent assembly oracle: preferred flat-cell lists do not exclude
     * prior occupants; only fallback land lists do (asm:7688-7856). */
    int expected_positions[208], expected_leaders[2]={0,0}, count=0;
    uint8_t expected_who[4096]={0};
    for (int owner=0; owner<2; ++owner) {
        int requested=owner ? current_start_pop_enemy : current_start_pop_you;
        int candidates[4096], available=0;
        for (int cell=128; cell<=(owner ? 3968 : 4095); ++cell)
            if (map_blk[cell]==15) candidates[available++]=cell;
        int preferred=requested < available ? requested : available;
        for (int index=0; index<preferred; ++index) {
            int cell=candidates[owner ? available-1-index : index];
            expected_positions[count++]=cell; expected_who[cell]=count;
        }
        if (preferred==requested) expected_leaders[owner]=count;
        available=0;
        for (int cell=owner ? 1 : 0; cell<4096; ++cell)
            if (map_blk[cell] && !expected_who[cell]) candidates[available++]=cell;
        assert(available >= requested-preferred);
        for (int index=0; index<requested-preferred; ++index) {
            int cell=candidates[owner ? available-1-index : index];
            expected_positions[count++]=cell; expected_who[cell]=count;
        }
        if (!owner) expected_leaders[owner]=count;
    }
    assert(no_peeps==count && core_leader(0)==expected_leaders[0] && core_leader(1)==expected_leaders[1]);
    assert(!memcmp(map_who,expected_who,sizeof expected_who));
    for (int cell = 0; cell < 4096; ++cell) {
        assert(map_blk[cell] <= 49 && map_alt[cell] <= 8);
        assert(!map_bk2[cell] || (map_bk2[cell] >= 50 && map_bk2[cell] <= 52));
    }
    for (int index = 0; index < no_peeps; ++index) {
        assert(peep_word(index,8)==expected_positions[index]);
        assert(peep_word(index,4)==45 && peep_word(index,12)==0);
        assert(peeps[index*22]==2 && peep_owner(index)==(index >= current_start_pop_you));
        assert(peeps[index*22+2]==1 && peeps[index*22+3]==1);
        assert(!peep_word(index,6) && !peep_word(index,10));
        for (int byte=14; byte<22; ++byte) assert(!peeps[index*22+byte]);
    }
}

static void test_original_reset(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    assert(game_reset(0)); /* no level.dat required */
    uint8_t saved[sizeof prg_a4_area];
    memcpy(saved, prg_a4_area, sizeof saved);
    GraphicsAssets *assets = game_assets;
    assert(!game_reset_start((StartSelection){START_ORIGINAL, 0}));
    assert(!memcmp(saved, prg_a4_area, sizeof saved) && game_assets == assets);
    write_level_fixture(1, 1);
    const int levels[] = {0, 4, 5, 494};
    const unsigned seeds[] = {0xabcd, 0xabd1, 0xabce, 0xabd3};
    for (int index = 0; index < 4; ++index) {
        assert(game_reset_start((StartSelection){START_ORIGINAL, levels[index]}));
        assert_original_world();
        assert(in_conquest == levels[index] * 5 && A4_WORD(0xae7e) == levels[index] * 5);
        assert((uint16_t)start_seed == seeds[index] && (uint16_t)conq_08_seed == 0xabcd);
        assert(LAB_516DE == 7 && LAB_516E2 == 9 && LAB_516E0 == 1087 && LAB_516B2 == 511);
        assert(game_mode == 19 && ground_in == 2 && no_peeps == 2);
        assert(core_mana(1) == (levels[index] == 494 ? 2700 : 399));
        int leader_cell = peep_word(core_leader(0) - 1, 8);
        assert(leader_cell % 64 >= xoff && leader_cell % 64 < xoff + 8);
        assert(leader_cell / 64 >= yoff && leader_cell / 64 < yoff + 8);
        memcpy(saved, prg_a4_area, sizeof saved);
        SDL_Event reset = {.type = SDL_KEYDOWN}; reset.key.keysym.sym = SDLK_r;
        game_handle_event(&reset);
        assert(!memcmp(saved, prg_a4_area, sizeof saved));
    }
    assets = game_assets;
    pause = 1;
    diagnostic_view = 1;
    core_tool = 1;
    core_battles = 17;
    memcpy(saved, prg_a4_area, sizeof saved);
    char *saved_assets_directory = strdup(getenv("POPULOUS_ASSET_DIR"));
    assert(saved_assets_directory);
    assert(!setenv("POPULOUS_ASSET_DIR", "/populous-test-missing-original-assets", 1));
    assert(!game_reset_start((StartSelection){START_ORIGINAL, 0}));
    assert(!memcmp(saved, prg_a4_area, sizeof saved) && game_assets == assets);
    assert(prg_a4 == prg_a4_area + 32768 && pause && diagnostic_view == 1);
    assert(core_tool == 1 && core_battles == 17);
    assert(current_start.kind == START_ORIGINAL && current_start.number == 494);
    assert(!setenv("POPULOUS_ASSET_DIR", saved_assets_directory, 1));
    free(saved_assets_directory);
    write_level_fixture(209, 1);
    assert(!game_reset_start((StartSelection){START_ORIGINAL, 0}));
    assert(!memcmp(saved, prg_a4_area, sizeof saved) && game_assets == assets);
    assert(current_start.kind == START_ORIGINAL && current_start.number == 494);
    char path[512]; snprintf(path, sizeof path, "%s/level.dat", test_data_directory);
    assert(!unlink(path));
    SDL_Event reset = {.type = SDL_KEYDOWN}; reset.key.keysym.sym = SDLK_r;
    game_handle_event(&reset);
    assert(!memcmp(saved, prg_a4_area, sizeof saved) && !prg_quit && game_assets == assets);
    assert(game_reset(0));
    _closeall(); SDL_Quit();
}

static void assert_live_occupancy(void)
{
    int towns[4096]={0};
    for (int cell=0; cell<4096; ++cell) if (map_who[cell]) {
        int person=map_who[cell]-1;
        /* A claim is the peep's cell or its departure cell pos-(+10), the
         * two cells _zero_population releases (asm:5662-5680). */
        assert(person<no_peeps && peep_word(person,4)>0);
        assert(peep_word(person,8)==cell ||
               (int16_t)(peep_word(person,8)-peep_word(person,10))==cell);
    }
    for (int person=0; person<no_peeps; ++person) if (peep_word(person,4)>0) {
        int position=peep_word(person,8);
        assert(position>=0 && position<4096);
        if (peeps[person*22]&1) assert(++towns[position]==1);
        if (peeps[person*22]&8) {
            int partner=peep_word(person,6);
            assert(partner>=0 && partner<no_peeps && partner!=person);
            assert(peep_word(partner,4)>0 && (peeps[partner*22]&8));
            assert(peep_word(partner,6)==person && peep_owner(partner)!=peep_owner(person));
        }
    }
}

static void assert_mailbox_defaults(int speed);
static void test_all_original_levels(const char *directory)
{
    LevelTable table; char error[160];
    assert(level_data_load(directory, &table, error, sizeof error));
    assert(!setenv("POPULOUS_DATA_DIR", directory, 1));
    char assets_directory[4096];
    snprintf(assets_directory, sizeof assets_directory, "%s/..", directory);
    assert(!setenv("POPULOUS_ASSET_DIR", assets_directory, 1));
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    for (unsigned number = 0; number < LEVEL_COUNT; ++number) {
        LevelStart start;
        assert(level_data_select(&table, number, &start));
        assert(game_reset_start((StartSelection){START_ORIGINAL, (int)number}));
        assert(start.record.speed>0);
        assert_mailbox_defaults(start.record.speed);
        uint8_t saved[sizeof prg_a4_area];
        memcpy(saved, prg_a4_area, sizeof saved);
        assert_original_world();
        assert(no_peeps == start.record.your_count + start.record.enemy_count);
        assert(in_conquest == start.internal_level && (uint16_t)start_seed == start.initial_seed);
        game_render();
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
        for (int tick = 0; tick < 100; ++tick) { game_tick(); assert_live_occupancy(); }
        uint8_t after_ticks[sizeof prg_a4_area]; memcpy(after_ticks,prg_a4_area,sizeof after_ticks);
        game_render();
        assert(!memcmp(after_ticks,prg_a4_area,sizeof after_ticks));
        SDL_Event reset = {.type = SDL_KEYDOWN}; reset.key.keysym.sym = SDLK_r;
        game_handle_event(&reset);
        assert_original_world();
        /* _clear_map leaves 0x99fc, good_pop and _stats+0x14/+0x16/+0x22/
         * +0x26/+0x2a (asm:1023-1188): those keep their post-tick values. */
        static const int kept[][2] = {{0x99fc,2},{0x99fe,4},{0x82ca,2},{0x82cc,2},{0x82d8,4},{0x82dc,4},{0x82e0,4}};
        for (int owner = 0; owner < 2; ++owner) for (int field = 0; field < 7; ++field) {
            int offset = (int16_t)(kept[field][0] + owner*(kept[field][0] >= 0x99fc ? 16 : 46)) + 32768;
            memcpy(saved+offset, after_ticks+offset, (size_t)kept[field][1]);
        }
        assert(!memcmp(saved,prg_a4_area,sizeof saved));
        assert(game_turn == 0 && current_start.number == (int)number);
    }
    _closeall(); SDL_Quit();
    assert(!prg_screen && !game_assets && live_allocations==0 && !SDL_WasInit(0));
    puts("Original starts: all 495 PASS (placement/ticks/render/reset; no tick-parity claim)");
}

static unsigned swamp_draws;
static uint16_t swamp_values[60];
static uint16_t swamp_random(void)
{
    assert(swamp_draws < 60);
    return swamp_values[swamp_draws++];
}
static void swamp_fixture(void)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    memset(map_blk,15,4096);
    memset(map_bk2,42,4096);
    memset(map_alt,2,4096);
    for (int cell=0;cell<4096;++cell) A4_WORD(0xff88+cell*2)=91;
    A4_LONG(0x8496)=5000;
    A4_LONG(0x9a02)=A4_LONG(0x9a12)=10000;
    A4_WORD(0x82c4)=A4_WORD(0x82c4+46)=0x10;
    core_result=0; player=0; seed=123;
    swamp_draws=0;
    for (int draw=0;draw<60;++draw) swamp_values[draw]=3;
}
static void test_swamp_cast(void)
{
    /* Catch wrong gate boundaries, editor bypass and mutation on rejection. */
    for (int side=0;side<2;++side) for (int gate=0;gate<11;++gate)
    for (int paint=0;paint<2;++paint) {
        if (paint && gate>=6 && gate<=9) continue;   /* asm:8297 bypass, test_paint_map_powers */
        swamp_fixture(); paint_map=paint;
        int owner=side,column=20,row=20;
        if (gate==0) owner=-1;
        if (gate==1) owner=2;
        if (gate==2) column=-1;
        if (gate==3) column=256;
        if (gate==4) row=-1;
        if (gate==5) row=256;
        if (gate==6) A4_LONG(0x9a02+side*16)=4999;
        if (gate==7) A4_WORD(0x82c4+side*46)=0xffef;
        if (gate==8) pause=1;
        if (gate==9) A4_WORD(0xae80)=1;
        if (gate==10) core_result=1;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_swamp(owner,column,row,swamp_random));
        assert(swamp_draws==0 && a4_same(before));
    }
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int mana=5000;mana<=5001;++mana) for (int paint=0;paint<2;++paint) {
        swamp_fixture(); player=local; paint_map=paint;
        A4_LONG(0x9a02+owner*16)=mana;
        assert(core_swamp(owner,255,255,swamp_random));
        assert(core_mana(owner)==(paint ? mana : mana-5000) && swamp_draws==60);   /* asm:8297 */
        assert(A4_LONG(0xae72)==(owner==local ? 50u : 0u));
        for (int cell=0;cell<4096;++cell) assert(map_blk[cell]==15);
    }
    /* Distinct column-first offsets, every accepted block, duplicate, occupied
     * and ineligible attempts. Only block/mana/score may change. */
    swamp_fixture();
    int cells[5]={19*64+17,20*64+18,21*64+19,22*64+20,23*64+21};
    map_blk[cells[0]]=15; map_blk[cells[1]]=31;
    map_blk[cells[2]]=32; map_blk[cells[3]]=66;
    map_blk[cells[4]]=47; map_who[20*64+20]=7;
    for (int attempt=0;attempt<5;++attempt) {
        swamp_values[attempt*2]=attempt;
        swamp_values[attempt*2+1]=attempt+2;
    }
    swamp_values[10]=0; swamp_values[11]=2; /* Duplicate first placement. */
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    assert(core_swamp(0,20,20,swamp_random));
    assert(swamp_draws==60 && core_mana(0)==5000 && A4_LONG(0xae72)==50);
    for (int index=0;index<4;++index) assert(map_blk[cells[index]]==53);
    assert(map_blk[cells[4]]==47 && map_blk[20*64+20]==15);
    map_blk[cells[0]]=15; map_blk[cells[1]]=31;
    map_blk[cells[2]]=32; map_blk[cells[3]]=66;
    A4_LONG(0x9a02)=10000; A4_LONG(0xae72)=0;
    assert(a4_same(before));
    static const int corners[4][2]={{0,0},{63,0},{0,63},{63,63}};
    for (int corner=0;corner<4;++corner) {
        swamp_fixture();
        for (int draw=0;draw<60;++draw) swamp_values[draw]=draw%2 ? 6 : 0;
        swamp_values[0]=3; swamp_values[1]=3;
        int column=corners[corner][0],row=corners[corner][1];
        assert(core_swamp(1,column,row,swamp_random));
        assert(swamp_draws==60 && map_blk[row*64+column]==53);
        int placed=0;
        for (int cell=0;cell<4096;++cell) placed+=map_blk[cell]==53;
        assert(placed==(corner==1 ? 2 : 1));
    }
}

static void swamp_walker(int owner, int flags, int frame)
{
    swamp_fixture(); no_peeps=1;
    peeps[0]=flags; peeps[1]=owner; peeps[2]=1; peeps[3]=1;
    set_peep_word(0,4,100); set_peep_word(0,8,1300);
    set_peep_word(0,10,1); set_peep_word(0,12,frame);
    map_blk[1300]=53; map_who[1300]=1;
    A4_WORD(0x2fb0)=3;
}
static void test_swamp_hazard(void)
{
    /* Catch early death, navigation/attrition before death, wrong owner/mode,
     * and accidental clearing of another occupant. _zero_population
     * (asm:4069-4073 -> 5671-5680) does clear the own previous-cell claim. */
    for (int owner=0;owner<2;++owner) for (int bottomless=0;bottomless<2;++bottomless)
    for (int fatal=0;fatal<2;++fatal) for (int owned=0;owned<2;++owned) {
        swamp_walker(owner,2,0);
        game_mode=(bottomless ? 2 : 0) | (fatal ? GAME_MODE_WATER_FATAL : 0);
        core_leader(owner)=1; core_target(owner)=777; view_who=1;
        A4_WORD(owner ? 0xae76 : 0xae78)=777;
        map_who[1300]=owned ? 1 : 2; map_who[1299]=1;
        int16_t ai_seed=123;
        for (int frame=0;frame<7;++frame) {
            uint8_t scratch[sizeof prg_a4_area]; memcpy(scratch,prg_a4_area,sizeof scratch);
            if (!frame) ai_seed=expect_ai_fixture_picks(scratch); /* first pass only */
            _move_peeps();
            assert(peep_word(0,4)==100 && peep_word(0,8)==1300);
            assert(peep_word(0,12)==frame+1 && map_blk[1300]==53);
            assert(A4_WORD(0x2fac)==0 && seed==ai_seed);
        }
        _move_peeps();
        assert(peep_word(0,4)==0 && peeps[0]==0 && peep_word(0,8)==1300);
        assert(map_blk[1300]==(bottomless ? 53 : 15) && A4_WORD(0x2fac)==66);
        assert(map_who[1300]==(owned ? 0 : 2) && !map_who[1299]);
        assert(core_leader(owner)==0 && core_target(owner)==1300 && view_who==0);
        assert(A4_WORD(owner ? 0xae76 : 0xae78)==1300);
        assert(core_target(1-owner)==0 && seed==ai_seed);
        assert(map_alt[1300]==2 && map_bk2[1300]==42 && A4_WORD(0xff88+1300*2)==91);
    }
    static const int flags[]={0x22,0x42,8,1};
    for (unsigned index=0;index<sizeof flags/sizeof flags[0];++index) {
        swamp_walker(0,flags[index],7); set_peep_word(0,6,1);
        _move_peeps();
        assert(peep_word(0,4)>0 && A4_WORD(0x2fac)==0 && map_blk[1300]==53);
    }
    swamp_walker(0,0x42,7); set_peep_word(0,6,15);
    memset(map_blk,47,4096); map_blk[1300]=53;
    core_target(0)=1300;
    _move_peeps();
    assert(peep_word(0,4)==97 && map_blk[1300]==53 && A4_WORD(0x2fac)==0);
    /* Nonleader death must preserve unrelated references. */
    swamp_walker(1,2,7); core_leader(1)=2; core_target(1)=777; view_who=2;
    _move_peeps();
    assert(core_leader(1)==2 && core_target(1)==777 && view_who==2);
}

static void test_swamp_input(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_s;
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN}; click.button.button=SDL_BUTTON_LEFT;
    for (int tool=0;tool<2;++tool) for (int diagnostic=0;diagnostic<2;++diagnostic)
    for (int mana=4999;mana<=5001;++mana) {
        assert(game_reset(0)); core_tool=tool; diagnostic_view=diagnostic;
        SDL_SetWindowSize(prg_screen->window,960,768);
        A4_LONG(0x9a02)=mana; A4_WORD(0x82c4)=0x10;
        if (diagnostic) { xoff=0; yoff=0; click.button.x=81*3; click.button.y=81*3; }
        else {
            xoff=16; yoff=16;
            int sx,sy; assert(graphics_vertex(4,4,core_vertex(20,20),&sx,&sy));
            click.button.x=sx*3; click.button.y=sy*3;
        }
        game_handle_event(&key);
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        click.button.button=SDL_BUTTON_RIGHT; game_handle_event(&click);
        assert(a4_same(before));
        click.button.button=SDL_BUTTON_LEFT; game_handle_event(&click);
        assert(core_mana(0)==(mana>5000 ? 1 : mana));
        assert(core_tool==tool);
        if (mana>5000) {
            assert(A4_LONG(0xae72)==50);
            for (int cell=0;cell<4096;++cell) if (map_blk[cell]==53)
                assert(cell%64>=16 && cell%64<=22 && cell/64>=16 && cell/64<=22);
            /* Consumed once: later clicks must use the underlying tool. */
            A4_LONG(0x9a02)=10000; A4_WORD(0x82c4)=1;
            core_leader(0)=1;
            game_handle_event(&click);
            assert(core_mana(0)==(tool ? 9800 : 9986));
        }
    }
    /* Selection at 5001, execution after mana drops to exactly 5000. */
    assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,640,512);
    A4_LONG(0x9a02)=5001; A4_WORD(0x82c4)=0x10;
    game_handle_event(&key); A4_LONG(0x9a02)=5000;
    iso_click_vertex(20,20,SDL_BUTTON_LEFT);
    assert(core_mana(0)==0 && A4_LONG(0xae72)==50);
    /* Player-side validation and source selection during war. */
    for (int owner=-1;owner<=2;++owner) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,640,512);
        player=owner; A4_LONG(0x9a02)=A4_LONG(0x9a12)=6000;
        A4_WORD(0x82c4)=A4_WORD(0x82c4+46)=0x10; A4_WORD(0xae80)=1;
        game_handle_event(&key); A4_WORD(0xae80)=0;
        iso_click_vertex(20,20,SDL_BUTTON_LEFT);
        assert(A4_LONG(0xae72)==(owner==0 || owner==1 ? 50u : 0u));
        assert(core_mana(0)==(owner==0 ? 1000 : 6000));
        assert(core_mana(1)==(owner==1 ? 1000 : 6000));
    }
    /* Mana/permission rejection consumes the target; war blocks before clear. */
    for (int failure=0;failure<3;++failure) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,640,512);
        A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)=0x11;
        game_handle_event(&key);
        if (failure==0) A4_LONG(0x9a02)=4999;
        if (failure==1) A4_WORD(0x82c4)=1;
        if (failure==2) A4_WORD(0xae80)=1;
        uint16_t old_seed=seed;
        iso_click_vertex(20,20,SDL_BUTTON_LEFT);
        assert(seed==old_seed && A4_LONG(0xae72)==0 && core_vertex(20,20)==1);
        assert(core_swamp_selected==(failure==2));
        A4_LONG(0x9a02)=10000; A4_WORD(0x82c4)=0x11; A4_WORD(0xae80)=0;
        iso_click_vertex(20,20,SDL_BUTTON_LEFT);
        if (failure==2) {
            assert(core_swamp_selected==0);
            assert(core_vertex(20,20)==1 && core_mana(0)==5000 && A4_LONG(0xae72)==50);
        } else assert(core_vertex(20,20)==2 && core_mana(0)==9986);
    }
    /* Repeats/missing spell cannot arm outside the editor. With _paint_map
     * the missing spell still arms (asm:2031) and casts free; the plain click
     * raises free (asm:7189,7206). */
    for (int paint=0;paint<2;++paint) for (int missing=0;missing<2;++missing) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,640,512);
        paint_map=paint; A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)=missing ? 1 : 0x11;
        key.key.repeat=!missing; game_handle_event(&key); key.key.repeat=0;
        iso_click_vertex(20,20,SDL_BUTTON_LEFT);
        if (!paint) assert(core_vertex(20,20)==2 && core_mana(0)==5986 && A4_LONG(0xae72)==0);
        else if (!missing) assert(core_vertex(20,20)==2 && core_mana(0)==6000 && A4_LONG(0xae72)==0);
        else assert(core_vertex(20,20)==1 && core_mana(0)==6000 && A4_LONG(0xae72)==50 && !core_swamp_selected);
    }
    /* HUD/toolbar/minimap clicks don't consume an armed target. Pause and
     * outcomes retain their existing click guard; S itself may select. */
    for (int blocked=0;blocked<3;++blocked) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,640,512);
        A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)=0x11;
        pause=blocked==1; core_result=blocked==2; game_handle_event(&key);
        click.button.x=100*2; click.button.y=220*2; game_handle_event(&click);
        click.button.x=20*2; click.button.y=120*2; game_handle_event(&click);
        if (blocked) iso_click_vertex(20,20,SDL_BUTTON_LEFT);
        assert(core_mana(0)==6000 && A4_LONG(0xae72)==0);
        pause=0; core_result=0;
        iso_click_vertex(20,20,SDL_BUTTON_LEFT);
        assert(core_mana(0)==1000 && A4_LONG(0xae72)==50);
    }
    /* Byte wrap, not clamping: vertex zero spends and draws but places none. */
    for (int diagnostic=0;diagnostic<2;++diagnostic) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,640,512);
        A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)=0x10;
        game_handle_event(&key); diagnostic_view=diagnostic; xoff=yoff=0;
        uint16_t old_seed=seed;
        for (int draw=0;draw<60;++draw) ___newrand();
        uint16_t expected_seed=seed; seed=old_seed;
        if (diagnostic) { click.button.x=0; click.button.y=0; game_handle_event(&click); }
        else iso_click_vertex(0,0,SDL_BUTTON_LEFT);
        assert(core_mana(0)==1000 && seed==expected_seed && A4_LONG(0xae72)==50);
        for (int cell=0;cell<4096;++cell) assert(map_blk[cell]!=53);
    }
    assert(game_reset(0)); A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)=0x11;
    game_handle_event(&key);
    /* Failed reset preserves target selection transactionally. */
    char *directory=strdup(getenv("POPULOUS_ASSET_DIR") ? getenv("POPULOUS_ASSET_DIR") : POPULOUS_DEFAULT_ASSET_DIR);
    assert(directory && !setenv("POPULOUS_ASSET_DIR","/nonexistent-populous-assets",1));
    assert(!game_reset(0));
    assert(!setenv("POPULOUS_ASSET_DIR",directory,1)); free(directory);
    iso_click_vertex(20,20,SDL_BUTTON_LEFT); assert(core_mana(0)==1000);
    A4_LONG(0x9a02)=6000; game_handle_event(&key);
    assert(game_reset(0)); A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)=0x11;
    iso_click_vertex(20,20,SDL_BUTTON_LEFT);
    assert(core_mana(0)==5986 && core_vertex(20,20)==2);
    _closeall(); SDL_Quit();
}

/* Click the centre of power icon (icon,line) in a 960x768 window. */
static void power_icon_click(int icon, int line, int button)
{
    int c=0x60+16*icon+8, r=0x90+16*line+8;
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN};
    click.button.button=button;
    click.button.x=(c-r+64)*3; click.button.y=(c+r)/2*3;
    game_handle_event(&click);
}

static void test_power_toolbar(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    /* _zoom_map asm:1942-2042: icon, line -> opcode, x, y (-1: viewport). */
    static const int posts[][5]={{0,0,14,0,4},{1,0,14,0,3},{1,1,6,-1,-1},
        {2,0,3,-1,-1},{2,1,14,0,5}};
    for (int owner=0;owner<2;++owner) for (int p=0;p<5;++p)
    for (int button=0;button<2;++button) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,960,768);
        player=owner; xoff=20; yoff=30;
        uint8_t *mailbox=A4_DISP(0x82b6+owner*46), *other=A4_DISP(0x82b6+(1-owner)*46);
        memset(mailbox,0,3); memset(other,0,3);
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        power_icon_click(posts[p][0],posts[p][1],button ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT);
        assert(mailbox[0]==posts[p][2]);
        assert(mailbox[1]==(posts[p][3]<0 ? 20 : posts[p][3]));
        assert(mailbox[2]==(posts[p][4]<0 ? 30 : posts[p][4]));
        assert(!other[0] && !other[1] && !other[2]);
        /* Only the three mailbox bytes change; camera and swamp stay put. */
        memcpy(before+(mailbox-prg_a4_area),mailbox,3);
        assert(a4_same(before));
        assert(xoff==20 && yoff==30 && !core_swamp_selected);
    }
    /* Mailbox posts reach the power routines; costs are paid by the consumer. */
    static const struct { int icon, line; uint16_t permission; int cost; } casts[]={
        {0,0,0x80,0x84a2},{1,0,0x100,0x84a6},{1,1,0x40,0x849e},{2,0,0x08,0x8492}};
    for (int k=0;k<4;++k) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,960,768);
        xoff=20; yoff=30; A4_LONG(0x9a02)=200000; A4_WORD(0x82c4)=casts[k].permission;
        power_icon_click(casts[k].icon,casts[k].line,SDL_BUTTON_LEFT);
        assert(core_mana(0)==200000);
        core_consume_terrain_mailboxes();
        assert(core_mana(0)==200000-(int32_t)A4_LONG(casts[k].cost));
        assert(!A4_DISP(0x82b6)[0]);
    }
    /* Missing permission: posted, consumed, refused by the routine. */
    assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,960,768);
    A4_LONG(0x9a02)=200000; A4_WORD(0x82c4)=0;
    power_icon_click(0,0,SDL_BUTTON_LEFT); core_consume_terrain_mailboxes();
    assert(core_mana(0)==200000);
    /* Editor mode suppresses only the Armageddon post (asm:1979). */
    assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,960,768);
    paint_map=1; memset(A4_DISP(0x82b6),0,3);
    power_icon_click(1,0,SDL_BUTTON_LEFT); assert(!A4_DISP(0x82b6)[0]);
    power_icon_click(0,0,SDL_BUTTON_LEFT); assert(A4_DISP(0x82b6)[0]==14);
    /* Swamp arms the map target only above cost with bit 4, or in the editor. */
    for (int gate=0;gate<5;++gate) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,960,768);
        int32_t cost=(int32_t)A4_LONG(0x8496);
        A4_LONG(0x9a02)=gate==0 ? cost : cost+1; A4_WORD(0x82c4)=gate==1 ? 0xffef : 0x10;
        if (gate==2) { A4_LONG(0x9a02)=0; A4_WORD(0x82c4)=0; paint_map=1; }
        if (gate==3) player=2;
        if (gate==4) pause=1;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        int16_t old_pointer=pointer;
        power_icon_click(2,2,SDL_BUTTON_LEFT);
        assert(core_swamp_selected==(gate==2));
        /* Only the armed swamp writes _pointer = 5 (asm:2042). */
        assert(pointer==(gate==2 ? 5 : old_pointer)); pointer=old_pointer;
        assert(a4_same(before));
    }
    /* Paused, finished, invalid player and diagnostic view post nothing;
     * non-power cells keep their former (camera/world) handling. */
    for (int gate=0;gate<5;++gate) for (int p=0;p<5;++p) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,960,768);
        xoff=20; yoff=30;
        if (gate==0) pause=1;
        if (gate==1) core_result=1;
        if (gate==2) player=2;
        if (gate==3) player=-1;
        if (gate==4) diagnostic_view=1;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        power_icon_click(posts[p][0],posts[p][1],SDL_BUTTON_LEFT);
        if (gate!=4) assert(a4_same(before));
        else assert(!A4_DISP(0x82b6)[0] && !A4_DISP(0x82b6+46)[0]);
    }
    assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,960,768);
    memset(A4_DISP(0x82b6),0,3);
    power_icon_click(0,1,SDL_BUTTON_LEFT); power_icon_click(1,2,SDL_BUTTON_LEFT);
    power_icon_click(0,0,SDL_BUTTON_MIDDLE);
    assert(!A4_DISP(0x82b6)[0] && !core_swamp_selected);
    _closeall(); SDL_Quit();
}

/* Click the centre of cell (icon,line) of a toolbar block in a 960x768 window. */
static void toolbar_click(int c0, int r0, int icon, int line, int button)
{
    int c=c0+16*icon+8, r=r0+16*line+8;
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN};
    click.button.button=button;
    click.button.x=(c-r+64)*3; click.button.y=(c+r)/2*3;
    game_handle_event(&click);
}
#define grid_click(icon,line,button) toolbar_click(0x60,0x90,icon,line,button)
#define system_click(icon,line,button) toolbar_click(0x110,0x20,icon,line,button)

static void toolbar_fixture(void)
{
    assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,960,768);
    xoff=yoff=20; memset(A4_DISP(0x82b6),0,3); memset(A4_DISP(0x82b6+46),0,3);
    view_who=old_view_who=view_timer=0;
}

static void test_toolbar_icons(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    /* Tendency icons post _do_action y=1; the consumer installs the tendency. */
    static const int tendencies[][3]={{3,3,0},{4,3,1},{4,4,3},{5,3,2}};
    for (int k=0;k<4;++k) for (int gate=0;gate<2;++gate) {
        toolbar_fixture(); core_tendency(0)=7; pause=gate;
        grid_click(tendencies[k][0],tendencies[k][1],SDL_BUTTON_RIGHT);
        uint8_t *mailbox=A4_DISP(0x82b6);
        if (gate) { assert(!mailbox[0] && core_tendency(0)==7); continue; }
        assert(mailbox[0]==14 && mailbox[1]==tendencies[k][2] && mailbox[2]==1);
        core_consume_terrain_mailboxes();
        assert(core_tendency(0)==tendencies[k][2] && !mailbox[0]);
    }
    /* Mode icons: terrain/magnet act even while paused and disarm the swamp. */
    for (int paused=0;paused<2;++paused) {
        toolbar_fixture(); pause=paused;
        core_tool=0; core_swamp_selected=1; grid_click(6,2,SDL_BUTTON_LEFT);
        assert(core_tool==1 && !core_swamp_selected);
        core_swamp_selected=1; grid_click(6,1,SDL_BUTTON_LEFT);
        assert(core_tool==0 && !core_swamp_selected);
        /* Query (asm:2090-2133 mode 1) is a third tool and also disarms. */
        core_swamp_selected=1; grid_click(6,0,SDL_BUTTON_LEFT);
        assert(core_tool==2 && !core_swamp_selected);
    }
    /* Scroll pad: eight directions, clamped to 0..56. */
    for (int icon=3;icon<=5;++icon) for (int line=0;line<=2;++line) {
        if (icon==4 && line==1) continue;
        int dx=(icon==5)-(icon==3), dy=(line==2)-(line==0);
        toolbar_fixture(); pause=1;
        grid_click(icon,line,SDL_BUTTON_LEFT);
        assert(xoff==20+dx && yoff==20+dy);
        xoff=dx<0 ? 0 : 56; yoff=dy<0 ? 0 : 56; int ex=xoff, ey=yoff;
        grid_click(icon,line,SDL_BUTTON_LEFT);
        assert(xoff==ex && yoff==ey);
    }
    /* Centre recentres on view_who; no subject leaves the view alone. */
    toolbar_fixture(); assert(no_peeps>0);
    set_peep_word(0,8,30*64+40); view_who=1;
    grid_click(4,1,SDL_BUTTON_LEFT); assert(xoff==37 && yoff==27);
    view_who=0; xoff=yoff=20; grid_click(4,1,SDL_BUTTON_LEFT); assert(xoff==20 && yoff==20);
    /* Leader/magnet view: left prefers the leader and marks a temporary view. */
    toolbar_fixture(); set_peep_word(0,8,30*64+40); core_leader(0)=1; core_target(0)=10*64+12;
    view_who=5; grid_click(7,0,SDL_BUTTON_LEFT);
    assert(xoff==37 && yoff==27 && view_who==1 && old_view_who==5 && view_timer==10);
    grid_click(7,0,SDL_BUTTON_RIGHT); assert(xoff==9 && yoff==7 && view_who==1);
    core_leader(0)=0; xoff=yoff=20; grid_click(7,0,SDL_BUTTON_LEFT); assert(xoff==9 && yoff==7);
    core_target(0)=0; xoff=yoff=20; grid_click(7,0,SDL_BUTTON_RIGHT); assert(xoff==20 && yoff==20);
    grid_click(7,0,SDL_BUTTON_LEFT); assert(xoff==0 && yoff==0);
    /* Battle cycle wraps from view_fight+1 and terminates without battles. */
    toolbar_fixture();
    for (int person=0;person<no_peeps;++person) peeps[person*22]&=(uint8_t)~0x08;
    view_fight=(int16_t)(no_peeps-1); grid_click(7,1,SDL_BUTTON_LEFT);
    assert(xoff==20 && yoff==20 && view_fight==no_peeps-1);
    peeps[0]|=0x08; set_peep_word(0,4,5); set_peep_word(0,8,12*64+8);
    grid_click(7,1,SDL_BUTTON_LEFT);
    assert(view_fight==0 && xoff==5 && yoff==9 && view_who==1 && view_timer==10);
    /* Own-walker cycle from view_people+1: right needs type 1, left a heading
     * reference; both skip foreign or empty peeps and wrap past no_peeps. */
    toolbar_fixture(); player=0; assert(no_peeps>=2);
    for (int person=0;person<no_peeps;++person) peeps[person*22+1]=1;
    peeps[1]=0; peeps[0]=1; set_peep_word(0,4,3); set_peep_word(0,8,12*64+8);
    memset(peeps+14,0,4);
    view_people=0; grid_click(8,0,SDL_BUTTON_RIGHT);
    assert(view_people==0 && xoff==5 && yoff==9 && view_who==1 && view_timer==10);
    xoff=yoff=20; grid_click(8,0,SDL_BUTTON_LEFT); assert(xoff==20 && yoff==20);
    memcpy(peeps+14,"\x01\x00\x00\x00",4); grid_click(8,0,SDL_BUTTON_LEFT);
    assert(xoff==5 && yoff==9);
    set_peep_word(0,4,0); xoff=yoff=20; grid_click(8,0,SDL_BUTTON_RIGHT); assert(xoff==20);
    /* System icons: sound flags and pause toggle at any time. */
    toolbar_fixture(); A4_WORD(0x8250)=A4_WORD(0x8252)=0;
    system_click(0,3,SDL_BUTTON_LEFT); assert(A4_WORD(0x8250)==1 && !A4_WORD(0x8252));
    system_click(0,4,SDL_BUTTON_RIGHT); assert(A4_WORD(0x8252)==1);
    system_click(0,3,SDL_BUTTON_LEFT); assert(!A4_WORD(0x8250));
    A4_WORD(0x9a1a)=1; system_click(1,3,SDL_BUTTON_LEFT); assert(pause==1 && !A4_WORD(0x9a1a));
    core_result=1; system_click(1,3,SDL_BUTTON_LEFT); assert(pause==0); core_result=0;
    /* Options, game options and message post their _do_action codes. The
     * consumer now opens _options (y=7) and _game_options (y=8), see
     * test_requesters; the message requester (y=2) stays inert. */
    static const int requests[][4]={{0,0,7,1},{1,1,8,0},{2,2,2,0}};
    for (int k=0;k<3;++k) {
        toolbar_fixture();
        system_click(requests[k][0],requests[k][1],SDL_BUTTON_LEFT);
        uint8_t *mailbox=A4_DISP(0x82b6);
        assert(mailbox[0]==14 && mailbox[2]==requests[k][2] && mailbox[1]==requests[k][3]);
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        core_consume_terrain_mailboxes();
        memset(before+(mailbox-prg_a4_area),0,3);
        assert(!mailbox[0]);
        if (k<2) assert(core_req.kind==(k ? CORE_REQ_GAME : CORE_REQ_OPTIONS) && (k || core_req.side==player));
        else assert(!core_req.kind && a4_same(before));
        core_req.kind=CORE_REQ_NONE;
        toolbar_fixture(); pause=1;
        system_click(requests[k][0],requests[k][1],SDL_BUTTON_LEFT);
        assert(!A4_DISP(0x82b6)[0]);
    }
    toolbar_fixture(); system_click(0,0,SDL_BUTTON_RIGHT);
    assert(A4_DISP(0x82b6)[0]==14 && A4_DISP(0x82b6)[1]==0 && A4_DISP(0x82b6)[2]==7);
    /* A temporary view lapses on the tenth turn. */
    toolbar_fixture(); old_view_who=0; view_who=(int16_t)(no_peeps+5); view_timer=2;
    game_tick(); assert(view_timer==1 && view_who==no_peeps+5);
    game_tick(); assert(view_timer==0 && view_who==0);
    _closeall(); SDL_Quit();
}

static void test_swamp_render(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)=0x11;
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_s;
    game_render();
    uint32_t unarmed[320*56];
    memcpy(unarmed,prg_screen->chunky_buffer+320*200,sizeof unarmed);
    for (int pixel=320*200;pixel<320*256;++pixel)
        prg_screen->chunky_buffer[pixel]=0xff123456;
    game_render();
    assert(!memcmp(unarmed,prg_screen->chunky_buffer+320*200,sizeof unarmed));
    game_handle_event(&key); game_render();
    assert(strstr(SDL_GetWindowTitle(prg_screen->window),"SWAMP ARMED (5000)"));
    assert(memcmp(unarmed,prg_screen->chunky_buffer+320*200,sizeof unarmed));
    /* Rendering frame 53 uses the loaded sheet unchanged (synthetic or real).
     * Bottom/right view tile has no later terrain tile painting over it. */
    no_peeps=0; memset(map_who,0,4096); memset(map_bk2,0,4096);
    core_tendency(0)=core_tendency(1)=1; mousex=mousey=0;
    xoff=yoff=6;
    int cell=13*64+13; map_blk[cell]=53;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    game_render();
    uint8_t expected[64000]; memcpy(expected,game_pixels,sizeof expected);
    int left,top; assert(graphics_tile(7,7,map_alt[cell],&left,&top));
    assert(graphics_blit(expected,sizeof expected,320,200,graphics_land(game_assets),53,left,top));
    assert(!memcmp(expected,game_pixels,sizeof expected));
    assert(a4_same(before));
    SDL_SetWindowSize(prg_screen->window,640,512);
    iso_click_vertex(20,20,SDL_BUTTON_LEFT); game_render();
    assert(!strstr(SDL_GetWindowTitle(prg_screen->window),"SWAMP ARMED"));
    _closeall(); SDL_Quit();
}

static unsigned quake_draws;
static uint16_t quake_values[162];
static uint16_t quake_random(void)
{
    assert(quake_draws<162);
    return quake_values[quake_draws++];
}
static void quake_fixture(int height)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
        core_set_vertex(column,row,height);
    memset(map_blk,15,4096); memset(map_bk2,42,4096); memset(map_alt,2,4096);
    for (int cell=0;cell<4096;++cell) A4_WORD(0xff88+cell*2)=91;
    A4_LONG(0x8492)=2500; A4_LONG(0x9a02)=A4_LONG(0x9a12)=10000;
    A4_WORD(0x82c4)=A4_WORD(0x82c4+46)=8;
    A4_WORD(0x2f88)=123; seed=321; core_result=0; player=0;
    quake_draws=0; memset(quake_values,0,sizeof quake_values);
}
static void test_original_lower(void)
{
    /* Zero/bounds are no-ops; each changed vertex increments a wrapping word. */
    quake_fixture(0);
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    assert(original_lower_point(0,0)==0);
    assert(original_lower_point(-1,0)==0 && original_lower_point(65,64)==0);
    assert(original_lower_point(0,-1)==0 && original_lower_point(64,65)==0);
    assert(a4_same(before));
    quake_fixture(8);
    A4_WORD(0x2f88)=-1;
    A4_WORD(0x2f8a)=A4_WORD(0x2f8c)=32;
    A4_WORD(0x2f8e)=A4_WORD(0x2f90)=32;
    assert(original_lower_point(32,32)==7 && A4_WORD(0x2f88)==0);
    assert(original_lower_point(32,32)==6 && A4_WORD(0x2f88)==9);
    assert(A4_WORD(0x2f8a)==31 && A4_WORD(0x2f8c)==33);
    assert(A4_WORD(0x2f8e)==31 && A4_WORD(0x2f90)==33);
    for (int row=0;row<=64;++row) for (int column=0;column<=64;++column) {
        int distance=abs(column-32)>abs(row-32) ? abs(column-32) : abs(row-32);
        assert(core_vertex(column,row)==(distance==0 ? 6 : distance==1 ? 7 : 8));
    }
    static const int corners[4][2]={{0,0},{64,0},{0,64},{64,64}};
    for (int corner=0;corner<4;++corner) {
        quake_fixture(8);
        int x=corners[corner][0],y=corners[corner][1];
        A4_WORD(0x2f8a)=A4_WORD(0x2f8c)=x;
        A4_WORD(0x2f8e)=A4_WORD(0x2f90)=y;
        for (int height=7;height>=0;--height) assert(original_lower_point(x,y)==height);
        assert(original_lower_point(x,y)==0);
        assert(A4_WORD(0x2f88)==327); /* 123 + 8+7*3+6*5+...+1*15. */
        assert(A4_WORD(0x2f8a)==(x ? 57 : 0) && A4_WORD(0x2f8c)==(x ? 64 : 7));
        assert(A4_WORD(0x2f8e)==(y ? 57 : 0) && A4_WORD(0x2f90)==(y ? 64 : 7));
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column) {
            int distance=abs(column-x)>abs(row-y) ? abs(column-x) : abs(row-y);
            assert(core_vertex(column,row)==(distance<8 ? distance : 8));
        }
    }
}
static int shake_frames;
static int16_t shake_heights[65*65];
static void shake_delay(int ms)
{
    /* Each frame: pre-quake terrain shifted by the offset, no sprites. */
    assert(ms==20 && core_shake_dy==core_quake_shake_dy(shake_frames));
    assert(game_pixels[(72+core_shake_dy)*320+192]==3);
    assert(game_pixels[72*320+192]!=3 && game_pixels[88*320+184]!=9);
    for (int v=0;v<65*65;++v) assert(core_vertex(v%65,v/65)==shake_heights[v]);
    ++shake_frames;
}
static void test_quake_shake(void)
{
    for (int c=0;c<20;++c) assert(core_quake_shake_dy(c)==(c%2 ? -1 : -4));
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    /* Headless reference: identical rules without a screen. */
    for (int run=0;run<2;++run) {
        static int16_t heights[2][65*65]; static int32_t mana[2], score[2]; static uint16_t seeds[2];
        assert(game_reset(0));
        memset(game_assets->background,1,sizeof game_assets->background);
        memset(game_assets->land_keep,1,sizeof game_assets->land_keep);
        memset(game_assets->land_indices,0,sizeof game_assets->land_indices);
        memset(game_assets->sprite_keep,1,sizeof game_assets->sprite_keep);
        memset(game_assets->sprite_indices,0,sizeof game_assets->sprite_indices);
        game_assets->land_indices[15*768+8*32+16]=2;
        game_assets->sprite_indices[8*256]=8;
        /* The synthetic start leaves +$C at 255: _move_sprite's _debug exit
         * (asm:16061-16069) would hide it until the first _set_frame. */
        toggle=0; set_peep_word(0,12,0); game_render(); assert(game_pixels[72*320+192]==3 && game_pixels[88*320+184]==9);
        set_peep_word(0,12,255);
        int sprites=no_sprites;
        GraphicsAssets *assets=game_assets;
        if (!run) game_assets=NULL;
        for (int v=0;v<65*65;++v) shake_heights[v]=core_vertex(v%65,v/65);
        A4_LONG(0x9a02)=10000; A4_WORD(0x82c4)|=8; pause=0; core_result=0; A4_WORD(0xae80)=0;
        shake_frames=0; void (*delay)(int)=core_quake_delay; core_quake_delay=shake_delay;
        assert(core_quake(0,0,0,___newrand));
        core_quake_delay=delay; game_assets=assets;
        assert(shake_frames==(run ? 20 : 0) && !core_shake_dy && no_sprites==sprites);
        int changed=0;
        for (int v=0;v<65*65;++v) changed+=(heights[run][v]=core_vertex(v%65,v/65))!=shake_heights[v];
        assert(changed>0);
        mana[run]=core_mana(0); score[run]=(int32_t)A4_LONG(0xae72); seeds[run]=seed;
        if (run) assert(!memcmp(heights[0],heights[1],sizeof heights[0]) &&
                        mana[0]==mana[1] && score[0]==score[1] && seeds[0]==seeds[1]);
    }
}
static void test_quake_cast(void)
{
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint)
    for (int gate=0;gate<13;++gate) {
        if (paint && gate>=6 && gate<=9) continue;   /* asm:7865 bypass, test_paint_map_powers */
        quake_fixture(1); paint_map=paint;
        int side=owner,x=20,y=30;
        if (gate==0) side=-1;
        if (gate==1) side=2;
        if (gate==2) x=-1;
        if (gate==3) x=57;
        if (gate==4) y=-1;
        if (gate==5) y=57;
        if (gate==6) A4_LONG(0x9a02+owner*16)=2499;
        if (gate==7) A4_WORD(0x82c4+owner*46)=0xfff7;
        if (gate==8) pause=1;
        if (gate==9) A4_WORD(0xae80)=1;
        if (gate>=10) core_result=gate-9;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_quake(side,x,y,quake_random));
        assert(!quake_draws && a4_same(before));
    }
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int paint=0;paint<2;++paint) for (int height=0;height<2;++height) {
        quake_fixture(height); player=local; paint_map=paint;
        A4_LONG(0x9a02+owner*16)=2500; A4_LONG(0xae72)=17;
        assert(core_quake(owner,20,30,quake_random));
        assert(core_mana(owner)==(paint ? 2500 : 0) && core_mana(1-owner)==10000);   /* asm:7865 */
        assert(A4_LONG(0xae72)==(owner==local ? 42u : 17u) && A4_WORD(0x2fac)==76);
        assert(quake_draws==(height ? 162u : 0u) && seed==321 && A4_WORD(0x2f88)==123);
        assert(A4_WORD(0x2f8a)==20 && A4_WORD(0x2f8c)==20);
        assert(A4_WORD(0x2f8e)==30 && A4_WORD(0x2f90)==30);
    }
    /* Column-major, second pass sees first-pass mutations: all initially-one
     * vertices lowered to zero consume only 81 draws, not a fixed 162. */
    static const int corners[4][2]={{0,0},{56,0},{0,56},{56,56}};
    for (int corner=0;corner<4;++corner) {
        quake_fixture(1);
        for (int draw=0;draw<162;++draw) quake_values[draw]=2+draw%3;
        int x=corners[corner][0],y=corners[corner][1];
        assert(core_quake(0,x,y,quake_random));
        assert(quake_draws==81 && A4_WORD(0x2f88)==204);
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
            assert(core_vertex(column,row)==!(column>=x && column<=x+8 && row>=y && row<=y+8));
        assert(A4_WORD(0x2f8a)==(x ? 56 : 1) && A4_WORD(0x2f8c)==(x ? 63 : 8));
        assert(A4_WORD(0x2f8e)==(y ? 56 : 1) && A4_WORD(0x2f90)==(y ? 63 : 8));
    }
    /* Mixed script: raise the third vertex, lower the first vertex in the
     * second column, then raise the first again on pass two. */
    quake_fixture(1); quake_values[2]=1; quake_values[9]=2; quake_values[81]=1;
    assert(core_quake(0,20,30,quake_random));
    assert(quake_draws==162 && A4_WORD(0x2f88)==127);
    /* The second-pass raise also repairs the adjacent zero before its visit. */
    assert(core_vertex(20,32)==2 && core_vertex(21,30)==1 && core_vertex(20,30)==2);
    assert(core_vertex(22,30)==1 && core_vertex(20,31)==1);
    /* Raising twice immediately grows previously-zero neighbours before their
     * turn; later draws must see them, not an initial height snapshot. */
    quake_fixture(0); core_set_vertex(20,30,1);
    quake_values[0]=quake_values[4]=1;
    assert(core_quake(0,20,30,quake_random));
    assert(quake_draws==13 && A4_WORD(0x2f88)==157);
    assert(core_vertex(20,30)==3 && core_vertex(21,31)==2 && core_vertex(22,32)==1);
    assert(A4_WORD(0x2f8a)==18 && A4_WORD(0x2f8c)==22);
    assert(A4_WORD(0x2f8e)==28 && A4_WORD(0x2f90)==32);
    /* Regional conversion resets changed map steps only and never touches
     * occupancy/peeps. No core_sculpt restrictions, price or town repaint. */
    quake_fixture(1); game_mode=GAME_MODE_NO_BUILD|GAME_MODE_BUILD_ONLY_UP|GAME_MODE_BUILD_NEAR_TOWNS;
    quake_values[0]=2;
    no_peeps=1; peeps[0]=1; set_peep_word(0,4,100); set_peep_word(0,8,30*64+20);
    map_who[30*64+20]=1;
    uint8_t people[208*22],occupancy[4096];
    memcpy(people,peeps,sizeof people); memcpy(occupancy,map_who,sizeof occupancy);
    assert(core_quake(0,20,30,quake_random));
    assert(core_mana(0)==7500 && A4_WORD(0x2f88)==124 && quake_draws==161);
    assert(!memcmp(people,peeps,sizeof people) && !memcmp(occupancy,map_who,sizeof occupancy));
    for (int row=0;row<64;++row) for (int column=0;column<64;++column) {
        int cell=row*64+column;
        if (column>=19 && column<=20 && row>=29 && row<=30) {
            assert(A4_WORD(0xff88+cell*2)==0 && map_bk2[cell]==42 && map_alt[cell]==0);
            static const int blocks[2][2]={{27,23},{29,30}};
            assert(map_blk[cell]==blocks[row-29][column-19]);
        } else {
            assert(A4_WORD(0xff88+cell*2)==91 && map_bk2[cell]==42 && map_alt[cell]==2 && map_blk[cell]==15);
        }
    }
}

static void test_quake_input(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_q;
    /* Q executes immediately at the viewport, not at the mouse or S target.
     * Exact-cost admission; _paint_map makes it free (asm:7865). */
    for (int diagnostic=0;diagnostic<2;++diagnostic) for (int tool=0;tool<2;++tool)
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint) {
        assert(game_reset(0)); diagnostic_view=diagnostic; core_tool=tool;
        player=owner; paint_map=paint; core_swamp_selected=1;
        xoff=20; yoff=30; mousex=mousey=0;
        memset(A4_DISP(0xae86),0,65*65*2);
        A4_LONG(0x9a02+owner*16)=2500; A4_WORD(0x82c4+owner*46)=8;
        uint16_t old_seed=seed;
        game_handle_event(&key);
        assert(core_mana(owner)==(paint ? 2500 : 0) && A4_LONG(0xae72)==25 && A4_WORD(0x2fac)==76);
        assert(seed==old_seed && core_swamp_selected && core_tool==tool);
        assert(A4_WORD(0x2f8a)==20 && A4_WORD(0x2f8e)==30);
        assert(xoff==20 && yoff==30);
        game_render();
        assert(strstr(SDL_GetWindowTitle(prg_screen->window),"Q quake 2500"));
    }
    /* Moving the viewport moves the actual footprint despite a fixed mouse. */
    for (int diagnostic=0;diagnostic<2;++diagnostic) for (int view=0;view<2;++view) {
        assert(game_reset(0)); diagnostic_view=diagnostic;
        xoff=view ? 40 : 10; yoff=view ? 40 : 20; mousex=mousey=0;
        A4_LONG(0x9a02)=5000; A4_WORD(0x82c4)=8; seed=123;
        int column=xoff,row=yoff;
        game_handle_event(&key);
        assert(core_mana(0)==2500 && A4_WORD(0xae72)==25 && seed!=123);
        assert(core_vertex(column,row+1)!=1 || core_vertex(column,row+2)!=1);
        for (int y=0;y<=64;++y) for (int x=0;x<=64;++x)
            if (x<column-8 || x>column+16 || y<row-8 || y>row+16)
                assert(core_vertex(x,y)==1);
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        key.key.repeat=1; game_handle_event(&key); key.key.repeat=0;
        assert(a4_same(before));
        game_render(); assert(a4_same(before));
    }
    /* Every rejected key preserves the entire emulated state and armed tool. */
    for (int diagnostic=0;diagnostic<2;++diagnostic) for (int paint=0;paint<2;++paint)
    for (int gate=0;gate<9;++gate) {
        if (paint && (gate==1 || gate==2 || gate==6 || gate==7)) continue;
        assert(game_reset(0)); diagnostic_view=diagnostic; paint_map=paint;
        xoff=20; yoff=30; core_tool=1; core_swamp_selected=1;
        A4_LONG(0x9a02)=2500; A4_WORD(0x82c4)=8;
        if (gate==0) key.key.repeat=1;
        if (gate==1) pause=1;
        if (gate==2) A4_WORD(0xae80)=1;
        if (gate==3) core_result=1;
        if (gate==4) core_result=2;
        if (gate==5) core_result=3;
        if (gate==6) A4_WORD(0x82c4)=0xfff7;
        if (gate==7) A4_LONG(0x9a02)=2499;
        if (gate==8) player=2;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        game_handle_event(&key); key.key.repeat=0;
        assert(a4_same(before));
        assert(core_tool==1 && core_swamp_selected==1);
        assert(core_result==(gate>=3 && gate<=5 ? gate-2 : 0));
    }
    assert(game_reset(0));
    key.key.keysym.sym=SDLK_d; game_handle_event(&key);
    for (int step=0;step<100;++step) {
        key.key.keysym.sym=SDLK_RIGHT; game_handle_event(&key);
        key.key.keysym.sym=SDLK_DOWN; game_handle_event(&key);
    }
    assert(diagnostic_view && xoff==56 && yoff==56);
    for (int step=0;step<100;++step) {
        key.key.keysym.sym=SDLK_LEFT; game_handle_event(&key);
        key.key.keysym.sym=SDLK_UP; game_handle_event(&key);
    }
    assert(xoff==0 && yoff==0);
    _closeall(); SDL_Quit();
}

static void flood_fixture(int height)
{
    quake_fixture(height);
    A4_LONG(0x84a2)=40000;
    A4_LONG(0x9a02)=A4_LONG(0x9a12)=40000;
    A4_WORD(0x82c4)=A4_WORD(0x82c4+46)=0x80;
    A4_WORD(0x2f8a)=7; A4_WORD(0x2f8c)=13;
    A4_WORD(0x2f8e)=23; A4_WORD(0x2f90)=29;
    core_battles=core_settlements=core_tool=core_swamp_selected=0;
}

static void test_flood_cast(void)
{
    /* Admission must be transactional, including P, either side, and RNG. */
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint)
    for (int gate=0;gate<9;++gate) {
        if (paint && gate>=2 && gate<=5) continue;   /* asm:7569 bypass, test_paint_map_powers */
        flood_fixture(2); paint_map=paint;
        int side=owner;
        if (gate==0) side=-1;
        if (gate==1) side=2;
        if (gate==2) A4_LONG(0x9a02+owner*16)=39999;
        if (gate==3) A4_WORD(0x82c4+owner*46)=0xff7f;
        if (gate==4) pause=1;
        if (gate==5) A4_WORD(0xae80)=1;
        if (gate>=6) core_result=gate-5;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_flood(side));
        assert(a4_same(before) && seed==321);
        assert(core_result==(gate>=6 ? gate-5 : 0));
    }
    /* An empty world still pays, scores, signals effect, and clears history. */
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int paint=0;paint<2;++paint) for (int height=0;height<3;++height)
    for (int extra=0;extra<2;++extra) {
        flood_fixture(height); player=local; paint_map=paint;
        A4_LONG(0x9a02+owner*16)=40000+extra; A4_LONG(0xae72)=17;
        assert(core_flood(owner));
        /* _paint_map: no payment, still the score (asm:7569-7604). */
        assert(core_mana(owner)==(paint ? 40000+extra : extra) && core_mana(1-owner)==40000);
        assert(A4_LONG(0xae72)==(owner==local ? 267u : 17u));
        assert(A4_WORD(0x2fac)==74 && seed==321 && A4_WORD(0x2f88)==123);
        assert(A4_WORD(0x2f8a)==7 && A4_WORD(0x2f8c)==13);
        assert(A4_WORD(0x2f8e)==23 && A4_WORD(0x2f90)==29);
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
            assert(core_vertex(column,row)==(height ? height-1 : 0));
        for (int cell=0;cell<4096;++cell) {
            assert(!A4_WORD(0xff88+cell*2) && !map_alt[cell]);
            assert(map_blk[cell]==(height==2 ? 15 : 0));
            assert(map_bk2[cell]==(height==2 ? 42 : 0));
        }
    }
    /* Discontinuous signed input catches recursive smoothing, unsigned tests,
     * clamping negatives, and a missing final vertex/outer row or column. */
    flood_fixture(0);
    static const int heights[]={0,1,2,8,-1,-32768,32767};
    for (int index=0;index<4225;++index)
        core_set_vertex(index%65,index/65,heights[index%7]);
    assert(core_flood(0));
    for (int index=0;index<4225;++index) {
        int original=heights[index%7];
        assert(core_vertex(index%65,index/65)==(original>0 ? original-1 : original));
    }
    assert(seed==321 && A4_WORD(0x2f88)==123);
    assert(A4_WORD(0x2f8a)==7 && A4_WORD(0x2f8c)==13);
    assert(A4_WORD(0x2f8e)==23 && A4_WORD(0x2f90)==29);

    /* Full conversion, unlike quake's local history invalidation. Land rock
     * survives; water rock and overlays disappear, but people do not change. */
    flood_fixture(1);
    for (int row=0;row<=64;++row) for (int column=0;column<=31;++column)
        core_set_vertex(column,row,2);
    memset(map_blk,47,4096);
    no_peeps=2; peeps[0]=1; peeps[22]=2; peeps[23]=1;
    set_peep_word(0,4,100); set_peep_word(0,8,650);
    set_peep_word(1,4,200); set_peep_word(1,8,3000);
    map_who[650]=1; map_who[3000]=2;
    core_leader(0)=1; core_leader(1)=2;
    core_target(0)=650; core_target(1)=3000;
    game_mode=GAME_MODE_NO_BUILD|GAME_MODE_BUILD_ONLY_UP|GAME_MODE_BUILD_NEAR_TOWNS;
    uint8_t people[208*22],occupancy[4096],magnets[32];
    memcpy(people,peeps,sizeof people); memcpy(occupancy,map_who,sizeof occupancy);
    memcpy(magnets,A4_DISP(0x99f6),sizeof magnets);
    /* Mana is part of magnet records; expect only the fixed debit. */
    memset(magnets+12,0,4);
    assert(core_flood(0));
    assert(core_mana(0)==0);
    assert(!memcmp(magnets,A4_DISP(0x99f6),sizeof magnets));
    assert(!memcmp(people,peeps,sizeof people) && !memcmp(occupancy,map_who,sizeof occupancy));
    assert(no_peeps==2 && !core_battles && !core_settlements);
    for (int cell=0;cell<4096;++cell) {
        assert(!A4_WORD(0xff88+cell*2));
        assert(map_blk[cell]==(cell%64<32 ? 47 : 0));
        assert(map_bk2[cell]==(cell%64<32 ? 42 : 0));
    }
}

static void test_flood_delayed_hazards(void)
{
    /* Flood does not damage directly. The first water tick changes state and
     * removes the footprint; the second owns loss/removal and leader cleanup. */
    for (int fatal=0;fatal<2;++fatal) for (int town=0;town<2;++town) {
        flood_fixture(1); memset(map_bk2,0,4096);
        no_peeps=1; peeps[0]=town ? 1 : 2; peeps[2]=peeps[3]=1;
        set_peep_word(0,4,100); set_peep_word(0,8,650); set_peep_word(0,12,town ? 42 : 7);
        map_who[650]=1; core_leader(0)=1; core_target(0)=900;
        if (town) core_set_town(0,0);
        game_mode=fatal ? GAME_MODE_WATER_FATAL : 0; A4_WORD(0x2fb0)=1;
        assert(core_flood(0));
        assert(peep_word(0,4)==100 && peeps[0]==(town ? 1 : 2));
        assert(map_who[650]==1 && core_leader(0)==1 && core_target(0)==900);
        assert(!map_blk[650] && !map_bk2[650]);
        _move_peeps();
        assert(peep_word(0,4)==100 && peeps[0]==18 && map_who[650]==1);
        assert(peep_word(0,12)==(town ? 0 : 7));
        assert(core_leader(0)==1 && core_target(0)==900);
        _move_peeps();
        assert(peep_word(0,4)==(fatal ? 0 : 98));
        assert(peeps[0]==(fatal ? 0 : 18) && map_who[650]==(fatal ? 0 : 1));
        assert(core_leader(0)==(fatal ? 0 : 1) && core_target(0)==(fatal ? 650 : 900));
    }
    /* Battle bit 8 is explicitly exempt from water handling in _move_peeps;
     * flood must not invent fatal drowning for a still-active battle. */
    flood_fixture(1); memset(map_bk2,0,4096); no_peeps=2;
    peeps[0]=8; peeps[22]=10; peeps[23]=1; peeps[3]=peeps[25]=1;
    for (int person=0;person<2;++person) {
        set_peep_word(person,4,100); set_peep_word(person,8,650);
        set_peep_word(person,6,1-person);
    }
    map_who[650]=1; game_mode=GAME_MODE_WATER_FATAL; seed=123;
    assert(core_flood(0));
    assert(peep_word(0,4)==100 && peep_word(1,4)==100 && seed==123);
    _move_peeps();
    assert(peep_word(0,4)==89 && peep_word(1,4)==89);
    assert(peeps[0]==8 && peeps[22]==10 && map_who[650]==1);

    /* A surviving isolated flat town loses its city capacity (3050 -> 65).
     * Reframing and the existing excess-population split happen only on tick. */
    flood_fixture(1); memset(map_bk2,0,4096);
    for (int row=10;row<=11;++row) for (int column=10;column<=11;++column)
        core_set_vertex(column,row,2);
    no_peeps=1; peeps[0]=1; peeps[2]=peeps[3]=1;
    set_peep_word(0,4,100); set_peep_word(0,8,650); set_peep_word(0,12,42);
    map_who[650]=1; core_leader(0)=1; core_set_town(0,0);
    assert(core_flood(0));
    assert(peep_word(0,12)==42 && peep_word(0,4)==100 && no_peeps==1);
    game_turn=1; _move_peeps();
    assert(peeps[0]==1 && peep_word(0,12)==34 && peep_word(0,4)==100);
    assert(map_bk2[650]==34 && no_peeps==1);
    game_turn=8; _move_peeps();
    assert(no_peeps==2 && peep_word(0,4)==32 && peep_word(1,4)==68);
    assert(peeps[22]==2 && core_leader(0)==2 && map_who[650]==2);
}

/* HUD line/column holds text exactly as amiga_text draws it on black. */
static int hud_text_at(int line, int column, const char *text)
{
    static uint8_t scratch[320*200]; memset(scratch,0,sizeof scratch);
    amiga_text(scratch,column*8,0,text);
    int lit=0;
    for (int y=0;y<8;++y) for (int x=column*8;x<column*8+8*(int)strlen(text);++x) {
        lit+=scratch[y*320+x]!=0;
        if (prg_screen->chunky_buffer[(200+line*8+y)*320+x]!=prg_screen->palette[scratch[y*320+x]]) return 0;
    }
    return lit>0;
}

static void test_flood_input(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_f;
    /* Camera, mouse, diagnostic mode and armed S never retarget a global cast. */
    for (int owner=0;owner<2;++owner) for (int view=0;view<3;++view)
    for (int diagnostic=0;diagnostic<2;++diagnostic) for (int tool=0;tool<2;++tool) {
        flood_fixture(2); player=owner; xoff=view*28; yoff=56-view*28;
        mousex=view*159; mousey=view*127; diagnostic_view=diagnostic;
        core_tool=tool; core_swamp_selected=1;
        game_handle_event(&key);
        assert(core_mana(owner)==0 && A4_LONG(0xae72)==250 && A4_WORD(0x2fac)==74);
        assert(core_tool==tool && core_swamp_selected==1 && seed==321);
        assert(xoff==view*28 && yoff==56-view*28 && mousex==view*159 && mousey==view*127);
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
            assert(core_vertex(column,row)==1);
    }
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint)
    for (int gate=0;gate<10;++gate) {
        if (paint && ((gate>=1 && gate<=2) || gate==6 || gate==7)) continue;
        flood_fixture(2); player=owner; paint_map=paint;
        core_tool=core_swamp_selected=1; xoff=56; yoff=0; mousex=319; mousey=255;
        if (gate==0) key.key.repeat=1;
        if (gate==1) pause=1;
        if (gate==2) A4_WORD(0xae80)=1;
        if (gate>=3 && gate<=5) core_result=gate-2;
        if (gate==6) A4_WORD(0x82c4+owner*46)=0xff7f;
        if (gate==7) A4_LONG(0x9a02+owner*16)=39999;
        if (gate==8) player=-1;
        if (gate==9) player=2;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        game_handle_event(&key); key.key.repeat=0;
        assert(a4_same(before));
        assert(core_tool==1 && core_swamp_selected==1);
    }
    assert(game_reset(0)); game_render();
    assert(strstr(SDL_GetWindowTitle(prg_screen->window),"F flood 40000"));
    /* Verify visible text rather than only the window title: the flood label
     * is HUD line 5 column 16 in the original 8x8 font. */
    mousex=-100; game_render();
    assert(hud_text_at(5,16,"F FLOOD 40000"));
    _closeall(); SDL_Quit();
}

static void test_flood_input_smoke(void)
{
    quake_fixture(1);
    A4_LONG(0x84a2)=40000; A4_LONG(0x9a02)=40000; A4_WORD(0x82c4)=0x80;
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_f;
    game_handle_event(&key);
    assert(core_mana(0)==0 && A4_WORD(0x2fac)==74 && A4_LONG(0xae72)==250);
    assert(core_vertex(64,64)==0);
}

static unsigned volcano_draws;
static uint16_t volcano_values[229];
static uint16_t volcano_random(void)
{
    assert(volcano_draws<229);
    assert(A4_WORD(0x2fac)==75);
    return volcano_values[volcano_draws++];
}
static void volcano_fixture(int height)
{
    quake_fixture(height);
    A4_LONG(0x849e)=10000;
    A4_WORD(0x82c4)=A4_WORD(0x82c4+46)=0x40;
    A4_WORD(0x2f8a)=7; A4_WORD(0x2f8c)=13;
    A4_WORD(0x2f8e)=23; A4_WORD(0x2f90)=29;
    core_battles=core_settlements=core_tool=core_swamp_selected=0;
    volcano_draws=0;
    for (int draw=0;draw<229;++draw) volcano_values[draw]=3;
}
static void test_volcano_cast(void)
{
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint)
    for (int gate=0;gate<19;++gate) {
        if (paint && gate>=6 && gate<=9) continue;   /* asm:8048 bypass, test_paint_map_powers */
        volcano_fixture(0); paint_map=paint;
        int side=owner,x=20,y=30;
        if (gate==0) side=-1;
        if (gate==1) side=2;
        if (gate==2) x=-1;
        if (gate==3) x=57;
        if (gate==4) y=-1;
        if (gate==5) y=57;
        if (gate==6) A4_LONG(0x9a02+owner*16)=9999;
        if (gate==7) A4_WORD(0x82c4+owner*46)=0xffbf;
        if (gate==8) pause=1;
        if (gate==9) A4_WORD(0xae80)=1;
        if (gate>=10 && gate<=12) core_result=gate-9;
        if (gate==13) core_leader(0)=-1;
        if (gate==14) { no_peeps=208; core_leader(0)=209; }
        if (gate==15) { no_peeps=1; core_leader(0)=2; }
        if (gate==16) { no_peeps=1; core_leader(0)=1; } /* Dead reference. */
        if (gate>=17) {
            no_peeps=1; core_leader(0)=1; set_peep_word(0,4,10);
            set_peep_word(0,8,gate==17 ? -1 : 4096);
        }
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_volcano(side,x,y,volcano_random));
        assert(!volcano_draws && seed==321 && a4_same(before));
    }
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int paint=0;paint<2;++paint) for (int height=0;height<=8;height+=8)
    for (int extra=0;extra<2;++extra) {
        volcano_fixture(height); player=local; paint_map=paint;
        A4_LONG(0x9a02+owner*16)=10000+extra; A4_LONG(0xae72)=17;
        for (int draw=0;draw<165;++draw) volcano_values[draw]=height ? 1 : draw%2 ? 3 : 0;
        assert(core_volcano(owner,20,30,volcano_random));
        assert(core_mana(owner)==(paint ? 10000+extra : extra) && core_mana(1-owner)==10000);   /* asm:8048 */
        assert(A4_LONG(0xae72)==(owner==local ? 117u : 17u));
        assert(volcano_draws==229 && seed==321 && A4_WORD(0x2f88)==123 && A4_WORD(0x2fac)==75);
        assert(A4_WORD(0x2f8a)==20 && A4_WORD(0x2f8c)==20);
        assert(A4_WORD(0x2f8e)==30 && A4_WORD(0x2f90)==30);
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
            assert(core_vertex(column,row)==height);
    }
    /* 81+49+25+9+1 = 165 raises, not five fixed 9x9 passes or a crater. */
    static const int origins[][2]={{20,30},{0,0},{56,0},{0,56},{56,56}};
    for (unsigned origin=0;origin<sizeof origins/sizeof origins[0];++origin) {
        volcano_fixture(0);
        for (int draw=0;draw<165;++draw) volcano_values[draw]=1;
        int x=origins[origin][0],y=origins[origin][1];
        assert(core_volcano(0,x,y,volcano_random));
        assert(volcano_draws==229 && A4_WORD(0x2f88)==288);
        assert(A4_WORD(0x2f8a)==(x ? x : 1) && A4_WORD(0x2f8c)==(x==56 ? 63 : x+8));
        assert(A4_WORD(0x2f8e)==(y ? y : 1) && A4_WORD(0x2f90)==(y==56 ? 63 : y+8));
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column) {
            int distance=abs(column-x-4)>abs(row-y-4) ? abs(column-x-4) : abs(row-y-4);
            assert(core_vertex(column,row)==(distance<=4 ? 5-distance : 0));
        }
    }
    /* Distinct coordinates identify x-outer/y-inner order and each pass start;
     * the last 64 draws must begin only after all 165 height decisions. */
    volcano_fixture(0);
    volcano_values[2]=1; volcano_values[9]=2; volcano_values[81]=4;
    volcano_values[130]=1; volcano_values[155]=2; volcano_values[164]=4;
    volcano_values[167]=0; volcano_values[173]=0;
    assert(core_volcano(0,20,30,volcano_random));
    assert(volcano_draws==229 && A4_WORD(0x2f88)==129);
    assert(core_vertex(20,32)==1 && core_vertex(21,30)==1 && core_vertex(21,31)==1);
    assert(core_vertex(22,32)==1 && core_vertex(23,33)==1 && core_vertex(24,34)==1);
    assert(core_vertex(20,31)==0 && core_vertex(22,30)==0);
    assert(map_blk[32*64+20]==47 && map_blk[30*64+21]==47);
    assert(map_bk2[32*64+20]==0 && map_bk2[30*64+21]==0);
}
static void test_volcano_lava(void)
{
    /* The two assembly leader locals both read blue, not red. The independently
     * protected targets do not imply height, owner, occupancy or block filters. */
    for (int height=0;height<=1;++height) for (int red_target=0;red_target<2;++red_target) {
        volcano_fixture(height); no_peeps=3;
        static const int positions[]={31*64+21,32*64+22,33*64+23};
        for (int person=0;person<3;++person) {
            peeps[person*22]=person==2 ? 1 : 2; peeps[person*22+1]=person&1;
            set_peep_word(person,4,100); set_peep_word(person,8,positions[person]);
            map_who[positions[person]]=person+1;
        }
        core_leader(0)=1; core_leader(1)=2;
        core_target(0)=34*64+24;
        core_target(1)=red_target ? positions[1] : 35*64+25;
        game_mode=GAME_MODE_NO_BUILD|GAME_MODE_BUILD_ONLY_UP|GAME_MODE_BUILD_NEAR_TOWNS;
        for (int cell=0;cell<4096;++cell) map_blk[cell]=(uint8_t)(cell%70);
        uint8_t people[208*22],occupancy[4096],blocks[4096];
        memcpy(people,peeps,sizeof people); memcpy(occupancy,map_who,sizeof occupancy);
        memcpy(blocks,map_blk,sizeof blocks);
        for (int draw=165;draw<229;++draw) volcano_values[draw]=0;
        assert(core_volcano(1,20,30,volcano_random));
        assert(volcano_draws==229 && A4_WORD(0x2f88)==123 && core_mana(1)==0);
        assert(!memcmp(people,peeps,sizeof people) && !memcmp(occupancy,map_who,sizeof occupancy));
        assert(no_peeps==3 && !core_battles && !core_settlements);
        assert(core_leader(0)==1 && core_leader(1)==2 && core_target(0)==34*64+24);
        for (int row=0;row<64;++row) for (int column=0;column<64;++column) {
            int cell=row*64+column;
            int lava=column>=20 && column<28 && row>=30 && row<38 &&
                cell!=positions[0] && cell!=core_target(0) && cell!=core_target(1);
            int rebuilt=column>=19 && column<=20 && row>=29 && row<=30;
            if (rebuilt) {
                assert(!A4_WORD(0xff88+cell*2));
                assert(map_alt[cell]==(height && (lava || blocks[cell]==47) ? 1 : 0));
                assert(map_blk[cell]==(height ? (lava || blocks[cell]==47 ? 47 : 15) : 0));
                assert(map_bk2[cell]==(height && !lava ? 42 : 0));
            } else {
                assert(map_blk[cell]==(lava ? 47 : blocks[cell]));
                assert(map_bk2[cell]==(lava ? 0 : 42));
                assert(map_alt[cell]==2 && A4_WORD(0xff88+cell*2)==91);
            }
        }
    }
    /* Absent blue leader protects cell zero. Protection prevents the lava
     * write only: existing rock still submerges in the tiny rebuild rectangle. */
    for (int height=0;height<=1;++height) {
        volcano_fixture(height); core_target(0)=1000; core_target(1)=2000;
        map_blk[0]=47;
        for (int draw=165;draw<229;++draw) volcano_values[draw]=0;
        assert(core_volcano(0,0,0,volcano_random));
        assert(volcano_draws==229 && map_blk[0]==(height ? 47 : 0));
        assert(map_bk2[0]==(height ? 42 : 0));
        assert(map_blk[1]==47 && map_bk2[1]==0 && A4_WORD(0xff88+2)==91);
        assert(A4_WORD(0x2f8a)==1 && A4_WORD(0x2f8c)==0);
        assert(A4_WORD(0x2f8e)==1 && A4_WORD(0x2f90)==0);
    }
}
static void test_volcano_input(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_v;
    for (int owner=0;owner<2;++owner) for (int diagnostic=0;diagnostic<2;++diagnostic)
    for (int tool=0;tool<2;++tool) for (int paint=0;paint<2;++paint)
    for (int view=0;view<2;++view) {
        volcano_fixture(0); player=owner; diagnostic_view=diagnostic; paint_map=paint;
        core_tool=tool; core_swamp_selected=1;
        xoff=view ? 40 : 10; yoff=view ? 40 : 20; mousex=319; mousey=255;
        uint16_t expected_seed=321;
        for (int draw=0;draw<229;++draw) expected_seed=(expected_seed*0x24a1u+0x24dfu)&0x7fff;
        game_handle_event(&key);
        assert(core_mana(owner)==(paint ? 10000 : 0) && seed==expected_seed && A4_LONG(0xae72)==100);
        assert(A4_WORD(0x2fac)==75 && core_swamp_selected==1 && core_tool==tool);
        assert(xoff==(view ? 40 : 10) && yoff==(view ? 40 : 20));
        assert(mousex==319 && mousey==255 && core_vertex(xoff+4,yoff+4)>0);
        assert(core_vertex(view ? 14 : 44,view ? 24 : 44)==0);
    }
    for (int gate=0;gate<11;++gate) for (int paint=0;paint<2;++paint) {
        if (paint && ((gate>=1 && gate<=2) || gate==6 || gate==7)) continue;
        volcano_fixture(0); paint_map=paint; xoff=20; yoff=30;
        core_tool=core_swamp_selected=1;
        if (gate==0) key.key.repeat=1;
        if (gate==1) pause=1;
        if (gate==2) A4_WORD(0xae80)=1;
        if (gate>=3 && gate<=5) core_result=gate-2;
        if (gate==6) A4_WORD(0x82c4)=0xffbf;
        if (gate==7) A4_LONG(0x9a02)=9999;
        if (gate==8) player=-1;
        if (gate==9) player=2;
        if (gate==10) core_leader(0)=209;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        game_handle_event(&key); key.key.repeat=0;
        assert(a4_same(before));
        assert(core_tool==1 && core_swamp_selected==1);
    }
    assert(game_reset(0)); game_render();
    assert(strstr(SDL_GetWindowTitle(prg_screen->window),"V volcano 10000"));
    mousex=-100; game_render();
    assert(hud_text_at(5,0,"V VOLCANO 10000")); /* original 8x8 font, HUD line 5 */
    _closeall(); SDL_Quit();
}

static void test_volcano_render(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    volcano_fixture(1); _make_map();
    memset(map_bk2,0,4096);
    core_tendency(0)=core_tendency(1)=1;
    diagnostic_view=0; xoff=yoff=6; mousex=mousey=0;
    /* Use a private sheet copy so real loaded assets remain untouched. Mark an
     * opaque interior pixel in frames 15 and 47 with distinguishable indices. */
    GraphicsAssets *loaded=game_assets, *fixture=malloc(sizeof *fixture);
    assert(fixture); memcpy(fixture,loaded,sizeof *fixture); game_assets=fixture;
    const int frame_pixel=12*32+16;
    fixture->land_indices[15*768+frame_pixel]=2;
    fixture->land_keep[15*768+frame_pixel]=0;
    fixture->land_indices[47*768+frame_pixel]=14;
    fixture->land_keep[47*768+frame_pixel]=0;
    /* Last tile (u=v=7), altitude zero: left=176, top=176. No later
     * terrain, people, magnet, cursor or front face covers this pixel. */
    const int screen_pixel=188*320+192, cell=13*64+13;
    game_render();
    assert(game_pixels[screen_pixel]==2);
    volcano_values[228]=0; /* Last of 64 lava draws, after 165 no-op raises. */
    assert(core_volcano(0,6,6,volcano_random));
    assert(volcano_draws==229 && map_blk[cell]==47 && !map_bk2[cell] && !map_alt[cell]);
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    game_render();
    assert(game_pixels[screen_pixel]==14);
    assert(prg_screen->chunky_buffer[screen_pixel]==prg_screen->palette[14]);
    assert(a4_same(before));
    game_assets=loaded; free(fixture);
    _closeall(); SDL_Quit();
}

static void test_volcano_input_smoke(void)
{
    quake_fixture(0);
    A4_LONG(0x849e)=10000; A4_WORD(0x82c4)=0x40;
    xoff=20; yoff=30;
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_v;
    game_handle_event(&key);
    assert(core_mana(0)==0 && A4_WORD(0x2fac)==75 && A4_LONG(0xae72)==100);
    assert(seed!=321);
}

/* Deferred terrain commands are test-injected, never produced by gameplay. */
static void terrain_mailbox_write(int owner, int opcode, int column, int row)
{
    A4_DISP(0x82b6+owner*46)[0]=opcode;
    A4_DISP(0x82b7+owner*46)[0]=column;
    A4_DISP(0x82b8+owner*46)[0]=row;
}
static void assert_mailbox_defaults(int speed)
{
    assert(A4_WORD(0x82bc)==0 && A4_WORD(0x82bc+46)==1);
    assert(A4_WORD(0x82be)==1 && A4_WORD(0x82be + 46)==0);
    assert(A4_WORD(0x82c6)==1 && A4_WORD(0x82c6+46)==speed);
    for (int owner=0;owner<2;++owner)
        for (int byte=0;byte<3;++byte) assert(!A4_DISP(0x82b6+owner*46)[byte]);
}
static void test_terrain_defaults(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for (int scenario=0;scenario<8;++scenario) {
        assert(game_reset(scenario));
        assert_mailbox_defaults(demo_levels[scenario][1]);
        for (int owner=0;owner<2;++owner) {
            terrain_mailbox_write(owner,1,64,64);
            A4_WORD(0x82bc+owner*46)=3;
            A4_WORD(0x82be + owner*46)=17;
            A4_WORD(0x82c6+owner*46)=0;
        }
        SDL_Event reset={.type=SDL_KEYDOWN}; reset.key.keysym.sym=SDLK_r;
        game_handle_event(&reset);
        assert_mailbox_defaults(demo_levels[scenario][1]);
    }
    if (!getenv("POPULOUS_TEST_REAL_DATA")) {
        write_level_fixture(1,1);
        assert(game_reset_start((StartSelection){START_ORIGINAL,0}));
        assert_mailbox_defaults(9);
        terrain_mailbox_write(0,1,20,30); terrain_mailbox_write(1,255,64,64);
        pause=1; diagnostic_view=1; core_tool=1; core_battles=17;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        GraphicsAssets *assets=game_assets;
        /* Zero remains a valid on-disk speed. Only runtime initialization rejects. */
        char path[512]; snprintf(path,sizeof path,"%s/level.dat",test_data_directory);
        FILE *file=fopen(path,"r+b"); assert(file);
        assert(!fseek(file,1,SEEK_SET) && fputc(0,file)!=EOF && !fclose(file));
        LevelTable levels; char error[160];
        assert(level_data_load(test_data_directory,&levels,error,sizeof error));
        assert(!game_reset_start((StartSelection){START_ORIGINAL,0}));
        assert(a4_same(before) && assets==game_assets);
        assert(pause && diagnostic_view==1 && core_tool==1 && core_battles==17);
        assert(current_start.kind==START_ORIGINAL && current_start.number==0);
        assert(prg_a4==prg_a4_area+32768);
        write_level_fixture(1,1);
        assert(game_reset_start((StartSelection){START_ORIGINAL,0}));
        assert_mailbox_defaults(9);
        assert(!unlink(path));
    }
    _closeall(); SDL_Quit();
}
static void terrain_mailbox_fixture(int height)
{
    quake_fixture(height);
    prg_quit=diagnostic_view=core_tool=core_swamp_selected=0;
    core_battles=core_settlements=0;
    A4_LONG(0x848a)=10;
    no_peeps=2;
    for (int owner=0;owner<2;++owner) {
        /* Remnants stay put and merely decrement their timer during movement. */
        int position=owner ? 3500 : 400;
        peeps[owner*22]=0x80; peeps[owner*22+1]=owner;
        set_peep_word(owner,4,45); set_peep_word(owner,6,100);
        set_peep_word(owner,8,position); set_peep_word(owner,12,7);
        map_who[position]=owner+1;
        core_target(owner)=position; core_leader(owner)=owner+1;
        A4_WORD(0x82c6+owner*46)=1;
        A4_WORD(0x82be + owner*46)=17;
    }
}
static void assert_mailbox_empty(void)
{
    for (int owner=0;owner<2;++owner)
        for (int byte=0;byte<3;++byte) assert(!A4_DISP(0x82b6+owner*46)[byte]);
}
static void test_terrain_mailbox_tick(void)
{
    terrain_mailbox_fixture(1);
    terrain_mailbox_write(0,1,10,10);
    terrain_mailbox_write(0,1,20,30); /* Last write replaces, not queues. */
    game_tick();
    assert(core_vertex(10,10)==1 && core_vertex(20,30)==2);
    assert(core_mana(0)==9986 && core_mana(1)==10000);
    assert(A4_WORD(0x2f88)==1 && game_turn==1);
    assert(A4_WORD(0x82be)==17 && A4_WORD(0x82be + 46)==17);
    assert_mailbox_empty();
    assert(seed==321 && !core_result && !core_battles && !core_settlements);
    for (int owner=0;owner<2;++owner) {
        assert(peep_word(owner,4)==45 && peep_word(owner,6)==99 && peep_word(owner,12)==7);
        assert(A4_LONG(0x99fe + owner*16)==45);
        assert(core_leader(owner)==owner+1 && core_target(owner)==(owner ? 3500 : 400));
    }
    /* Same vertex: first raise costs 14, second recursively raises 9 vertices (46).
     * Reversing owner order would exchange the payments. */
    terrain_mailbox_fixture(1);
    terrain_mailbox_write(0,1,20,30); terrain_mailbox_write(1,1,20,30);
    game_tick();
    assert(core_mana(0)==9986 && core_mana(1)==9954 && A4_WORD(0x2f88)==9);
    for (int row=0;row<=64;++row) for (int column=0;column<=64;++column) {
        int distance=abs(column-20)>abs(row-30) ? abs(column-20) : abs(row-30);
        assert(core_vertex(column,row)==(distance==0 ? 3 : distance==1 ? 2 : 1));
    }
    assert_mailbox_empty();
}
static void test_terrain_mailbox_cadence(void)
{
    static const struct { unsigned turn,divisor; int controller,latch,expected; } cases[]={
        {5,3,1,17,0},{4,3,1,17,17},{5,3,0,17,17},{5,3,2,17,17},
        {4,3,1,0,0},{65534,65535,1,17,0},{32767,32768,1,17,0},
        {65535,3,1,17,0},{4,0,1,17,17},{4,0,1,0,0},{4,0,0,17,17}
    };
    for (unsigned index=0;index<sizeof cases/sizeof cases[0];++index)
    for (int opcode=0;opcode<=2;++opcode) for (int owner=0;owner<2;++owner) {
        terrain_mailbox_fixture(1);
        game_turn=(int16_t)cases[index].turn;
        A4_WORD(0x82bc+owner*46)=cases[index].controller;
        A4_WORD(0x82be + owner*46)=cases[index].latch;
        A4_WORD(0x82c6+owner*46)=(int16_t)cases[index].divisor;
        terrain_mailbox_write(owner,opcode,20,30);
        /* A clear latch lets this owner's AI (tendency 0, no towns) replace
         * the queued command with 14/x/1 first (asm:3254-3261, 9612-9633). */
        int ai=!cases[index].latch;
        uint16_t picked=ai_lcg((uint16_t)seed);
        game_tick();
        int executed=!ai && !(cases[index].controller==1 && !cases[index].divisor);
        int raised=opcode==1 && executed;
        /* Opcode 2 is _do_lower_point (dispatch asm:$4b81e): 1->0, cost 1*4+10. */
        int lowered=opcode==2 && executed;
        assert(core_vertex(20,30)==1+raised-lowered);
        assert(core_mana(owner)==10000-14*raised-14*lowered);
        /* The pick latches; a zero divisor discards it, else it sets x. */
        assert(A4_WORD(0x82be + owner*46)==(ai ? 1 : cases[index].expected));
        if (ai) assert(core_tendency(owner)==(cases[index].divisor ? picked%3+1 : 0));
        assert(A4_WORD(0x82bc+owner*46)==cases[index].controller);
        assert((uint16_t)A4_WORD(0x82c6+owner*46)==cases[index].divisor);
        assert_mailbox_empty();
    }
    /* Opcode 2 lowers (above). Jump table asm:18200-18216: 3 is _do_quake
     * (permission 8 and 2500 mana here), 5 _do_magnet (no permission bit;
     * cost $848e is 0, target 30*64+20), 4/6 lack permission, 14 with
     * y=30 is no _do_action case, 7..13 and 15..255 have no consumer. */
    for (int opcode=3;opcode<=255;++opcode) {
        terrain_mailbox_fixture(1); terrain_mailbox_write(0,opcode,20,30);
        game_tick();
        if (opcode==3) assert(core_mana(0)==7500 && A4_WORD(0x2fac)==76);
        else {
            assert(core_vertex(20,30)==1 && core_mana(0)==10000 && A4_WORD(0x2f88)==123);
            assert(core_target(0)==(opcode==5 ? 1940 : 400) && A4_WORD(0x2fac)==(opcode==5 ? 77 : 0));
        }
        assert(A4_WORD(0x82be)==17); assert_mailbox_empty();
    }
}
static void test_terrain_mailbox_gates(void)
{
    for (int gate=0;gate<5;++gate) {
        terrain_mailbox_fixture(1);
        terrain_mailbox_write(0,1,20,30); terrain_mailbox_write(1,1,21,30);
        if (gate==0) pause=1;
        else if (gate==1) prg_quit=1;
        else core_result=gate-1;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        game_tick(); assert(a4_same(before));
    }
    /* Only old water dies before the queued raise. Newly submerged residents
     * survive this tick, so the existing mailbox consumer can rescue them. */
    for (int old_water=0; old_water<2; ++old_water) {
        terrain_mailbox_fixture(0); game_mode=GAME_MODE_WATER_FATAL;
        if (old_water) peeps[0]=18;
        map_blk[400]=0; A4_WORD(0x82bc)=1; A4_WORD(0x82c6)=1;
        terrain_mailbox_write(0,1,16,6); terrain_mailbox_write(1,1,20,30);
        game_tick();
        if (old_water) {
            /* asm:3459-3474: the removed resident's 45 still counts, so
             * asm:4323-4347 does not end this pass and the queued raise is
             * consumed as below; the loss follows on the next pass. */
            assert(!core_result && !peep_word(0,4) && A4_LONG(0x99fe)==45 && A4_LONG(0x9a0e)==45);
            assert(core_vertex(16,6)==1 && core_mana(0)==9986 && A4_WORD(0x2f88)==1);
            assert(A4_WORD(0x82be)==0 && A4_WORD(0x82be + 46)==17);
            assert_mailbox_empty();
            game_tick(); assert(core_result==2 && !A4_LONG(0x99fe) && A4_LONG(0x9a0e)==45);
        } else {
            assert(!core_result && peeps[0]==18 && peep_word(0,4)==45);
            assert(peep_word(0,6)==100 && peep_word(0,12)==7);
            assert(A4_LONG(0x99fe)==45 && A4_LONG(0x9a0e)==45);
            assert(core_vertex(16,6)==1 && core_mana(0)==9986 && A4_WORD(0x2f88)==1);
            assert(A4_WORD(0x82be)==0 && A4_WORD(0x82be + 46)==17);
        }
        assert_mailbox_empty();
    }
}
static void test_terrain_mailbox_raise(void)
{
    /* Invalid wrapper arguments must not touch even bounds/count/controller data. */
    static const int invalid[][3]={{-1,20,30},{2,20,30},{0,-1,30},{1,65,30},{0,20,-1},{1,20,255}};
    for (unsigned index=0;index<sizeof invalid/sizeof invalid[0];++index) {
        terrain_mailbox_fixture(1);
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_raw_raise(invalid[index][0],invalid[index][1],invalid[index][2]));
        assert(a4_same(before));
    }
    static const int coordinates[][2]={{0,0},{64,0},{0,64},{64,64},{20,30},{65,30},{255,30},{20,65},{20,255}};
    for (unsigned index=0;index<sizeof coordinates/sizeof coordinates[0];++index)
    for (int owner=0;owner<2;++owner) for (int height=1;height<=8;height+=7) {
        terrain_mailbox_fixture(height);
        int column=coordinates[index][0],row=coordinates[index][1];
        terrain_mailbox_write(owner,1,column,row);
        game_tick();
        int valid=column<=64 && row<=64;
        assert(core_mana(owner)==10000-(valid ? (height==8 ? 10 : 14) : 0));
        assert(A4_WORD(0x2f88)==(valid ? (height==8 ? 0 : 1) : 123));
        for (int y=0;y<=64;++y) for (int x=0;x<=64;++x)
            assert(core_vertex(x,y)==height+(valid && height<8 && x==column && y==row));
        if (valid) {
            assert(A4_WORD(0x2f8a)==(column<1 ? 1 : column));
            assert(A4_WORD(0x2f8c)==(column>63 ? 63 : column));
            assert(A4_WORD(0x2f8e)==(row<1 ? 1 : row));
            assert(A4_WORD(0x2f90)==(row>63 ? 63 : row));
        }
        for (int y=0;y<64;++y) for (int x=0;x<64;++x) {
            int cell=y*64+x;
            int rebuilt=valid && x>=column-1 && x<=column && y>=row-1 && y<=row;
            assert(A4_WORD(0xff88+cell*2)==(rebuilt ? 0 : 91));
            assert(map_bk2[cell]==42);
            if (!rebuilt) assert(map_blk[cell]==15 && map_alt[cell]==2);
            else if (height==8) assert(map_blk[cell]==15 && map_alt[cell]==7);
            else {
                /* Vertex at BR/BL/TR/TL gives bits 4/8/2/1. */
                int block=y==row ? (x==column ? 1 : 2) : (x==column ? 8 : 4);
                assert(map_blk[cell]==block && map_alt[cell]==1);
            }
        }
        assert_mailbox_empty();
    }
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint)
    for (int diagnostic=0;diagnostic<2;++diagnostic) for (int war=0;war<2;++war)
    for (int admitted=0;admitted<2;++admitted) {
        terrain_mailbox_fixture(1);
        paint_map=paint; diagnostic_view=diagnostic; A4_WORD(0xae80)=war;
        game_mode=GAME_MODE_NO_BUILD|GAME_MODE_BUILD_NEAR_TOWNS|GAME_MODE_BUILD_ONLY_UP;
        mode=3; core_tool=1; core_swamp_selected=1;
        A4_WORD(0x82c4+owner*46)=0;
        A4_LONG(0x848a)=20; A4_LONG(0x9a02+owner*16)=admitted ? 20 : UINT32_MAX;
        terrain_mailbox_write(owner,1,20,30); game_tick();
        /* _paint_map, like _war, skips admission and payment (asm:7189,7206). */
        assert(core_vertex(20,30)==1+(war || admitted || paint));
        assert(core_mana(owner)==(admitted ? (war || paint ? 20 : 6) : -1));
        assert(paint_map==paint && diagnostic_view==diagnostic && mode==3);
        assert(core_tool==1 && core_swamp_selected==1);
        assert_mailbox_empty();
    }
    /* Legal-height steep terrain exposes source recursion, not host cone-fill.
     * A lone height 7 becomes 8; each initially flat neighbour is raised ONCE,
     * leaving height 1, rather than being filled to height 7. */
    terrain_mailbox_fixture(0); core_set_vertex(20,30,7);
    A4_LONG(0x9a02)=10;
    terrain_mailbox_write(0,1,20,30); game_tick();
    assert(A4_WORD(0x2f88)==9 && core_mana(0)==-36);
    for (int row=0;row<=64;++row) for (int column=0;column<=64;++column) {
        int distance=abs(column-20)>abs(row-30) ? abs(column-20) : abs(row-30);
        assert(core_vertex(column,row)==(distance==0 ? 8 : distance==1 ? 1 : 0));
    }
    /* With heights 0..8, recursion descends in height at every call: radius
     * <=7, at most 225 vertices and <=1800 increments even on steep terrain.
     * A negative signed-word cost cannot be reached without corrupt heights;
     * do not fake build_count (the wrapper resets it) to manufacture that case. */
    terrain_mailbox_fixture(0); core_set_vertex(20,30,7);
    A4_WORD(0x82c4)=1;
    assert(core_sculpt(0,20,30,1));
    assert(core_vertex(21,30)==7); /* Raw recursion above leaves it at 1. */

    /* A valid height-7 cone raises 225 vertices. Admission is not affordability;
     * its 910 charge must not be truncated by core_sculpt's host -250 cap. */
    terrain_mailbox_fixture(0);
    for (int row=0;row<=64;++row) for (int column=0;column<=64;++column) {
        int distance=abs(column-20)>abs(row-30) ? abs(column-20) : abs(row-30);
        core_set_vertex(column,row,distance<7 ? 7-distance : 0);
    }
    A4_LONG(0x9a02)=10;
    terrain_mailbox_write(0,1,20,30); game_tick();
    assert(A4_WORD(0x2f88)==225 && core_mana(0)==-900);
    assert(A4_WORD(0x2f8a)==13 && A4_WORD(0x2f8c)==27);
    assert(A4_WORD(0x2f8e)==23 && A4_WORD(0x2f90)==37);
    for (int row=0;row<=64;++row) for (int column=0;column<=64;++column) {
        int distance=abs(column-20)>abs(row-30) ? abs(column-20) : abs(row-30);
        assert(core_vertex(column,row)==(distance<8 ? 8-distance : 0));
    }
    for (int row=0;row<64;++row) for (int column=0;column<64;++column) {
        int rebuilt=column>=12 && column<=27 && row>=22 && row<=37;
        assert(A4_WORD(0xff88+2*(row*64+column))==(rebuilt ? 0 : 91));
    }
    /* Explicit unsigned 32-bit subtraction crosses INT32_MIN without C UB. */
    terrain_mailbox_fixture(1);
    A4_LONG(0x848a)=A4_LONG(0x9a02)=0x80000000u;
    terrain_mailbox_write(0,1,20,30); game_tick();
    assert(A4_LONG(0x9a02)==0x7ffffff2u);
}
static void test_terrain_mailbox_isolation(void)
{
    /* Saturated commands still rebuild exactly four cells, without repainting
     * even a remote town. Compare the complete A4 image, including people,
     * population caches, frames, RNG, score, stats and navigation history. */
    terrain_mailbox_fixture(8);
    peeps[0]=1; set_peep_word(0,12,42);
    A4_LONG(0x99fe)=123; A4_LONG(0x9a0e)=456;
    A4_LONG(0xae72)=789;
    terrain_mailbox_write(0,1,20,30); terrain_mailbox_write(1,255,50,50);
    _Alignas(4) uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
    uint8_t *live=prg_a4; prg_a4=expected+32768;
    A4_LONG(0x9a02)=9990; A4_WORD(0x2f88)=0;
    A4_WORD(0x2f8a)=A4_WORD(0x2f8c)=20;
    A4_WORD(0x2f8e)=A4_WORD(0x2f90)=30;
    for (int row=29;row<=30;++row) for (int column=19;column<=20;++column) {
        map_alt[row*64+column]=7;
        A4_WORD(0xff88+2*(row*64+column))=0;
    }
    for (int owner=0;owner<2;++owner) memset(A4_DISP(0x82b6+owner*46),0,3);
    prg_a4=live;
    core_consume_terrain_mailboxes();
    assert(!memcmp(expected,prg_a4_area,sizeof expected));

    /* Movement never consumes pending terrain. Old water takes damage before
     * a later raise; new entry keeps its population until the next invocation. */
    for (int old_water=0; old_water<2; ++old_water) {
        terrain_mailbox_fixture(0);
        if (old_water) peeps[0]=18;
        map_blk[400]=0; A4_WORD(0x2fb0)=3;
        terrain_mailbox_write(0,1,16,6);
        _move_peeps();
        assert(core_vertex(16,6)==0 && A4_DISP(0x82b6)[0]==1 && core_mana(0)==10000);
        assert(peep_word(0,4)==(old_water ? 39 : 45));
        terrain_mailbox_fixture(0);
        if (old_water) peeps[0]=18;
        map_blk[400]=0; A4_WORD(0x2fb0)=3;
        terrain_mailbox_write(0,1,16,6);
        game_tick();
        assert(peep_word(0,4)==(old_water ? 39 : 45));
        assert(A4_LONG(0x99fe)==45u && !core_result); /* asm:3459-3474: pre-loss */
        assert(core_vertex(16,6)==1 && map_blk[400]==17);
        assert_mailbox_empty();
    }

    /* The guard must report the zero divisor, not silently substitute one. */
    terrain_mailbox_fixture(1);
    A4_WORD(0x82bc)=1; A4_WORD(0x82c6)=0;
    terrain_mailbox_write(0,1,20,30); terrain_mailbox_write(1,1,21,30);
    FILE *diagnostics=tmpfile(); assert(diagnostics);
    fflush(stderr); int saved_stderr=dup(STDERR_FILENO); assert(saved_stderr>=0);
    assert(dup2(fileno(diagnostics),STDERR_FILENO)>=0);
    core_consume_terrain_mailboxes();
    fflush(stderr); assert(dup2(saved_stderr,STDERR_FILENO)>=0); assert(!close(saved_stderr));
    rewind(diagnostics); char message[256]; assert(fgets(message,sizeof message,diagnostics));
    assert(strstr(message,"owner 0") && strstr(message,"zero") && strstr(message,"divisor"));
    assert(!fclose(diagnostics));
    assert(core_vertex(20,30)==1 && core_vertex(21,30)==2);
    assert(core_mana(0)==10000 && core_mana(1)==9986 && A4_WORD(0x82be)==17);
    assert_mailbox_empty();
}
static void test_terrain_mailbox(void)
{
    test_terrain_mailbox_tick(); test_terrain_mailbox_cadence(); test_terrain_mailbox_gates();
    test_terrain_mailbox_raise(); test_terrain_mailbox_isolation();
}

/* Bounded war producers: no Armageddon activation, strategic AI, heading-pointer
 * interpretation or swamp command production is admitted here. */
static void war_terrain_fixture(void)
{
    setup_magnet_walker(0);
    memset(map_blk,47,4096); map_blk[650]=15; map_blk[715]=0;
    memset(map_bk2,0,4096);
    A4_WORD(0xae80)=1; prg_quit=0;
    set_peep_word(0,12,7);
    for (int owner=0;owner<2;++owner) {
        A4_WORD(0x82c6+owner*46)=1;
        A4_LONG(0x9a02+owner*16)=100;
    }
}
static void assert_war_command(int owner, int position)
{
    assert(A4_DISP(0x82b6+owner*46)[0]==1);
    assert(A4_DISP(0x82b7+owner*46)[0]==(position&63));
    assert(A4_DISP(0x82b8+owner*46)[0]==position/64);
    assert(A4_WORD(0x82be + owner*46)==1);
}
static void test_war_terrain_bounds(void)
{
    /* Cell producer cannot accept vertex-only coordinate 64 or unsafe owners. */
    const int invalid[][2]={{-1,650},{2,650},{255,650},{0,-1},{1,4096},{0,32767}};
    for (unsigned sample=0;sample<sizeof invalid/sizeof invalid[0];++sample) {
        war_terrain_fixture();
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        core_queue_terrain_raise(invalid[sample][0],invalid[sample][1]);
        assert(a4_same(before));
    }
    const int corners[]={0,63,4032,4095};
    for (int owner=0;owner<2;++owner) for (int corner=0;corner<4;++corner) {
        war_terrain_fixture(); core_queue_terrain_raise(owner,corners[corner]);
        assert_war_command(owner,corners[corner]); assert(seed==123);
    }
    war_terrain_fixture(); A4_WORD(0xae80)=0;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    core_queue_terrain_raise(0,650); assert(a4_same(before));
}
static void test_war_terrain_magnet(void)
{
    for (int fallback=0;fallback<2;++fallback) {
        war_terrain_fixture(); core_target(0)=715;
        if (fallback) map_blk[651]=15;
        terrain_mailbox_write(0,255,50,51); A4_WORD(0x82be)=77;
        uint8_t terrain[65*65*2],blocks[4096];
        memcpy(terrain,A4_DISP(0xae86),sizeof terrain); memcpy(blocks,map_blk,sizeof blocks);
        core_move(0);
        assert_war_command(0,650); /* Before move, never destination 651 or 715. */
        assert(peep_word(0,8)==(fallback ? 651 : 650));
        assert(peeps[0]==(fallback ? 2 : 0x42));
        if (!fallback) assert(peep_word(0,6)==7);
        assert(!memcmp(terrain,A4_DISP(0xae86),sizeof terrain));
        assert(!memcmp(blocks,map_blk,sizeof blocks));
        assert(seed==123 && core_mana(0)==100 && !core_battles && !core_settlements);
    }
    /* War bypasses controller, all mode bits (including fatal water), UI/tool,
     * tendency, mana and power permission. Nonwar controller 1 requires build. */
    const int wars[]={0,1,0x8000,0xffff};
    for (int owner=0;owner<2;++owner) for (int war=0;war<4;++war)
    for (int controller=0;controller<3;++controller) for (int bits=0;bits<32;++bits) {
        war_terrain_fixture(); peeps[1]=owner; core_target(owner)=715;
        A4_WORD(0xae80)=wars[war]; A4_WORD(0x82bc+owner*46)=controller;
        game_mode=bits; mode=3; core_tool=core_swamp_selected=1;
        core_tendency(owner)=3; A4_LONG(0x9a02+owner*16)=0x80000000u;
        A4_WORD(0x82c4+owner*46)=0;
        assert(core_magnet_direction(0)==999);
        if (war || (controller==1 && !(bits&4))) assert_war_command(owner,650); else assert_mailbox_empty();
        assert(seed==123 && A4_LONG(0x9a02+owner*16)==0x80000000u);
    }
    for (int byte=14;byte<18;++byte) {
        war_terrain_fixture(); core_target(0)=715; peeps[byte]=0xff;
        assert(core_magnet_direction(0)==999); assert_mailbox_empty();
        assert(peeps[byte]==0xff && seed==123); /* Opaque, not heading pointers. */
    }
    war_terrain_fixture(); core_target(0)=715; map_blk[715]=47;
    assert(core_magnet_direction(0)==65); assert_mailbox_empty(); /* Direct rock returns first. */
    war_terrain_fixture(); core_target(0)=715; map_blk[651]=47;
    assert(core_magnet_direction(0)==999); assert_war_command(0,650); /* Fallback rock rejects. */
    /* Source-valid 1 (off-map), not water 3: no mailbox at the top edge. */
    war_terrain_fixture(); set_peep_word(0,8,0); core_target(0)=0;
    assert(core_magnet_direction(0)==999); assert_mailbox_empty();
}
/* war_terrain_fixture leaves both owners unlatched with tendency 0, so a
 * fixture's first pass queues 14/1/1 (owner 0) and 14/3/1 (owner 1) and
 * draws 123 -> 15930 -> 28505 (asm:3254-3270, 9612-9633), unless a later
 * war raise of the same pass replaced that owner's command. */
static void assert_war_fixture_picks(int replaced)
{
    for (int side=0; side<2; ++side) {
        if (side==replaced) continue;
        assert(A4_DISP(0x82b6+side*46)[0]==14 && A4_DISP(0x82b7+side*46)[0]==(side ? 3 : 1));
        assert(A4_DISP(0x82b8+side*46)[0]==1 && A4_WORD(0x82be + side*46)==1);
    }
}
static void test_war_terrain_old_water(void)
{
    war_terrain_fixture(); map_blk[650]=0;
    _move_peeps();
    assert(peep_word(0,4)==100 && peeps[0]==18); assert_war_fixture_picks(-1);
    _move_peeps();
    assert(peep_word(0,4)==98 && peeps[0]==18); assert_war_command(0,650);
    assert(core_vertex(10,10)==0 && seed==28505 && core_mana(0)==100);
    for (int owner=0;owner<2;++owner) for (int war=0;war<2;++war)
    for (int fatal=0;fatal<2;++fatal) for (int land=0;land<2;++land)
    for (int no_build=0;no_build<2;++no_build) for (int controller=0;controller<3;++controller) {
        war_terrain_fixture(); peeps[1]=owner; peeps[0]=18;
        A4_WORD(0xae80)=war; game_mode=26|(no_build ? 4 : 0)|fatal;
        A4_WORD(0x82bc+owner*46)=controller; A4_WORD(0x82c4+owner*46)=0;
        A4_LONG(0x9a02+owner*16)=UINT32_MAX;
        map_blk[650]=land ? 15 : 0;
        /* Metadata guard belongs ONLY to magnet, not this entry flag producer. */
        peeps[14]=0xff;
        _move_peeps();
        if (!fatal && (war || (controller==1 && !no_build))) { assert_war_command(owner,650); assert_war_fixture_picks(owner); }
        else assert_war_fixture_picks(-1);
        assert(peep_word(0,4)==(fatal ? 0 : 98));
        if (land) assert(peeps[0]==(fatal ? 0 : 2) && peep_word(0,12)==(fatal ? 7 : 1));
        assert(core_vertex(10,10)==0 && seed==28505 && core_mana(owner)==-1);
    }
    /* Existing host battle exemption and recovery stay unchanged, even with
     * flag16: no new source fatal-on-land or return-to-land attrition imported. */
    war_terrain_fixture(); peeps[0]=24; map_blk[650]=0;
    _move_peeps(); assert_war_fixture_picks(-1); assert(peep_word(0,4)==100);
    /* Land primary-water queues despite fatal mode; old wet fatal never queues. */
    war_terrain_fixture(); game_mode=1;
    _move_peeps(); assert_war_command(0,650); assert_war_fixture_picks(0); assert(peep_word(0,4)==99);
    war_terrain_fixture(); game_mode=1; peeps[0]=18; map_blk[650]=0;
    _move_peeps(); assert_war_fixture_picks(-1); assert(!peep_word(0,4));
}
static void test_war_terrain_order_and_death(void)
{
    for (int reverse=0;reverse<2;++reverse) for (int separate=0;separate<2;++separate) {
        war_terrain_fixture(); no_peeps=2;
        memcpy(peeps+22,peeps,22); peeps[23]=separate;
        memset(map_who,0,4096);
        int first=reverse ? 780 : 650, last=reverse ? 650 : 780;
        set_peep_word(0,8,first); set_peep_word(1,8,last);
        map_who[first]=1; map_who[last]=2;
        map_blk[780]=15; map_blk[845]=0;
        map_blk[651]=map_blk[781]=15;
        _move_peeps();
        assert_war_command(0,separate ? first : last);
        if (separate) assert_war_command(1,last);
        else assert_war_fixture_picks(0);
        assert(peep_word(0,8)==first+1 && peep_word(1,8)==last+1);
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
            assert(core_vertex(column,row)==0); /* Full movement finishes before execution. */
        assert(seed==28505 && core_mana(0)==100 && core_mana(1)==100);
        core_consume_terrain_mailboxes();
        assert(core_vertex(last%64,last/64)==1);
        assert(core_vertex(first%64,first/64)==separate);
        /* Source consumer executes owner 0 then 1, regardless of walker cells. */
        assert(A4_WORD(0x2f8a)==last%64 && A4_WORD(0x2f8e)==last/64);
        assert_mailbox_empty(); assert(seed==28505);
    }
    /* The last enemy dying later in the pass still counted its pre-loss 1
     * (asm:3459-3474), so asm:4323-4347 does not end this pass: both owners'
     * producers execute, and the win follows on the next pass. */
    war_terrain_fixture(); no_peeps=2;
    peeps[22]=18; peeps[23]=1; set_peep_word(1,4,1); set_peep_word(1,8,900);
    map_blk[900]=0; map_who[900]=2;
    game_tick();
    assert(!core_result && !peep_word(1,4) && peep_word(0,4)==99);
    assert(A4_LONG(0x99fe)==100 && A4_LONG(0x9a0e)==1);
    assert(core_vertex(10,10)==1 && core_vertex(4,14)==1 && seed==28505);
    assert_mailbox_empty();
    game_tick(); assert(core_result==1 && !A4_LONG(0x9a0e));
    /* A dying old-water producer belongs to the owner, not the person. */
    war_terrain_fixture(); no_peeps=3; peeps[0]=18; map_blk[650]=0; set_peep_word(0,4,1);
    for (int person=1;person<3;++person) {
        peeps[person*22]=0x80; peeps[person*22+1]=person-1;
        set_peep_word(person,4,100); set_peep_word(person,6,100);
        set_peep_word(person,8,900+person); map_who[900+person]=person+1;
    }
    game_tick();
    assert(!peep_word(0,4) && !core_result && core_population(0)==100 && core_population(1)==100);
    assert(core_vertex(10,10)==1 && map_blk[650]==17 && core_mana(0)==100 && seed==28505);
    assert_mailbox_empty();
}
static void test_water_loss_rescue(void)
{
    for (int owner=0; owner<2; ++owner) {
        war_terrain_fixture(); peeps[0]=0x96; peeps[1]=owner; map_blk[650]=0;
        set_peep_word(0,4,3); set_peep_word(0,6,-32768); set_peep_word(0,12,32767);
        no_peeps=3;
        for (int person=1; person<3; ++person) {
            peeps[person*22]=8; peeps[person*22+1]=person-1;
            set_peep_word(person,4,100); set_peep_word(person,6,person);
            set_peep_word(person,8,900+person); map_who[900+person]=person+1;
        }
        /* Start-only setup: source producer, deferred terrain consumer, then
         * unchanged host dry recovery; no state injection between steps. */
        _move_peeps();
        assert(peep_word(0,4)==1 && peeps[0]==0x96 && peep_word(0,12)==32767);
        assert(peep_word(0,6)==-32768 && map_blk[650]==0 && core_vertex(10,10)==0);
        assert_war_command(owner,650);
        core_consume_terrain_mailboxes();
        assert(map_blk[650]==17 && core_vertex(10,10)==1 && peep_word(0,4)==1);
        assert_mailbox_empty();
        game_tick();
        assert(peeps[0]==2 && peep_word(0,4)==1 && peep_word(0,12)==1);
        assert(peep_word(0,6)==-32768 && peep_word(0,8)==650 && map_who[650]==1);
        assert(!core_result && !core_battles && !core_settlements && seed==28505 /* two AI picks */);
        assert(core_population(owner)==101 && core_population(1-owner)==100);
        assert_mailbox_empty();
    }
}

static void test_water_recovery_rescue(void)
{
    /* Real deferred command consumption: exact-18 wet loss -> raised terrain
     * -> dry loss/frame. No edits between calls; both owners stay alive through
     * rescue, including the case that dies on the next dry update. */
    for (int owner=0; owner<2; ++owner) for (int low=0; low<2; ++low) {
        war_terrain_fixture(); peeps[0]=18; peeps[1]=owner; map_blk[650]=0;
        set_peep_word(0,4,low ? 3 : 100); set_peep_word(0,12,7);
        no_peeps=3;
        for (int person=1; person<3; ++person) {
            peeps[person*22]=2; peeps[person*22+1]=person-1;
            set_peep_word(person,4,100);
            set_peep_word(person,8,900+person); map_who[900+person]=person+1;
        }
        game_tick();
        assert(peeps[0]==18 && peep_word(0,4)==(low ? 1 : 98) && peep_word(0,12)==7);
        assert(map_blk[650]==17 && core_vertex(10,10)==1 && !core_result);
        assert(core_population(owner)==(low ? 101 : 198) && core_population(1-owner)==100);
        assert_mailbox_empty();
        game_tick();
        assert(peeps[0]==(low ? 0 : 2) && peep_word(0,4)==(low ? 0 : 96));
        assert(peep_word(0,12)==(low ? 0 : 1) && peep_word(0,8)==650);
        assert(map_who[650]==(low ? 0 : 1) && core_vertex(10,10)==2);
        assert(core_population(owner)==(low ? 100 : 196) && core_population(1-owner)==100);
        assert(!core_result && !core_battles && !core_settlements && seed==28505 /* two AI picks */);
        assert_mailbox_empty();
    }
    /* A previously queued raise can rescue an ordinary new-water entry even
     * in fatal mode. The next exact-18 update must still remove it on dry land. */
    for (int owner=0; owner<2; ++owner) {
        war_terrain_fixture(); peeps[1]=owner; map_blk[650]=0; game_mode=1;
        no_peeps=2; peeps[22]=2; peeps[23]=1-owner;
        set_peep_word(1,4,100); set_peep_word(1,8,901);
        map_who[901]=2;
        terrain_mailbox_write(owner,1,10,10);
        /* The latch is clear and both tendencies are 0, so _set_devil_magnet
         * (asm:9612-9633) overwrites the queued raise with 14/x/1 before
         * _get_message: no rescue, tendency x instead (1 owner 0, 3 owner 1). */
        game_tick();
        assert(peeps[0]==18 && peep_word(0,4)==100 && peep_word(0,12)==7);
        assert(map_blk[650]==0 && core_vertex(10,10)==0 && !core_result);
        assert(core_tendency(0)==1 && core_tendency(1)==3);
        assert(core_population(0)==100 && core_population(1)==100); assert_mailbox_empty();
        game_tick();
        assert(!peeps[0] && !peep_word(0,4) && peep_word(0,12)==7 && !map_who[650]);
        /* asm:3459-3474: the removed resident's pre-removal 100 counts. */
        assert(!core_result && A4_LONG(0x99fe + owner*16)==100 && peep_word(1,4)==100);
        assert(core_vertex(10,10)==0 && seed==28505 /* two AI picks */ && !core_battles); assert_mailbox_empty();
        game_tick(); assert(core_result==(owner ? 1 : 2)); /* asm:4323-4347 */
    }
}

static void test_water_wait_rescue(void)
{
    const int states[]={0x32,0x52,0x72};
    for (int owner=0; owner<2; ++owner) for (unsigned state=0; state<3; ++state)
    for (int population=3; population<=6; ++population) {
        war_terrain_fixture(); peeps[0]=states[state]; peeps[1]=owner; map_blk[650]=0;
        set_peep_word(0,4,population); set_peep_word(0,6,14); set_peep_word(0,12,5);
        no_peeps=3;
        for (int person=1; person<3; ++person) {
            peeps[person*22]=2; peeps[person*22+1]=person-1;
            set_peep_word(person,4,100); set_peep_word(person,8,900+person);
            map_who[900+person]=person+1;
        }
        /* No injection between wet producer, deferred terrain, and dry waiting. */
        game_tick();
        assert(peeps[0]==states[state] && peep_word(0,4)==population-2);
        assert(peep_word(0,12)==5 && peep_word(0,6)==14);
        assert(map_blk[650]==17 && core_vertex(10,10)==1 && !core_result);
        assert(core_population(owner)==population+98 && core_population(1-owner)==100);
        assert_mailbox_empty();
        game_tick();
        assert(peeps[0]==(population==6 ? states[state]-16 : 0));
        assert(peep_word(0,4)==(population==6 ? 1 : 0));
        assert(peep_word(0,12)==(population==4 ? 5 : 102) && peep_word(0,6)==15);
        assert(peep_word(0,8)==650 && map_who[650]==(population==6));
        assert(core_vertex(10,10)==2 && peep_word(1,12)==2 && peep_word(2,12)==2);
        assert(core_population(owner)==(population==6 ? 101 : 100) && core_population(1-owner)==100);
        assert(!core_result && !core_battles && !core_settlements && seed==28505 /* two AI picks */);
        assert_mailbox_empty();
    }
}

static void test_water_remnant_rescue(void)
{
    const struct { int initial, wet, dry; } populations[]={{5,3,1},{4,2,0},{3,1,0}};
    for (int owner=0; owner<2; ++owner) for (unsigned sample=0; sample<3; ++sample) {
        war_terrain_fixture(); peeps[0]=0x90; peeps[1]=owner; map_blk[650]=0;
        set_peep_word(0,4,populations[sample].initial); set_peep_word(0,6,40); set_peep_word(0,12,42);
        no_peeps=3;
        for (int person=1; person<3; ++person) {
            peeps[person*22]=2; peeps[person*22+1]=person-1;
            set_peep_word(person,4,100); set_peep_word(person,8,900+person);
            map_who[900+person]=person+1;
        }
        /* No injection after setup: wet producer -> deferred terrain -> dry
         * remnant, including zero and negative post-loss words on dry land. */
        game_tick();
        assert(peeps[0]==0x90 && peep_word(0,4)==populations[sample].wet);
        assert(peep_word(0,6)==40 && peep_word(0,12)==42 && !core_result);
        assert(map_blk[650]==17 && core_vertex(10,10)==1); assert_mailbox_empty();
        game_tick();
        assert(peeps[0]==(populations[sample].dry ? 0x80 : 0));
        assert(peep_word(0,4)==populations[sample].dry && peep_word(0,6)==39);
        assert(peep_word(0,12)==42 && peep_word(0,8)==650 && peep_word(0,10)==0);
        assert(map_who[650]==(populations[sample].dry ? 1 : 0) && core_vertex(10,10)==2);
        assert(peep_word(1,12)==2 && peep_word(2,12)==2);
        assert(core_population(owner)==100+populations[sample].dry && core_population(1-owner)==100);
        assert(!core_result && !core_battles && !core_settlements && seed==28505 /* two AI picks */); assert_mailbox_empty();
    }
    for (int owner=0; owner<2; ++owner) for (int terminal=0; terminal<2; ++terminal)
    for (int dry=0; dry<2; ++dry) {
        war_terrain_fixture(); peeps[0]=0x90; peeps[1]=owner; map_blk[650]=dry ? 17 : 0;
        set_peep_word(0,4,1); set_peep_word(0,6,40); set_peep_word(0,12,42);
        no_peeps=terminal ? 2 : 3;
        for (int person=1; person<no_peeps; ++person) {
            peeps[person*22]=2; peeps[person*22+1]=person==1 ? 1-owner : owner;
            set_peep_word(person,4,100); set_peep_word(person,8,900+person);
            map_who[900+person]=person+1;
        }
        game_tick();
        assert(!peeps[0] && !peep_word(0,4) && peep_word(0,6)==(dry ? 39 : 40));
        assert(peep_word(0,12)==42 && !map_who[650]);
        /* asm:3459-3474: the dying remnant's 1 counts this pass, so the game
         * continues and the dying producer still raises for its owner. */
        assert(!core_result && A4_LONG(0x99fe + owner*16)==(terminal ? 1 : 101));
        assert(core_vertex(10,10)==1 && map_blk[650]==17);
        assert(peep_word(1,12)==1 && !core_battles && !core_settlements && seed==28505 /* two AI picks */);
        assert_mailbox_empty();
        /* asm:4323-4347: the terminal end follows on the next pass. */
        if (terminal) { game_tick(); assert(core_result==(owner ? 1 : 2)); }
    }
}

static void test_water_celebration_rescue(void)
{
    /* No mid-sequence edits: wet old flags -> deferred raise -> dry recovery.
     * Two other owners survive so a dying producer cannot stop command use. */
    for (int owner=0; owner<2; ++owner) for (int state=0x15; state<=0x16; ++state)
    for (int population=3; population<=5; ++population) {
        war_terrain_fixture(); peeps[0]=state; peeps[1]=owner; map_blk[650]=0;
        set_peep_word(0,4,population); set_peep_word(0,12,87);
        no_peeps=3;
        for (int person=1; person<3; ++person) {
            peeps[person*22]=2; peeps[person*22+1]=person-1;
            set_peep_word(person,4,100); set_peep_word(person,8,900+person);
            map_who[900+person]=person+1;
        }
        game_tick();
        assert(peeps[0]==state && peep_word(0,4)==population-2 && peep_word(0,12)==87);
        assert(map_blk[650]==17 && core_vertex(10,10)==1 && !core_result);
        assert_mailbox_empty();
        game_tick();
        assert(peeps[0]==(population==5 ? state-20 : 0));
        assert(peep_word(0,4)==(population==5 ? 1 : 0));
        assert(peep_word(0,12)==(population==4 ? 87 : state==0x15 ? 32 : 0));
        assert(peep_word(0,8)==650 && map_who[650]==(population==5));
        assert(core_vertex(10,10)==2 && peep_word(1,12)==2 && peep_word(2,12)==2);
        assert(core_population(owner)==(population==5 ? 101 : 100));
        assert(core_population(1-owner)==100 && seed==28505 /* two AI picks */);
        assert(!core_result && !core_battles && !core_settlements);
        assert_mailbox_empty();
    }
}

static void test_war_terrain_obstacle(void)
{
    /* Start-only fixture: two height-1 islands separated by a four-vertex sea
     * gap. No terrain/population/command injection after this setup. This is a
     * movement prerequisite for later usercast, NOT full original-match parity. */
    war_terrain_fixture(); memset(map_who,0,4096); memset(map_blk,0,4096);
    for (int row=30;row<=34;++row) for (int column=26;column<=37;++column)
        if (column<=29 || column>=34) core_set_vertex(column,row,1);
    _make_map(); no_peeps=2; memset(peeps,0,208*22);
    for (int person=0;person<2;++person) {
        int position=32*64+(person ? 35 : 28);
        peeps[person*22]=2; peeps[person*22+1]=person;
        peeps[person*22+2]=1; peeps[person*22+3]=100;
        set_peep_word(person,4,1000); set_peep_word(person,8,position);
        set_peep_word(person,12,7); map_who[position]=person+1;
    }
    assert(map_blk[32*64+31]==0 && core_vertex(31,32)==0);
    int raised=0,first_battle=0;
    for (int tick=1;tick<=2000 && !core_result;++tick) {
        game_tick();
        if (core_vertex(31,32)>0) raised=1;
        if (core_battles && !first_battle) first_battle=tick;
        if (!core_battles) assert(seed==28505); /* the two first-tick AI picks */
        assert_mailbox_empty();
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
            assert(core_vertex(column,row)>=0 && core_vertex(column,row)<=8);
    }
    printf("war obstacle: raised=%d battle_tick=%d outcome=%d tick=%d populations=%d/%d\n",
           raised,first_battle,core_result,game_turn,core_population(0),core_population(1));
    /* Phased movement (_move_explorer asm:4438-4617): both walkers enter
     * 2080 on tick 41 while their claims stay at 2079/2016, so the encounter
     * is found by the next departure's current-cell check, one 8-frame
     * cycle later (formerly 41/45 under immediate destination occupancy).
     * The two first-tick AI picks start the battle rolls at 28505: tick 50
     * rolls 3,1 (lesser 993, loss 9*100+10: 993 -> 83), tick 51 rolls 1,2
     * (loss 10: -> 73), tick 52 rolls 2,3 (lesser 146, loss 110): both die
     * on turn 52 (from seed 123 the same fight ended on turn 53). Slot 0 is
     * counted (asm:3459-3474) before it dispatches the battle (asm:4270-4282);
     * slot 1 dies there before its own visit and is never counted, so the
     * asm:4323-4347 check sees 73/0: won, not a draw (the original has none). */
    assert(raised && first_battle==49 && core_result==1 && game_turn==52);
    assert(A4_LONG(0x99fe)==73 && A4_LONG(0x9a0e)==0);
}
static void test_war_terrain(void)
{
    test_war_terrain_bounds(); test_war_terrain_magnet(); test_war_terrain_old_water();
    test_war_terrain_order_and_death(); test_war_terrain_obstacle();
}

static void armageddon_fixture(void)
{
    flood_fixture(1); prg_quit=0;
    A4_LONG(0x84a6)=80000;
    A4_LONG(0x9a02)=A4_LONG(0x9a12)=80000;
    A4_WORD(0x82c4)=A4_WORD(0x82c4+46)=0x100;
    core_tool=core_swamp_selected=1; xoff=12; yoff=19;
    core_leader(0)=3; core_leader(1)=7; core_target(0)=650; core_target(1)=3000;
    A4_WORD(0xae78)=789; A4_WORD(0xae76)=987;
    for (int owner=0;owner<2;++owner) {
        memset(A4_DISP(0x82b6+owner*46),0x5a,3);
        A4_WORD(0x82be + owner*46)=1;
    }
}

static void test_armageddon_locks(void)
{
    const int wars[]={1,2,0x7fff,0x8000,0xffff};
    for (unsigned sample=0;sample<sizeof wars/sizeof wars[0];++sample)
    for (int owner=0;owner<2;++owner) for (int direction=-1;direction<=1;direction+=2) {
        armageddon_fixture(); A4_WORD(0xae80)=wars[sample];
        A4_WORD(0x82c4+owner*46)=0xffff;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_sculpt(owner,20,20,direction));
        assert(a4_same(before));
        assert(core_raw_raise(owner,20,20));
        assert(core_vertex(20,20)==2 && core_mana(owner)==80000);
    }
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    for (int diagnostic=0;diagnostic<2;++diagnostic)
    for (int tool=0;tool<2;++tool) for (int armed=0;armed<2;++armed)
    for (int button=SDL_BUTTON_LEFT;button<=SDL_BUTTON_RIGHT;button+=2) {
        diagnostic_view=diagnostic; xoff=yoff=6;
        core_tool=tool; core_swamp_selected=armed;
        A4_WORD(0xae80)=-1; A4_LONG(0x9a02)=160000;
        A4_WORD(0x82c4)=0xffff; core_leader(0)=1;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        if (!diagnostic) iso_click_vertex(10,10,button);
        else {
            int width,height; SDL_GetWindowSize(prg_screen->window,&width,&height);
            SDL_Event click={.type=SDL_MOUSEBUTTONDOWN}; click.button.button=button;
            click.button.x=80*width/320; click.button.y=80*height/256;
            game_handle_event(&click);
        }
        assert(a4_same(before));
        assert(core_tool==tool && core_swamp_selected==armed);
    }
    diagnostic_view=0; xoff=yoff=6;
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_RIGHT;
    game_handle_event(&key); assert(xoff==7);
    int width,height; SDL_GetWindowSize(prg_screen->window,&width,&height);
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN}; click.button.button=SDL_BUTTON_LEFT;
    click.button.x=64*width/320; click.button.y=32*height/256;
    game_handle_event(&click); assert(xoff==28 && yoff==28 && core_swamp_selected==1);
    key.key.keysym.sym=SDLK_SPACE; game_handle_event(&key); assert(pause);
    game_handle_event(&key); assert(!pause);
    key.key.keysym.sym=SDLK_r; game_handle_event(&key); assert(!A4_WORD(0xae80));
    key.key.keysym.sym=SDLK_ESCAPE; game_handle_event(&key); assert(prg_quit);
    _closeall(); SDL_Quit();
}

static void test_armageddon_cast(void)
{
    /* Catch payment before validation, signed admission, editor bypass, and
     * confusing the low byte build permission with the high byte war bit. */
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint)
    for (int gate=0;gate<13;++gate) {
        if (paint && gate>=2 && gate<=6) continue;   /* asm:8484 bypass, test_paint_map_powers */
        armageddon_fixture(); paint_map=paint; int side=owner;
        if (gate==0) side=-1;
        if (gate==1) side=2;
        if (gate==2) A4_LONG(0x9a02+owner*16)=79999;
        if (gate==3) A4_LONG(0x9a02+owner*16)=0xffffffffu;
        if (gate==4) A4_WORD(0x82c4+owner*46)=1;
        if (gate==5) A4_WORD(0x82c4+owner*46)=0xfeff;
        if (gate==6) pause=1;
        if (gate==7) prg_quit=1;
        if (gate>=8 && gate<=10) core_result=gate-7;
        if (gate==11) no_peeps=-1;
        if (gate==12) no_peeps=209;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_war(side)); assert(a4_same(before));
        SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_a;
        player=side; memcpy(before,prg_a4_area,sizeof before);
        game_handle_event(&key); assert(a4_same(before));
        assert(core_tool==1 && core_swamp_selected==1 && !core_battles && !core_settlements);
    }
    const int counts[]={0,1,7,207,208};
    const int wars[]={0,1,2,0x7fff,0x8000,0xffff};
    const int flags[]={2,1,8,0x22,0x42,0x80,18};
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int paint=0;paint<2;++paint) for (unsigned count=0;count<5;++count)
    for (unsigned active=0;active<6;++active) {
        armageddon_fixture(); player=local; paint_map=paint;
        no_peeps=counts[count]; A4_WORD(0xae80)=wars[active];
        game_mode=0xffff; A4_LONG(0xae72)=17;
        for (int index=0;index<208*22;++index) peeps[index]=(uint8_t)(index%255+1);
        memset(peeps+208*22,0xa5,22); /* Canary beyond the last allocated record. */
        for (int person=0;person<208;++person) {
            peeps[person*22]=flags[person%7]; peeps[person*22+1]=person%2;
            set_peep_word(person,4,person%3==0 ? 0 : person%3==1 ? -1 : 100);
        }
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(core_war(owner));
        /* _paint_map jumps past payment and score (asm:8484-8516). */
        assert(core_mana(owner)==(paint ? 80000 : 0) && core_mana(1-owner)==80000);
        assert(A4_LONG(0xae72)==(owner==local && !paint ? 5017u : 17u));
        assert(A4_WORD(0x2fac)==73 && A4_WORD(0xae80)==1);
        for (int person=0;person<no_peeps;++person)
            for (int byte=14;byte<18;++byte) assert(!peeps[person*22+byte]);
        /* Restore ONLY the documented cast writes, then compare every A4 byte:
         * no target reset, town teardown, RNG, map/history/leader/mailbox change,
         * no record filtering or overclear at either count or allocation limit. */
        A4_LONG(0x9a02+owner*16)=80000; A4_LONG(0xae72)=17;
        A4_WORD(0x2fac)=0; A4_WORD(0xae80)=wars[active];
        for (int person=0;person<no_peeps;++person)
            memcpy(peeps+person*22+14,before+(peeps-prg_a4_area)+person*22+14,4);
        assert(a4_same(before));
        assert(core_tool==1 && core_swamp_selected==1 && !core_result && !core_battles && !core_settlements);
    }
    /* Active cost, signed equality, and defined SUB.L/ADD.L wrap, not caps. */
    armageddon_fixture(); A4_LONG(0x84a6)=0xffffffffu; A4_LONG(0x9a02)=0x7fffffffu;
    A4_LONG(0xae72)=0xfffffff0u;
    assert(core_war(0)); assert(A4_LONG(0x9a02)==0x80000000u && A4_LONG(0xae72)==4984);
    armageddon_fixture(); A4_LONG(0x84a6)=0x80000000u; A4_LONG(0x9a02)=0x80000000u;
    assert(core_war(0) && A4_LONG(0x9a02)==0);
}

static void test_armageddon_render(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    A4_WORD(0xae80)=1; core_tool=core_swamp_selected=1;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    game_render(); assert(a4_same(before));
    const char *title=SDL_GetWindowTitle(prg_screen->window);
    assert(strstr(title,"WAR") && strstr(title,"A Armageddon 80000") && strstr(title,"Esc quit"));
    mousex=-100; memcpy(before,prg_a4_area,sizeof before); game_render(); assert(a4_same(before));
    uint32_t hud[320*56]; memcpy(hud,prg_screen->chunky_buffer+320*200,sizeof hud);
    /* Original 8x8 font, 40 columns x 7 lines (strings <= 40 chars). */
    const char *lines[]={
        "WAR BLUE","WAR: TERRAIN+MAGNET LOCKED, ARROWS/MAP","AFTER TARGET: MAGNET / D VIEW",
        "0 GATHER 1 SETTLE 2 JOIN 3 FIGHT R RESET","S SWAMP 5000 Q QUAKE 2500 K KNIGHT 7500",
        "V VOLCANO 10000 F FLOOD 40000 M MAGNET","A ARMAGEDDON 80000 SPC PAUSE ESC QUIT"
    };
    for (int line=0;line<7;++line) assert(strlen(lines[line])<=40 && hud_text_at(line,0,lines[line]));
    game_render(); assert(a4_same(before));
    assert(!memcmp(hud,prg_screen->chunky_buffer+320*200,sizeof hud));
    pause=1; game_render(); assert(strstr(SDL_GetWindowTitle(prg_screen->window),"PAUSED"));
    core_result=1; game_render(); assert(strstr(SDL_GetWindowTitle(prg_screen->window),"VICTORY"));
    _closeall(); SDL_Quit();
}

static void test_armageddon_delayed(void)
{
    setup_war_town(0,650,1); A4_WORD(0xae80)=0;
    A4_LONG(0x84a6)=80000; A4_LONG(0x9a02)=80000; A4_WORD(0x82c4)=0x100;
    player=0; peeps[14]=0x80; core_target(0)=123; core_target(1)=456;
    A4_WORD(0xae78)=789; A4_WORD(0xae76)=987;
    uint8_t town[22]; memcpy(town,peeps,22); memset(town+14,0,4);
    assert(core_war(0)); assert(!memcmp(town,peeps,22));
    assert(core_target(0)==123 && core_target(1)==456 && map_bk2[650]);
    game_tick();
    assert(core_target(0)==0x820 && core_target(1)==0x820);
    assert(A4_WORD(0xae78)==0x820 && A4_WORD(0xae76)==0x820);
    assert(peeps[0]==2 && peep_word(0,8)==650 && peep_word(0,4)==3100);
    assert(!map_bk2[650]);
}

static void test_armageddon_islands(void)
{
    for (int local=0;local<2;++local) {
        /* Controlled legal-height setup only. Funding is a fixture, not a
         * claim that a human earned mana. No world injection after SDL A. */
        war_terrain_fixture(); A4_WORD(0xae80)=0; player=local; start_state();
        memset(map_who,0,4096); memset(map_blk,0,4096);
        for (int row=30;row<=34;++row) for (int column=26;column<=37;++column)
            if (column<=29 || column>=34) core_set_vertex(column,row,1);
        _make_map(); no_peeps=2; memset(peeps,0,208*22);
        for (int person=0;person<2;++person) {
            int position=32*64+(person ? 35 : 28);
            peeps[person*22]=2; peeps[person*22+1]=person;
            peeps[person*22+2]=1; peeps[person*22+3]=person ? 1 : 100;
            set_peep_word(person,4,person ? 1000 : 3000);
            set_peep_word(person,8,position); set_peep_word(person,12,7);
            peeps[person*22+14]=0x80; map_who[position]=person+1;
            A4_WORD(0x82c4+person*46)=0xffff;
        }
        A4_LONG(0x84a6)=80000; A4_LONG(0x9a02+local*16)=80000;
        SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_a;
        game_handle_event(&key); assert(A4_WORD(0xae80)==1 && core_mana(local)==0);
        int raised=0,first_battle=0;
        for (int tick=1;tick<=2000 && !core_result;++tick) {
            game_tick();
            if (core_vertex(31,32)>0) raised=1;
            if (core_battles && !first_battle) first_battle=tick;
            /* Only the two first-tick AI picks (both tendencies 0, latches
             * clear, controllers 0 so never re-armed) draw before a battle. */
            if (!core_battles) assert(seed==28505);
            assert_mailbox_empty();
            for (int vertex=0;vertex<4225;++vertex)
                assert(core_vertex(vertex%65,vertex/65)>=0 && core_vertex(vertex%65,vertex/65)<=8);
        }
        printf("Armageddon cast islands: caster=%d raised=%d battle=%d outcome=%d tick=%d populations=%d/%d\n",
               local,raised,first_battle,core_result,game_turn,core_population(0),core_population(1));
        /* asm:4323-4347 is _player-relative: owner 1 wiped out is a win for
         * player 0 but ___end_game(1), lost, for player 1. */
        assert(raised && first_battle && core_result==(local ? 2 : 1) && core_population(0)>0 && !core_population(1));
    }
    player=0; start_state();
}

static void test_armageddon_input_smoke(void)
{
    armageddon_fixture();
    A4_WORD(0x82c4)=0x100; A4_LONG(0x9a02)=160000;
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_a;
    key.key.repeat=1; game_handle_event(&key);
    assert(!A4_WORD(0xae80) && core_mana(0)==160000);
    key.key.repeat=0; game_handle_event(&key);
    assert(A4_WORD(0xae80)==1 && core_mana(0)==80000);
    assert(A4_LONG(0xae72)==5000 && A4_WORD(0x2fac)==73);
    game_handle_event(&key);
    assert(core_mana(0)==0 && A4_LONG(0xae72)==10000);
}

static void test_armageddon_original(const char *directory)
{
    assert(!setenv("POPULOUS_DATA_DIR",directory,1));
    char assets[4096]; snprintf(assets,sizeof assets,"%s/..",directory);
    assert(!setenv("POPULOUS_ASSET_DIR",assets,1));
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    /* An ordinary original-data start, with only fixture funding/permission.
     * No terrain/population edits; this is not human-playability evidence. */
    assert(game_reset_start((StartSelection){START_ORIGINAL,0}));
    assert_original_world(); assert(A4_LONG(0x84a6)==80000);
    A4_LONG(0x9a02)=80000; A4_WORD(0x82c4)|=0x100;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_a;
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    assert(A4_WORD(0xae80)==1 && core_mana(0)==0 && A4_WORD(0x2fac)==73);
    /* Compare the immediate world independently of its delayed consumers. */
    uint8_t after[sizeof prg_a4_area]; memcpy(after,prg_a4_area,sizeof after);
    const int writes[]={0x9a02,0xae72,0x2fac,0xae80};
    for (unsigned field=0;field<4;++field) {
        size_t offset=A4_DISP(writes[field])-prg_a4_area;
        memcpy(before+offset,after+offset,field<2 ? 4 : 2);
    }
    for (int person=0;person<no_peeps;++person) {
        for (int byte=14;byte<18;++byte) assert(!peeps[person*22+byte]);
        memset(before+(peeps-prg_a4_area)+person*22+14,0,4);
    }
    assert(a4_same(before));
    game_render(); assert(!memcmp(after,prg_a4_area,sizeof after));
    assert(strstr(SDL_GetWindowTitle(prg_screen->window),"A Armageddon 80000"));
    for (int tick=0;tick<100 && !core_result;++tick) { game_tick(); assert_live_occupancy(); }
    printf("Original-data funded SDL A smoke: tick=%d outcome=%d populations=%d/%d (no parity/playability claim)\n",
           game_turn,core_result,core_population(0),core_population(1));
    _closeall(); SDL_Quit();
}

/* Host references are slot identities, not serialized Amiga addresses. */
static void heading_fixture(int count)
{
    memset(prg_a4_area,0xa5,sizeof prg_a4_area); start_state();
    no_peeps=count;
    for (int person=0;person<208;++person) {
        peeps[person*22]=2; peeps[person*22+1]=0;
        set_peep_word(person,4,100); set_peep_word(person,8,650);
    }
}

static void assert_heading(int subject, uint32_t expected_reference)
{
    uint8_t expected[sizeof prg_a4_area];
    memcpy(expected,prg_a4_area,sizeof expected);
    size_t offset=(size_t)(peeps+subject*22+14-prg_a4_area);
    memcpy(expected+offset,&expected_reference,4);
    assert(core_get_heading(subject));
    /* Catches RNG, map, other-record and adjacent-field writes too. */
    assert(!memcmp(expected,prg_a4_area,sizeof expected));
    assert(peep_reference(subject)==(int)expected_reference-1);
}

static void test_peep_references(void)
{
    /* Catch zero-based/self-zero encoding, narrow/overwide copies, and unchecked
     * decoding. All records (including two-byte-aligned ones) round-trip. */
    heading_fixture(208);
    for (int subject=0;subject<208;++subject) {
        for (int target=-1;target<208;++target) {
            uint8_t expected[sizeof prg_a4_area];
            memcpy(expected,prg_a4_area,sizeof expected);
            uint32_t raw=(uint32_t)(target+1);
            size_t offset=(size_t)(peeps+subject*22+14-prg_a4_area);
            memcpy(expected+offset,&raw,4);
            assert(set_peep_reference(subject,target));
            assert(peep_reference(subject)==target);
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
    }
    const uint32_t raw_values[]={0,1,2,208,209,UINT32_MAX,0x53014};
    const int decoded_full[]={-1,0,1,207,-2,-2,-2};
    const int decoded_one[]={-1,0,-2,-2,-2,-2,-2};
    for (unsigned sample=0;sample<sizeof raw_values/sizeof raw_values[0];++sample) {
        heading_fixture(208);
        memcpy(peeps+14,&raw_values[sample],4);
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(peep_reference(0)==decoded_full[sample]);
        assert(a4_same(before));
        no_peeps=1;
        assert(peep_reference(0)==decoded_one[sample]);
    }
    const int counts[]={-1,0,1,207,208,209};
    const int subjects[]={INT_MIN,-1,0,1,206,207,208,INT_MAX};
    for (unsigned count=0;count<sizeof counts/sizeof counts[0];++count)
    for (unsigned sample=0;sample<sizeof subjects/sizeof subjects[0];++sample) {
        heading_fixture(counts[count]);
        int subject=subjects[sample];
        if (counts[count]>=1 && counts[count]<=208 && subject>=0 && subject<counts[count]) continue;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(peep_reference(subject)==-2);
        assert(!set_peep_reference(subject,0));
        assert(!set_peep_reference(subject,-1));
        assert(!core_get_heading(subject));
        assert(a4_same(before));
    }
    const int targets[]={INT_MIN,-2,1,207,208,INT_MAX};
    heading_fixture(1);
    for (unsigned sample=0;sample<sizeof targets/sizeof targets[0];++sample) {
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!set_peep_reference(0,targets[sample]));
        assert(a4_same(before));
    }
}

static void test_heading_selection(void)
{
    /* No eligible enemy overwrites stale/absent references with self, including
     * the first and last allocated slots. Count, not capacity, bounds the scan. */
    heading_fixture(1); peeps[23]=1; set_peep_word(1,8,650);
    assert_heading(0,1);
    heading_fixture(208); assert_heading(207,208);
    heading_fixture(208); assert(set_peep_reference(0,-1)); assert_heading(0,1);
    heading_fixture(208); peeps[207*22+1]=1; assert_heading(0,208);
    heading_fixture(208); peeps[1]=1; assert_heading(207,1);

    heading_fixture(5);
    peeps[23]=peeps[45]=peeps[67]=peeps[89]=1;
    set_peep_word(1,8,654); /* distance 4 */
    set_peep_word(2,8,648); /* distance 2 */
    set_peep_word(3,8,778); /* distance 2, loses ascending tie */
    set_peep_word(4,8,653); /* distance 3 */
    assert_heading(0,3);
    set_peep_word(4,8,650); assert_heading(0,5); /* Same-cell enemy allowed. */
    set_peep_word(1,8,650); assert_heading(0,2);

    /* Compare full unsigned owner bytes, not boolean sides or signed bytes. */
    const uint8_t owners[]={0,1,2,127,128,255};
    for (unsigned source=0;source<sizeof owners;++source)
    for (unsigned target=0;target<sizeof owners;++target) {
        heading_fixture(2); peeps[1]=owners[source]; peeps[23]=owners[target];
        assert_heading(0,owners[source]==owners[target] ? 1 : 2);
    }
    /* Every state below bit 7 is eligible, including town, battle and waiting;
     * every combination containing bit 7 is excluded, regardless of other bits. */
    const int populations[]={INT16_MIN,-1,0,1,INT16_MAX};
    for (int flags=0;flags<256;++flags)
    for (unsigned population=0;population<sizeof populations/sizeof populations[0];++population) {
        heading_fixture(2); peeps[22]=flags; peeps[23]=1;
        set_peep_word(1,4,populations[population]);
        assert_heading(0,flags<128 && populations[population]>0 ? 2 : 1);
    }
}

static void test_heading_signed_positions(void)
{
    /* Literal distances expose unsigned rows, truncation instead of ASR floor,
     * map-range rejection, toroidal wrap, and masking the complete position. */
    const int cases[][4]={
        {0,63,2,3},             /* 63 versus 2, not wrapped 1 versus 2 */
        {0,4032,128,3},         /* rows 63 versus 2, no vertical wrap */
        {0,-32768,32767,2},     /* 512 versus 574 */
        {-32768,32767,-1,3},    /* 1086 versus 574 */
        {-1,32767,-32768,2},    /* 512 versus 574 */
        {-1,0,63,3},            /* 64 versus 1 */
        {63,-1,-64,2},          /* 1 versus 64 */
        {0,-1,64,3},            /* 64 versus 1 */
        {32767,-32768,-1,3},    /* 1086 versus 512 */
        {0,32767,32767,2}       /* Farther than map rows, still eligible/tie first. */
    };
    for (unsigned sample=0;sample<sizeof cases/sizeof cases[0];++sample) {
        heading_fixture(3); peeps[23]=peeps[45]=1;
        for (int person=0;person<3;++person) set_peep_word(person,8,cases[sample][person]);
        assert_heading(0,(uint32_t)cases[sample][3]);
    }
    /* -1 is row -1, not row zero: floor changes the winner, not just distance. */
    heading_fixture(3); peeps[23]=peeps[45]=1;
    set_peep_word(0,8,63); set_peep_word(1,8,-1); set_peep_word(2,8,63);
    assert_heading(0,3);
}

static void test_heading_relocation(void)
{
    heading_fixture(208);
    assert(set_peep_reference(0,207)); assert(set_peep_reference(207,0));
    _Alignas(4) uint8_t staged[sizeof prg_a4_area];
    memcpy(staged,prg_a4_area,sizeof staged);
    uint8_t *original=prg_a4;
    prg_a4=staged+32768;
    assert(peep_reference(0)==207 && peep_reference(207)==0);
    peeps[207*22+1]=255; set_peep_word(207,4,1234);
    assert(peep_owner(peep_reference(0))==255 && peep_word(peep_reference(0),4)==1234);
    assert(core_get_heading(0)); assert(peep_reference(0)==207);
    assert(set_peep_reference(207,-1)); assert(peep_reference(207)==-1);
    prg_a4=original;
    assert(peep_owner(peep_reference(0))==0 && peep_word(peep_reference(0),4)==100);
    assert(peep_reference(207)==0);
    /* An A4 image can also be copied back without pointer fixups. */
    memcpy(prg_a4_area,staged,sizeof staged);
    assert(peep_reference(0)==207 && peep_word(peep_reference(0),4)==1234);
    assert(peep_reference(207)==-1);
}

static void test_heading_reference_transfers(void)
{
    /* Donor-self must stay donor-self on the recipient, even after donor removal
     * and reuse. Do not rebase, eagerly clear, or add generation identity. */
    for (int source=0;source<2;++source) for (int target=0;target<3;++target) {
        setup_friendly_merge(source,0);
        assert(set_peep_reference(source,target));
        uint8_t reference[4]; memcpy(reference,peeps+source*22+14,4);
        core_join_forces(source,1-source);
        assert(!peep_word(source,4) && peep_word(1-source,4)==300);
        assert(!memcmp(reference,peeps+(1-source)*22+14,4));
        assert(peep_reference(1-source)==target);
        peeps[source*22]=2; set_peep_word(source,4,77);
        assert(peep_reference(1-source)==target);
        if (target==source) assert(peep_word(peep_reference(1-source),4)==77);
    }
    for (int source=0;source<=2;source+=2) for (int owner=0;owner<2;++owner)
    for (int target=0;target<4;++target) {
        setup_reinforcement(source,owner,1,0);
        int first=source==0 ? 1 : 0, recipient=first+owner;
        assert(set_peep_reference(source,target));
        uint8_t reference[4]; memcpy(reference,peeps+source*22+14,4);
        core_join_battle(source,first);
        assert(!peep_word(source,4) && peep_word(recipient,4)==145);
        assert(!memcmp(reference,peeps+recipient*22+14,4));
        assert(peep_reference(recipient)==target);
        peeps[source*22]=2; set_peep_word(source,4,77);
        assert(peep_reference(recipient)==target);
        if (target==source) assert(peep_word(peep_reference(recipient),4)==77);
    }
    armageddon_fixture(); no_peeps=208;
    for (int person=0;person<208;++person) assert(set_peep_reference(person,207-person));
    assert(core_war(0));
    for (int person=0;person<208;++person) assert(peep_reference(person)==-1);
}

static void test_heading(void)
{
    test_peep_references(); test_heading_selection(); test_heading_signed_positions();
    test_heading_relocation(); test_heading_reference_transfers();
}

/* Navigation consumes existing host references; it does not create knights. */
static void heading_navigation_fixture(int owner)
{
    setup_magnet_walker(owner); no_peeps=3;
    for (int person=1;person<3;++person) {
        memcpy(peeps+person*22,peeps,22);
        peeps[person*22+1]=1-owner;
        set_peep_word(person,8,person==1 ? 654 : 649);
    }
    assert(set_peep_reference(0,1));
    core_target(owner)=586;
}

static void test_heading_navigation_targets(void)
{
    const int cells[]={585,649,713,586,714,587,651,715};
    const int deltas[]={-65,-1,63,-64,64,-63,1,65};
    for (int owner=0;owner<2;++owner) for (int war=0;war<2;++war) {
        heading_navigation_fixture(owner); A4_WORD(0xae80)=war;
        core_leader(owner)=3; view_who=17;
        /* Keep the far eastern target despite a closer western enemy. */
        assert(core_magnet_direction(0)==1 && peep_reference(0)==1);
        for (int sample=0;sample<8;++sample) {
            set_peep_word(1,8,cells[sample]);
            assert(core_magnet_direction(0)==deltas[sample]);
            assert((int8_t)peeps[21]==deltas[sample] && peep_reference(0)==1);
            assert(core_leader(owner)==3 && view_who==17 && seed==123);
        }
    }
    for (int owner=0;owner<2;++owner) {
        heading_navigation_fixture(owner); peeps[45]=owner;
        set_peep_word(2,8,714); core_leader(owner)=3;
        assert(core_magnet_direction(0)==1 && core_leader(owner)==3);
        core_leader(owner)=0; core_target(owner)=650;
        assert(core_magnet_direction(0)==1 && !core_leader(owner) && !view_who);
    }
    /* Refresh dead, negative, friendly, bit-7, same-cell and self references.
     * A same-cell enemy remains eligible, so refresh must not retry in a loop. */
    for (int owner=0;owner<2;++owner) for (int cause=0;cause<7;++cause) {
        heading_navigation_fixture(owner);
        if (cause==0) set_peep_word(1,4,0);
        if (cause==1) set_peep_word(1,4,-1);
        if (cause==2) peeps[23]=owner;
        if (cause==3) peeps[22]=0x82;
        if (cause==4) set_peep_word(1,8,650);
        if (cause==5) assert(set_peep_reference(0,0));
        if (cause==6) { assert(set_peep_reference(0,0)); peeps[23]=peeps[45]=owner; }
        uint8_t others[44]; memcpy(others,peeps+22,sizeof others);
        assert(core_magnet_direction(0)==(cause==4 || cause==6 ? -64 : -1));
        assert(peep_reference(0)==(cause==4 ? 1 : cause==6 ? 0 : 2));
        assert(!memcmp(others,peeps+22,sizeof others));
        assert(!core_leader(owner) && !view_who && seed==123);
        if (cause==6) {
            core_tendency(owner)=1; core_move(0);
            assert(peeps[0]==2 && peep_word(0,8)==586 && !core_settlements);
        }
    }
}

static void test_heading_navigation_bounds(void)
{
    const int bad_cells[]={INT16_MIN,-1,4096,INT16_MAX};
    for (unsigned sample=0;sample<sizeof bad_cells/sizeof bad_cells[0];++sample) {
        heading_navigation_fixture(0); set_peep_word(1,8,bad_cells[sample]);
        assert(core_magnet_direction(0)==-1 && peep_reference(0)==2);
        /* The exact selector is allowed to reselect an off-map enemy. The
         * caller must stop, not filter, wrap or use that position as a map index. */
        heading_navigation_fixture(0); set_peep_word(1,8,bad_cells[sample]);
        set_peep_word(2,4,0);
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==999 && peep_reference(0)==1);
        assert(a4_same(before));
    }
    const int counts[]={-1,0,1,208,209};
    const int subjects[]={INT_MIN,-1,0,1,207,208,INT_MAX};
    for (unsigned count=0;count<sizeof counts/sizeof counts[0];++count)
    for (unsigned sample=0;sample<sizeof subjects/sizeof subjects[0];++sample) {
        heading_navigation_fixture(0); no_peeps=counts[count];
        int subject=subjects[sample];
        if (no_peeps>0 && no_peeps<=208 && subject>=0 && subject<no_peeps) continue;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(subject)==999);
        assert(a4_same(before));
    }
    for (int invalid=0;invalid<6;++invalid) {
        heading_navigation_fixture(0);
        if (invalid<4) set_peep_word(0,8,bad_cells[invalid]);
        else peeps[1]=invalid==4 ? 2 : 255;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==999);
        assert(a4_same(before));
    }
}

static void test_heading_navigation_opaque(void)
{
    const uint32_t refs[]={4,209,0x53014,0x10001,0x01000002,0x80000000,UINT32_MAX};
    for (int owner=0;owner<2;++owner) for (unsigned sample=0;sample<sizeof refs/sizeof refs[0];++sample)
    for (int leader=0;leader<2;++leader) {
        heading_navigation_fixture(owner); memcpy(peeps+14,&refs[sample],4);
        peeps[45]=owner; set_peep_word(2,8,714);
        core_leader(owner)=leader ? 3 : 0; core_target(owner)=650; view_who=0;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==-64); /* Neither acquire nor follow. */
        peeps[21]=0;
        assert(a4_same(before));
        core_target(owner)=-1;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==999);
        assert(a4_same(before));
        core_target(owner)=4096;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==999);
        assert(a4_same(before));
    }
}

static void test_heading_navigation_dispatch(void)
{
    for (int owner=0;owner<2;++owner) for (int tendency=0;tendency<4;++tendency)
    for (int opaque=0;opaque<2;++opaque) {
        heading_navigation_fixture(owner); core_tendency(owner)=tendency;
        if (opaque) { uint32_t raw=UINT32_MAX; memcpy(peeps+14,&raw,4); core_target(owner)=654; }
        core_move(0);
        assert(peep_word(0,8)==651 && peep_word(0,10)==1 && peeps[0]==2);
        /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
        assert(map_who[650]==1 && !map_who[651] && !core_settlements && seed==123);
    }
    heading_navigation_fixture(0); core_tendency(0)=1;
    /* Keep other records inert, but eligible as targets. */
    peeps[22]=peeps[44]=4;
    memset(map_blk,47,4096); map_blk[650]=15;
    set_peep_word(0,12,6);
    _move_peeps();
    assert(peep_word(0,8)==650 && peep_word(0,12)==7 && peep_word(0,4)==100);
    _move_peeps();
    assert(peeps[0]==0x42 && peep_word(0,6)==7 && peep_word(0,12)==0);
    assert(peep_word(0,4)==99 && peep_reference(0)==1);
    map_blk[651]=15;
    for (int tick=1;tick<=8;++tick) {
        _move_peeps();
        assert(peeps[0]==0x42 && peep_word(0,8)==650 && peep_word(0,6)==7+tick);
        assert(peep_word(0,12)==(tick%2 ? 101 : 102));
    }
    _move_peeps();
    assert(peeps[0]==2 && peep_word(0,8)==651 && peep_word(0,12)==0 && peep_word(0,4)==90);
    assert(!core_settlements && peep_reference(0)==1);
}

static void test_heading_navigation_swamp(void)
{
    for (int owner=0;owner<2;++owner) for (int war=0;war<2;++war)
    for (int presence=0;presence<3;++presence) for (int fallback=0;fallback<2;++fallback) {
        heading_navigation_fixture(owner); core_target(owner)=654; A4_WORD(0xae80)=war;
        if (!presence) assert(set_peep_reference(0,-1));
        if (presence==2) { uint32_t raw=UINT32_MAX; memcpy(peeps+14,&raw,4); }
        memset(map_blk,0,4096); map_blk[650]=15;
        map_blk[fallback ? 587 : 651]=53;
        assert(core_valid_move(650,fallback ? -63 : 1)==0);
        assert(core_magnet_direction(0)==(presence ? 999 : fallback ? -63 : 1));
        assert(core_valid_move(650,fallback ? -63 : 1)==0);
        /* This fixture has controller 0: only the existing war/water producer
         * can queue. Controller-1 swamp production is covered by --swamp-rescue. */
        if (!war || !fallback || presence==2) assert_mailbox_empty();
    }
    for (int presence=1;presence<3;++presence) {
        heading_navigation_fixture(0); A4_WORD(0xae80)=1; core_target(0)=654;
        if (presence==2) { uint32_t raw=UINT32_MAX; memcpy(peeps+14,&raw,4); }
        map_blk[651]=47;
        assert(core_magnet_direction(0)==1 && (int8_t)peeps[21]==1);
        assert_mailbox_empty();
    }
}

static void test_heading_navigation_fallback(void)
{
    const int deltas[]={-64,-63,1,65,64,63,-1,-65};
    const int opposites[]={64,63,-1,-65,-64,-63,1,65};
    const int failed[]={65,-64,63,-1,-65,-64,-63,1};
    for (int owner=0;owner<2;++owner) for (int opaque=0;opaque<2;++opaque)
    for (int direction=0;direction<8;++direction) {
        heading_navigation_fixture(owner);
        int primary=(direction+1)%8;
        set_peep_word(1,8,650+deltas[primary]); core_target(owner)=650+deltas[primary];
        if (opaque) { uint32_t raw=UINT32_MAX; memcpy(peeps+14,&raw,4); }
        memset(map_blk,47,4096); map_blk[650]=15;
        map_blk[650+deltas[direction]]=15;
        assert(core_magnet_direction(0)==deltas[direction]);
        assert((int8_t)peeps[21]==opposites[direction]);
        peeps[21]=(uint8_t)deltas[direction];
        assert(core_magnet_direction(0)==999 && (int8_t)peeps[21]==failed[primary]);
        assert(peep_reference(0)==(opaque ? -2 : 1));
    }
}

static void test_heading_navigation_water(void)
{
    for (int owner=0;owner<2;++owner) for (int war=0;war<2;++war)
    for (int opaque=0;opaque<2;++opaque) {
        heading_navigation_fixture(owner); A4_WORD(0xae80)=war; core_target(owner)=654;
        if (opaque) { uint32_t raw=UINT32_MAX; memcpy(peeps+14,&raw,4); }
        memset(map_blk,47,4096); map_blk[650]=15; map_blk[651]=0;
        uint8_t blocks[4096]; memcpy(blocks,map_blk,sizeof blocks);
        assert(core_magnet_direction(0)==999); /* Fallback never accepts rock. */
        assert((int8_t)peeps[21]==63);
        if (war && !opaque) assert_war_command(owner,650); else assert_mailbox_empty();
        assert(!memcmp(blocks,map_blk,sizeof blocks) && seed==123);
    }
}

static void test_heading_leader_release(void)
{
    for (int paused=0;paused<2;++paused) for (int war=0;war<2;++war)
    for (int presence=0;presence<3;++presence) {
        setup_magnet_walker(0); no_peeps=2; pause=paused; A4_WORD(0xae80)=war;
        view_who=2;
        for (int owner=0;owner<2;++owner) {
            peeps[owner*22+1]=owner;
            set_peep_word(owner,4,0); /* Source prepass does not require life. */
            set_peep_word(owner,8,owner ? 900 : 650);
            core_leader(owner)=owner+1; core_target(owner)=1234;
            A4_WORD(owner ? 0xae76 : 0xae78)=2345; core_tendency(owner)=3;
            uint32_t raw=presence==1 ? (uint32_t)owner+1 : presence==2 ? UINT32_MAX : 0;
            memcpy(peeps+owner*22+14,&raw,4);
        }
        uint8_t records[44]; memcpy(records,peeps,sizeof records);
        _move_peeps();
        for (int owner=0;owner<2;++owner) {
            int cell=owner ? 900 : 650;
            assert(core_leader(owner)==(presence ? 0 : owner+1));
            assert(core_target(owner)==(war ? 0x820 : presence && !paused ? cell : 1234));
            assert(A4_WORD(owner ? 0xae76 : 0xae78)==(war ? 0x820 : presence && !paused ? cell : 2345));
            assert(core_tendency(owner)==3);
        }
        assert(view_who==2 && !memcmp(records,peeps,sizeof records));
    }
    const int invalid[]={-1,3,209,INT16_MAX};
    for (unsigned sample=0;sample<sizeof invalid/sizeof invalid[0];++sample) {
        setup_magnet_walker(0); set_peep_word(0,4,0);
        core_leader(0)=invalid[sample]; core_leader(1)=invalid[sample];
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        expect_ai_fixture_picks(before); /* path A: leaders are not read */
        _move_peeps(); assert(a4_same(before));
    }
    setup_magnet_walker(0); assert(set_peep_reference(0,0));
    set_peep_word(0,4,0); set_peep_word(0,8,-1); core_leader(0)=1;
    core_target(0)=1234; A4_WORD(0xae78)=2345;
    _move_peeps();
    assert(!core_leader(0) && core_target(0)==1234 && A4_WORD(0xae78)==2345);
    setup_magnet_walker(0); assert(set_peep_reference(0,0)); core_leader(0)=1; pause=1;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    game_tick(); assert(a4_same(before));
}

static void test_heading_seeded_encounter(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(game_reset(0));
    /* Seed an existing walker, not a knight producer or a combat result. */
    assert(peeps[0]==2 && core_get_heading(0));
    int target=peep_reference(0), start=peep_word(0,8);
    assert(target>=0 && peep_owner(target)==1);
    for (int tick=0;tick<2048 && !(peeps[0]&8) && peep_word(0,4)>0;++tick) game_tick();
    assert(peep_word(0,4)>0 && (peeps[0]&8) && core_battles>0);
    assert(peep_word(0,8)!=start && !core_leader(0));
    assert(peep_reference(0)==target && peep_word(0,6)==target);
    /* Remove the encountered target through existing cleanup; navigation, not
     * death cleanup, refreshes the retained reference on its next invocation. */
    core_remove(target);
    assert(peep_reference(0)==target && peeps[0]==2 && peep_word(target,4)==0);
    int population=peep_word(0,4), battles=core_battles;
    core_move(0);
    assert(peep_reference(0)!=target && peep_word(0,4)==population);
    assert(core_battles>=battles);
    _closeall(); SDL_Quit();
}

static void setup_heading_battle(int winner, int owner)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    memset(map_blk,15,4096); memset(map_bk2,37,4096);
    no_peeps=4; core_result=core_settlements=0; core_battles=17; prg_quit=0;
    int loser=3-winner;
    for (int person=1; person<=3; ++person) {
        peeps[person*22]=8; peeps[person*22+1]=person==winner ? owner : 1-owner;
        peeps[person*22+2]=3;
        set_peep_word(person,4,person==winner ? 100 : 10);
        set_peep_word(person,6,3-person); set_peep_word(person,8,650);
        set_peep_word(person,10,-64); set_peep_word(person,12,102);
        for (int byte=18; byte<22; ++byte) peeps[person*22+byte]=byte;
    }
    peeps[66]=0x80; set_peep_word(3,8,900); set_peep_word(3,6,100);
    map_who[650]=loser+1; map_who[900]=4;
    /* Previous-cell claim: _zero_population asm:5671-5680 clears it on death. */
    map_who[714]=loser+1;
    peeps[winner*22+14]=0x80;
    core_leader(owner)=winner+1; core_leader(1-owner)=0;
    core_target(0)=1234; core_target(1)=2345;
    A4_WORD(0xae78)=345; A4_WORD(0xae76)=456;
    view_who=loser+1;
    A4_WORD(0x3010)=23; A4_WORD(0x3012)=24; A4_WORD(0x3014)=25;
    A4_LONG(0x8486)=(uint32_t)-250;
    A4_LONG(0x9a02+owner*16)=1000; A4_LONG(0x9a02+(1-owner)*16)=2000;
    A4_WORD(0xae7a)=7; A4_WORD(0xae7c)=7;
    A4_WORD(0x2ff8)=1234; A4_WORD(0x2fb2)=2345;
    for (int side=0; side<2; ++side) {
        A4_WORD(0x82d2+side*46)=71+side;
        A4_WORD(0x82d4+side*46)=801+side;
    }
    seed=123;
}

static void check_heading_battle(int winner, int32_t winner_mana, int32_t loser_mana)
{
    /* Explicit full-state oracle: missing normalization, over-wide heading reads,
     * capped mana, map clobber, or global removal changes must all fail here. */
    int loser=3-winner, owner=peep_owner(winner), enemy=peep_owner(loser);
    uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
    memcpy(before,prg_a4_area,sizeof before);
    peeps[winner*22]=(peeps[winner*22]&1) ? 5 : 6;
    set_peep_word(winner,4,90); set_peep_word(winner,10,0); set_peep_word(winner,12,85);
    int position=peep_word(winner,8);
    if (!map_who[position] || map_who[position]==loser+1) map_who[position]=winner+1;
    if (peeps[winner*22+20]) {
        A4_WORD(0x82d4+owner*46)=position; A4_WORD(0x82d2+owner*46)=2;
        --peeps[winner*22+20];
    }
    if (peeps[loser*22+20]) A4_WORD(0x82d2+enemy*46)=0;
    A4_LONG(0x9a02+owner*16)=(uint32_t)winner_mana;
    A4_LONG(0x9a02+enemy*16)=(uint32_t)loser_mana;
    A4_WORD(0xae7a+owner*2)=(uint16_t)((uint16_t)A4_WORD(0xae7a+owner*2)+1u);
    if (view_who==loser+1) view_who=winner+1;
    position=peep_word(loser,8);
    if (map_who[position]==loser+1) map_who[position]=0;
    /* _zero_population asm:5671-5680: previous-cell claim. */
    int previous=(int16_t)(position-peep_word(loser,10));
    if (map_who[previous]==loser+1) map_who[previous]=0;
    if (core_leader(enemy)==loser+1) {
        core_leader(enemy)=0;
        if (!pause) { core_target(enemy)=position; A4_WORD(enemy ? 0xae76 : 0xae78)=position; }
    }
    peeps[loser*22]=0; set_peep_word(loser,4,0); set_peep_word(loser,6,0);
    seed=28505;
    memcpy(expected,prg_a4_area,sizeof expected);
    memcpy(prg_a4_area,before,sizeof before);
    core_battle(1,2);
    assert(peeps[winner*22]==expected[peeps-prg_a4_area+winner*22]);
    assert(!memcmp(expected,prg_a4_area,sizeof expected));
    assert(core_battles==17 && !core_settlements && !core_result);
}

static void test_heading_battle_presence(void)
{
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int side=0; side<=3; ++side) for (int byte=14; byte<18; ++byte)
    for (int high=0; high<2; ++high) {
        setup_heading_battle(winner,owner);
        int loser=3-winner;
        memset(peeps+winner*22+14,0,4);
        if (side&1) peeps[winner*22+byte]=high ? 0x80 : 1;
        if (side&2) peeps[loser*22+byte]=high ? 0x80 : 1;
        check_heading_battle(winner,side&2 ? 1024 : 1023,side&2 ? 1976 : 1977);
    }
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int side=0; side<=3; ++side) for (int kind=0; kind<3; ++kind)
    for (int leader=0; leader<2; ++leader) {
        setup_heading_battle(winner,owner);
        int loser=3-winner;
        memset(peeps+winner*22+14,0,4);
        for (int person=1; person<=2; ++person) {
            if (!(side&(person==winner ? 1 : 2))) continue;
            if (kind==2) memset(peeps+person*22+14,0xff,4);
            else assert(set_peep_reference(person,kind ? person : 3));
        }
        if (leader) core_leader(1-owner)=loser+1;
        int reward=(side&2) ? 24 : leader ? 25 : 23;
        check_heading_battle(winner,1000+reward,2000-reward);
    }
}

static void test_heading_battle_state(void)
{
    const uint8_t flags[]={8,9,0x68,0x69}, markers[]={0,1,255};
    const uint16_t counters[]={7,0x7fff,0xffff};
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int headed=0; headed<2; ++headed)
    for (unsigned state=0; state<4; ++state) for (int occupancy=0; occupancy<4; ++occupancy)
    for (int distinct=0; distinct<2; ++distinct) for (int view=0; view<4; ++view) {
        setup_heading_battle(winner,owner);
        int loser=3-winner;
        if (!headed) memset(peeps+winner*22+14,0,4);
        peeps[winner*22]=flags[state];
        map_who[650]=occupancy==0 ? 0 : occupancy==1 ? winner+1 : occupancy==2 ? loser+1 : 4;
        if (distinct) { set_peep_word(loser,8,750); map_who[750]=loser+1; }
        view_who=view==0 ? 0 : view==1 ? winner+1 : view==2 ? loser+1 : 4;
        check_heading_battle(winner,1023,1977);
    }
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int headed=0; headed<2; ++headed)
    for (int mark=0; mark<3; ++mark) for (int enemy_mark=0; enemy_mark<3; ++enemy_mark)
    for (int count=0; count<3; ++count) for (int paused=0; paused<2; ++paused) {
        setup_heading_battle(winner,owner);
        int loser=3-winner;
        if (!headed) memset(peeps+winner*22+14,0,4);
        peeps[winner*22+20]=markers[mark]; peeps[loser*22+20]=markers[enemy_mark];
        A4_WORD(0xae7a+owner*2)=counters[count];
        core_leader(1-owner)=loser+1; pause=paused;
        check_heading_battle(winner,1025,1975);
    }
}

static void test_heading_battle_mana(void)
{
    const struct { int16_t reward; int32_t winner,loser,floor,want_winner,want_loser; } cases[]={
        {23,1999990,2000,-250,2000013,1977},
        {0,-251,-300,-250,-251,-250},
        {-7,-249,2000,-250,-256,2007},
        {-32768,1000,2000,-250,-31768,34768},
        {23,2147483640,2000,-250,INT32_MIN+15,1977},
        {23,1000,INT32_MIN+8,-250,1023,2147483633},
        {-7,1000,2147483644,-250,993,-250},
        {23,1000,-10,-17,1023,-17},
        {-7,-249,2147483644,-17,-256,-17},
        {-32768,1000,2147483644,-17,-31768,-17}
    };
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int headed=0; headed<2; ++headed) for (int kind=0; kind<3; ++kind)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        setup_heading_battle(winner,owner);
        int loser=3-winner;
        if (!headed) memset(peeps+winner*22+14,0,4);
        if (kind==1) { peeps[loser*22+17]=0x80; core_leader(1-owner)=loser+1; }
        if (kind==2) core_leader(1-owner)=loser+1;
        A4_WORD(kind==0 ? 0x3010 : kind==1 ? 0x3012 : 0x3014)=cases[sample].reward;
        A4_LONG(0x8486)=(uint32_t)cases[sample].floor;
        A4_LONG(0x9a02+owner*16)=(uint32_t)cases[sample].winner;
        A4_LONG(0x9a02+(1-owner)*16)=(uint32_t)cases[sample].loser;
        check_heading_battle(winner,cases[sample].want_winner,cases[sample].want_loser);
    }
}

static void setup_heading_town(int winner, int owner, int population)
{
    setup_heading_battle(winner,owner);
    int loser=3-winner;
    memset(map_blk,1,4096); memset(map_bk2,0,4096);
    peeps[loser*22]=9; set_peep_word(loser,4,population);
    set_peep_word(loser,12,35);
    map_blk[650]=31+1-owner; map_bk2[650]=35;
    for (int level=0; level<11; ++level) A4_WORD(0x2ffa+level*2)=101+level;
    core_leader(1-owner)=loser+1;
}

/* Full A4 oracle for the remnant producer. Only the enumerated peep words,
 * footprint, occupancy, scratch (negative only), reward/bookkeeping/view and
 * two RNG draws may differ. In particular headings, leaders, counter of the
 * winner, altitudes, visit counts and unrelated residents must survive. */
static void check_heading_town_remnant(int winner, int frame, int reward,
                                      int scratch_city, int scratch_land)
{
    int loser=3-winner, owner=peep_owner(winner), enemy=1-owner;
    int center=peep_word(loser,8), position=peep_word(winner,8);
    int city=map_bk2[center]==42, negative=peep_word(loser,4)!=10;
    uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
    memcpy(before,prg_a4_area,sizeof before);
    /* Independent coordinate oracle: a 5x5 city, or a 3x3 plus distance-two
     * axes/corners. City intentionally allows in-array left/right row wrap. */
    for (int row=-2; row<=2; ++row) for (int column=-2; column<=2; ++column) {
        int cell=center+row*64+column;
        if (cell<0 || cell>=4096) continue;
        if (!city) {
            if ((abs(row)==2 && abs(column)==1) || (abs(column)==2 && abs(row)==1)) continue;
            if ((center%64)+column<0 || (center%64)+column>63) continue;
        }
        if (map_blk[cell]==31+enemy) map_blk[cell]=66;
        if (city && abs(row)<=1 && abs(column)<=1 && (row || column) &&
            map_bk2[cell]>=41 && map_bk2[cell]<=44) map_bk2[cell]+=21;
    }
    if (map_bk2[center]>=32 && map_bk2[center]<=42) map_bk2[center]+=21;
    if (map_who[center]==loser+1) map_who[center]=0;
    peeps[loser*22]=0x80; set_peep_word(loser,4,1);
    set_peep_word(loser,6,40); set_peep_word(loser,12,frame);
    peeps[winner*22]=6; set_peep_word(winner,4,90);
    set_peep_word(winner,10,0); set_peep_word(winner,12,85);
    if (!map_who[position] || map_who[position]==loser+1) map_who[position]=winner+1;
    if (negative) { A4_WORD(0x2ff8)=scratch_city; A4_WORD(0x2fb2)=scratch_land; }
    if (peeps[winner*22+20]) {
        --peeps[winner*22+20]; A4_WORD(0x82d2+owner*46)=2; A4_WORD(0x82d4+owner*46)=position;
    }
    if (peeps[loser*22+20]) A4_WORD(0x82d2+enemy*46)=0;
    A4_WORD(0xae7a+owner*2)=(uint16_t)((uint16_t)A4_WORD(0xae7a+owner*2)+1u);
    uint32_t gain=A4_LONG(0x9a02+owner*16)+(uint32_t)reward;
    int64_t loss=(int64_t)(int32_t)A4_LONG(0x9a02+enemy*16)-reward;
    if (loss>INT32_MAX) loss-=INT64_C(4294967296);
    if (loss<INT32_MIN) loss+=INT64_C(4294967296);
    if (loss<(int32_t)A4_LONG(0x8486)) loss=(int32_t)A4_LONG(0x8486);
    A4_LONG(0x9a02+owner*16)=gain; A4_LONG(0x9a02+enemy*16)=(uint32_t)loss;
    if (view_who==loser+1) view_who=winner+1;
    seed=28505;
    memcpy(expected,prg_a4_area,sizeof expected); memcpy(prg_a4_area,before,sizeof before);
    core_battle(1,2);
    assert(peeps[loser*22]==0x80 && peep_word(loser,4)==1);
    assert(peep_word(loser,12)==frame);
    assert(!memcmp(expected,prg_a4_area,sizeof expected));
    assert(core_battles==17 && !core_settlements && !core_result);
}

static void test_heading_town_remnants(void)
{
    const int frames[]={31,32,35,42,43,130,INT16_MIN,INT16_MAX};
    const int rewards[]={100,101,104,111,100,100,100,100};
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int negative=0; negative<2; ++negative) for (int byte=14; byte<18; ++byte)
    for (int both=0; both<2; ++both)
    for (unsigned sample=0; sample<sizeof frames/sizeof frames[0]; ++sample) {
        setup_heading_town(winner,owner,negative ? 9 : 10);
        memset(peeps+winner*22+14,0,4); peeps[winner*22+byte]=0x80;
        if (both) peeps[(3-winner)*22+byte]=0xff;
        set_peep_word(3-winner,12,frames[sample]);
        check_heading_town_remnant(winner,negative ? 34 : frames[sample],negative ? 103 : rewards[sample],0,0);
    }
    const int centers[]={650,640,703,0,63,4032,4095};
    const int overlays[]={31,32,40,41,42,43};
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (unsigned place=0; place<sizeof centers/sizeof centers[0]; ++place)
    for (unsigned overlay=0; overlay<sizeof overlays/sizeof overlays[0]; ++overlay)
    for (int phase=0; phase<4; ++phase) for (int terrain=0; terrain<5; ++terrain) {
        setup_heading_town(winner,owner,10);
        int center=centers[place], loser=3-winner;
        const int blocks[]={31+1-owner,31+owner,15,47,0};
        memset(map_blk,blocks[terrain],4096);
        /* Cycle boundaries across every offset, including 8/9/16/17/24. */
        const int buildings[]={40,41,44,45};
        for (int cell=0; cell<4096; ++cell) map_bk2[cell]=buildings[(cell+phase)%4];
        map_bk2[center]=overlays[overlay]; set_peep_word(loser,8,center);
        set_peep_word(loser,12,overlays[overlay]==42 ? 32 : 42);
        map_who[center]=loser+1;
        check_heading_town_remnant(winner,overlays[overlay]==42 ? 32 : 42,
                                   overlays[overlay]==42 ? 101 : 111,0,0);
    }
    /* Negative city reframe observes all nine buildings BEFORE their burning. */
    setup_heading_town(1,0,9); memset(map_blk,32,4096);
    for (int row=-1; row<=1; ++row) for (int column=-1; column<=1; ++column)
        map_bk2[650+row*64+column]=41;
    map_bk2[650]=42; map_blk[650-128]=15;
    check_heading_town_remnant(1,42,111,9,1);
    /* Center overlay, not recomputed frame, decides the city footprint. */
    setup_heading_town(1,0,9); map_bk2[650]=42; map_blk[523]=32;
    check_heading_town_remnant(1,34,103,1,0);
    setup_heading_town(1,0,9); memset(map_blk,32,4096); map_bk2[650]=35;
    check_heading_town_remnant(1,42,111,0,0);
    const int16_t signed_rewards[]={0,-7,INT16_MIN,INT16_MAX};
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int occupancy=0; occupancy<4; ++occupancy) for (int distinct=0; distinct<2; ++distinct)
    for (int sample=0; sample<4; ++sample) {
        setup_heading_town(winner,owner,10);
        int loser=3-winner;
        peeps[winner*22]=0x69; set_peep_word(winner,6,-123);
        if (distinct) { set_peep_word(winner,8,750); map_who[750]=0; }
        map_who[650]=occupancy==0 ? 0 : occupancy==1 ? loser+1 : occupancy==2 ? winner+1 : 4;
        A4_WORD(0x2ffa+6)=signed_rewards[sample];
        A4_LONG(0x9a02+owner*16)=0x7ffffffc;
        A4_LONG(0x9a02+(1-owner)*16)=sample&1 ? 0x7ffffffc : 0x80000008;
        A4_WORD(0xae7a+owner*2)=-1;
        peeps[winner*22+20]=sample==0 ? 0 : 255;
        peeps[loser*22+20]=sample==1 ? 0 : 255; view_who=sample==2 ? 4 : loser+1;
        check_heading_town_remnant(winner,35,signed_rewards[sample],0,0);
    }
}

static void test_ordinary_town_full_state(void)
{
    /* Catch signed reward, reframe ordering, cleanup and stray terrain/A4 writes.
     * Each isolated farmland center has capacity 65 and town frame 34. */
    const struct { int16_t reward; int32_t winner_mana, loser_mana; } cases[]={
        {0,1000,2000}, {-7,993,2007}, {-32768,-31768,34768}, {104,1104,1896}
    };
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int negative=0; negative<2; ++negative) for (int success=0; success<2; ++success)
    for (int town=0; town<2; ++town)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        setup_heading_town(winner,owner,negative ? 9 : 10);
        int loser=3-winner, enemy=1-owner;
        memset(peeps+winner*22+14,0,4);
        memset(peeps+loser*22+14,0,4);
        peeps[winner*22]=town ? 9 : 8;
        set_peep_word(winner,8,750);
        map_blk[750]=success ? 15 : 1;
        game_turn=12345;
        A4_WORD(0xae7a+owner*2)=-1;
        /* Zero retains frame 35/index 3; negative reframes to 34/index 2. */
        A4_WORD(0x2ffe)=A4_WORD(0x3000)=cases[sample].reward;
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);

        /* All neighbours are block 1 with no buildings: only these two centers
         * can change. Failed conquest keeps the original winner town bit/link. */
        map_blk[650]=15; map_bk2[650]=0; map_who[650]=0;
        map_who[714]=0; /* _zero_population asm:5671-5680, 650-(-64) */
        map_blk[750]=success ? 31+owner : 1;
        map_bk2[750]=success ? 34 : 0; map_who[750]=winner+1;
        peeps[winner*22]=success || town ? 5 : 6;
        set_peep_word(winner,4,90);
        set_peep_word(winner,6,success ? 12345 : loser);
        set_peep_word(winner,10,0); set_peep_word(winner,12,85);
        peeps[loser*22]=0; set_peep_word(loser,4,0);
        set_peep_word(loser,6,success ? 0 : winner);
        set_peep_word(loser,12,negative ? 34 : 35);
        A4_WORD(0x2ff8)=0; A4_WORD(0x2fb2)=success;
        peeps[winner*22+20]=19;
        A4_WORD(0x82d2+owner*46)=2; A4_WORD(0x82d4+owner*46)=750;
        A4_WORD(0x82d2+enemy*46)=0;
        A4_WORD(0xae7a+owner*2)=0;
        A4_LONG(0x9a02+owner*16)=(uint32_t)cases[sample].winner_mana;
        A4_LONG(0x9a02+enemy*16)=(uint32_t)cases[sample].loser_mana;
        core_leader(enemy)=0; core_target(enemy)=650;
        A4_WORD(enemy ? 0xae76 : 0xae78)=650;
        view_who=winner+1; seed=28505;
        memcpy(expected,prg_a4_area,sizeof expected);
        memcpy(prg_a4_area,before,sizeof before);

        core_battle(1,2);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==17 && !core_settlements && !core_result);
    }
}

static void test_heading_town_conquest(void)
{
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int negative=0; negative<2; ++negative) for (int byte=13; byte<18; ++byte)
    for (int town=0; town<2; ++town) for (int location=0; location<3; ++location) {
        setup_heading_town(winner,owner,negative ? 9 : 10);
        int loser=3-winner, position=location==0 ? 651 : 750;
        memset(peeps+winner*22+14,0,4);
        if (byte>=14) peeps[loser*22+byte]=0x80;
        peeps[winner*22]=town ? 0x69 : 0x68;
        set_peep_word(winner,8,position); set_peep_word(winner,6,-123);
        map_blk[position]=location==2 ? 1 : 15; map_who[position]=4;
        game_turn=12345;
        core_battle(1,2);
        assert(peeps[winner*22]==(location!=2 || town ? 5 : 6));
        assert(peep_word(winner,6)==(location==2 ? -123 : 12345));
        assert(peep_word(winner,12)==85 && !peep_word(winner,10));
        assert(!peeps[loser*22] && !peep_word(loser,4));
        /* Failed conquest leaves flags 1 for cleanup: it does not clear counter. */
        assert(peep_word(loser,6)==(location==2 ? winner : 0));
        assert(peep_word(loser,12)==(negative ? 34 : 35));
        assert(map_who[position]==4 && !map_who[650]);
        assert(map_blk[position]==(location==2 ? 1 : 31+owner));
        assert(map_bk2[position]==(location==2 ? 0 : 34));
        assert(map_blk[650]==(location==0 ? 31+owner : 15));
        assert(!map_bk2[650]);
        assert(view_who==winner+1 && !core_leader(1-owner));
        assert(core_mana(owner)==(negative ? 1103 : 1104));
        assert(core_mana(1-owner)==(negative ? 1897 : 1896));
        assert(A4_WORD(0xae7a+owner*2)==8 && seed==28505);
    }
}

static void test_heading_town_ticks(void)
{
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int negative=0; negative<2; ++negative) {
        int loser=3-winner;
        setup_heading_town(winner,owner,negative ? 9 : 10); set_peep_word(3,4,0);
        /* The producer itself does not run the heading leader-release prepass. */
        core_battle(1,2);
        assert(peep_word(loser,6)==40 && peep_word(winner,12)==85);
        assert(core_leader(owner)==winner+1 && core_leader(1-owner)==loser+1);
        _move_peeps();
        assert(peep_word(loser,6)==39 && peep_word(winner,12)==86);
        assert(!core_leader(owner) && core_leader(1-owner)==loser+1);
        assert(core_population(owner)==90 && core_population(1-owner)==1);
        assert(view_who==winner+1 && map_who[650]==winner+1);
        setup_heading_town(winner,owner,negative ? 9 : 10); set_peep_word(3,4,0);
        _move_peeps();
        assert(peep_word(loser,6)==(loser==2 ? 39 : 40));
        assert(peep_word(winner,12)==(winner==2 ? 86 : 85));
        assert(peep_word(loser,12)==(negative ? 34 : 35));
        /* AI picks 15930, 28505 then rolls 12248 (3), 7095 (1): weapons 0
         * and a lesser strength below 100, so the losses are unchanged. */
        assert(seed==7095);
        /* Dry baseline: keep existing walking and zero loss, not water timing.
         * Source accumulates population BEFORE each resident (asm:3462-3474),
         * so expiry's source total still includes 1 until the following pass:
         * asm:4323-4347 therefore ends the game on pass 42, not 41. */
        setup_heading_town(winner,owner,10); set_peep_word(3,4,0);
        core_battle(1,2);
        uint8_t winner_heading[4], loser_heading[4];
        memcpy(winner_heading,peeps+winner*22+14,4);
        memcpy(loser_heading,peeps+loser*22+14,4);
        for (int tick=1; tick<=40; ++tick) {
            game_tick();
            assert(game_turn==tick && !core_result && peep_word(loser,6)==40-tick);
            assert(peep_word(loser,4)==1 && A4_LONG(owner ? 0x99fe : 0x9a0e)==1);
            assert(core_leader(1-owner)==loser+1);
            assert(!memcmp(winner_heading,peeps+winner*22+14,4));
            assert(!memcmp(loser_heading,peeps+loser*22+14,4));
            /* _move_explorer asm:4590-4617: a walker's claim is its
             * departure cell, pos-(+10), until its next departure, unless
             * the bit-7 remnant already holds 650 (asm:4530-4534,4600). */
            int claim_cell=(int16_t)(peep_word(winner,8)-peep_word(winner,10));
            assert(map_who[claim_cell]==winner+1 ||
                   (claim_cell==650 && map_who[650]==loser+1));
        }
        game_tick();
        assert(game_turn==41 && !peep_word(loser,4));
        assert((uint16_t)peep_word(loser,6)==0xffff);
        assert(A4_LONG(owner ? 0x99fe : 0x9a0e)==1 && !core_result);
        assert(A4_LONG(owner ? 0x9a0e : 0x99fe)>0);
        assert(!core_leader(1-owner) && core_target(1-owner)==650);
        assert(A4_WORD(owner ? 0xae78 : 0xae76)==650);
        assert(view_who==winner+1);
        assert(map_who[(int16_t)(peep_word(winner,8)-peep_word(winner,10))]==winner+1);
        assert(!memcmp(winner_heading,peeps+winner*22+14,4));
        assert(!memcmp(loser_heading,peeps+loser*22+14,4));
        game_tick();
        assert(game_turn==42 && A4_LONG(owner ? 0x99fe : 0x9a0e)==0 && core_result==(owner ? 2 : 1));
        assert(A4_LONG(owner ? 0x9a0e : 0x99fe)>0);
    }
    /* Successful conquest must feed the town consumer in resident order. */
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int headed=0; headed<2; ++headed) {
        setup_heading_town(winner,owner,10); set_peep_word(3,4,0);
        int loser=3-winner;
        memset(peeps+winner*22+14,0,4);
        if (headed) peeps[loser*22+17]=0x80;
        core_leader(owner)=core_leader(1-owner)=0; game_turn=12345;
        _move_peeps();
        assert(peeps[winner*22]==5 && !peep_word(loser,4));
        assert(peep_word(winner,6)==12345 && peep_word(winner,12)==(winner==1 ? 85 : 86));
        assert(map_blk[650]==31+owner && map_bk2[650]==34 && map_who[650]==winner+1);
        int remaining=winner==1 ? 3 : 2;
        for (int tick=0; tick<remaining; ++tick) _move_peeps();
        assert(peeps[winner*22]==1 && peep_word(winner,12)==34);
        assert(peep_word(winner,4)==90 && peep_word(winner,6)==12345);
        assert(map_blk[650]==31+owner && map_bk2[650]==34 && map_who[650]==winner+1);
        assert(seed==7095); /* two AI picks before the unchanged-outcome rolls */
    }
    for (int fatal=0; fatal<2; ++fatal) {
        setup_heading_town(1,0,10); core_battle(1,2); set_peep_word(3,4,0);
        map_blk[650]=0; game_mode=fatal; A4_WORD(0x2fb0)=1;
        _move_peeps();
        assert(peeps[2*22]==18 && peep_word(2,4)==1 && peep_word(2,6)==40);
        assert(peep_word(2,12)==0); /* New water wins before remnant timer. */
        _move_peeps();
        assert(!peep_word(2,4) && peep_word(2,6)==40 && peep_word(2,12)==0);
    }
}

static void test_heading_town_casualty_controls(void)
{
    for (int owner=0; owner<2; ++owner) for (int winner=1; winner<=2; ++winner)
    for (int both=0; both<2; ++both) {
        setup_heading_town(winner,owner,32000);
        int loser=3-winner;
        set_peep_word(winner,4,32000); peeps[winner*22+3]=255;
        peeps[loser*22+3]=both ? 255 : 0;
        /* SUB.W loss 81610 wraps to +15926: neither conquest nor burning. */
        core_battle(1,2);
        assert(seed==28505);
        assert(peep_word(winner,4)==(both ? 15926 : 31990));
        assert(peep_word(loser,4)==15926);
        assert(peeps[winner*22]==8 && peeps[loser*22]==9);
        assert(peep_word(winner,12)==(owner ? 134 : 130));
        assert(peep_word(loser,12)==(owner ? 134 : 130));
        assert(peep_word(loser,6)==winner);
        assert(map_blk[650]==32-owner && map_bk2[650]==35);
        assert(A4_WORD(0xae7a+owner*2)==7 && core_mana(owner)==1000);
    }
    for (int population=9; population<=10; ++population) {
        setup_heading_town(1,0,population); memset(peeps+22+14,0,4);
        game_turn=12345;
        core_battle(1,2);
        /* Direct zero completion retains town frame 35; negative reframes first. */
        assert(peeps[22]==5 && !peeps[44] && !peep_word(2,4));
        assert(peep_word(1,12)==85 && peep_word(1,6)==12345);
        assert(map_blk[650]==31 && map_bk2[650]==34);
        assert(core_mana(0)==(population==9 ? 1103 : 1104) && A4_WORD(0xae7a)==8);
    }
}

static void test_heading_town(void)
{
    test_heading_town_remnants(); test_heading_town_conquest(); test_ordinary_town_full_state();
    test_heading_town_ticks(); test_heading_town_casualty_controls();
}

static void test_heading_battle_controls(void)
{
    /* No-heading town baseline, plus unchanged both-live and both-dead routes. */
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int route=0; route<3; ++route) for (int town=0; town<2; ++town) {
        setup_heading_battle(winner,owner);
        int loser=3-winner;
        memset(peeps+winner*22+14,0,4);
        if (route==0 || town) peeps[loser*22]=9;
        if (route==1) {
            set_peep_word(1,4,1000); set_peep_word(2,4,400);
            peeps[25]=2; peeps[47]=3;
        }
        if (route==2) set_peep_word(winner,4,10);
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        core_battle(1,2);
        assert(seed==28505);
        if (route==1) assert(peep_word(1,4)==978 && peep_word(2,4)==382);
        if (route==2) assert(!peep_word(1,4) && !peep_word(2,4));
        if (route) peeps[winner*22+17]=peeps[loser*22+14]=0x80;
        /* Only the live combat frame words differ when both gain headings. */
        if (route==1) { set_peep_word(1,12,138); set_peep_word(2,12,138); }
        memcpy(expected,prg_a4_area,sizeof expected);
        memcpy(prg_a4_area,before,sizeof before);
        if (route) peeps[winner*22+17]=peeps[loser*22+14]=0x80;
        core_battle(1,2);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==17 && !core_settlements);
    }
    /* Nonzero trailing bytes 18..21 are not a heading (including byte 20). */
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner) {
        setup_heading_battle(winner,owner); memset(peeps+winner*22+14,0,4);
        check_heading_battle(winner,1023,1977);
    }
}

static void test_heading_battle_ticks(void)
{
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int headed=0; headed<2; ++headed) for (int town=0; town<2; ++town) {
        setup_heading_battle(winner,owner);
        if (!headed) memset(peeps+winner*22+14,0,4);
        int loser=3-winner;
        peeps[winner*22]=town ? 9 : 8;
        core_leader(owner)=0; set_peep_word(3,4,0);
        /* No elimination gate: exercise the actual ordered resident pass. */
        _move_peeps();
        assert(peeps[winner*22]==(town ? 5 : 6));
        assert(peep_word(winner,12)==(winner==1 ? 85 : 86));
        assert(peep_word(winner,4)==90 && !peep_word(loser,4));
        /* Two AI picks (both tendencies 0) draw 15930, 28505 before the
         * rolls 12248 (3), 7095 (1); weapons 0, so both lose 10 as before. */
        assert(peep_word(winner,6)==loser && seed==7095);
        /* Start a fresh completion at frame 85, then consume exactly three times. */
        setup_heading_battle(winner,owner); peeps[winner*22]=town ? 9 : 8;
        if (!headed) memset(peeps+winner*22+14,0,4);
        core_leader(owner)=0; set_peep_word(3,4,0);
        core_battle(1,2);
        uint8_t terrain[4096], buildings[4096], metadata[8];
        memcpy(terrain,map_blk,sizeof terrain); memcpy(buildings,map_bk2,sizeof buildings);
        memcpy(metadata,peeps+winner*22+14,sizeof metadata);
        for (int tick=0; tick<3; ++tick) {
            _move_peeps();
            assert(peeps[winner*22]==(tick==2 ? (town ? 1 : 2) : (town ? 5 : 6)));
            /* Walker wraps 88 to 0; neighbouring buildings exclude town life. */
            assert(peep_word(winner,12)==(tick==2 ? (town ? 32 : 0) : 86+tick));
            assert(peep_word(winner,4)==90 && peep_word(winner,6)==loser);
            assert(peep_word(winner,8)==650 && !peep_word(winner,10));
            assert(!memcmp(metadata,peeps+winner*22+14,sizeof metadata));
            assert(!memcmp(terrain,map_blk,sizeof terrain) && !memcmp(buildings,map_bk2,sizeof buildings));
            /* The first pass's two AI picks continue 28505 -> 12248 -> 7095. */
            assert(seed==7095 && core_battles==17 && !core_settlements);
        }
    }
}

static void test_heading_battle_guards(void)
{
    setup_heading_battle(1,0); set_peep_word(2,4,0);
    memset(peeps+22+14,0,4);
    assert(core_battle_over(1,2,false));
    assert(peeps[22]==6 && peep_word(1,12)==85 && !peeps[44]);
    assert(peep_word(1,4)==100 && seed==123 && view_who==2);
    assert(core_mana(0)==1023 && core_mana(1)==1977 && A4_WORD(0xae7a)==8);
    const int invalid[]={-1,4,208,32767};
    for (unsigned sample=0; sample<sizeof invalid/sizeof invalid[0]; ++sample) {
        setup_heading_battle(1,0); set_peep_word(2,4,0);
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_battle_over(invalid[sample],2,false));
        assert(!core_battle_over(1,invalid[sample],false));
        assert(!core_battle_over(1,1,false));
        assert(a4_same(before));
    }
    for (int variant=0; variant<13; ++variant) {
        setup_heading_battle(1,0); set_peep_word(2,4,0);
        switch (variant) {
        case 0: no_peeps=-1; break;
        case 1: no_peeps=209; break;
        case 2: set_peep_word(1,4,0); break;
        case 3: set_peep_word(1,4,-1); break;
        case 4: set_peep_word(2,4,1); break;
        case 5: set_peep_word(2,4,10); peeps[44]=9; break;
        case 6: peeps[23]=2; break;
        case 7: peeps[45]=2; break;
        case 8: peeps[45]=0; break;
        case 9: set_peep_word(1,8,-1); break;
        case 10: set_peep_word(1,8,4096); break;
        case 11: set_peep_word(2,8,-1); break;
        case 12: set_peep_word(2,8,4096); break;
        }
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_battle_over(1,2,false));
        assert(a4_same(before));
        assert(core_battles==17 && !core_settlements && !core_result);
    }
}

static void test_heading_battle(void)
{
    test_heading_battle_controls(); test_heading_battle_presence();
    test_heading_battle_state(); test_heading_battle_mana(); test_heading_battle_ticks();
    test_heading_battle_guards(); test_heading_town();
}

static void test_heading_navigation(void)
{
    test_heading_navigation_targets(); test_heading_navigation_bounds();
    test_heading_navigation_opaque(); test_heading_navigation_dispatch();
    test_heading_navigation_swamp(); test_heading_navigation_fallback();
    test_heading_navigation_water(); test_heading_leader_release();
}

static void knight_fixture(int owner)
{
    armageddon_fixture(); no_peeps=3; player=owner;
    A4_LONG(0x849a)=7500;
    A4_LONG(0x9a02)=A4_LONG(0x9a12)=15000;
    A4_WORD(0x82c4)=A4_WORD(0x82c4+46)=0x20;
    for (int person=0; person<3; ++person) {
        memset(peeps+person*22,0x5a,22);
        peeps[person*22]=2; peeps[person*22+1]=person==1 ? 1-owner : owner;
        set_peep_word(person,4,100); set_peep_word(person,8,650+person);
    }
    core_leader(owner)=1; diagnostic_view=0;
}

/* Source cast oracle: only these fields may change, including byte-wide marker
 * overwrite and a full four-byte heading. No consumer is run to obtain it. */
static void knight_expected(uint8_t *expected, int owner, uint32_t heading)
{
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    int subject=core_leader(owner)-1, position=peep_word(subject,8);
    if (!paint_map) A4_LONG(0x9a02+owner*16)-=A4_LONG(0x849a);   /* asm:8404 */
    if (owner==player) A4_LONG(0xae72)+=150;
    peeps[subject*22+20]=1; memcpy(peeps+subject*22+14,&heading,4);
    core_target(owner)=position; A4_WORD(owner ? 0xae76 : 0xae78)=position;
    core_leader(owner)=0; A4_WORD(0x2fac)=69;
    memcpy(expected,prg_a4_area,sizeof prg_a4_area);
    memcpy(prg_a4_area,before,sizeof before);
}

static void test_knight_guards(void)
{
    /* Every rejection compares all A4 bytes through both core and SDL entry. */
    const int invalid_owner[]={INT_MIN,-1,2,255,INT_MAX};
    const int invalid_count[]={INT16_MIN,-1,0,209,INT16_MAX};
    const int invalid_leader[]={INT16_MIN,-1,0,4,209,INT16_MAX};
    const int invalid_position[]={INT16_MIN,-1,4096,INT16_MAX};
    for (int owner=0; owner<2; ++owner) for (int paint=0; paint<2; ++paint)
    for (int diagnostic=0; diagnostic<2; ++diagnostic) for (int gate=0; gate<41; ++gate) {
        /* _paint_map skips pause/war/mana/permission (asm:8404): test_paint_map_powers. */
        if (paint && ((gate>=25 && gate<=27) || gate>=32)) continue;
        knight_fixture(owner); paint_map=paint; diagnostic_view=diagnostic;
        int side=owner;
        if (gate<5) side=invalid_owner[gate];
        else if (gate<10) no_peeps=invalid_count[gate-5];
        else if (gate<16) core_leader(owner)=invalid_leader[gate-10];
        else if (gate<20) set_peep_word(0,8,invalid_position[gate-16]);
        else if (gate==20) set_peep_word(0,4,0);
        else if (gate==21) set_peep_word(0,4,-1);
        else if (gate==22) set_peep_word(0,4,INT16_MIN);
        else if (gate==23) peeps[1]=1-owner;
        else if (gate==24) peeps[1]=255;
        else if (gate==25) pause=-1;
        else if (gate==26) A4_WORD(0xae80)=1;
        else if (gate==27) A4_WORD(0xae80)=INT16_MIN;
        else if (gate>=28 && gate<=30) core_result=gate-27;
        else if (gate==31) prg_quit=1;
        else if (gate==32) A4_LONG(0x9a02+owner*16)=7499;
        else if (gate==33) A4_LONG(0x9a02+owner*16)=0x80000000u;
        else if (gate==34) A4_LONG(0x9a02+owner*16)=0xffffffffu;
        else if (gate==35) A4_WORD(0x82c4+owner*46)=0xffdf;
        else if (gate==36) A4_WORD(0x82c4+owner*46)=4;
        else if (gate==37) A4_WORD(0x82c4+owner*46)=0x2000;
        else if (gate==38) { A4_LONG(0x849a)=16001; }
        else if (gate==39) { A4_LONG(0x849a)=0x7fffffffu; A4_LONG(0x9a02+owner*16)=0x80000000u; }
        else { A4_LONG(0x849a)=0xffffffffu; A4_LONG(0x9a02+owner*16)=0xfffffffeu; }
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!core_knight(side)); assert(a4_same(before));
        /* The core takes int; player is a signed word, so use representative
         * invalid words for extremes which would truncate to a valid owner. */
        player=side==INT_MIN || side==INT_MAX ? -1 : side;
        memcpy(before,prg_a4_area,sizeof before);
        SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_k;
        game_handle_event(&key); assert(a4_same(before));
        assert(core_tool==1 && core_swamp_selected==1 && !core_battles && !core_settlements);
    }
}

static void assert_knight_cast(int owner, uint32_t heading)
{
    uint8_t expected[sizeof prg_a4_area]; knight_expected(expected,owner,heading);
    assert(core_knight(owner)); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    assert(core_tool==1 && core_swamp_selected==1 && !core_battles && !core_settlements);
}

static void test_knight_cast(void)
{
    const uint32_t headings[]={0,1,208,UINT32_MAX,0x53014};
    /* All 256 source flags admitted; no inferred weapon/terrain/occupancy gate. */
    for (int owner=0; owner<2; ++owner) for (int local=0; local<2; ++local)
    for (int state=0; state<256; ++state) {
        knight_fixture(owner); player=local; peeps[0]=state;
        paint_map=state&1; diagnostic_view=(state>>1)&1;
        peeps[3]=state; peeps[20]=state; map_blk[650]=state; map_who[650]=state;
        memcpy(peeps+14,&headings[state%5],4); A4_LONG(0xae72)=0xfffffff0u;
        assert_knight_cast(owner,2);
        assert(A4_LONG(0xae72)==(owner==local ? 134u : 0xfffffff0u));
    }
    const struct { uint32_t mana,cost,remainder; } cases[]={
        {7500,7500,0}, {173,173,0}, {174,173,1},
        {0x7fffffffu,0xffffffffu,0x80000000u},
        {0x80000000u,0x80000000u,0}, {0xffffffffu,0xffffffffu,0},
        {0xffffffffu,0x80000000u,0x7fffffffu}, {0,0,0},
        {0x7fffffffu,0x7fffffffu,0}
    };
    for (int owner=0; owner<2; ++owner)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        knight_fixture(owner); A4_LONG(0x9a02+owner*16)=cases[sample].mana;
        A4_LONG(0x849a)=cases[sample].cost;
        assert_knight_cast(owner,2); assert(A4_LONG(0x9a02+owner*16)==cases[sample].remainder);
    }
    const int counts[]={1,207,208}, positions[]={0,4095};
    for (int owner=0; owner<2; ++owner) for (int count=0; count<3; ++count)
    for (int place=0; place<2; ++place) {
        knight_fixture(owner); no_peeps=counts[count];
        memset(peeps,0,208*22); memset(peeps+208*22,0xa5,22);
        int subject=no_peeps-1; core_leader(owner)=subject+1;
        peeps[subject*22+1]=owner; set_peep_word(subject,4,INT16_MAX);
        set_peep_word(subject,8,positions[place]); assert_knight_cast(owner,subject+1);
    }
    /* Selector stays unmodified: nearest, ascending tie, dead/remnant skip,
     * self fallback, and out-of-map signed enemy positions remain eligible. */
    for (int sample=0; sample<7; ++sample) {
        knight_fixture(0); peeps[45]=1;
        set_peep_word(1,8,654); set_peep_word(2,8,648);
        uint32_t expected=3;
        if (sample==1) { set_peep_word(1,8,652); expected=2; }
        if (sample==2) { set_peep_word(2,4,0); expected=2; }
        if (sample==3) { peeps[44]=0x80; expected=2; }
        if (sample==4) { peeps[23]=peeps[45]=0; expected=1; }
        if (sample==5) { set_peep_word(0,8,63); set_peep_word(1,8,-1); set_peep_word(2,8,1000); expected=2; }
        if (sample==6) { set_peep_word(1,8,INT16_MIN); set_peep_word(2,8,INT16_MAX); expected=2; }
        assert_knight_cast(0,expected);
    }
    knight_fixture(0); assert_knight_cast(0,2);
    A4_LONG(0x84a6)=80000; A4_LONG(0x9a02)=80000; A4_WORD(0x82c4)=0x100;
    assert(core_war(0)); assert(peep_reference(0)==-1 && peeps[20]==1);
}

static void test_knight_render(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    const char *line="S SWAMP 5000 Q QUAKE 2500 K KNIGHT 7500";
    /* Original 8x8 font: the knight line fits 40 columns. */
    assert(strlen(line)<=40); mousex=-100;
    for (int state=0; state<6; ++state) {
        core_result=state>=3 ? state-2 : 0; pause=state==1; A4_WORD(0xae80)=state==2;
        core_swamp_selected=core_tool=1;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        game_render(); assert(a4_same(before));
        const char *title=SDL_GetWindowTitle(prg_screen->window);
        assert(strstr(title,"K knight 7500") && strstr(title,"Esc quit") && strlen(title)<512);
        assert(hud_text_at(4,0,line));
        assert(a4_same(before));
    }
    _closeall(); SDL_Quit();
}

static void test_knight_input(void)
{
    knight_fixture(0);
    uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
    memcpy(before,prg_a4_area,sizeof before); knight_expected(expected,0,2);
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_k;
    key.key.repeat=1; game_handle_event(&key);
    assert(a4_same(before));
    key.key.repeat=0; game_handle_event(&key);
    assert(!memcmp(expected,prg_a4_area,sizeof expected));
    game_handle_event(&key); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    core_leader(0)=3; knight_expected(expected,0,2);
    game_handle_event(&key); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    assert(core_mana(0)==0 && A4_LONG(0xae72)==300);
    assert(core_tool==1 && core_swamp_selected==1 && !core_battles && !core_settlements);
}

static void test_knight_lifecycle(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for (int owner=0; owner<2; ++owner) {
        assert(game_reset(0)); player=owner;
        for (int vertex=0; vertex<4225; ++vertex) A4_WORD(0xae86+vertex*2)=1;
        memset(map_blk,0,4096); memset(map_bk2,0,4096); memset(map_who,0,4096);
        _make_map(); memset(peeps,0,208*22); no_peeps=3;
        const int positions[]={650,654,3000};
        for (int person=0; person<3; ++person) {
            peeps[person*22]=person ? 1 : 2; peeps[person*22+1]=person ? 1-owner : owner;
            peeps[person*22+2]=1; peeps[person*22+3]=person ? 1 : 100;
            set_peep_word(person,4,person==0 ? 3000 : person==1 ? 10 : 1000);
            set_peep_word(person,8,positions[person]); set_peep_word(person,12,person ? 32 : 7);
            map_who[positions[person]]=person+1;
            if (person) core_set_town(person,0);
        }
        core_leader(owner)=1; core_leader(1-owner)=3;
        core_tool=core_swamp_selected=1;
        A4_LONG(0x9a02+owner*16)=7500; A4_WORD(0x82c4+owner*46)|=0x20;
        uint8_t expected[sizeof prg_a4_area]; knight_expected(expected,owner,2);
        SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_k;
        assert(SDL_PushEvent(&key)==1); ___keyboard();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        int moved=0, battle=0, remnant=0, celebrated=0, resumed=0, expired=0;
        /* No world/state writes after K: all phases come from ordinary ticks. */
        for (int tick=1; tick<=500 && !expired && !core_result; ++tick) {
            game_tick();
            if (peep_word(0,8)!=650) moved=1;
            if (peeps[0]&8) { assert(moved); battle=1; }
            if (peeps[22]==0x80) {
                assert(battle); remnant=1;
                /* A controller-1 walker leaving the 0x42 ruin asks for it to be
                 * lowered (_move_explorer asm:4450-4509); the overlay remains. */
                assert(map_bk2[654]>=53 && map_bk2[654]<=63);
                assert(map_blk[654]==66 ||
                       (A4_WORD(0x82bc+owner*46)==1 && peep_word(0,8)!=654));
            }
            if (peeps[0]==6) { assert(remnant); celebrated=1; }
            if (celebrated && peeps[0]==2) resumed=1;
            if (remnant && !peep_word(1,4)) expired=1;
            assert(peep_word(0,4)>0 && core_tool==1 && core_swamp_selected==1);
            assert_mailbox_empty(); assert_live_occupancy();
            if (!(tick%10)) {
                /* Moving walkers are now drawn from their departure cell, so
                 * the render-owned _sprite/_ok_to_build block may change. */
                memcpy(expected,prg_a4_area,sizeof expected); game_render();
                assert(a4_same(expected));
            }
        }
        printf("Knight controlled ticks: owner=%d moved=%d battle=%d remnant=%d celebration=%d resumed=%d expired=%d tick=%d\n",
               owner,moved,battle,remnant,celebrated,resumed,expired,game_turn);
        assert(moved && battle && remnant && celebrated && resumed && expired);
        assert(core_battles>=1 && peeps[20]==0 && peep_word(2,4)>0);
    }
    _closeall(); SDL_Quit();
}

static void test_knight_ordinary_actions(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    SDL_SetWindowSize(prg_screen->window,640,512);
    core_leader(0)=1; A4_LONG(0x9a02)=20000; A4_WORD(0x82c4)=0x31;
    core_tool=core_swamp_selected=0;
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_k;
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    assert(core_mana(0)==12500 && peep_reference(0)>=0);
    /* A knight is not a global terrain or tool lock. */
    iso_click_vertex(20,20,SDL_BUTTON_LEFT);
    assert(core_vertex(20,20)==2 && core_mana(0)==12486);
    key.key.keysym.sym=SDLK_s; assert(SDL_PushEvent(&key)==1); ___keyboard();
    assert(core_swamp_selected);
    iso_click_vertex(25,25,SDL_BUTTON_LEFT);
    assert(!core_swamp_selected && core_mana(0)==7486);
    assert(A4_LONG(0xae72)==200 && peep_reference(0)>=0);
    /* Existing owner mailboxes remain executable after the cast. */
    A4_DISP(0x82b6)[0]=1; A4_DISP(0x82b7)[0]=40; A4_DISP(0x82b8)[0]=40;
    core_consume_terrain_mailboxes();
    assert(core_vertex(40,40)==2 && core_mana(0)==7472);
    assert_mailbox_empty();
    _closeall(); SDL_Quit();
}

static void test_knight_original(const char *directory)
{
    assert(!setenv("POPULOUS_DATA_DIR",directory,1));
    char assets[4096]; snprintf(assets,sizeof assets,"%s/..",directory);
    assert(!setenv("POPULOUS_ASSET_DIR",assets,1));
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for (int owner=0; owner<2; ++owner) {
        assert(game_reset_start((StartSelection){START_ORIGINAL,0}));
        assert_original_world(); assert(A4_LONG(0x849a)==7500);
        /* The original start's player is untouched: core entry tests red;
         * blue traverses the SDL queue. Only funding/permission is injected. */
        int subject=core_leader(owner)-1, position=peep_word(subject,8);
        assert(subject>=0 && subject<no_peeps && peep_owner(subject)==owner);
        uint32_t heading=subject+1; int distance=9999;
        for (int person=0; person<no_peeps; ++person) {
            if (peep_owner(person)==owner || peep_word(person,4)<=0 || (peeps[person*22]&0x80)) continue;
            int cell=peep_word(person,8);
            assert(cell>=0 && cell<4096);
            int candidate=abs(cell%64-position%64)+abs(cell/64-position/64);
            if (candidate<distance) { distance=candidate; heading=person+1; }
        }
        A4_LONG(0x9a02+owner*16)=7500; A4_WORD(0x82c4+owner*46)|=0x20;
        uint8_t expected[sizeof prg_a4_area]; knight_expected(expected,owner,heading);
        if (owner==player) {
            SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_k;
            assert(SDL_PushEvent(&key)==1); ___keyboard();
        } else assert(core_knight(owner));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        game_render(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(strstr(SDL_GetWindowTitle(prg_screen->window),"K knight 7500"));
        for (int tick=0; tick<100; ++tick) {
            assert(!core_result); game_tick(); assert_live_occupancy(); assert_mailbox_empty();
        }
        memcpy(expected,prg_a4_area,sizeof expected); game_render();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        printf("Original funded knight smoke: owner=%d ticks=%d populations=%d/%d (not manual-play evidence)\n",
               owner,game_turn,core_population(0),core_population(1));
    }
    _closeall(); SDL_Quit();
}

static void check_battle_frame_only(int person, int partner, bool raw, int want, int reset)
{
    uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
    memcpy(before,prg_a4_area,sizeof before);
    if (person>=0 && person<208) set_peep_word(person,12,want);
    memcpy(expected,prg_a4_area,sizeof expected);
    memcpy(prg_a4_area,before,sizeof before);
    assert(core_frame_battle(person,partner,raw)==reset);
    assert(!memcmp(expected,prg_a4_area,sizeof expected));
}

static void test_battle_frame_helper(void)
{
    const int owners[]={0,1,255};
    uint32_t headings[]={0,1,2,UINT32_MAX,0,0,0,0};
    for (int byte=0; byte<4; ++byte) ((uint8_t *)&headings[4+byte])[byte]=0x80;
    /* Crossed raw presence and arbitrary owners: only the headed owner's byte
     * chooses 130/134. Neither target resolution nor partner state is admission. */
    for (int own=0; own<3; ++own) for (int enemy=0; enemy<3; ++enemy)
    for (int first=0; first<8; ++first) for (int second=0; second<8; ++second) {
        setup_heading_battle(1,0);
        peeps[23]=owners[own]; peeps[45]=owners[enemy];
        memcpy(peeps+22+14,&headings[first],4); memcpy(peeps+44+14,&headings[second],4);
        set_peep_word(1,6,-123); set_peep_word(2,6,6);
        set_peep_word(1,8,-1); set_peep_word(2,8,32767);
        peeps[44]=1; set_peep_word(2,4,0);
        int want=70;
        if (first && second) want=138;
        else if (first) want=own==0 ? 130 : 134;
        else if (second) want=enemy==0 ? 130 : 134;
        check_battle_frame_only(1,2,true,want,1);
    }
    /* Independently enumerate every 16-bit encoding, using the three permitted
     * predecessor words rather than duplicating the production signed bounds. */
    const int bases[]={70,130,134,138};
    for (int group=0; group<4; ++group) {
        setup_heading_battle(1,group==2);
        memset(peeps+22+14,0,4);
        if (group) peeps[22+14]=1;
        if (group==3) peeps[44+17]=0x80;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        for (unsigned bits=0; bits<65536; ++bits) {
            int decoded=(bits&0x8000u) ? (int)bits-65536 : (int)bits;
            set_peep_word(1,12,decoded);
            int want=bases[group], reset=1;
            if (bits==(unsigned)(bases[group]-1)) reset=0;
            if (bits==(unsigned)bases[group]) { want=bases[group]+1; reset=0; }
            if (bits==(unsigned)(bases[group]+1)) { want=bases[group]+2; reset=0; }
            assert(core_frame_battle(1,2,true)==reset);
            assert(peep_word(1,12)==want);
        }
        set_peep_word(1,12,102);
        assert(a4_same(before));
    }
    for (int flags=0; flags<256; ++flags) for (int raw=0; raw<2; ++raw)
    for (int population=-1; population<=1; ++population) {
        setup_heading_battle(1,0); peeps[22]=flags;
        set_peep_word(1,4,population); set_peep_word(1,12,130);
        int admitted=raw && (flags/8)%2 && !((flags/16)%2);
        check_battle_frame_only(1,2,raw,admitted ? 131 : 130,0);
        set_peep_word(1,12,32767);
        check_battle_frame_only(1,2,raw,admitted ? 130 : 32767,admitted);
    }
    const int counts[]={-32768,-1,0,1,2,3,4,207,208,209,32767};
    const int indices[]={-32768,-1,0,1,2,3,206,207,208,32767};
    for (unsigned count=0; count<sizeof counts/sizeof counts[0]; ++count)
    for (unsigned first=0; first<sizeof indices/sizeof indices[0]; ++first)
    for (unsigned second=0; second<sizeof indices/sizeof indices[0]; ++second) {
        memset(prg_a4_area,0,sizeof prg_a4_area); start_state(); no_peeps=counts[count];
        for (int person=0; person<208; ++person) {
            peeps[person*22]=8; set_peep_word(person,12,71);
        }
        int person=indices[first], partner=indices[second];
        int valid=counts[count]>=0 && counts[count]<=208 && person>=0 &&
            person<counts[count] && partner>=0 && partner<counts[count] && person!=partner;
        check_battle_frame_only(person,partner,true,valid ? 72 : 71,0);
    }
}

static void test_battle_frame_integration(void)
{
    /* Only population, the two frame words and the RNG may change while live. */
    for (int heads=0; heads<4; ++heads) for (int owner=0; owner<2; ++owner) {
        setup_heading_battle(1,owner);
        memset(peeps+22+14,0,4);
        if (heads&1) peeps[22+17]=0x80;
        if (heads&2) peeps[44+14]=0xff;
        set_peep_word(1,4,1000); set_peep_word(2,4,400);
        peeps[25]=2; peeps[47]=3;
        /* Counters are not the actual partner argument. */
        set_peep_word(1,6,-123); set_peep_word(2,6,6);
        const int bases[2][4]={{70,130,134,138},{70,134,130,138}};
        int base=bases[owner][heads];
        set_peep_word(1,12,base); set_peep_word(2,12,base+2);
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        set_peep_word(1,4,978); set_peep_word(2,4,382);
        set_peep_word(1,12,base+1); set_peep_word(2,12,base);
        seed=28505;
        memcpy(expected,prg_a4_area,sizeof expected);
        memcpy(prg_a4_area,before,sizeof before);
        core_battle(1,2);
        assert(peep_word(1,12)==base+1 && peep_word(2,12)==base);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_battle_frame_deaths(void)
{
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int heads=0; heads<4; ++heads) for (int both=0; both<2; ++both)
    for (int negative=0; negative<4; ++negative) {
        setup_heading_battle(winner,owner);
        int loser=3-winner;
        memset(peeps+winner*22+14,0,4);
        if (heads&1) peeps[winner*22+17]=0x80;
        if (heads&2) peeps[loser*22+14]=0xff;
        const int bases[2][4]={{70,130,134,138},{70,134,130,138}};
        int base=bases[owner][heads];
        set_peep_word(1,12,base); set_peep_word(2,12,base+2);
        if (both) set_peep_word(winner,4,10);
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        core_battle(1,2);
        assert(seed==28505 && !peep_word(loser,4));
        if (both) assert(!peep_word(winner,4));
        else assert(peep_word(winner,4)==90 && peep_word(winner,12)==85 && peeps[winner*22]==6);
        /* Same host outcome for 9 and 10; only nonzero casualties animate the
         * retained dead record. Explicit literal successor/wrap per slot. */
        if (negative&(1<<(loser-1))) set_peep_word(loser,12,loser==1 ? base+1 : base);
        if (both && (negative&(1<<(winner-1)))) set_peep_word(winner,12,winner==1 ? base+1 : base);
        memcpy(expected,prg_a4_area,sizeof expected);
        memcpy(prg_a4_area,before,sizeof before);
        for (int person=1; person<=2; ++person)
            if ((person==loser || both) && (negative&(1<<(person-1)))) set_peep_word(person,4,9);
        core_battle(1,2);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    for (int winner=1; winner<=2; ++winner) for (int both=0; both<2; ++both) {
        setup_heading_battle(winner,0);
        set_peep_word(1,4,32000); set_peep_word(2,4,32000);
        peeps[winner*22+3]=255;
        if (both) peeps[(3-winner)*22+3]=255;
        core_battle(1,2);
        assert(seed==28505 && peep_word(3-winner,4)==15926);
        assert(peep_word(3-winner,12)==130);
        assert(peep_word(winner,4)==(both ? 15926 : 31990));
        assert(peep_word(winner,12)==130);
        assert(peeps[winner*22]==8 && peeps[(3-winner)*22]==8);
    }
    /* Mixed water remains in host combat, but the source-priority helper skips it. */
    setup_heading_battle(1,0); peeps[22]=0x18;
    set_peep_word(1,4,1000); set_peep_word(2,4,400);
    core_battle(1,2);
    assert(peep_word(1,12)==102 && peep_word(2,12)==130);
}

/* Literal SUB.W witnesses: changing draw order, division order, byte signedness,
 * word wrapping or the signed winner test must break these full-A4 checks. */
static void test_battle_loss_survivors(void)
{
    static const struct {
        int first, second, weapon_first, weapon_second, initial_seed, final_seed;
        int want_first, want_second;
    } cases[]={
        {1000,400,2,3,123,28505,978,382},
        {32000,32000,0,255,123,28505,15926,31990},
        {32000,32000,255,255,123,28505,15926,15926},
        {10975,30000,0,255,123,28505,1,29990},
        {32201,32000,0,203,123,28505,32767,31990},
        /* lesser 99/100/101: divide before multiplying by the unsigned weapon. */
        {1000,99,2,3,123,28505,990,89},
        {1000,100,2,3,123,28505,987,88},
        {1000,101,2,3,123,28505,987,89},
        {1000,400,2,127,123,28505,482,382},
        {1000,400,2,128,123,28505,478,382},
        {2000,400,2,255,123,28505,970,382},
        /* All nine roll pairs; unequal strengths and either lesser operand. */
        {1000,400,2,3,1,24159,978,382},
        {1000,400,2,3,16,1326,966,374},
        {1000,400,2,3,2,2976,960,370},
        {1000,400,2,3,5,4963,978,382},
        {1000,400,2,3,0,12574,966,374},
        {1000,400,2,3,19,3313,954,366},
        {1000,400,2,3,22,5300,978,382},
        {1000,400,2,3,8,6950,966,374},
        {1000,400,2,3,3,14561,954,366},
        {400,1200,2,3,123,28505,354,1166}
    };
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample)
    for (int first=1; first<=2; ++first) for (int owner=0; owner<2; ++owner)
    for (int heads=0; heads<4; ++heads) {
        int second=3-first;
        setup_heading_battle(first,owner);
        memset(peeps+first*22+14,0,4);
        if (heads&1) peeps[first*22+17]=0x80;
        if (heads&2) peeps[second*22+14]=0xff;
        set_peep_word(first,4,cases[sample].first);
        set_peep_word(second,4,cases[sample].second);
        peeps[first*22+3]=cases[sample].weapon_first;
        peeps[second*22+3]=cases[sample].weapon_second;
        /* Direct calls do not require reciprocal counters; foreign occupancy
         * and all metadata/map bytes must remain untouched while both live. */
        set_peep_word(first,6,-123); set_peep_word(second,6,6); map_who[650]=4;
        const int bases[2][4]={{70,130,134,138},{70,134,130,138}};
        int base=bases[owner][heads];
        set_peep_word(first,12,base); set_peep_word(second,12,base+2);
        seed=cases[sample].initial_seed;
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        set_peep_word(first,4,cases[sample].want_first);
        set_peep_word(second,4,cases[sample].want_second);
        set_peep_word(first,12,base+1); set_peep_word(second,12,base);
        seed=cases[sample].final_seed;
        memcpy(expected,prg_a4_area,sizeof expected); memcpy(prg_a4_area,before,sizeof before);
        core_battle(first,second);
        assert(peep_word(first,4)==cases[sample].want_first);
        assert(peep_word(second,4)==cases[sample].want_second);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==17 && !core_settlements && !core_result);
    }
}

static void test_battle_loss_completion(void)
{
    static const struct { int population, weapon, residual; } cases[]={
        {10973,255,-1}, {10974,255,0},
        {32202,203,-32768}, {32203,203,-32767}
    };
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample)
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int heads=0; heads<4; ++heads) for (int town=0; town<2; ++town)
    for (int reverse=0; reverse<2; ++reverse) {
        int loser=3-winner, enemy=1-owner, nonzero=cases[sample].residual!=0;
        setup_heading_town(winner,owner,cases[sample].population);
        if (!town) { peeps[loser*22]=8; map_blk[650]=1; map_bk2[650]=0; }
        memset(peeps+winner*22+14,0,4);
        if (heads&1) peeps[winner*22+17]=0x80;
        if (heads&2) peeps[loser*22+14]=0xff;
        int population=sample<2 ? 30000 : 32000;
        set_peep_word(winner,4,population); peeps[winner*22+3]=cases[sample].weapon;
        set_peep_word(winner,12,102); game_turn=12345;
        seed=reverse ? 2 : 123;
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        /* Zero retains town frame 35/reward 104, while negative reframes to
         * 34/reward 103. No production completion/frame/life helper is an oracle. */
        int reward=town ? (nonzero ? 103 : 104) : ((heads&2) ? 24 : 25);
        peeps[winner*22]=town && !(heads&1) ? 5 : 6;
        set_peep_word(winner,4,population-10);
        set_peep_word(winner,10,0); set_peep_word(winner,12,85);
        if (town && !(heads&1)) set_peep_word(winner,6,12345);
        const int bases[2][4]={{70,130,134,138},{70,134,130,138}};
        set_peep_word(loser,12,nonzero ? (town ? 34 : bases[owner][heads]) : 35);
        int remnant=town && (heads&1);
        peeps[loser*22]=remnant ? 0x80 : 0;
        set_peep_word(loser,4,remnant ? 1 : 0); set_peep_word(loser,6,remnant ? 40 : 0);
        if (town) {
            map_blk[650]=remnant ? 66 : 31+owner;
            map_bk2[650]=remnant ? 56 : 34;
            if (nonzero || !remnant) { A4_WORD(0x2ff8)=0; A4_WORD(0x2fb2)=remnant ? 0 : 1; }
        }
        map_who[650]=winner+1;
        --peeps[winner*22+20];
        A4_WORD(0x82d2+owner*46)=2; A4_WORD(0x82d4+owner*46)=650;
        A4_WORD(0x82d2+enemy*46)=0;
        A4_WORD(0xae7a+owner*2)=8;
        A4_LONG(0x9a02+owner*16)=1000+reward;
        A4_LONG(0x9a02+enemy*16)=2000-reward;
        if (!remnant) {
            core_leader(enemy)=0; core_target(enemy)=650;
            A4_WORD(enemy ? 0xae76 : 0xae78)=650;
        }
        view_who=winner+1; seed=reverse ? 2976 : 28505;
        /* _battle_over asm:6556-6560 -> _zero_population asm:5671-5680
         * clears the dead loser's previous-cell claim (650-(-64)). */
        if (!remnant) map_who[714]=0;
        memcpy(expected,prg_a4_area,sizeof expected); memcpy(prg_a4_area,before,sizeof before);
        /* Seed 2 draws 28193/2976 (rolls 3/1), reversing seed 123's 1/3.
         * Both argument orders retain winner roll 1 and loser roll 3. */
        core_battle(reverse ? winner : loser,reverse ? loser : winner);
        assert(peep_word(winner,4)==population-10);
        assert(peep_word(loser,12)==(nonzero ? (town ? 34 : bases[owner][heads]) : 35));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_battle_loss_both_dead(void)
{
    static const struct { int first, second, weapon, initial_seed, final_seed, first_frame, second_frame; } cases[]={
        /* Literal frame offsets from each heading-dependent combat base. */
        {10,10,0,123,28505,1,2},
        {10,9,0,123,28505,1,0},
        {9,10,0,123,28505,2,2},
        {9,9,0,123,28505,2,0},
        /* Seed 3 rolls 3/3 in either order: loss 250675 wraps to -21300. */
        {32767,32767,255,3,14561,2,0}
    };
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample)
    for (int owner=0; owner<2; ++owner) for (int foreign=0; foreign<2; ++foreign)
    for (int heads=0; heads<4; ++heads) for (int reverse=0; reverse<2; ++reverse) {
        setup_heading_battle(1,owner); memset(peeps+22+14,0,4);
        if (heads&1) peeps[22+17]=0x80;
        if (heads&2) peeps[44+14]=0xff;
        const int bases[2][4]={{70,130,134,138},{70,134,130,138}};
        int base=bases[owner][heads];
        set_peep_word(1,4,cases[sample].first); set_peep_word(2,4,cases[sample].second);
        peeps[25]=peeps[47]=cases[sample].weapon;
        set_peep_word(1,12,base+1); set_peep_word(2,12,base+2);
        core_leader(1-owner)=3; map_who[650]=foreign ? 4 : 3;
        seed=cases[sample].initial_seed;
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        for (int person=1; person<=2; ++person) {
            peeps[person*22]=0; set_peep_word(person,4,0); set_peep_word(person,6,0);
        }
        set_peep_word(1,12,base+cases[sample].first_frame);
        set_peep_word(2,12,base+cases[sample].second_frame);
        core_leader(0)=core_leader(1)=0; core_target(0)=core_target(1)=650;
        A4_WORD(0xae78)=A4_WORD(0xae76)=650; view_who=0;
        map_who[650]=foreign ? 4 : 0; seed=cases[sample].final_seed;
        /* _do_battle asm:6175-6179 zeroes both; _zero_population
         * asm:5671-5680 clears slot 2's previous-cell claim at 714. */
        map_who[714]=0;
        memcpy(expected,prg_a4_area,sizeof expected); memcpy(prg_a4_area,before,sizeof before);
        core_battle(reverse ? 2 : 1,reverse ? 1 : 2);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==17 && !core_settlements && !core_result);
    }
}

static void test_battle_loss_ticks(void)
{
    /* Low slot owns dispatch; the later survivor/remnant consumes its new state
     * once, never drawing a second pair of battle rolls in the same pass. */
    for (int tick=0; tick<2; ++tick) for (int later=0; later<2; ++later)
    for (int winner=1; winner<=2; ++winner) for (int owner=0; owner<2; ++owner)
    for (int headed=0; headed<2; ++headed) for (int route=0; route<3; ++route) {
        int loser=3-winner;
        setup_heading_town(winner,owner,route==1 ? 10974 : 10973);
        if (!headed) memset(peeps+winner*22+14,0,4);
        set_peep_word(winner,4,route ? 30000 : 32000);
        peeps[winner*22+3]=255;
        if (!route) { set_peep_word(loser,4,32000); peeps[loser*22+3]=255; }
        core_leader(0)=core_leader(1)=0;
        int first=later ? 5 : 1, second=later ? 9 : 2;
        if (later) {
            memcpy(peeps+first*22,peeps+22,22); memcpy(peeps+second*22,peeps+44,22);
            memset(peeps+22,0,44); no_peeps=10;
        }
        int win=winner==1 ? first : second, lose=winner==1 ? second : first;
        set_peep_word(first,6,second); set_peep_word(second,6,first);
        map_who[650]=lose+1; view_who=lose+1;
        seed=route && winner==1 ? 2 : 123;
        game_turn=0;
        if (tick) game_tick(); else _move_peeps();
        /* Both owners pick first (tendency 0, clear latches, asm:9612-9633):
         * 2 -> 28193 -> 2976 or 123 -> 15930 -> 28505; the battle rolls then
         * draw 29823 -> 17598 (rolls 1,1) or 12248 -> 7095 (rolls 3,1). The
         * lesser strength becomes the loser's own population, so route 1's
         * 10974 no longer wraps to a raw-zero residual (10974-(109*255+10)
         * = -16831): both routes take the raw-nonzero remnant path. */
        assert(seed==(route && winner==1 ? 17598 : 7095));
        assert(game_turn==tick && !core_result && core_battles==17);
        if (!route) {
            assert(peep_word(win,4)==15926 && peep_word(lose,4)==15926);
            assert(peeps[win*22]==8 && peeps[lose*22]==9);
            int frame=headed ? (owner ? 134 : 130) : 70;
            assert(peep_word(win,12)==frame && peep_word(lose,12)==frame);
            assert(map_bk2[650]==35 && core_mana(owner)==1000);
        } else {
            assert(peep_word(win,4)==29990 && peep_word(win,12)==(winner==2 ? 86 : 85));
            assert(peep_word(lose,12)==34);
            assert(peeps[win*22]==(headed ? 6 : 5));
            assert(peeps[lose*22]==(headed ? 0x80 : 0));
            assert(peep_word(lose,4)==headed);
            assert(peep_word(lose,6)==(headed ? (winner==1 ? 39 : 40) : 0));
            assert(core_mana(owner)==1103);
            assert(map_bk2[650]==(headed ? 56 : 34));
            assert(map_who[650]==win+1 && view_who==win+1);
        }
        if (tick) {
            /* asm:3459-3474 adds each word at its own visit: the low slot
             * before it dispatches the battle (asm:4270-4282), the high slot
             * after, so with the post-battle word. */
            uint32_t own=winner==1 ? (route ? 30000u : 32000u) : (route ? 29990u : 15926u);
            uint32_t foe=10u+(winner==1 ? (route ? (uint32_t)headed : 15926u)
                                        : (route ? (route==1 ? 10974u : 10973u) : 32000u));
            assert(A4_LONG(owner ? 0x9a0e : 0x99fe)==own);
            assert(A4_LONG(owner ? 0x99fe : 0x9a0e)==foe);
        }
    }
}

static void test_battle_loss(void)
{
    test_battle_loss_survivors(); test_battle_loss_completion(); test_battle_loss_both_dead();
    test_battle_loss_ticks();
}

static void setup_battle_entry(int attacker, int owner, int destination)
{
    setup_heading_battle(attacker,owner);
    int defender=3-attacker;
    memset(map_blk,1,4096); memset(map_bk2,0,4096);
    memset(map_who,0,4096);
    for (int person=1; person<=2; ++person) {
        peeps[person*22]=2;
        memset(peeps+person*22+14,0,4);
        set_peep_word(person,4,100);
        set_peep_word(person,6,123);
        set_peep_word(person,12,35);
    }
    set_peep_word(attacker,8,destination ? 649 : 650);
    map_who[650]=defender+1;
    if (destination) map_who[649]=attacker+1;
    core_target(owner)=651; core_tendency(owner)=0;
}

static void test_battle_entry_state(void)
{
    /* Missing either call, wrong partner/owner, always resetting, or changing
     * host movement/occupancy must fail the full-A4 oracle. Raw byte headings
     * deliberately cannot resolve to a host target and must stay untouched. */
    const int bases[2][2][2]={{{70,134},{130,138}},{{70,130},{134,138}}};
    const int flags[]={0,1,2,0x12,0x61};
    const int offsets[]={-1,0,1,2,-32768,32767,-1};
    const int successors[]={0,1,2,0,0,0,0};
    for (int destination=0; destination<2; ++destination)
    for (int attacker=1; attacker<=2; ++attacker) for (int owner=0; owner<2; ++owner)
    for (int first=0; first<=4; ++first) for (int second=0; second<=4; ++second)
    for (unsigned state=0; state<sizeof flags/sizeof flags[0]; ++state)
    for (int frame=0; frame<7; ++frame) {
        setup_battle_entry(attacker,owner,destination);
        int defender=3-attacker, base=bases[owner][first!=0][second!=0];
        if (first) peeps[attacker*22+13+first]=0x80;
        if (second) peeps[defender*22+13+second]=0x80;
        peeps[defender*22]=flags[state];
        int old_first=frame<4 ? base+offsets[frame] : offsets[frame];
        int reverse=6-frame;
        int old_second=reverse<4 ? base+offsets[reverse] : offsets[reverse];
        set_peep_word(attacker,12,old_first); set_peep_word(defender,12,old_second);
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        peeps[attacker*22+21]=1;
        /* _move_explorer asm:4510-4617 has no destination encounter: a walker
         * stepping onto the defender only claims/counts 649 and advances. */
        int attacker_frame=destination ? old_first : base+successors[frame];
        int defender_frame=destination ? old_second : base+successors[reverse];
        if (destination) {
            set_peep_word(attacker,8,650); set_peep_word(attacker,10,1);
            A4_WORD(0xff88+649*2)=(uint16_t)A4_WORD(0xff88+649*2)+1;
        } else {
            peeps[attacker*22]=8; peeps[defender*22]=(flags[state]&7)|8;
            set_peep_word(attacker,6,defender); set_peep_word(defender,6,attacker);
            map_who[650]=attacker+1;
        }
        set_peep_word(attacker,12,attacker_frame); set_peep_word(defender,12,defender_frame);
        memcpy(expected,prg_a4_area,sizeof expected); memcpy(prg_a4_area,before,sizeof before);
        core_move(attacker);
        assert(peep_word(attacker,12)==attacker_frame);
        assert(peep_word(defender,12)==defender_frame);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==(destination ? 17 : 18) && !core_settlements && !core_result);
    }
    /* Current-cell admission precedes the nonpositive-attacker guard; there is
     * no destination admission. Defenders have no population gate, but zero
     * alone must skip frame initialization. */
    for (int destination=0; destination<2; ++destination)
    for (int population=-1; population<=0; ++population) {
        setup_battle_entry(1,0,destination); set_peep_word(1,4,population);
        core_move(1);
        assert(peep_word(1,12)==(!destination && population ? 70 : 35));
        assert(peep_word(2,12)==(destination ? 35 : 70));
        assert(peep_word(1,4)==population && peep_word(2,4)==100 && seed==123);
        assert(core_battles==(destination ? 17 : 18));
        /* asm:4510-4559: the current-cell encounter has no defender
         * population gate either. */
        setup_battle_entry(1,0,0); set_peep_word(2,4,population);
        core_move(1);
        assert(peep_word(1,12)==70 && peep_word(2,12)==(population ? 70 : 35));
        assert(peep_word(2,4)==population && core_battles==18 && seed==123);
    }
}

static void test_battle_entry_wait_retry(void)
{
    for (int owner=0; owner<2; ++owner) {
        setup_battle_entry(1,owner,0);
        peeps[22]=0x42; set_peep_word(1,6,15); set_peep_word(1,12,101);
        set_peep_word(1,4,1); A4_WORD(0x2fb0)=2;
        uint8_t before[sizeof prg_a4_area], expected[sizeof prg_a4_area];
        memcpy(before,prg_a4_area,sizeof before);
        /* Retry population is -1 at entry: its retained dead frame proves raw
         * nonzero (not positive) was passed, despite subsequent host cleanup. */
        peeps[22]=0; set_peep_word(1,4,0); set_peep_word(1,6,0); set_peep_word(1,12,70);
        peeps[22+21]=1;
        set_peep_word(2,6,0); set_peep_word(2,10,0); set_peep_word(2,12,1);
        core_leader(owner)=0; core_target(owner)=650;
        A4_WORD(owner ? 0xae76 : 0xae78)=650;
        set_peep_word(3,6,99); map_who[900]=4;
        memcpy(expected,prg_a4_area,sizeof expected); memcpy(prg_a4_area,before,sizeof before);
        expect_ai_fixture_picks(expected);
        expect_pass_good_pop(expected); _move_peeps();
        assert(peep_word(1,12)==70 && !peep_word(1,4));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==18 && !core_settlements && !core_result);
    }
}

static void test_battle_entry_town_reward(void)
{
    for (int destination=0; destination<2; ++destination)
    for (int attacker=1; attacker<=2; ++attacker) for (int owner=0; owner<2; ++owner)
    for (int headed=0; headed<2; ++headed) for (int negative=0; negative<2; ++negative) {
        setup_heading_town(attacker,owner,negative ? 9 : 10);
        if (!headed) memset(peeps+attacker*22+14,0,4);
        int defender=3-attacker, base=headed ? (owner ? 134 : 130) : 70;
        game_turn=12345;
        peeps[attacker*22]=2; peeps[defender*22]=1;
        set_peep_word(attacker,8,destination ? 649 : 650);
        if (destination) map_who[649]=attacker+1;
        core_target(owner)=650+!destination; core_tendency(owner)=0;
        if (destination) {
            /* _move_explorer asm:4590-4617: entering the town cell claims
             * 649 only; the encounter waits for the next departure. */
            core_move(attacker);
            assert(peep_word(attacker,8)==650 && peep_word(attacker,10)==1);
            assert(peeps[attacker*22]==2 && peeps[defender*22]==1 && core_battles==17);
            assert(map_who[649]==attacker+1 && map_who[650]==defender+1);
            core_target(owner)=651;
        }
        core_move(attacker);
        assert(!map_who[649]);
        assert(peep_word(attacker,12)==base && peep_word(defender,12)==base);
        assert(peeps[attacker*22]==8 && peeps[defender*22]==9);
        assert(seed==123 && core_mana(owner)==1000 && core_mana(1-owner)==2000);
        assert(peep_word(attacker,4)==100 && peep_word(defender,4)==(negative ? 9 : 10));
        core_battle(1,2);
        assert(peeps[attacker*22]==(headed ? 6 : 5) && peep_word(attacker,12)==85 && peep_word(attacker,4)==90);
        assert(peeps[defender*22]==(headed ? 0x80 : 0));
        assert(peep_word(defender,4)==headed && peep_word(defender,6)==(headed ? 40 : 0));
        assert(peep_word(attacker,6)==(headed ? defender : 12345));
        assert(peep_word(defender,12)==(negative ? 34 : base));
        assert(core_mana(owner)==(negative ? 1103 : 1100));
        assert(core_mana(1-owner)==(negative ? 1897 : 1900));
        assert(A4_WORD(0xae7a+owner*2)==8 && seed==28505 && core_battles==18);
        assert(map_who[650]==attacker+1 && map_blk[650]==(headed ? 66 : 31+owner));
        assert(map_bk2[650]==(headed ? 56 : 34));
        assert(A4_WORD(0x2ff8)==(!headed || negative ? 0 : 1234));
        assert(A4_WORD(0x2fb2)==(!headed ? 1 : negative ? 0 : 2345));
    }
}

/* _move_sprite walking branch (asm:16061-16224) and SUB_49F72 (asm:16225-16254):
 * a walker is drawn from the map_who (departure) cell, moved by +$C along
 * +$A with the altitude difference, and hidden in the second half of moves
 * leaving the 8x8 view. Literal per-direction table from the asm. */
static void test_walker_interpolation(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    static const struct { int step,dx,dy,hide_u,hide_v,frame; } moves[]={
        {-65,0,-2,0,0,0},{-64,2,-1,-1,0,2},{-63,4,0,7,0,4},{1,2,1,7,-1,6},
        {65,0,2,7,7,8},{64,-2,1,-1,7,10},{63,-4,0,0,7,12},{-1,-2,-1,0,-1,14},{5,0,0,-1,-1,8}};
    for (unsigned m=0;m<sizeof moves/sizeof *moves;++m)
    for (int u=0;u<8;u+=7) for (int v=0;v<8;v+=7) for (int counter=0;counter<9;++counter)
    for (int water=0;water<(counter%3==2 ? 2 : 1);++water) {   /* water: counters 2,5,8 */
        int flat=(counter+u)&1, owner=(counter>>1)&1, headed=v>0, tog=counter&1, turn=counter+m;
        sprite_scene(); toggle=tog; game_turn=turn; player=1;
        int cell=(16+v)*64+16+u, next=cell+moves[m].step;
        map_alt[cell]=3; if (next>=0 && next<4096) map_alt[next]=1;
        if (!flat) map_blk[cell]=20;
        uint8_t expected[64000],before[sizeof prg_a4_area];
        draw_original(16,16); memcpy(expected,game_pixels,sizeof expected);
        /* Departure-cell layout of core_move: +8 is the destination. */
        peeps[0]=water ? 0x12 : 2; peeps[1]=owner; peeps[17]=headed ? 0x80 : 0;
        set_peep_word(0,4,100); set_peep_word(0,8,next); set_peep_word(0,10,moves[m].step);
        set_peep_word(0,12,counter); set_peep_word(0,18,water);
        map_who[cell]=1;
        int frame=water ? 93+(turn&3)+16*headed+4*owner : moves[m].frame+tog+16*owner+32*headed;
        int sx=176+16*(u-v)+8+moves[m].dx*counter;
        int sy=64+8*(u+v)-8*3-8+(flat ? 0 : 4)+2*counter+moves[m].dy*counter;
        int hidden=counter>=8 || (counter>4 && (u==moves[m].hide_u || v==moves[m].hide_v));
        if (!hidden) sprite_mask_oracle(expected,frame,sx,sy);
        memcpy(before,prg_a4_area,sizeof before); draw_original(16,16);
        assert(a4_same(before)); assert_sprite_pixels(expected);
        /* asm:15965-15982, before the dispatch (so also when hidden): own
         * peeps OR the whole flag byte (+1 for +$12), others flags&$F8. */
        assert(ok_to_build==(owner==1 ? peeps[0]|water : peeps[0]&0xf8));
        if (hidden) { assert(no_sprites==0); continue; }
        assert(no_sprites==1 && A4_WORD(0x9a22)==sx && A4_WORD(0x9a24)==sy &&
               A4_WORD(0x9a26)==frame && A4_WORD(0x9a28)==1);
    }
    /* A walker whose map_who cell is neither +8 nor +8-+$A is not drawn. */
    sprite_scene(); peeps[0]=2; set_peep_word(0,4,100);
    set_peep_word(0,8,16*64+18); set_peep_word(0,10,1); map_who[16*64+16]=1;
    draw_original(16,16); assert(no_sprites==0);
    /* The view_who marker: frame $44 at x+8, after the occupant. */
    sprite_scene(); toggle=0; peeps[0]=2; set_peep_word(0,4,100);
    set_peep_word(0,8,16*64+17); set_peep_word(0,10,1); set_peep_word(0,12,2);
    map_who[16*64+16]=1; view_who=1;
    uint8_t expected[64000];
    map_who[16*64+16]=0; draw_original(16,16); memcpy(expected,game_pixels,sizeof expected);
    map_who[16*64+16]=1;
    sprite_mask_oracle(expected,6,188,58); sprite_mask_oracle(expected,0x44,196,58);
    draw_original(16,16); assert_sprite_pixels(expected);
    assert(no_sprites==2 && A4_WORD(0x9a2a)==196 && A4_WORD(0x9a2c)==58 &&
           A4_WORD(0x9a2e)==0x44 && A4_WORD(0x9a30)==1);
    view_who=0;
    _closeall(); SDL_Quit();
}

static void test_battle_frame_render_and_entry(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    const int frames[]={70,71,72,130,131,132,134,135,136,138,139,140};
    const int invalid[]={0,35,73,129,133,137,141,32767,-32768,-1};
    uint32_t headings[]={0,1,UINT32_MAX,0x80000000u,0x00008000u};
    /* _move_sprite tests bit 4 before bit 3 (asm:16004-16012): 0x18/0xff
     * take the walking branch, whose +$C >= 8 is the _debug exit. */
    const int flags[]={8,9,0x0c,0x8b};
    for (int owner=0; owner<2; ++owner) for (int turn=0; turn<8; ++turn)
    for (unsigned raw=0; raw<sizeof headings/sizeof headings[0]; ++raw) {
        for (unsigned frame=0; frame<sizeof frames/sizeof frames[0]; ++frame) {
            sprite_case(flags[frame%4],owner,frames[frame],headings[raw],turn,1,15,0,0,0,
                        frames[frame],192,56);
            sprite_case(frame%2 ? 0xff : 0x18,owner,frames[frame],headings[raw],turn,1,15,0,0,0,
                        -1,192,56);
        }
        for (unsigned frame=0; frame<sizeof invalid/sizeof invalid[0]; ++frame)
            sprite_case(8,owner,invalid[frame],headings[raw],turn,1,15,0,0,0,
                        turn<4 ? 138 : 139,192,56);
    }
    /* Canonical new battles render their initialized base, not the legacy
     * phase fallback, before any casualty/RNG pass in either encounter path. */
    const int bases[2][4]={{70,130,134,138},{70,134,130,138}};
    for (int destination=0; destination<2; ++destination) for (int phase=0; phase<2; ++phase)
    for (int owner=0; owner<2; ++owner) for (int heads=0; heads<4; ++heads) {
        assert(game_reset(0)); sprite_scene(); game_turn=phase*4;
        int cell=16*64+16;
        for (int person=0; person<2; ++person) {
            peeps[person*22]=2; peeps[person*22+1]=owner^person;
            set_peep_word(person,4,100);
            set_peep_word(person,8,cell+(person && destination));
            set_peep_word(person,12,person ? 35 : 0);
            if (heads&(1<<person)) peeps[person*22+17]=0x80;
        }
        core_target(owner)=cell+1; core_tendency(owner)=0; core_leader(owner)=1;
        map_who[cell+destination]=2;
        seed=123;
        if (destination) {
            /* _move_explorer asm:4510-4617: stepping onto the defender is not
             * an encounter; the next departure's current-cell check is. */
            core_move(0);
            assert(peeps[0]==2 && peeps[22]==2 && peep_word(0,8)==cell+1);
            assert(map_who[cell]==1 && map_who[cell+1]==2);
            core_target(owner)=cell+2;
        }
        core_move(0);
        assert((peeps[0]&8) && (peeps[22]&8) && seed==123);
        assert(peep_word(0,12)==bases[owner][heads] && peep_word(1,12)==bases[owner][heads]);
        assert(peep_word(0,4)==100 && peep_word(1,4)==100);
        assert(peep_word(0,8)==cell+destination && peep_word(1,8)==cell+destination);
        /* _set_battle asm:6604-6656: the attacker represents the cell. */
        assert(map_who[cell+destination]==1 && !map_who[cell+1-destination]);
        uint8_t expected[64000], before[sizeof prg_a4_area];
        int occupant=map_who[cell+destination];
        map_who[cell+destination]=0;
        draw_original(16,16); memcpy(expected,game_pixels,sizeof expected);
        sprite_mask_oracle(expected,bases[owner][heads],destination ? 208 : 192,destination ? 64 : 56);
        map_who[cell+destination]=occupant;
        memcpy(before,prg_a4_area,sizeof before); draw_original(16,16);
        assert(a4_same(before)); assert_sprite_pixels(expected);
    }
    _closeall(); SDL_Quit();
}

/* Dry exact-0x11 recovery: independent literal capacities and word arithmetic.
 * In particular, removing the first town frame rewrites sentinel overlays for
 * negative words; treating zero as nonzero suppresses a required refresh. */
static void setup_water_town(int owner, int fixture, int position)
{
    setup_town_economy(owner,100);
    memset(map_blk,1,4096); memset(map_bk2,55,4096); memset(map_who,0,4096);
    set_peep_word(0,8,position); set_peep_word(0,12,87); peeps[0]=0x11;
    map_who[position]=1;
    for (int cell=0; cell<4096; ++cell) {
        int dx=abs(cell%64-position%64), dy=abs(cell/64-position/64);
        int inner=dx<=1 && dy<=1;
        int ordinary=inner || (dx==2 && (dy==0 || dy==2)) || (dy==2 && dx==0);
        if ((fixture==1 && cell==position) || (fixture==2 && inner) ||
            (fixture>=3 && ordinary)) map_blk[cell]=31+owner;
        if (fixture>=5 && inner) map_bk2[cell]=41;
    }
    if (fixture==3 || fixture==6) map_blk[position+130]=32-owner;
    if (fixture>=5) map_bk2[position]=42;
    if (fixture==7) map_bk2[position-65]=55;
    if (fixture==8) map_blk[position]=15;
    core_leader(1-owner)=0;
    for (int level=0; level<11; ++level) {
        A4_WORD(0x2fb6+level*2)=-17;
        A4_WORD(0x2fcc+level*2)=0xab09;
        A4_WORD(0x2fe2+level*2)=11;
    }
}

static void test_water_town_state(void)
{
    const struct { int fixture, capacity, frame, city, flat; } plots[]={
        {0,0,32,0,0},{1,65,34,0,0},{2,185,38,0,0},{3,290,41,0,0},
        {4,3050,42,0,0},{5,3050,42,9,0},{6,290,41,9,0},
        {7,3050,42,8,0},{8,3050,42,9,1}
    };
    const struct { int initial, loss, water, grown; } populations[]={
        {100,1,98,109},{3,1,1,12},{2,1,0,11},{1,1,-1,10},
        {100,30000,5636,5647},{1,16385,32767,-32758},
        {100,16384,-32668,-32657},{1,0,1,12}
    };
    unsigned checked=0;
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int war=0; war<2; ++war) for (int heading=0; heading<9; ++heading)
    for (unsigned plot=0; plot<sizeof plots/sizeof plots[0]; ++plot)
    for (unsigned sample=0; sample<sizeof populations/sizeof populations[0]; ++sample)
    for (int turn=7; turn<=8; ++turn) {
        setup_water_town(owner,plots[plot].fixture,650);
        set_peep_word(0,4,populations[sample].initial);
        A4_WORD(0x2fb0)=populations[sample].loss; game_turn=turn;
        game_mode=fatal; A4_WORD(0xae80)=war;
        if (heading) {
            core_leader(owner)=0;
            peeps[14+(heading-1)/2]=(heading&1) ? 1 : 0x80;
        }
        /* Full pool excludes splits here while retaining their real predicate.
         * Live inert battle records keep allocation full without other work. */
        for (int person=1; person<208; ++person) {
            peeps[person*22]=8; peeps[person*22+1]=1-owner;
            set_peep_word(person,4,77); set_peep_word(person,6,person);
            set_peep_word(person,8,900);
        }
        no_peeps=208;
        /* A full pool also places a funny once (asm:3996-4006); that is
         * covered by test_funny_objects, here it has already happened. */
        funny_done=1;
        int occupant=sample%3; map_who[650]=occupant;
        uint8_t expected[sizeof prg_a4_area];
        int evacuate=!plots[plot].capacity || war || heading;
        int population=populations[sample].water;
        if (!evacuate && turn==8) population=populations[sample].grown;
        if (fatal) water_loss_expected(expected,owner,650,0);
        else {
            /* Reuse only the existing positive loss/queue oracle; death is
             * deliberately deferred until AFTER the town consumer below. */
            water_loss_expected(expected,owner,650,1);
            celebration_expected_word(expected,0x9c26+4,population);
            expected[peeps-prg_a4_area]=evacuate ? 2 : 1;
            celebration_expected_word(expected,0x2ff8,plots[plot].city);
            celebration_expected_word(expected,0x2fb2,plots[plot].flat);
            celebration_expected_word(expected,0x9c26+12,evacuate ? 0 : plots[plot].frame);
            if (evacuate) {
                celebration_expected_word(expected,0x9c26+10,0);
                town_footprint_map(expected,owner,650,0,0,1);
            } else {
                if (!occupant) expected[map_who-prg_a4_area+650]=1;
                expect_town_bookkeeping(expected,0,plots[plot].frame);
                expect_town_publish(expected,owner,plots[plot].frame!=42,plots[plot].frame==42);
                /* asm:3779-3797: clear latch; +20 is 0x94, so _make_level
                 * runs only on a reframe (water 0 here) or a_flat_block. */
                if (populations[sample].water==0 || plots[plot].flat) expect_make_level_exit(expected,0);
                if (turn==8) {
                    expected[peeps-prg_a4_area+3]=9;
                    town_expected_long(expected,0x9a02+owner*16,owner ? 23439 : 12328);
                }
                int refresh=populations[sample].water==0 || plots[plot].flat ||
                    (plots[plot].frame==42 && plots[plot].city<9);
                town_footprint_map(expected,owner,650,plots[plot].frame,refresh,population<=0);
                if (population<=0) {
                    expected[peeps-prg_a4_area]=0;
                    celebration_expected_word(expected,0x9c26+4,0);
                    if (occupant<=1) expected[map_who-prg_a4_area+650]=0;
                    celebration_expected_word(expected,0x99f6+owner*16,0);
                    celebration_expected_word(expected,0x99f8+owner*16,650);
                    celebration_expected_word(expected,owner ? 0xae76 : 0xae78,650);
                    celebration_expected_word(expected,0xae68,0);
                }
            }
        }
        expect_pass_good_pop(expected); _move_peeps();
        if (memcmp(expected,prg_a4_area,sizeof expected))
            fprintf(stderr,"water town owner=%d fatal=%d war=%d heading=%d plot=%u pop=%u turn=%d flags=%x pop=%d frame=%d\n",
                    owner,fatal,war,heading,plot,sample,turn,peeps[0],peep_word(0,4),peep_word(0,12));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==123);
        if (!fatal && evacuate && population<=0) {
            memcpy(expected,prg_a4_area,sizeof expected);
            expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        ++checked;
    }
    printf("dry town full-A4 state cases: %u\n",checked);
}

static void test_water_town_edges_and_skips(void)
{
    const int positions[]={0,63,4032,4095};
    const int blocks[]={0,1,15,31,32,53};
    for (int owner=0; owner<2; ++owner) for (int city=0; city<2; ++city)
    for (unsigned place=0; place<4; ++place) for (unsigned terrain=0; terrain<6; ++terrain)
    for (int fatal=0; fatal<2; ++fatal) {
        int position=positions[place];
        setup_town_footprint(owner,position,city,0);
        peeps[0]=0x11; set_peep_word(0,4,1); set_peep_word(0,12,87);
        map_blk[position]=blocks[terrain]; map_who[position]=1; game_mode=fatal;
        game_turn=7;
        uint8_t expected[sizeof prg_a4_area];
        /* Both zero-capacity evacuation and valid off-cadence death retain
         * their different record/ref/occupancy semantics at map boundaries. */
        int valid=blocks[terrain]==15 || blocks[terrain]==31+owner;
        if (fatal || !blocks[terrain] || valid) {
            water_loss_expected(expected,owner,position,0);
            if (!fatal && blocks[terrain]) {
                celebration_expected_word(expected,0x2ff8,city ? 4 : 0);
                celebration_expected_word(expected,0x2fb2,blocks[terrain]==15);
                celebration_expected_word(expected,0x9c26+12,37);
                expect_town_bookkeeping(expected,0,37);
                expect_town_publish(expected,owner,1,0);
                /* Clear latch, +20 0x94, frame already 37 at the town
                 * consumer: _make_level runs only with a_flat_block on a
                 * map_alt 0 cell (asm:3774-3786; the alt branch needs turn > 250). */
                if (blocks[terrain]==15 && !map_alt[position]) expect_make_level_exit(expected,0);
            }
        } else {
            water_loss_expected(expected,owner,position,1);
            expected[peeps-prg_a4_area]=2;
            celebration_expected_word(expected,0x9c26+4,-1);
            celebration_expected_word(expected,0x9c26+10,0);
            celebration_expected_word(expected,0x9c26+12,0);
            celebration_expected_word(expected,0x2ff8,0);
            celebration_expected_word(expected,0x2fb2,0);
            town_footprint_map(expected,owner,position,0,0,1);
        }
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    const int dead[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (unsigned sample=0; sample<3; ++sample) {
        setup_water_town(owner,5,650); game_mode=fatal; set_peep_word(0,4,dead[sample]);
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_water_town_split_and_growth(void)
{
    const struct { int initial, loss, controller, latch, cheat, growth, mother, child, final_latch; } cases[]={
        {308,1,1,0,0,11,163,154,1},{308,1,1,1,0,11,317,0,1},
        {308,1,0,0,0,11,317,0,0},{308,1,2,0,0,11,317,0,0},
        {308,1,1,0,1,11,36,281,1},{53,1,0,0,1,11,36,26,0},
        {53,1,0,0,2,11,62,0,0},{100,30000,0,0,0,11,1536,4111,0},
        {1,16385,0,0,0,11,1536,31242,0},
        {100,16384,0,0,0,-101,32767,0,0},
        {308,1,1,0,0,32767,0,154,1},{2,1,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0}
    };
    unsigned checked=0;
    for (int owner=0; owner<2; ++owner) for (int occupant=0; occupant<3; ++occupant)
    for (int stale=0; stale<3; ++stale)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        setup_town_capacity(owner,cases[sample].initial); peeps[0]=0x11;
        A4_WORD(0x2fb0)=cases[sample].loss;
        A4_WORD(0x82bc+owner*46)=cases[sample].controller;
        A4_WORD(0x82be + owner*46)=cases[sample].latch;
        A4_WORD(0x2fb4)=cases[sample].cheat==1 ? owner+1 : cases[sample].cheat==2 ? 2-owner : 0;
        A4_WORD(0x2ff6)=cases[sample].growth;
        for (int byte=0; byte<22; ++byte) peeps[22+byte]=0xa0+byte;
        const int dead[]={0,-1,-32768}; set_peep_word(1,4,dead[stale]);
        set_peep_word(1,6,0x8001+stale); peeps[43]=193+stale;
        /* Later ordinary enemy would expose recovery fallthrough/collision,
         * and proves processing continues even after parent growth death. */
        no_peeps=3; peeps[44]=2; peeps[45]=1-owner;
        set_peep_word(2,4,77); set_peep_word(2,8,650);
        set_peep_word(2,12,0); core_leader(1-owner)=3;
        map_who[650]=occupant==2 ? 3 : occupant;
        uint8_t expected[sizeof prg_a4_area];
        town_capacity_expected(expected,owner,cases[sample].mother,cases[sample].child,
                               cases[sample].final_latch,1,owner ? 23473 : 12362);
        /* asm:3461-3474: the later child slot is visited and counted too. */
        if (cases[sample].child>0) expect_good_pop_add(expected,owner,cases[sample].child);
        expect_town_publish(expected,owner,0,1);
        /* A wet town keeps frame 42 unless the water loss empties it, which
         * reframes it before the consumer: then a clear latch reaches
         * _make_level (asm:3779-3797). */
        if (!cases[sample].latch && cases[sample].initial==2*cases[sample].loss) expect_make_level_exit(expected,0);
        expected[peeps-prg_a4_area]=cases[sample].mother ? 1 : 0;
        celebration_expected_word(expected,0xae5e,3);
        celebration_expected_word(expected,0x9c26+44+12,1);
        if (cases[sample].child) {
            celebration_expected_word(expected,0x9c26+22+12,1);
            if (occupant==2) expected[map_who-prg_a4_area+650]=3;
            if (!cases[sample].mother) {
                celebration_expected_word(expected,0x99f8+owner*16,core_target(owner));
                celebration_expected_word(expected,owner ? 0xae76 : 0xae78,A4_WORD(owner ? 0xae76 : 0xae78));
            }
            /* asm:4148-4184: the exact-2 child (frame 0->1) stands on 650,
             * which names enemy peep 2 at frame 0: mark 0x20/101/0, then
             * slot 2 consumes the wait (frame 102, counter 1, walking loss). */
            if (occupant==2) {
                celebration_expected_word(expected,0x9c26+44+12,102);
                celebration_expected_word(expected,0x9c26+44+6,1);
                if (cases[sample].loss==1) {
                    expected[peeps-prg_a4_area+44]=0x22;
                    celebration_expected_word(expected,0x9c26+44+4,76);
                } else { /* 77-30000 and 77-16385 die: core_remove. */
                    expected[peeps-prg_a4_area+44]=0;
                    celebration_expected_word(expected,0x9c26+44+4,0);
                    expected[map_who-prg_a4_area+650]=0;
                    celebration_expected_word(expected,0x99f6+(1-owner)*16,0);
                    celebration_expected_word(expected,0x99f8+(1-owner)*16,650);
                    celebration_expected_word(expected,owner ? 0xae78 : 0xae76,650);
                }
            }
        }
        expect_pass_good_pop(expected); _move_peeps();
        if (memcmp(expected,prg_a4_area,sizeof expected)) {
            fprintf(stderr,"water split owner=%d occupant=%d stale=%d sample=%u\n",owner,occupant,stale,sample);
            for (size_t byte=0; byte<sizeof expected; ++byte)
                if (expected[byte]!=prg_a4_area[byte])
                    fprintf(stderr,"A4 %04zx expected=%02x actual=%02x\n",(byte-32768)&0xffff,expected[byte],prg_a4_area[byte]);
        }
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==123); ++checked;
    }
    printf("dry town split/growth A4 cases: %u\n",checked);
}

static void test_water_town_deferred_raise(void)
{
    for (int owner=0; owner<2; ++owner) for (int fertile=0; fertile<2; ++fertile)
    for (int initial=3; initial<=5; ++initial) for (int terminal=0; terminal<2; ++terminal) {
        setup_water_town(owner,0,650); game_turn=6;
        map_blk[650]=0; set_peep_word(0,4,initial);
        no_peeps=terminal ? 2 : 3;
        for (int person=1; person<no_peeps; ++person) {
            peeps[person*22]=2; peeps[person*22+1]=person==1 ? 1-owner : owner;
            set_peep_word(person,4,100); set_peep_word(person,8,900+person);
            map_who[900+person]=person+1;
        }
        if (fertile) {
            /* Start-only pending raise completes a height-one flat cell.
             * No mid-sequence edits to population, terrain or commands. */
            core_set_vertex(11,10,1); core_set_vertex(10,11,1); core_set_vertex(11,11,1);
            A4_DISP(0x82b6+owner*46)[0]=1;
            A4_DISP(0x82b7+owner*46)[0]=10;
            A4_DISP(0x82b8+owner*46)[0]=10;
        } else A4_WORD(0xae80)=1;
        game_tick();
        assert(peeps[0]==0x11 && peep_word(0,4)==initial-2 && peep_word(0,12)==87);
        assert(map_blk[650]==(fertile ? 15 : 17) && core_vertex(10,10)==1);
        assert(!core_result); assert_mailbox_empty();
        game_tick();
        assert(peeps[0]==(fertile ? 1 : 2));
        assert(peep_word(0,4)==(fertile ? initial+7 : initial-4));
        assert(peep_word(0,12)==(fertile ? 34 : 0));
        assert(map_who[650]==1 && view_who==1 && core_leader(owner)==1);
        assert(peep_word(0,6)==1234 && peep_word(0,10)==(fertile ? -65 : 0));
        assert(peep_word(1,12)==2 && core_population(1-owner)==100);
        assert(core_population(owner)==(terminal ? 0 : 100)+(fertile ? initial+7 : initial==5));
        /* asm:3459-3474: the dying town's pre-loss word still counts this
         * pass; the asm:4323-4347 end follows on the next pass. */
        assert(!core_result);
        assert(!core_battles && !core_settlements && seed==123); assert_mailbox_empty();
        if (!fertile && initial<5) {
            uint8_t record[22]; memcpy(record,peeps,22);
            game_tick(); assert(!memcmp(record,peeps,22));
            assert(map_who[650]==1 && view_who==1 && core_leader(owner)==1);
            assert(core_result==(terminal ? (owner ? 1 : 2) : 0));
        }
    }
}

static void test_water_town_gates(void)
{
    /* Catch running either the heading/war prepass, resident recovery or
     * pending terrain commands through any public tick freeze gate. */
    unsigned checked=0;
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (int war=0; war<2; ++war) for (int heading=0; heading<5; ++heading)
    for (int gate=0; gate<5; ++gate) {
        setup_water_town(owner,5,650); game_mode=fatal; A4_WORD(0xae80)=war;
        set_peep_word(0,4,1); game_turn=7;
        if (heading) peeps[14+heading-1]=0x80;
        no_peeps=18; peeps[17*22]=2; peeps[17*22+1]=1-owner;
        set_peep_word(17,4,100); set_peep_word(17,8,900);
        map_blk[900]=1; map_who[900]=18;
        A4_DISP(0x82b6+owner*46)[0]=1;
        A4_DISP(0x82b7+owner*46)[0]=10;
        A4_DISP(0x82b8+owner*46)[0]=10;
        pause=gate==0; core_result=gate>=1 && gate<=3 ? gate : 0; prg_quit=gate==4;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        game_tick();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_result==(gate>=1 && gate<=3 ? gate : 0) && prg_quit==(gate==4));
        assert(!core_battles && !core_settlements);
        ++checked;
    }
    pause=core_result=prg_quit=0;
    printf("dry town frozen full-A4 gate cases: %u\n",checked);
}

static void test_water_town(void)
{
    test_water_town_state(); test_water_town_edges_and_skips();
    test_water_town_split_and_growth(); test_water_town_deferred_raise();
    test_water_town_gates();
}

/* Admission must depend on the entry water bit, exact controller WORD and
 * no-build/fatal bits only. Literal resident outcomes are independent of the
 * queue helper and the existing water-loss oracle. */
static void test_water_rescue_admission(void)
{
    const int controllers[]={0,1,2,-1,0x0100,0x0101};
    const unsigned controller_count=sizeof controllers/sizeof controllers[0];
    const int wars[]={0,1,0x8000};
    const int positions[]={0,63,4032,4095};
    const struct { int flags, dry, initial, after_flags, population, frame; } cases[]={
        {18,0,100,18,98,0},{18,1,100,2,98,2},
        {0x10,0,100,0x10,98,0},{0x96,0,100,0x96,98,0},
        {0x96,1,100,2,100,1},{0x18,0,100,2,100,1},
        {0x18,1,100,2,100,1},{2,0,100,18,100,0},
        {18,0,0,18,0,0},{18,1,-1,18,-1,0},
        {18,0,1,0,0,0},{18,1,1,0,0,1}
    };
    unsigned checked=0;
    for (int owner=0; owner<2; ++owner) for (unsigned controller=0; controller<controller_count; ++controller)
    for (unsigned war=0; war<3; ++war) for (int bits=0; bits<44; ++bits)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        int position=positions[sample%4];
        setup_water_entry(owner,cases[sample].flags,0,0);
        core_leader(0)=core_leader(1)=0; view_who=0;
        memset(map_who,0,4096); map_who[position]=1;
        set_peep_word(0,8,position); set_peep_word(0,4,cases[sample].initial);
        map_blk[position]=cases[sample].dry ? 1 : 0;
        memcpy(peeps+14,"\x12\x34\x56\x78",4);
        A4_WORD(0xae80)=wars[war];
        /* All low-bit combinations, each unrelated high bit, and all bits. */
        game_mode=bits<32 ? bits : bits==43 ? -1 : 1<<(bits-27);
        A4_WORD(0x82bc+owner*46)=controllers[controller];
        A4_WORD(0x82bc+(1-owner)*46)=controllers[controller_count-1-controller];
        for (int side=0; side<2; ++side) {
            terrain_mailbox_write(side,2,27,31);
            A4_WORD(0x82be + side*46)=sample%2 ? -1 : 0;
            A4_WORD(0x82c4+side*46)=0;
            A4_WORD(0x82c6+side*46)=0;
            A4_LONG(0x9a02+side*16)=0x80000000u;
        }
        game_turn=7; mode=3; core_tool=core_swamp_selected=1;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        if (war) {
            celebration_expected_word(expected,0x99f8,0x820);
            celebration_expected_word(expected,0x9a08,0x820);
            celebration_expected_word(expected,0xae78,0x820);
            celebration_expected_word(expected,0xae76,0x820);
        }
        int old=cases[sample].initial>0 && (cases[sample].flags&16) && !(cases[sample].flags&8);
        int fatal=old && (game_mode&1);
        int admitted=old && !fatal && (war || (controller==1 && !(game_mode&4)));
        /* Clear latches: owner 1's AI runs first and replaces its mailbox. */
        if (!(sample%2)) expect_ai_tendency_pick(expected);
        if (admitted) {
            expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
            expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=position%64;
            expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=position/64;
            celebration_expected_word(expected,0x82be + owner*46,1);
        }
        /* Mixed dry 0x96 retains host recovery, even in fatal mode. */
        int removed=fatal && !(cases[sample].dry && cases[sample].flags==0x96);
        int flags=removed ? 0 : cases[sample].after_flags;
        int population=removed ? 0 : cases[sample].population;
        expected[peeps-prg_a4_area]=flags;
        celebration_expected_word(expected,0x9c26+4,population);
        celebration_expected_word(expected,0x9c26+12,removed ? 0 : cases[sample].frame);
        if (!flags) expected[map_who-prg_a4_area+position]=0;
        if (cases[sample].initial>0) expect_good_pop(expected,owner,cases[sample].initial);
        _move_peeps();
        if (memcmp(expected,prg_a4_area,sizeof expected))
            fprintf(stderr,"water rescue owner=%d controller=%d war=%x mode=%x case=%u\n",
                    owner,controllers[controller],wars[war],(uint16_t)game_mode,sample);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(seed==(sample%2 ? 123 : 15930) && !core_battles && !core_settlements); ++checked;
    }
    printf("water rescue admission full-A4 cases: %u\n",checked);
}

static void test_water_rescue_bounds(void)
{
    const int invalid[][2]={{-1,650},{2,650},{255,650},{INT_MAX,650},{INT_MIN,650},
                           {0,-1},{1,4096},{0,32767},{1,INT_MIN},{0,INT_MAX}};
    for (int war=0; war<2; ++war) for (unsigned sample=0; sample<sizeof invalid/sizeof invalid[0]; ++sample) {
        war_terrain_fixture(); A4_WORD(0xae80)=war;
        A4_WORD(0x82bc)=A4_WORD(0x82bc+46)=1;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        core_queue_terrain_raise(invalid[sample][0],invalid[sample][1]);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    const int positions[]={0,63,4032,4095};
    for (int owner=0; owner<2; ++owner) for (unsigned sample=0; sample<4; ++sample) {
        war_terrain_fixture(); A4_WORD(0xae80)=0; A4_WORD(0x82bc+owner*46)=1;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
        expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=positions[sample]%64;
        expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=positions[sample]/64;
        celebration_expected_word(expected,0x82be + owner*46,1);
        core_queue_terrain_raise(owner,positions[sample]);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void water_rescue_fixture(int owner, int population, int terminal)
{
    war_terrain_fixture(); A4_WORD(0xae80)=0;
    A4_WORD(0x82bc+owner*46)=1; A4_WORD(0x82c6+owner*46)=3;
    A4_LONG(0x848a)=10; A4_LONG(0x9a02+owner*16)=100;
    A4_WORD(0x82be + owner*46)=-1; A4_WORD(0x82c4+owner*46)=0;
    peeps[0]=18; peeps[1]=owner; map_blk[650]=0;
    set_peep_word(0,4,population); set_peep_word(0,12,0);
    no_peeps=terminal ? 2 : 3;
    for (int person=1; person<no_peeps; ++person) {
        peeps[person*22]=8; peeps[person*22+1]=person==1 ? 1-owner : owner;
        set_peep_word(person,4,100); set_peep_word(person,6,person);
        set_peep_word(person,8,900+person); map_who[900+person]=person+1;
    }
}

static void test_water_rescue_deferred(void)
{
    for (int owner=0; owner<2; ++owner) {
        water_rescue_fixture(owner,100,0); peeps[0]=2;
        game_tick();
        assert(peeps[0]==18 && peep_word(0,4)==100 && !map_blk[650]);
        assert(core_vertex(10,10)==0 && core_mana(owner)==100); assert_mailbox_empty();
        game_tick();
        assert(peeps[0]==18 && peep_word(0,4)==98 && map_blk[650]==17);
        assert(core_vertex(10,10)==1 && core_mana(owner)==86); assert_mailbox_empty();
        water_rescue_fixture(owner,100,0);
        /* No injection: paid wet raise, paid dry-entry raise, then no producer.
         * Cadence resets the latch only on turn three; it never gates admission. */
        _move_peeps(); assert_war_command(owner,650);
        assert(peep_word(0,4)==98 && !map_blk[650] && core_mana(owner)==100);
        core_consume_terrain_mailboxes();
        assert(core_vertex(10,10)==1 && map_blk[650]==17 && core_mana(owner)==86);
        assert_mailbox_empty();
        game_tick();
        assert(peeps[0]==2 && peep_word(0,4)==96 && peep_word(0,12)==2);
        assert(core_vertex(10,10)==2 && core_mana(owner)==40 && A4_WORD(0x82be + owner*46)==1);
        assert_mailbox_empty();
        game_tick();
        assert(peep_word(0,12)==3 && peep_word(0,4)==96 && core_mana(owner)==40);
        assert(core_vertex(10,10)==2 && A4_WORD(0x82be + owner*46)==1);
        game_tick();
        assert(peep_word(0,12)==4 && A4_WORD(0x82be + owner*46)==0);
        assert(core_vertex(10,10)==2 && core_mana(owner)==40 && !core_result);
        assert_mailbox_empty();
        /* Lack of funds rejects consumption, not production; retries and latch
         * cadence still occur naturally without editing any intermediate state. */
        water_rescue_fixture(owner,100,0); A4_LONG(0x9a02+owner*16)=9;
        for (int turn=1; turn<=4; ++turn) {
            game_tick();
            assert(peeps[0]==18 && peep_word(0,4)==100-2*turn && !map_blk[650]);
            assert(core_vertex(10,10)==0 && core_mana(owner)==9);
            assert(A4_WORD(0x82be + owner*46)==(turn==3 ? 0 : 1));
            assert_mailbox_empty(); assert(!core_result);
        }
        for (int terminal=0; terminal<2; ++terminal) {
            water_rescue_fixture(owner,1,terminal);
            game_tick();
            assert(!peeps[0] && !peep_word(0,4) && !map_who[650]);
            /* asm:3459-3474: the drowning 1 (plus the non-terminal battler's
             * 100) counts this pass, so the asm:4323-4347 end waits a pass
             * and the mailbox is consumed. */
            assert(!core_result && A4_LONG(0x99fe + owner*16)==(terminal ? 1 : 101));
            assert(core_vertex(10,10)==1 && core_mana(owner)==86);
            assert(core_population(1-owner)==100 && core_population(owner)==(terminal ? 0 : 100));
            assert_mailbox_empty();
            if (terminal) { game_tick(); assert(core_result==(owner ? 1 : 2)); }
        }
        for (int gate=0; gate<5; ++gate) {
            water_rescue_fixture(owner,100,0); terrain_mailbox_write(owner,2,27,31);
            pause=gate==0; prg_quit=gate==1; core_result=gate>=2 ? gate-1 : 0;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
    }
    pause=prg_quit=core_result=0;
}

static void test_water_rescue_order(void)
{
    for (int owner=0; owner<2; ++owner) for (int reverse=0; reverse<2; ++reverse)
    for (int separate=0; separate<2; ++separate) for (int dying=0; dying<2; ++dying) {
        water_rescue_fixture(owner,100,0); no_peeps=2;
        A4_WORD(0x82bc)=A4_WORD(0x82bc+46)=1;
        int positions[]={reverse ? 780 : 650,reverse ? 650 : 780};
        for (int person=0; person<2; ++person) {
            peeps[person*22]=18; peeps[person*22+1]=person && separate ? 1-owner : owner;
            set_peep_word(person,8,positions[person]); set_peep_word(person,4,person && dying ? 1 : 100);
            map_blk[positions[person]]=0; map_who[positions[person]]=person+1;
        }
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        /* war_terrain_fixture leaves both tendencies 0: the other side picks. */
        expect_ai_pick(expected,1-owner,1,15930);
        for (int person=0; person<2; ++person) {
            int side=person && separate ? 1-owner : owner;
            celebration_expected_word(expected,0x9c26+person*22+4,person && dying ? 0 : 98);
            expected[A4_DISP(0x82b6+side*46)-prg_a4_area]=1;
            expected[A4_DISP(0x82b7+side*46)-prg_a4_area]=positions[person]%64;
            expected[A4_DISP(0x82b8+side*46)-prg_a4_area]=positions[person]/64;
            celebration_expected_word(expected,0x82be + side*46,1);
        }
        if (dying) {
            expected[peeps-prg_a4_area+22]=0;
            expected[map_who-prg_a4_area+positions[1]]=0;
        }
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    /* Fatal old-water cannot erase an earlier pending owner command. */
    for (int owner=0; owner<2; ++owner) for (int war=0; war<2; ++war) {
        water_rescue_fixture(owner,1,0); game_mode=1; A4_WORD(0xae80)=war;
        terrain_mailbox_write(owner,1,27,31); A4_WORD(0x82be + owner*46)=7;
        uint8_t expected[sizeof prg_a4_area]; water_loss_expected(expected,owner,650,0);
        expect_ai_pick(expected,1-owner,1,15930);
        expect_pass_good_pop(expected); _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(A4_DISP(0x82b7+owner*46)[0]==27 && A4_WORD(0x82be + owner*46)==7);
    }
}

static void test_water_rescue_town_latch(void)
{
    /* Earlier rescue suppresses the controller-1 capacity override; a later
     * rescue cannot undo an already-created child. Foreign latches are separate.
     * Dry old-water towns must also produce before their OWN capacity check. */
    for (int owner=0; owner<2; ++owner) for (int ordering=0; ordering<3; ++ordering)
    for (int foreign=0; foreign<2; ++foreign) {
        int town=ordering==0 ? 1 : 0, producer=1-town;
        int side=foreign ? 1-owner : owner;
        setup_town_capacity(owner,ordering==2 ? 308 : 306);
        game_mode=0; A4_WORD(0x82be + owner*46)=0;
        A4_WORD(0x82bc+side*46)=1; A4_WORD(0x82be + side*46)=0;
        core_leader(0)=core_leader(1)=view_who=0;
        uint8_t town_record[22]; memcpy(town_record,peeps,22);
        memset(peeps,0,208*22); memcpy(peeps+town*22,town_record,22);
        map_who[650]=town+1; no_peeps=2;
        peeps[producer*22]=18; peeps[producer*22+1]=side;
        set_peep_word(producer,4,100); set_peep_word(producer,8,900);
        map_who[900]=producer+1; map_blk[900]=0;
        if (ordering==2) peeps[town*22]=0x11;
        int split=ordering!=2 && (ordering==1 || foreign);
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x2ff8,9);
        celebration_expected_word(expected,0x2fb2,0);
        celebration_expected_word(expected,0x9c26+town*22+12,42);
        celebration_expected_word(expected,0x9c26+town*22+4,split ? 163 : 317);
        expected[peeps-prg_a4_area+town*22]=1;
        expected[peeps-prg_a4_area+town*22+3]=9;
        town_expected_long(expected,0x9a02+owner*16,owner ? 23473 : 12362);
        celebration_expected_word(expected,0x9c26+producer*22+4,98);
        celebration_expected_word(expected,0x82be + side*46,1);
        expected[A4_DISP(0x82b6+side*46)-prg_a4_area]=1;
        expected[A4_DISP(0x82b7+side*46)-prg_a4_area]=4;
        expected[A4_DISP(0x82b8+side*46)-prg_a4_area]=14;
        if (ordering==2 || split) celebration_expected_word(expected,0x82be + owner*46,1);
        if (ordering==2 && foreign) {
            expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
            expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=10;
            expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=10;
        }
        if (split) {
            uint8_t *child=expected+(peeps-prg_a4_area)+44;
            child[0]=2; child[1]=owner; child[2]=3; child[3]=9;
            celebration_expected_word(expected,0x9c26+44+4,154);
            celebration_expected_word(expected,0x9c26+44+8,650);
            celebration_expected_word(expected,0x9c26+44+12,1);
            celebration_expected_word(expected,0x9c26+44+18,650);
            celebration_expected_word(expected,0xae5e,3);
            expected[peeps-prg_a4_area+town*22+2]=4;
            expected[map_who-prg_a4_area+650]=3;
            /* asm:3461-3463 re-reads _no_peeps: the child is visited too. */
            expect_good_pop_add(expected,owner,154);
        }
        expect_town_bookkeeping(expected,town,42);
        expect_town_publish(expected,owner,0,1);
        /* asm:3766-3797: unless an earlier same-side rescue latched it, the
         * town owner's latch is clear: _make_level's permission-0 exit
         * leaves its _stats address low byte in +20 (asm:9025-9031). */
        if (ordering!=2 && (ordering || foreign)) expect_make_level_exit(expected,town);
        expect_pass_good_pop(expected); _move_peeps();
        if (memcmp(expected,prg_a4_area,sizeof expected))
            fprintf(stderr,"rescue town owner=%d ordering=%d foreign=%d\n",owner,ordering,foreign);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==123);
    }
}

static void test_water_rescue(void)
{
    test_water_rescue_admission(); test_water_rescue_bounds();
    test_water_rescue_deferred(); test_water_rescue_order(); test_water_rescue_town_latch();
}

/* Catch war-only admission, byte-sized controllers, fatal-water leakage,
 * target-cell writes, conditional overwrites and direction-helper movement. */
static void test_magnet_rescue_admission(void)
{
    const int controllers[]={0,1,2,-1,0x0100,0x0101};
    const int wars[]={0,1,0x8000};
    unsigned checked=0;
    for (int owner=0; owner<2; ++owner) for (int controller=0; controller<6; ++controller)
    for (int war=0; war<3; ++war) for (int bits=0; bits<44; ++bits)
    for (int reference=0; reference<9; ++reference) {
        war_terrain_fixture(); peeps[1]=owner; core_target(owner)=715;
        A4_WORD(0xae80)=wars[war]; A4_WORD(0x82bc+owner*46)=controllers[controller];
        A4_WORD(0x82bc+(1-owner)*46)=controllers[5-controller];
        game_mode=bits<32 ? bits : bits==43 ? -1 : 1<<(bits-27);
        if (reference>=1 && reference<=3) {
            no_peeps=2; memcpy(peeps+22,peeps,22);
            peeps[23]=reference==3 ? owner : 1-owner; set_peep_word(1,8,715);
            assert(set_peep_reference(0,reference==1 ? 1 : 0));
        } else if (reference>=4 && reference<=7) peeps[14+reference-4]=0xff;
        else if (reference==8) memset(peeps+14,0xff,4);
        if (reference==3) map_blk[586]=0; /* Self survives an empty enemy scan: north. */
        for (int side=0; side<2; ++side) {
            terrain_mailbox_write(side,255,27,31);
            A4_WORD(0x82be + side*46)=bits%2 ? -1 : 0;
            A4_WORD(0x82c4+side*46)=A4_WORD(0x82c6+side*46)=0;
            A4_LONG(0x9a02+side*16)=0x80000000u;
        }
        game_turn=7; mode=3; core_tool=core_swamp_selected=1; peeps[21]=0x80;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        expected[peeps-prg_a4_area+21]=(uint8_t)(reference==3 ? 65 : -1);
        if (reference==2) { uint32_t heading=2; memcpy(expected+(peeps-prg_a4_area)+14,&heading,4); }
        if (reference<4 && (war || (controller==1 && !(game_mode&4)))) {
            expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
            expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=10;
            expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=10;
            celebration_expected_word(expected,0x82be + owner*46,1);
        }
        assert(core_magnet_direction(0)==999);
        if (memcmp(expected,prg_a4_area,sizeof expected))
            fprintf(stderr,"magnet rescue owner=%d controller=%d war=%x mode=%x reference=%d\n",
                    owner,controllers[controller],wars[war],(uint16_t)game_mode,reference);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(seed==123 && !core_battles && !core_settlements); ++checked;
    }
    printf("magnet rescue admission full-A4 cases: %u\n",checked);
}

static void test_magnet_rescue_navigation(void)
{
    const struct { int position,target,water,opening,history,delta,after; } cases[]={
        {0,1,1,-1,0,999,63},{63,62,62,-1,0,999,-63},
        {4032,4033,4033,-1,0,999,63},{4095,4094,4094,-1,0,999,-63},
        {0,0,-1,-1,0,999,65}, /* Off-map north is not water. */
        {650,715,715,651,0,1,-1},{650,715,715,651,1,999,-1},
        {650,715,715,586,0,-64,64},{650,715,715,586,-64,999,-1}
    };
    for (int owner=0; owner<2; ++owner) for (int war=0; war<2; ++war)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        war_terrain_fixture(); peeps[1]=owner; A4_WORD(0xae80)=war;
        A4_WORD(0x82bc+owner*46)=1; core_target(owner)=cases[sample].target;
        core_leader(owner)=1; set_peep_word(0,8,cases[sample].position);
        memset(map_blk,47,4096); map_blk[cases[sample].position]=15;
        if (cases[sample].water>=0) map_blk[cases[sample].water]=0;
        if (cases[sample].opening>=0) map_blk[cases[sample].opening]=15;
        peeps[21]=(uint8_t)cases[sample].history;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        expected[peeps-prg_a4_area+21]=(uint8_t)cases[sample].after;
        if (cases[sample].water>=0) {
            expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
            expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=cases[sample].position%64;
            expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=cases[sample].position/64;
            celebration_expected_word(expected,0x82be + owner*46,1);
        }
        assert(core_magnet_direction(0)==cases[sample].delta);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    for (int owner=0; owner<2; ++owner) for (int war=0; war<2; ++war) {
        war_terrain_fixture(); peeps[1]=owner; A4_WORD(0xae80)=war;
        A4_WORD(0x82bc+owner*46)=1; core_target(owner)=715; map_blk[715]=47;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        expected[peeps-prg_a4_area+21]=(uint8_t)(war ? 65 : -1);
        assert(core_magnet_direction(0)==(war ? 65 : 999));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
}

static void test_magnet_rescue_mixed(void)
{
    /* Walker and wet caller share one last-writer mailbox, but only old water
     * obeys fatal mode. Initial new-water entry must not produce at all. */
    for (int owner=0; owner<2; ++owner) for (int reverse=0; reverse<2; ++reverse)
    for (int separate=0; separate<2; ++separate) for (int fatal=0; fatal<2; ++fatal)
    for (int old=0; old<2; ++old) for (int dying=0; dying<2; ++dying) {
        war_terrain_fixture(); A4_WORD(0xae80)=0; no_peeps=2; game_mode=fatal;
        core_target(0)=core_target(1)=845;
        A4_WORD(0x82bc)=A4_WORD(0x82bc+46)=1;
        memset(map_who,0,4096);
        for (int person=0; person<2; ++person) {
            int walker=person==reverse, side=separate && !walker ? 1-owner : owner;
            int position=person ? 780 : 650;
            if (person) memcpy(peeps+person*22,peeps,22);
            peeps[person*22]=walker || !old ? 2 : 18; peeps[person*22+1]=side;
            set_peep_word(person,4,dying ? 1 : 100); set_peep_word(person,8,position);
            map_blk[position]=walker ? 15 : 0; map_blk[position+65]=0;
            map_who[position]=person+1;
            terrain_mailbox_write(person,255,27,31); A4_WORD(0x82be + person*46)=-1;
            A4_LONG(0x9a02+person*16)=0;
        }
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        uint32_t sums[2]={0,0};
        for (int person=0; person<2; ++person) {
            int walker=person==reverse, side=separate && !walker ? 1-owner : owner;
            int position=person ? 780 : 650;
            int removed=walker ? dying : old && (fatal || dying);
            sums[side]+=dying ? 1 : 100; /* asm:3459-3474: pre-move word */
            memcpy(A4_DISP(0x99fe)-prg_a4_area+expected,sums,4);
            memcpy(A4_DISP(0x9a0e)-prg_a4_area+expected,sums+1,4);
            expected[peeps-prg_a4_area+person*22]=removed ? 0 : walker ? 0x42 : 18;
            celebration_expected_word(expected,0x9c26+person*22+4,
                                      removed ? 0 : walker ? 99 : old ? 98 : dying ? 1 : 100);
            if (walker) {
                celebration_expected_word(expected,0x9c26+person*22+6,7);
                celebration_expected_word(expected,0x9c26+person*22+12,0);
                expected[peeps-prg_a4_area+person*22+21]=(uint8_t)-1;
                expect_stats_pointer(expected,person);
            }
            if (removed) expected[map_who-prg_a4_area+position]=0;
            if (walker || (old && !fatal)) {
                expected[A4_DISP(0x82b6+side*46)-prg_a4_area]=1;
                expected[A4_DISP(0x82b7+side*46)-prg_a4_area]=position%64;
                expected[A4_DISP(0x82b8+side*46)-prg_a4_area]=position/64;
                celebration_expected_word(expected,0x82be + side*46,1);
            }
        }
        _move_peeps();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(seed==123 && !core_battles && !core_settlements);
    }
}

static void test_magnet_rescue_deferred(void)
{
    for (int owner=0; owner<2; ++owner) {
        water_rescue_fixture(owner,100,0); peeps[0]=2; set_peep_word(0,12,7);
        core_target(owner)=651; map_blk[650]=17; map_blk[651]=0;
        core_set_vertex(10,10,1);
        /* Entire terrain and input are set before the first tick. The paid
         * current-cell raise rebuilds the water to the east; waiting retries it. */
        game_tick();
        assert(peeps[0]==0x42 && peep_word(0,4)==99 && peep_word(0,8)==650);
        assert(core_vertex(10,10)==2 && core_vertex(11,10)==1 && map_blk[651]!=0);
        assert(core_mana(owner)==54 && A4_WORD(0x82be + owner*46)==1); assert_mailbox_empty();
        for (int tick=2; tick<=9; ++tick) {
            game_tick(); assert(peeps[0]==0x42 && peep_word(0,8)==650);
            assert(core_mana(owner)==54 && peep_word(0,4)==100-tick);
            /* Turn-3 cadence clears the latch; tick 4's AI pick (second draw,
             * x 3) latches it until the turn-6 cadence (asm:9612-9633). */
            assert(A4_WORD(0x82be + owner*46)==(tick<3 || tick==4 || tick==5)); assert_mailbox_empty();
            assert(core_tendency(owner)==(tick<4 ? 0 : 3));
        }
        game_tick();
        assert(peeps[0]==2 && peep_word(0,8)==651 && peep_word(0,10)==1);
        assert(peep_word(0,4)==90 && core_mana(owner)==54 && !core_result);
        /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
        assert(map_who[650]==1 && !map_who[651]); assert_mailbox_empty();
        for (int gate=0; gate<5; ++gate) {
            water_rescue_fixture(owner,100,0); peeps[0]=2; map_blk[650]=15;
            set_peep_word(0,12,7); core_target(owner)=715;
            terrain_mailbox_write(owner,2,27,31);
            pause=gate==0; prg_quit=gate==1; core_result=gate>=2 ? gate-1 : 0;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        pause=prg_quit=core_result=0;
    }
}

static void test_magnet_rescue(void)
{
    test_magnet_rescue_admission(); test_magnet_rescue_navigation();
    test_magnet_rescue_mixed(); test_magnet_rescue_deferred();
}

/* Catch wrong-cell writes, water-style war admission, fatal-mode leakage,
 * byte controllers, fallback-only production and premature terrain/payment. */
static void test_swamp_rescue_admission(void)
{
    const int controllers[]={0,1,2,-1,0x0100,0x0101};
    const int wars[]={0,1,0x8000};
    unsigned checked=0;
    for (int owner=0; owner<2; ++owner) for (int controller=0; controller<6; ++controller)
    for (int war=0; war<3; ++war) for (int bits=0; bits<44; ++bits)
    for (int reference=0; reference<9; ++reference) for (int fallback=0; fallback<2; ++fallback) {
        heading_navigation_fixture(owner); no_peeps=2; core_target(owner)=654;
        A4_WORD(0xae80)=wars[war]; A4_WORD(0x82bc+owner*46)=controllers[controller];
        A4_WORD(0x82bc+(1-owner)*46)=controllers[5-controller];
        game_mode=bits<32 ? bits : bits==43 ? -1 : 1<<(bits-27);
        if (!reference) assert(set_peep_reference(0,-1));
        if (reference==2 || reference==3) assert(set_peep_reference(0,0));
        if (reference==3) peeps[23]=owner; /* Empty scan retains self: north. */
        if (reference>=4) {
            memset(peeps+14,0,4);
            if (reference==8) memset(peeps+14,0xff,4);
            else peeps[14+reference-4]=0xff;
        }
        memset(map_blk,47,4096); map_blk[650]=15;
        map_blk[reference==3 ? 586 : 651]=53;
        if (fallback) map_blk[reference==3 ? 585 : 587]=15;
        for (int side=0; side<2; ++side) {
            terrain_mailbox_write(side,255,27,31);
            A4_WORD(0x82be + side*46)=bits%2 ? -1 : 0;
            A4_WORD(0x82c4+side*46)=A4_WORD(0x82c6+side*46)=0;
            A4_LONG(0x9a02+side*16)=0x80000000u;
        }
        game_turn=7; mode=3; core_tool=core_swamp_selected=1; peeps[21]=0x80;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        int delta=reference==0 ? 1 : fallback ? (reference==3 ? -65 : -63) : 999;
        expected[peeps-prg_a4_area+21]=(uint8_t)(reference==0 ? 1 : reference==3 ? 65 : 63);
        if (reference==2) { uint32_t heading=2; memcpy(expected+(peeps-prg_a4_area)+14,&heading,4); }
        if (reference>=1 && reference<=3 && !war && controller==1 && !(game_mode&12)) {
            expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
            expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=reference==3 ? 10 : 11;
            expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=reference==3 ? 9 : 10;
            celebration_expected_word(expected,0x82be + owner*46,1);
        }
        assert(core_magnet_direction(0)==delta);
        if (memcmp(expected,prg_a4_area,sizeof expected))
            fprintf(stderr,"swamp rescue owner=%d controller=%d war=%x mode=%x reference=%d fallback=%d\n",
                    owner,controllers[controller],wars[war],(uint16_t)game_mode,reference,fallback);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(seed==123 && !core_battles && !core_settlements); ++checked;
    }
    printf("swamp rescue admission full-A4 cases: %u\n",checked);
}

static void test_swamp_rescue_navigation(void)
{
    const struct { int position,target,swamp,opening,history,delta,after,x,y; } cases[]={
        {650,654,651,-1,0,999,63,11,10},
        {650,654,651,587,0,-63,63,11,10},
        {650,654,651,587,-63,999,63,11,10},
        {650,654,587,-1,0,999,63,-1,-1}, /* Rock primary; swamp only in fallback. */
        {0,1,1,-1,0,999,63,1,0},{63,62,62,-1,0,999,-63,62,0},
        {4032,4033,4033,-1,0,999,63,1,63},{4095,4094,4094,-1,0,999,-63,62,63},
        {0,0,4032,-1,0,999,65,-1,-1}, /* Same-cell heading tries off-map north. */
        {63,63,4095,-1,0,999,65,-1,-1}
    };
    for (int owner=0; owner<2; ++owner)
    for (unsigned sample=0; sample<sizeof cases/sizeof cases[0]; ++sample) {
        heading_navigation_fixture(owner); no_peeps=2;
        A4_WORD(0x82bc+owner*46)=1;
        set_peep_word(0,8,cases[sample].position); set_peep_word(1,8,cases[sample].target);
        memset(map_blk,47,4096); map_blk[cases[sample].position]=15;
        map_blk[cases[sample].swamp]=53;
        if (cases[sample].opening>=0) map_blk[cases[sample].opening]=15;
        peeps[21]=(uint8_t)cases[sample].history;
        terrain_mailbox_write(owner,2,27,31); A4_WORD(0x82be + owner*46)=-1;
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        expected[peeps-prg_a4_area+21]=(uint8_t)cases[sample].after;
        if (cases[sample].x>=0) {
            expected[A4_DISP(0x82b6+owner*46)-prg_a4_area]=1;
            expected[A4_DISP(0x82b7+owner*46)-prg_a4_area]=cases[sample].x;
            expected[A4_DISP(0x82b8+owner*46)-prg_a4_area]=cases[sample].y;
            celebration_expected_word(expected,0x82be + owner*46,1);
        }
        assert(core_magnet_direction(0)==cases[sample].delta);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    /* Geometric sign navigation cannot generate row wrap; keep its underlying
     * validity guard explicit rather than inventing an unreachable heading. */
    heading_navigation_fixture(0); map_blk[64]=map_blk[63]=53;
    assert(core_valid_move(63,1)==1 && core_valid_move(64,-1)==1);
}

static void swamp_rescue_fixture(int owner, int population, int terminal)
{
    water_rescue_fixture(owner,population,terminal);
    peeps[0]=2; set_peep_word(0,12,7); map_blk[650]=15; map_blk[651]=53;
    assert(set_peep_reference(0,1)); set_peep_word(1,8,654);
    map_who[901]=0; map_who[654]=2;
    core_target(owner)=654;
}

static void test_swamp_rescue_deferred(void)
{
    for (int owner=0; owner<2; ++owner) {
        swamp_rescue_fixture(owner,100,0);
        _move_peeps(); assert_war_command(owner,651);
        assert(peeps[0]==0x42 && peep_word(0,8)==650 && peep_word(0,4)==99);
        assert(map_blk[651]==53 && core_vertex(11,10)==0 && core_mana(owner)==100);
        core_consume_terrain_mailboxes();
        /* _make_map preserves rocks, not swamp: one raised NW corner -> 17. */
        assert(map_blk[651]==17 && map_blk[650]==18 && core_vertex(11,10)==1);
        assert(core_vertex(10,10)==0 && core_vertex(14,10)==0 && core_mana(owner)==86);
        assert_mailbox_empty();

        swamp_rescue_fixture(owner,100,0);
        for (int turn=1; turn<=10; ++turn) {
            game_tick();
            assert(peep_word(0,4)==100-turn && core_mana(owner)==86);
            assert(map_blk[651]==17 && core_vertex(11,10)==1);
            /* The turn-3 cadence clears the latch; turn 4's AI pick (the
             * second draw: 28505, x 3, asm:9612-9633) latches it again until
             * the turn-6 cadence; with tendency 3 the AI then stays idle. */
            assert(A4_WORD(0x82be + owner*46)==(turn<3 || turn==4 || turn==5)); assert_mailbox_empty();
            assert(core_tendency(owner)==(turn<4 ? 0 : 3) && seed==(turn<4 ? 15930 : 28505));
            assert(peeps[0]==(turn<10 ? 0x42 : 2));
            assert(peep_word(0,8)==(turn<10 ? 650 : 651));
        }
        /* _move_explorer asm:4590-4617: the departure cell keeps the claim. */
        assert(peep_word(0,10)==1 && map_who[650]==1 && !map_who[651] && !core_result);
        swamp_rescue_fixture(owner,100,0); A4_LONG(0x9a02+owner*16)=9;
        for (int turn=1; turn<=10; ++turn) {
            game_tick();
            assert(peep_word(0,8)==650 && peeps[0]==0x42 && peep_word(0,4)==100-turn);
            assert(map_blk[651]==53 && core_vertex(11,10)==0 && core_mana(owner)==9);
            assert(A4_WORD(0x82be + owner*46)==(turn<3 || turn==4 || turn==5 || turn==10)); assert_mailbox_empty();
        }
        for (int terminal=0; terminal<2; ++terminal) {
            swamp_rescue_fixture(owner,1,terminal); game_tick();
            assert(!peeps[0] && !peep_word(0,4) && !map_who[650]);
            /* asm:3459-3474: the dying walker's pre-loss 1 still counts in
             * good_pop this pass, so asm:4323-4347 does not end the game and
             * the mailbox is consumed; the terminal end follows next pass. */
            assert(!core_result && map_blk[651]==17);
            assert(core_vertex(11,10)==1 && core_mana(owner)==86);
            assert_mailbox_empty();
            if (terminal) { game_tick(); assert(core_result==(owner ? 1 : 2)); }
        }
        for (int gate=0; gate<5; ++gate) {
            swamp_rescue_fixture(owner,100,0); terrain_mailbox_write(owner,2,27,31);
            pause=gate==0; prg_quit=gate==1; core_result=gate>=2 ? gate-1 : 0;
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        pause=prg_quit=core_result=0;
    }
}

static void test_swamp_rescue_order(void)
{
    /* Slot order, not producer type, owns the last write. Foreign owners retain
     * separate mailboxes. Old-water fatal exclusion must not infect swamp. */
    for (int owner=0; owner<2; ++owner) for (int reverse=0; reverse<2; ++reverse)
    for (int separate=0; separate<2; ++separate) for (int fatal=0; fatal<2; ++fatal) {
        swamp_rescue_fixture(owner,100,0); game_mode=fatal;
        A4_WORD(0x82bc)=A4_WORD(0x82bc+46)=1;
        uint8_t walker[22]; memcpy(walker,peeps,22);
        memset(peeps,0,66); no_peeps=3;
        int wet=1-reverse, wet_owner=separate ? 1-owner : owner;
        memcpy(peeps+reverse*22,walker,22); assert(set_peep_reference(reverse,2));
        peeps[wet*22]=18; peeps[wet*22+1]=wet_owner;
        set_peep_word(wet,4,100); set_peep_word(wet,8,780);
        peeps[44]=8; peeps[45]=1-owner;
        set_peep_word(2,4,100); set_peep_word(2,6,2); set_peep_word(2,8,654);
        memset(map_who,0,4096); map_who[650]=reverse+1; map_who[780]=wet+1; map_who[654]=3;
        map_blk[780]=0;
        _move_peeps();
        assert(peep_word(reverse,8)==650 && peeps[reverse*22]==0x42);
        assert(peep_word(reverse,4)==99 && peep_word(wet,4)==(fatal ? 0 : 98));
        assert_war_command(owner,!fatal && !separate && !reverse ? 780 : 651);
        if (separate && !fatal) assert_war_command(wet_owner,780);
        else { /* only the other owner is unlatched: its pick, first draw */
            assert(A4_DISP(0x82b6+(1-owner)*46)[0]==14 && A4_DISP(0x82b7+(1-owner)*46)[0]==1);
            assert(A4_DISP(0x82b8+(1-owner)*46)[0]==1 && A4_WORD(0x82be + (1-owner)*46)==1);
        }
        assert(map_blk[651]==53 && core_vertex(11,10)==0 && core_mana(owner)==100);
    }
    /* The same owner latch suppresses the later town capacity override. A
     * foreign rescue must not suppress that split; an earlier town still splits. */
    for (int owner=0; owner<2; ++owner) for (int foreign=0; foreign<2; ++foreign)
    for (int town_first=0; town_first<2; ++town_first) {
        setup_town_capacity(owner,306); game_mode=0; no_peeps=3;
        int side=foreign ? 1-owner : owner;
        A4_WORD(0x82bc)=A4_WORD(0x82bc+46)=1;
        A4_WORD(0x82be)=A4_WORD(0x82be + 46)=0;
        peeps[22]=2; peeps[23]=side;
        set_peep_word(1,4,100); set_peep_word(1,8,900); assert(set_peep_reference(1,2));
        peeps[44]=8; peeps[45]=1-side;
        set_peep_word(2,4,100); set_peep_word(2,8,904);
        memset(map_blk+835,47,132); map_blk[900]=15; map_blk[901]=53;
        if (town_first) core_settle_tick(0);
        assert(core_magnet_direction(1)==999);
        if (!town_first) core_settle_tick(0);
        assert_war_command(side,901);
        assert(peep_word(0,4)==(town_first || foreign ? 163 : 317));
        assert(no_peeps==(town_first || foreign ? 4 : 3));
    }
}

static void test_swamp_rescue(void)
{
    test_swamp_rescue_admission(); test_swamp_rescue_navigation(); test_swamp_rescue_deferred();
    test_swamp_rescue_order();
}

/* Computer-player AI (asm:3254-3270, 9017-9900). Fixture: an empty peep
 * list so _move_peeps runs only the AI calls and per-pass bookkeeping; the
 * player's latch is held so only owner 1 (_stats at $82e4) acts. */
static void ai_fixture(void)
{
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    no_peeps=0; seed=123; game_turn=50; player=0;
    A4_WORD(0x82be)=1;
    A4_LONG(0x848e)=50;  A4_LONG(0x8492)=100; A4_LONG(0x8496)=200; A4_LONG(0x849a)=300;
    A4_LONG(0x849e)=400; A4_LONG(0x84a2)=500; A4_LONG(0x84a6)=600;
    core_tendency(1)=1; core_target(0)=core_target(1)=0x820;
}
/* _move_peeps bookkeeping on an empty pass: asm:3290-3335 clears the
 * pointers, town counts and good_pop, asm:4349-4370 publishes zero counts. */
static void ai_expect_pass(uint8_t *expected)
{
    uint8_t *saved=prg_a4; prg_a4=expected+32768;
    for (int owner=0;owner<2;++owner) {
        A4_WORD(0x99fc+owner*16)=0; A4_LONG(0x99fe + owner*16)=0; /* asm:3296-3301 */
        A4_LONG(0x82d8+owner*46)=A4_LONG(0x82dc+owner*46)=A4_LONG(0x82e0+owner*46)=0;
        A4_WORD(0x82ca+owner*46)=A4_WORD(0x82cc+owner*46)=0;
    }
    prg_a4=saved;
}
#define AI_EXPECT(offset) (expected+32768+(int16_t)(offset))
static void ai_expect_word(uint8_t *expected, int offset, int16_t value) { memcpy(AI_EXPECT(offset),&value,2); }
static void ai_expect_long(uint8_t *expected, int offset, uint32_t value) { memcpy(AI_EXPECT(offset),&value,4); }
static void ai_expect_mail(uint8_t *expected, int command, int x, int y, int latch)
{
    AI_EXPECT(0x82e4)[0]=command; AI_EXPECT(0x82e5)[0]=x; AI_EXPECT(0x82e6)[0]=y;
    if (latch) ai_expect_word(expected,0x82ec,1);
}
static void ai_peep(int slot, int flags, int owner, int population, int cell)
{
    peeps[slot*22]=flags; peeps[slot*22+1]=owner;
    set_peep_word(slot,4,population); set_peep_word(slot,8,cell);
}
static void ai_run(uint8_t *expected)
{
    _move_peeps(); assert(!memcmp(expected,prg_a4_area,65536));
}
static void test_set_devil_magnet(void)
{
    static uint8_t expected[65536];
    /* asm:9612-9633: 0 towns < 0*2+15 with tendency 0: one newrand,
     * 15930%3+1 = 1 (seed 123), y=1, latch. devil_effect then sees the latch. */
    ai_fixture(); core_tendency(1)=0;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_mail(expected,14,1,1,1); ai_expect_word(expected,0x99e2,15930);
    ai_run(expected);
    /* Same branch, tendency set: no command, no draw (and devil_effect idle). */
    ai_fixture(); memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected); ai_run(expected);
    /* asm:9620-9627: 15 towns, level 0, turn 50: 50%90 >= 10 -> leader path.
     * Unsigned BCC: turn 9 (9 < 10) stays on the tendency path. */
    ai_fixture(); A4_WORD(0x82fa)=15; A4_WORD(0x8300)=5; game_turn=9;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected); ai_run(expected);
    /* asm:9686-9703: no own leader, tendency 1 -> 14/0/1, +$1c cleared. */
    ai_fixture(); A4_WORD(0x82fa)=10; A4_WORD(0x82f8)=5; A4_WORD(0x8300)=5;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_mail(expected,14,0,1,1); ai_expect_word(expected,0x8300,0);
    ai_expect_word(expected,0x82f8,0); ai_expect_word(expected,0x82fa,0);
    ai_run(expected);
    /* asm:9704-9776: leader below 6000, +$1c clear: magnet to own youngest
     * town (slot 2, cell 1234 = 18,19), remember it, +$1c = 2. */
    ai_fixture(); A4_WORD(0x82fa)=15; core_leader(1)=1;
    ai_peep(0,2,1,100,600); ai_peep(2,1,1,50,1234); A4_LONG(0x830e)=3;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_mail(expected,5,18,19,1); ai_expect_word(expected,0x8302,1234);
    ai_expect_word(expected,0x8300,2); ai_expect_word(expected,0x82fa,0);
    ai_run(expected);
    /* asm:9704-9717,9726-9740: leader stands on +$1e: count 2 -> 1, target
     * already 1234 -> tendency 1 resets to 0. */
    ai_fixture(); A4_WORD(0x82fa)=15; core_leader(1)=1; core_target(1)=1234;
    ai_peep(0,2,1,100,1234); A4_WORD(0x8302)=1234; A4_WORD(0x8300)=2;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_mail(expected,14,0,1,1); ai_expect_word(expected,0x8300,1);
    ai_expect_word(expected,0x82fa,0);
    ai_run(expected);
    /* Count still set and target elsewhere: re-magnet to +$1e (asm:9718-9737). */
    ai_fixture(); A4_WORD(0x82fa)=15; core_leader(1)=1;
    ai_peep(0,2,1,100,600); A4_WORD(0x8302)=2000; A4_WORD(0x8300)=2;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_mail(expected,5,16,31,1); ai_expect_word(expected,0x82fa,0);
    ai_run(expected);
    /* asm:9784-9864: leader 6000 > enemy 5499+500, enemy tendency 0, bit 2:
     * tendency 1 -> 14/0/1; with tendency 0 -> magnet to enemy target 2000. */
    for (int tendency=1; tendency>=0; --tendency) {
        ai_fixture(); A4_WORD(0x82fa)=15; core_leader(1)=1; core_leader(0)=2;
        core_tendency(1)=tendency; core_tendency(0)=0; core_target(0)=2000; A4_WORD(0x82f2)=4;
        ai_peep(0,2,1,6000,600); ai_peep(1,2,0,5499,700);
        memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
        if (tendency) ai_expect_mail(expected,14,0,1,1); else ai_expect_mail(expected,5,16,31,1);
        ai_expect_word(expected,0x82fa,0);
        ai_run(expected);
    }
    /* Enemy 5500: 6000 is not > 6000, so asm:9865-9898 bit 1 targets the
     * enemy's best town (_stats+$22 = slot 3, cell 300 = 44,4). */
    ai_fixture(); A4_WORD(0x82fa)=15; core_leader(1)=1; core_leader(0)=2;
    core_tendency(0)=0; A4_WORD(0x82f2)=6;
    ai_peep(0,2,1,6000,600); ai_peep(1,2,0,5500,700); ai_peep(3,1,0,90,300); A4_LONG(0x8306)=4;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_mail(expected,5,44,4,1); ai_expect_word(expected,0x8302,300);
    ai_expect_word(expected,0x8300,2); ai_expect_word(expected,0x82fa,0);
    ai_run(expected);
    puts("set_devil_magnet: PASS");
}
static void test_devil_effect(void)
{
    static uint8_t expected[65536];
    /* asm:9317-9349: war needs mana > 600+999 and good_pop 5 > 4. */
    for (int mana=1599; mana<=1600; ++mana) {
        ai_fixture(); A4_WORD(0x82f2)=0x100; A4_LONG(0x9a12)=mana;
        A4_LONG(0x9a0e)=5; A4_LONG(0x99fe)=4;
        memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
        if (mana==1600) ai_expect_mail(expected,14,0,3,1);
        ai_run(expected);
    }
    /* asm:9350-9365: flood above 500+1999. */
    ai_fixture(); A4_WORD(0x82f2)=0x180; A4_LONG(0x9a12)=2500;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_mail(expected,14,0,4,1); ai_run(expected);
    /* asm:9366-9396: knight for a 3001 leader above 300+500; at mana 800 the
     * branch still returns before the volcano would fire. */
    for (int mana=800; mana<=801; ++mana) {
        ai_fixture(); A4_WORD(0x82f2)=0x60; A4_LONG(0x9a12)=mana; core_leader(1)=1; A4_LONG(0x849e)=0;
        ai_peep(0,2,1,3001,600); A4_LONG(0x830a)=1;
        memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
        if (mana==801) ai_expect_mail(expected,14,0,5,1);
        ai_run(expected);
    }
    /* asm:9398-9419 + _do_computer_effect fallback: +$26 slot 2 at 1234
     * -> (15,16); counter cleared. A cell at (1,2) clamps x to 0. */
    for (int cell=1234; cell>=0; cell-=1234-129) {
        ai_fixture(); A4_WORD(0x82f2)=0x40; A4_LONG(0x9a12)=901; A4_WORD(0x82f6)=7;
        ai_peep(2,1,0,50,cell); A4_LONG(0x830a)=3;
        memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
        if (cell==1234) ai_expect_mail(expected,6,15,16,1); else ai_expect_mail(expected,6,0,0,1);
        ai_expect_word(expected,0x82f6,0);
        ai_run(expected);
        if (cell!=1234) break;
    }
    /* _do_computer_effect asm:9526-9567: TST.W _magnet; BNE LAB_450B8. Only
     * WITHOUT an own leader and with an enemy on the own magnet cell 2000 is
     * (16,31) aimed at; a leader or a friendly occupant falls back to (15,16). */
    for (int leader=0; leader<2; ++leader) for (int foe=1; foe>=0; --foe) {
        ai_fixture(); A4_WORD(0x82f2)=0x40; A4_LONG(0x9a12)=901; core_leader(1)=leader;
        core_target(1)=2000; ai_peep(0,2,1,100,600); ai_peep(2,1,0,50,1234); A4_LONG(0x830a)=3;
        ai_peep(4,2,foe ? 0 : 1,10,2000); map_who[2000]=5;
        memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
        if (foe && !leader) ai_expect_mail(expected,6,16,31,1); else ai_expect_mail(expected,6,15,16,1);
        ai_run(expected);
    }
    /* asm:9420-9482: swamp on the enemy leader (slot 1 at 700 = 60,10),
     * counter 0 -> 1, and no latch. The own leader must be weaker. */
    for (int ours=0; ours<3; ++ours) {
        ai_fixture(); A4_WORD(0x82f2)=0x10; A4_LONG(0x9a12)=701; core_leader(0)=2;
        ai_peep(1,2,0,50,700); A4_LONG(0x830a)=2;
        if (ours) { core_leader(1)=1; ai_peep(0,2,1,ours==1 ? 49 : 50,600); }
        memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
        if (ours<2) { ai_expect_mail(expected,4,60,10,0); ai_expect_word(expected,0x82f6,1); }
        ai_run(expected);
    }
    /* asm:9483-9524: quake on a town target with counter 0 < limit 1; the
     * swamp branch defers to it (count < limit with bit 3). Fallback aim. */
    ai_fixture(); A4_WORD(0x82f2)=0x18; A4_LONG(0x9a12)=701; A4_WORD(0x82fe)=1; core_leader(0)=2;
    ai_peep(1,2,0,50,700); ai_peep(2,1,0,50,1234); A4_LONG(0x830a)=3;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_mail(expected,3,15,16,1); ai_expect_word(expected,0x82f6,1);
    ai_run(expected);
    /* Counter at the limit with bit 4 set: no quake (asm:9505-9509). A walker
     * target (flags 2) is not quaked either. */
    for (int variant=0; variant<2; ++variant) {
        ai_fixture(); A4_WORD(0x82f2)=variant ? 0x08 : 0x18; A4_LONG(0x9a12)=601;
        A4_WORD(0x82f6)=variant ? 0 : 1; A4_WORD(0x82fe)=1;
        ai_peep(2,variant ? 2 : 1,0,50,1234); A4_LONG(0x830a)=3;
        memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected); ai_run(expected);
    }
    puts("devil_effect: PASS");
}
static void test_computer_effect(void)
{
    /* _do_computer_effect writes only x/y; the command byte is kept. */
    ai_fixture(); A4_LONG(0x830a)=3; ai_peep(2,1,0,50,130); A4_DISP(0x82e4)[0]=3;
    core_do_computer_effect(1);
    assert(A4_DISP(0x82e4)[0]==3 && A4_DISP(0x82e5)[0]==0 && A4_DISP(0x82e6)[0]==0);
    ai_fixture(); A4_LONG(0x830a)=3; ai_peep(2,1,0,50,4095); A4_DISP(0x82e4)[0]=6;
    core_do_computer_effect(1);
    assert(A4_DISP(0x82e4)[0]==6 && A4_DISP(0x82e5)[0]==60 && A4_DISP(0x82e6)[0]==60);
    puts("computer_effect: PASS");
}
/* Every _devil_effect / _set_devil_magnet choice tied to the _stats+$e bit
 * the asm BTSTs before writing it (asm:9317-9524, 9784-9898). */
static int ai_power_bit(const uint8_t *mail)
{
    if (mail[0]==14) return mail[2]==3 ? 0x100 : mail[2]==4 ? 0x80 : mail[2]==5 ? 0x20 : mail[2]==1 ? -1 : 0;
    return mail[0]==6 ? 0x40 : mail[0]==4 ? 0x10 : mail[0]==3 ? 0x08 : 0;
}
/* Every branch eligible: rich, more populous, a 3001 own leader under a
 * stronger enemy leader, an old town (flags 1) as the +$26 subject. */
static void ai_power_fixture(uint16_t powers)
{
    ai_fixture(); A4_WORD(0x82f2)=(int16_t)powers; A4_LONG(0x9a12)=100000;
    A4_LONG(0x9a0e)=5; A4_LONG(0x99fe)=4;
    core_leader(1)=1; core_leader(0)=2; no_peeps=3;
    ai_peep(0,2,1,3001,600); ai_peep(1,2,0,5000,700); ai_peep(2,1,0,50,1234);
    A4_LONG(0x830a)=3; A4_WORD(0x82fe)=1; A4_WORD(0x82fc)=3;
}
static void test_ai_level_parameters(void)
{
    /* 1. Allowed powers (_stats+$e): over all 512 masks the computer only
     * ever queues a power whose bit is set; each power is reached. */
    int seen=0;
    for (unsigned powers=0; powers<0x200; ++powers) {
        ai_power_fixture((uint16_t)powers); core_devil_effect(1);
        int bit=ai_power_bit(A4_DISP(0x82e4));
        assert(bit>=0 && (!bit || (powers&(unsigned)bit)));
        seen|=bit;
        /* Without the knight's early return the order continues. */
        ai_power_fixture((uint16_t)(powers&~0x20u)); peeps[0*22]=0; core_leader(1)=0;
        core_devil_effect(1); bit=ai_power_bit(A4_DISP(0x82e4));
        assert(bit>=0 && (!bit || (powers&(unsigned)bit)));
        seen|=bit;
    }
    assert(seen==(0x100|0x80|0x40|0x20|0x10|0x08));
    /* Consumers re-check the bit (asm:7863ff,8046ff,8295ff,7567ff,8392ff,
     * 8482ff): a forged disallowed mailbox costs nothing and changes nothing. */
    static const uint8_t forged[6][3]={{3,10,10},{4,10,10},{6,10,10},{14,0,3},{14,0,4},{14,0,5}};
    for (int f=0; f<6; ++f) {
        ai_power_fixture(0x7); int bit=ai_power_bit(forged[f]);
        A4_WORD(0x82f2)=(int16_t)(0x1ff&~bit);
        memcpy(A4_DISP(0x82e4),forged[f],3);
        int16_t heights=A4_WORD(0xae86+(10*65+10)*2); uint16_t war=A4_WORD(0xae80);
        core_consume_terrain_mailboxes();
        assert(A4_LONG(0x9a12)==100000 && core_leader(1)==1 && A4_WORD(0xae80)==war &&
               A4_WORD(0xae86+(10*65+10)*2)==heights);
    }
    /* Level records install rating/speed/powers exactly as asm:406-427. */
    LevelStart start={{7,4,0x15,0x3,0,0,1,1,0},0,123};
    memset(prg_a4_area,0,sizeof prg_a4_area); start_state();
    original_generate(&start);
    assert(A4_WORD(0x82f0)==7 && A4_WORD(0x82f4)==4 &&
           A4_WORD(0x82f2)==((0x15<<3)|7) && A4_WORD(0x82c4)==((0x3<<3)|7));

    /* 2. Reaction speed (_stats+$10): _get_message asm:17932-17950 clears the
     * latch of a controller-1 side only when game_turn % speed == 0, so the
     * computer re-decides exactly once per `speed` turns. */
    for (int speed=1; speed<=6; ++speed) {
        uint16_t trace[2][60]={{0}}; int decisions[2]={0,0};
        for (int run=0; run<2; ++run) {
            ai_fixture(); A4_WORD(0x82ea)=1; A4_WORD(0x82f4)=(int16_t)speed; game_turn=0;
            for (int turn=0; turn<60; ++turn) {
                core_tendency(1)=0;                     /* keep the tendency path live */
                int latched_before=A4_WORD(0x82ec)!=0;
                ++game_turn; _move_peeps();             /* game_tick order: ADDQ game_turn (asm:3248) first */
                int decided=!latched_before && A4_WORD(0x82ec);
                assert(decided==(A4_DISP(0x82e4)[0]==14));
                decisions[run]+=decided;
                trace[run][turn]=decided ? (uint16_t)(seed ^ A4_DISP(0x82e5)[0]<<8) : 0;
                core_consume_terrain_mailboxes();
                assert(!A4_WORD(0x82ec)==((uint16_t)game_turn%speed==0));
            }
        }
        /* First pass decides (latch clear), then every turn after a multiple. */
        int expected=1;
        for (int t=1; t<60; ++t) if (t%speed==0) ++expected;
        assert(decisions[0]==expected);
        /* 4. Deterministic: same seed, same decisions and same RNG trace. */
        assert(decisions[0]==decisions[1] && !memcmp(trace[0],trace[1],sizeof trace[0]));
    }
    /* A human side (controller 0) never has its latch cleared by cadence. */
    ai_fixture(); A4_WORD(0x82ea)=0; A4_WORD(0x82ec)=1; A4_WORD(0x82f4)=1;
    for (int turn=0; turn<10; ++turn) { ++game_turn; core_consume_terrain_mailboxes(); assert(A4_WORD(0x82ec)==1); }

    /* 3. Rating (_stats+$c): asm:9612-9627 keeps the computer passive
     * (tendency path) while towns < 2*rating+15 or game_turn%90 < rating+10;
     * otherwise its leader goes after the youngest own town (+$2a). */
    for (int rating=0; rating<=10; ++rating) for (int dt=-1; dt<=0; ++dt) for (int dturn=-1; dturn<=0; ++dturn) {
        ai_fixture(); core_tendency(1)=0; A4_WORD(0x82f0)=(int16_t)rating;
        A4_WORD(0x82f8)=0; A4_WORD(0x82fa)=(int16_t)(2*rating+15+dt);
        game_turn=(int16_t)(90*3+rating+10+dturn);
        core_leader(1)=1; ai_peep(0,2,1,100,600); ai_peep(3,1,1,90,300); A4_LONG(0x830e)=4;
        uint16_t before=seed; core_set_devil_magnet(1);
        const uint8_t *mail=A4_DISP(0x82e4);
        if (dt<0 || dturn<0) {
            assert(mail[0]==14 && mail[2]==1 && seed!=before);   /* passive, one newrand */
        } else {
            assert(mail[0]==5 && mail[1]==44 && mail[2]==4 && seed==before);
            assert(A4_WORD(0x8302)==300 && A4_WORD(0x8300)==2);
        }
        assert(A4_WORD(0x82ec)==1);
    }
    /* Same world and turn: rating 0 attacks, rating 10 stays passive. */
    for (int rating=0; rating<=10; rating+=10) {
        ai_fixture(); core_tendency(1)=0; A4_WORD(0x82f0)=(int16_t)rating; A4_WORD(0x82fa)=20; game_turn=15;
        core_leader(1)=1; ai_peep(0,2,1,100,600); ai_peep(3,1,1,90,300); A4_LONG(0x830e)=4;
        core_set_devil_magnet(1);
        assert(A4_DISP(0x82e4)[0]==(rating ? 14 : 5));
    }
    puts("ai_level_parameters: PASS");
}
static void test_make_level(void)
{
    static uint8_t expected[65536];
    /* asm:9017-9031: permission bit 0 clear -> D0 is the _stats address. */
    ai_fixture(); memcpy(expected,prg_a4_area,65536);
    assert(core_make_level(650,1)==MAKE_LEVEL_EXIT_BYTE_OWNER1 && core_make_level(650,0)==MAKE_LEVEL_EXIT_BYTE_OWNER0);
    assert(MAKE_LEVEL_EXIT_BYTE_OWNER0==0xa4 && MAKE_LEVEL_EXIT_BYTE_OWNER1==0xd2);
    assert(!memcmp(expected,prg_a4_area,65536));
    ai_fixture(); A4_WORD(0x82f2)=1; game_mode=4; assert(core_make_level(650,1)==MAKE_LEVEL_EXIT_BYTE_OWNER1);
    /* Flat 9x9 of height 2: level. */
    ai_fixture(); A4_WORD(0x82f2)=1;
    for (int v=0; v<65*65; ++v) A4_WORD(0xae86+v*2)=2;
    memcpy(expected,prg_a4_area,65536);
    assert(core_make_level(650,1)==1 && !memcmp(expected,prg_a4_area,65536));
    /* First a_flat entry (-4,-4) from (10,10): vertex (6,6) lower -> raise. */
    A4_WORD(0xae86+(6*65+6)*2)=1; memcpy(expected,prg_a4_area,65536);
    ai_expect_mail(expected,1,6,6,1);
    assert(core_make_level(650,1)==0 && !memcmp(expected,prg_a4_area,65536));
    /* Higher vertex (14,6) is 3rd group first entry (4,-4): lower. */
    ai_fixture(); A4_WORD(0x82f2)=1;
    for (int v=0; v<65*65; ++v) A4_WORD(0xae86+v*2)=2;
    A4_WORD(0xae86+(6*65+14)*2)=3; memcpy(expected,prg_a4_area,65536);
    ai_expect_mail(expected,2,14,6,1);
    assert(core_make_level(650,1)==0 && !memcmp(expected,prg_a4_area,65536));
    game_mode=8; A4_WORD(0x82ec)=0; memcpy(expected,prg_a4_area,65536);
    assert(core_make_level(650,1)==1 && !memcmp(expected,prg_a4_area,65536));
    /* Rock 0x2f on a level cell: increment and lower (asm:9070-9090);
     * swamp 0x35 on a level cell: lower (asm:9113-9131). */
    for (int block=0x2f; block<=0x35; block+=6) {
        ai_fixture(); A4_WORD(0x82f2)=1;
        for (int v=0; v<65*65; ++v) A4_WORD(0xae86+v*2)=2;
        map_blk[6*64+6]=block; memcpy(expected,prg_a4_area,65536);
        if (block==0x2f) AI_EXPECT(0xcf88)[6*64+6]=0x30;
        ai_expect_mail(expected,2,6,6,1);
        assert(core_make_level(650,1)==0 && !memcmp(expected,prg_a4_area,65536));
    }
    /* Edge clip from (0,0): groups dx=-4/-3 and dy<0 are skipped; the first
     * visited entry is (4,0), lower than the town's height 3 -> raise it. */
    ai_fixture(); A4_WORD(0x82f2)=1;
    for (int v=0; v<65*65; ++v) A4_WORD(0xae86+v*2)=2;
    A4_WORD(0xae86+(0*65+0)*2)=3; A4_WORD(0xae86+(4*65+4)*2)=1;
    memcpy(expected,prg_a4_area,65536); ai_expect_mail(expected,1,4,0,1);
    assert(core_make_level(0,1)==0 && !memcmp(expected,prg_a4_area,65536));
    puts("make_level: PASS");
}
/* _get_message asm:18200-18216 consumers for the AI's opcodes. */
static void ai_consume_fixture(int opcode, int x, int y)
{
    ai_fixture(); A4_WORD(0x82be + 46)=1; game_turn=1;
    A4_DISP(0x82e4)[0]=opcode; A4_DISP(0x82e5)[0]=x; A4_DISP(0x82e6)[0]=y;
}
static void ai_expect_consumed(uint8_t *expected)
{
    memset(AI_EXPECT(0x82e4),0,3);
}
static void test_ai_opcode_magnet(void)
{
    static uint8_t expected[65536];
    /* _do_magnet asm:6769-6820: pay 50 of 120, sound 77, clamp 70 -> 63. */
    ai_consume_fixture(5,70,31); core_leader(1)=1; A4_LONG(0x9a12)=120;
    memcpy(expected,prg_a4_area,65536); ai_expect_consumed(expected);
    ai_expect_long(expected,0x9a12,70); ai_expect_word(expected,0x2fac,77);
    ai_expect_word(expected,0x9a08,31*64+63); ai_expect_word(expected,0xae76,31*64+63);
    core_consume_terrain_mailboxes(); assert(!memcmp(expected,prg_a4_area,65536));
    /* Below cost, no leader, or war: only the mailbox clears. */
    for (int gate=0; gate<3; ++gate) {
        ai_consume_fixture(5,10,10); core_leader(1)=gate!=1; A4_LONG(0x9a12)=gate ? 50 : 49;
        A4_WORD(0xae80)=gate==2;
        memcpy(expected,prg_a4_area,65536); ai_expect_consumed(expected);
        core_consume_terrain_mailboxes(); assert(!memcmp(expected,prg_a4_area,65536));
    }
    /* _set_magnet_to asm:8271-8293: paused pays but keeps the target. */
    ai_consume_fixture(5,10,10); core_leader(1)=1; A4_LONG(0x9a12)=60; pause=1;
    memcpy(expected,prg_a4_area,65536); ai_expect_consumed(expected);
    ai_expect_long(expected,0x9a12,10); ai_expect_word(expected,0x2fac,77);
    core_consume_terrain_mailboxes(); assert(!memcmp(expected,prg_a4_area,65536)); pause=0;
    puts("opcode 5 magnet: PASS");
}
static void test_ai_opcode_action(void)
{
    static uint8_t expected[65536];
    /* _do_action y=1 asm:18410-18431: tendency = x. */
    ai_consume_fixture(14,3,1);
    memcpy(expected,prg_a4_area,65536); ai_expect_consumed(expected);
    ai_expect_word(expected,0x9a0a,3);
    core_consume_terrain_mailboxes(); assert(!memcmp(expected,prg_a4_area,65536));
    /* y=3 -> _do_war asm:8482-8530: pay 600, sound 73, war on. */
    ai_consume_fixture(14,0,3); A4_WORD(0x82f2)=0x100; A4_LONG(0x9a12)=1600;
    memcpy(expected,prg_a4_area,65536); ai_expect_consumed(expected);
    ai_expect_long(expected,0x9a12,1000); ai_expect_word(expected,0x2fac,73);
    ai_expect_word(expected,0xae80,1);
    core_consume_terrain_mailboxes(); assert(!memcmp(expected,prg_a4_area,65536));
    /* y=5 -> _do_knight asm:8392-8478 on leader slot 0 at 600. */
    ai_consume_fixture(14,0,5); A4_WORD(0x82f2)=0x20; A4_LONG(0x9a12)=301; core_leader(1)=1;
    no_peeps=1; ai_peep(0,2,1,100,600);
    core_consume_terrain_mailboxes();
    assert(core_mana(1)==1 && !core_leader(1) && peeps[20]==1 && A4_WORD(0x2fac)==69);
    assert(core_target(1)==600 && A4_WORD(0xae76)==600);
    /* y=4 -> _do_flood asm:7567-7648: pay 500, every positive vertex -1. */
    ai_consume_fixture(14,0,4); A4_WORD(0x82f2)=0x80; A4_LONG(0x9a12)=500;
    A4_WORD(0xae86+100*2)=3;
    core_consume_terrain_mailboxes();
    assert(core_mana(1)==0 && A4_WORD(0xae86+100*2)==2 && A4_WORD(0x2fac)==74);
    /* y=0/2/6+ are not AI actions: only the mailbox clears. */
    ai_consume_fixture(14,0,0); memcpy(expected,prg_a4_area,65536); ai_expect_consumed(expected);
    core_consume_terrain_mailboxes(); assert(!memcmp(expected,prg_a4_area,65536));
    puts("opcode 14 action: PASS");
}
static void test_ai_opcode_effects(void)
{
    /* Opcodes 3/4/6 reach _do_quake/_do_swamp/_do_volcano with x, y, owner. */
    ai_consume_fixture(3,10,12); A4_WORD(0x82f2)=8; A4_LONG(0x9a12)=150;
    core_consume_terrain_mailboxes();
    assert(core_mana(1)==50 && A4_WORD(0x2fac)==76 && A4_WORD(0x2f8e)>=1 && A4_DISP(0x82e4)[0]==0);
    ai_consume_fixture(4,10,12); A4_WORD(0x82f2)=0x10; A4_LONG(0x9a12)=250;
    uint16_t before=seed; core_consume_terrain_mailboxes();
    assert(core_mana(1)==50 && seed!=before && A4_DISP(0x82e4)[0]==0);
    ai_consume_fixture(6,10,12); A4_WORD(0x82f2)=0x40; A4_LONG(0x9a12)=450;
    core_consume_terrain_mailboxes();
    assert(core_mana(1)==50 && A4_WORD(0x2fac)==75 && A4_DISP(0x82e4)[0]==0);
    /* Gates fail without the permission bit: nothing paid. */
    for (int opcode=3; opcode<=6; opcode+=opcode==4 ? 2 : 1) {
        ai_consume_fixture(opcode,10,12); A4_LONG(0x9a12)=1000; core_leader(1)=opcode!=5;
        core_consume_terrain_mailboxes(); assert(core_mana(1)==1000);
    }
    puts("opcodes 3/4/6 effects: PASS");
}
/* good_pop: signed long per owner at A4+$99fe, stride 16 (bad_pop $9a0e). */
static void gp_expect_peep_word(uint8_t *expected, int slot, int offset, int16_t value)
{ ai_expect_word(expected,0x9c26+slot*22+offset,value); }
static void gp_expect_peep_byte(uint8_t *expected, int slot, int offset, uint8_t value)
{ AI_EXPECT(0x9c26+slot*22+offset)[0]=value; }
static void test_good_pop(void)
{
    static uint8_t expected[65536];
    /* asm:3296-3301 reset, asm:3459-3474 accumulate EXT.L of the +4 word for
     * every peep with a positive word, owner byte *16, before any water,
     * battle or movement work. All-zero map_blk: every cell is water. */
    ai_fixture(); game_mode=GAME_MODE_WATER_FATAL;
    A4_LONG(0x99fe)=0x12345678; A4_LONG(0x9a0e)=0xfedcba98;
    ai_peep(0,0x12,0,300,100); map_who[100]=1;                 /* walker, drowns */
    ai_peep(1,1,1,700,200); set_peep_word(1,12,9);             /* town -> wet 18 */
    ai_peep(2,8,0,40,300); set_peep_word(2,6,1);               /* battle, lower partner */
    ai_peep(3,2,1,0,350);                                      /* zero: skipped */
    ai_peep(4,2,1,-5,360);                                     /* signed negative: skipped */
    ai_peep(5,0x12,1,32000,400); map_who[400]=6;              /* drowns */
    no_peeps=6;
    memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
    ai_expect_long(expected,0x99fe,300+40);                    /* not post-move 40 */
    ai_expect_long(expected,0x9a0e,700+32000);                 /* not post-move 700 */
    gp_expect_peep_word(expected,0,4,0); gp_expect_peep_byte(expected,0,0,0); AI_EXPECT(0x1f88+100)[0]=0;
    gp_expect_peep_byte(expected,1,0,18); gp_expect_peep_word(expected,1,12,0);
    gp_expect_peep_word(expected,5,4,0); gp_expect_peep_byte(expected,5,0,0); AI_EXPECT(0x1f88+400)[0]=0;
    ai_run(expected);
    /* asm:9317-9349 war boundary: _devil_effect runs at the top of the next
     * pass (asm:3254-3270) on the previous pass's pre-move sums. Both owners
     * drown in pass 1, so post-move totals would be 0/0 and never war. */
    for (int friendly=4; friendly<=5; ++friendly) {
        ai_fixture(); game_mode=GAME_MODE_WATER_FATAL;
        A4_WORD(0x82f2)=0x100; A4_LONG(0x9a12)=1600;
        ai_peep(0,0x12,1,5,500); ai_peep(1,0x12,0,friendly,501); no_peeps=2;
        memcpy(expected,prg_a4_area,65536); ai_expect_pass(expected);
        ai_expect_long(expected,0x9a0e,5); ai_expect_long(expected,0x99fe,friendly);
        gp_expect_peep_word(expected,0,4,0); gp_expect_peep_byte(expected,0,0,0);
        gp_expect_peep_word(expected,1,4,0); gp_expect_peep_byte(expected,1,0,0);
        ai_run(expected);
        ai_expect_long(expected,0x9a0e,0); ai_expect_long(expected,0x99fe,0);
        if (friendly==4) ai_expect_mail(expected,14,0,3,1);    /* 5 > 4 */
        ai_run(expected);                                      /* 5 > 5 fails */
    }
    /* asm:5605-5620 _join_forces: after the merge, a target index above the
     * source (not yet visited) loses the source's word from the source
     * owner's sum, even when the target word clamped at $7d00. */
    for (int order=0; order<2; ++order) {
        int source=order ? 3 : 1, target=order ? 1 : 3;
        ai_fixture();
        ai_peep(source,2,1,30000,700); map_who[700]=source+1;
        ai_peep(target,2,1,5000,701); map_who[701]=target+1; set_peep_word(target,12,4);
        no_peeps=4; A4_LONG(0x9a0e)=35000; A4_LONG(0x99fe)=77;
        memcpy(expected,prg_a4_area,65536);
        gp_expect_peep_word(expected,target,4,32000); gp_expect_peep_word(expected,target,12,0);
        gp_expect_peep_word(expected,source,4,0); gp_expect_peep_byte(expected,source,0,0);
        AI_EXPECT(0x1f88+700)[0]=0;
        if (target>source) ai_expect_long(expected,0x9a0e,5000);
        core_join_forces(source,target);
        assert(!memcmp(expected,prg_a4_area,65536));
    }
    /* asm:6725-6740 _join_battle: the comparison uses the redirected same-owner
     * recipient (+6 partner, asm:6678-6687) against the newcomer index. */
    for (int order=0; order<2; ++order) {
        int newcomer=order ? 5 : 2, encountered=order ? 2 : 1, partner=order ? 1 : 5;
        ai_fixture();
        ai_peep(newcomer,2,0,60,800); map_who[800]=newcomer+1;
        ai_peep(encountered,8,1,90,801); set_peep_word(encountered,6,partner);
        ai_peep(partner,8,0,110,802); set_peep_word(partner,6,encountered);
        no_peeps=6; A4_LONG(0x99fe)=1000; A4_LONG(0x9a0e)=2000;
        memcpy(expected,prg_a4_area,65536);
        gp_expect_peep_word(expected,partner,4,170);
        gp_expect_peep_word(expected,newcomer,4,0); gp_expect_peep_byte(expected,newcomer,0,0);
        AI_EXPECT(0x1f88+800)[0]=0;
        if (partner>newcomer) ai_expect_long(expected,0x99fe,940);
        core_join_battle(newcomer,encountered);
        assert(!memcmp(expected,prg_a4_area,65536));
    }
    /* asm:4323-4347: after the pass, _player's zero sum or surrender ends
     * lost (___end_game(1)); else _not_player's zero sum or surrender won.
     * Pass 1 counts the drowning enemy's 5, so the game ends one pass later. */
    for (int scenario=0; scenario<4; ++scenario) {
        ai_fixture(); game_mode=GAME_MODE_WATER_FATAL; core_result=0; pause=0; prg_quit=0;
        player=0; not_player=1; A4_WORD(0x8258)=-1;
        /* +6 = 0 battle peeps never reach _do_battle (other > self fails). */
        ai_peep(0,0x12,1,5,500); ai_peep(1,scenario==1 ? 0x12 : 8,0,9,901); no_peeps=2;
        if (scenario==2) A4_WORD(0x8258)=0;
        if (scenario==3) { A4_WORD(0x8258)=1; ai_peep(0,8,1,5,902); }
        game_tick();
        if (scenario==2) { assert(core_result==2 && game_turn==51); continue; }
        if (scenario==3) { assert(core_result==1 && game_turn==51); continue; }
        assert(!core_result && A4_LONG(0x9a0e)==5);
        game_tick();
        /* Both drowned before the first check: lost, not a draw. */
        assert(core_result==(scenario==1 ? 2 : 1) && game_turn==52);
    }
    core_result=0; core_clear_terrain_mailboxes();
    puts("good_pop: PASS");
}
static void test_computer_ai(void)
{
    test_set_devil_magnet(); test_devil_effect(); test_good_pop(); test_computer_effect(); test_ai_level_parameters(); test_make_level();
    test_ai_opcode_magnet(); test_ai_opcode_action(); test_ai_opcode_effects();
}

static void test_start_reset(void)
{
    /* _setup_display -> _clear_map (asm:404/458,1023-1188) writes, per owner,
     * _magnet 0x99f6=0 (asm:1081), 0x99f8=0x820 (asm:1074), tendency
     * 0x99fa=1 (asm:1064), mana 0x9a02=399 (asm:1069), _stats+0x12/+0x18/
     * +0x1a/+0x1c (asm:1084-1132) and, via _clear_send (asm:1135,18234-18254),
     * the mailbox bytes _stats+0..2. It never writes 0x99fc, good_pop/bad_pop
     * (0x99fe/0x9a0e), _stats+0x14/+0x16 or the +0x22/+0x26/+0x2a pointers:
     * those are zeroed only by _move_peeps (asm:3280-3320), after the
     * _devil_effect call (asm:3268), so the first pass reads the old game's. */
    static uint8_t clean[sizeof prg_a4_area], expected[sizeof prg_a4_area];
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    assert(game_reset(0));
    memcpy(clean, prg_a4_area, sizeof clean);
    assert(A4_WORD(0x99fc) == 0 && A4_LONG(0x99fe) == 0 && A4_WORD(0x9a0c) == 0 && A4_LONG(0x9a0e) == 0);
    assert(A4_WORD(0x99fa) == 1 && A4_WORD(0x9a0a) == 1 && A4_WORD(0x99f8) == 0x820 && A4_WORD(0x9a08) == 0x820);
    assert(A4_LONG(0x9a02) == 399 && A4_LONG(0x9a12) == 399);
    for (int pass = 0; pass < 2; ++pass) {
        memset(prg_a4_area, 0xa5, sizeof prg_a4_area);
        A4_WORD(0x99fc) = 0x1111; A4_LONG(0x99fe) = 0x00012345;
        A4_WORD(0x9a0c) = 0x2222; A4_LONG(0x9a0e) = 0xfffffff6;
        A4_WORD(0x82ca) = 0x0303; A4_WORD(0x82cc) = 0x0404;
        A4_WORD(0x82ca+46) = 0x0505; A4_WORD(0x82cc+46) = 0x0606;
        A4_LONG(0x82d8) = 0x00000007; A4_LONG(0x82dc) = 0x00000008; A4_LONG(0x82e0) = 0x00000009;
        A4_LONG(0x82d8+46) = 0x0000000a; A4_LONG(0x82dc+46) = 0x0000000b; A4_LONG(0x82e0+46) = 0x0000000c;
        memcpy(expected, clean, sizeof expected);
        uint8_t *live = prg_a4; prg_a4 = expected + 32768;
        A4_WORD(0x99fc) = 0x1111; A4_LONG(0x99fe) = 0x00012345;
        A4_WORD(0x9a0c) = 0x2222; A4_LONG(0x9a0e) = 0xfffffff6;
        A4_WORD(0x82ca) = 0x0303; A4_WORD(0x82cc) = 0x0404;
        A4_WORD(0x82ca+46) = 0x0505; A4_WORD(0x82cc+46) = 0x0606;
        A4_LONG(0x82d8) = 0x00000007; A4_LONG(0x82dc) = 0x00000008; A4_LONG(0x82e0) = 0x00000009;
        A4_LONG(0x82d8+46) = 0x0000000a; A4_LONG(0x82dc+46) = 0x0000000b; A4_LONG(0x82e0+46) = 0x0000000c;
        /* Sound statics (_music_now.._sound_in, _seqlen, _no_sounds.._Soundpri,
         * _music_off/_effect_off) are loaded once by _start_game (asm:13966-13972)
         * and never written by a start. */
        memset(A4_DISP(0x3152), 0xa5, 6); memset(A4_DISP(0x3378), 0xa5, 2);
        memset(A4_DISP(0x3542), 0xa5, 0x14); memset(A4_DISP(0x8250), 0xa5, 4);
        prg_a4 = live;
        if (!pass) assert(game_reset(0));
        else { SDL_Event reset = {.type = SDL_KEYDOWN}; reset.key.keysym.sym = SDLK_r; game_handle_event(&reset); }
        for (size_t index = 0; index < sizeof expected; ++index)
            if (prg_a4_area[index] != expected[index]) {
                fprintf(stderr, "start reset: A4%+d = %02x, expected %02x\n",
                        (int)index - 32768, prg_a4_area[index], expected[index]);
                assert(0);
            }
    }
    puts("start reset: PASS");
}

static void no_quake_delay(int ms) { (void)ms; }

/* _end_game rows and score (asm:14019-14316). */
static void end_stats_fixture(void)
{
    memset(prg_a4_area, 0, sizeof prg_a4_area);
    player = 0; not_player = 1;
    A4_WORD(0x82c4) = 0x1ff;       /* every bonus bit owned: no +1000 */
}
static void test_end_stats(void)
{
    end_stats_fixture();
    assert(core_end_stats(0) == 500 && A4_LONG(0xae72) == 500);   /* floor */
    for (int r = 0; r < 7; ++r) {
        assert(A4_WORD(0x8e7e + r*46) == core_end_template[r].x && A4_WORD(0x8e80 + r*46) == core_end_template[r].y);
    }
    assert(!strcmp(core_end_row(0), "GAME LOST"));
    assert(!strcmp(core_end_row(1), "                   YOU  HIM"));
    assert(!strcmp(core_end_row(2), "BATTLES WON        0    0"));
    assert(!strcmp(core_end_row(5), "NUMBER OF CASTLES  0    0"));
    assert(!strcmp(core_end_row(6), "YOUR SCORE         500"));
    end_stats_fixture(); A4_LONG(0xae72) = 501; assert(core_end_stats(0) == 501);
    end_stats_fixture(); A4_LONG(0xae72) = 0x80000000u; assert(core_end_stats(0) == 500); /* signed BGE */
    end_stats_fixture(); A4_LONG(0xae72) = 1000; assert(core_end_stats(1) == 10000);
    assert(!strcmp(core_end_row(0), "GAME WON") && !strcmp(core_end_row(6), "YOUR SCORE         10000"));
    end_stats_fixture(); A4_LONG(0xae72) = 555555; assert(core_end_stats(0) == 555555);
    end_stats_fixture(); A4_LONG(0xae72) = 555556; assert(core_end_stats(0) == 515090);
    end_stats_fixture(); A4_LONG(0xae72) = 55556; assert(core_end_stats(1) == 515090);
    for (int bit = 8; bit <= 256; bit <<= 1) {
        end_stats_fixture(); A4_LONG(0xae72) = 10000;
        A4_WORD(0x82c4) = (int16_t)(0x1ff & ~bit); assert(core_end_stats(0) == 11000);
        end_stats_fixture(); A4_LONG(0xae72) = 10000;
        A4_WORD(0x82c4 + 46) = (int16_t)bit; assert(core_end_stats(0) == 11000);
    }
    end_stats_fixture(); A4_WORD(0x82c4) = 7; A4_WORD(0x82c4 + 46) = 0x1ff; /* bits 1..4 ignored */
    assert(core_end_stats(0) == 12000);
    end_stats_fixture(); A4_LONG(0xae72) = 1000; A4_WORD(0xae7a) = 3; A4_WORD(0xae7c) = 2;
    assert(core_end_stats(0) == 6000 && !strcmp(core_end_row(2), "BATTLES WON        3    2"));
    end_stats_fixture(); A4_LONG(0xae72) = 1000; A4_WORD(0xae7a) = 2; A4_WORD(0xae7c) = 2;
    assert(core_end_stats(0) == 1000);
    end_stats_fixture(); A4_LONG(0xae72) = 1000; A4_WORD(0xae7a) = -1; A4_WORD(0xae7c) = 0;
    assert(core_end_stats(0) == 1000 && !strcmp(core_end_row(2), "BATTLES WON        -1   0"));
    end_stats_fixture(); A4_WORD(0xae7a) = 12345; A4_WORD(0xae7c) = 7;
    assert(core_end_stats(1) == 50000 && !strcmp(core_end_row(2), "BATTLES WON        123457"));
    /* asm:14239-14247: (10 - computer speed) * 15 when _serial_off. */
    end_stats_fixture(); A4_LONG(0xae72) = 1000; A4_WORD(0x82c6 + 46) = 3;
    assert(core_end_stats(0) == 1000); serial_off = 1; A4_LONG(0xae72) = 1000;
    assert(core_end_stats(0) == 1105);
    /* Swapped sides read the other words. */
    end_stats_fixture(); player = 1; not_player = 0; A4_WORD(0x82c4) = 0; A4_WORD(0x82c4 + 46) = 0x1ff;
    A4_WORD(0xae7c) = 4; A4_LONG(0xae72) = 1000;
    assert(core_end_stats(0) == 6000 && !strcmp(core_end_row(2), "BATTLES WON        4    0"));
    /* Knights: heading (+0xe) and population; towns/castles: flag 1, frame 0x2a. */
    end_stats_fixture();
    struct { int flag, owner, pop, frame, heading; } fixture[] = {
        {2,0,5,0,1},{2,1,5,0,1},{2,1,5,0,1},{2,0,0,0,1},{8,0,-3,0,9},
        {1,0,10,0x29,0},{1,0,10,0x29,0},{1,1,10,0x29,0},{1,0,0,0x29,0},{3,0,10,0x29,0},
        {1,0,10,0x2a,0},{1,1,10,0x2a,0},{1,1,10,0x2a,0},{1,1,10,0x2a,0},{1,2,1,0x2a,0}};
    no_peeps = sizeof fixture / sizeof fixture[0];
    for (int p = 0; p < no_peeps; ++p) {
        peeps[p*22] = (uint8_t)fixture[p].flag; peeps[p*22+1] = (uint8_t)fixture[p].owner;
        set_peep_word(p,4,(int16_t)fixture[p].pop); set_peep_word(p,12,(int16_t)fixture[p].frame);
        uint32_t heading = (uint32_t)fixture[p].heading; memcpy(peeps+p*22+14,&heading,4);
    }
    core_end_stats(0);
    assert(!strcmp(core_end_row(3), "NUMBER OF KNIGHTS  2    2"));
    assert(!strcmp(core_end_row(4), "NUMBER OF TOWNS    2    1"));
    assert(!strcmp(core_end_row(5), "NUMBER OF CASTLES  1    4"));
    no_peeps = 3; core_end_stats(0); assert(!strcmp(core_end_row(3), "NUMBER OF KNIGHTS  1    2"));
    no_peeps = -1; core_end_stats(0); assert(!strcmp(core_end_row(3), "NUMBER OF KNIGHTS  2    2")); /* clamped */
}

/* _code/_decode (asm:14793-15118). */
static void test_level_codes(void)
{
    char name[16];
    seed = 1234;
    for (int n = 0; n <= 2470; n += 5) {
        core_code(name, core_code_rand(n));
        assert(strlen(name) <= 12 && core_decode(name) == n);
    }
    assert(seed == 1234);
    core_code(name, core_code_rand(0)); assert(!strcmp(name, "SHISODING"));
    core_code(name, core_code_rand(2470)); assert(!strcmp(name, "WEAVUSPERT"));   /* asm:14488 */
    assert(core_decode("GENESIS") == 0);
    assert(core_decode("") == -1 && core_decode("XRING") == -1);
    assert(core_decode("RINGX") == -2 && core_decode("RING") == -2);
    assert(core_decode("RINGOUTX") == -3 && core_decode("RINGOUT") == -3);
    assert(core_decode("RINGOUTHILLS") == -4);
    static uint8_t produced[32768];
    for (int n = 0; n <= 5000; n += 5) produced[core_code_rand(n)] = 1;
    int unused = 0;
    while (produced[unused]) ++unused;
    core_code(name, (uint16_t)unused);
    if (core_decode(name) != -5) {   /* a prefix may decode as another code */
        for (int code = unused; code < 32768; ++code) {
            core_code(name, (uint16_t)code);
            int result = core_decode(name);
            if (result == -5) break;
            assert(result >= -5);
        }
    }
    assert(core_decode(name) == -5);
    StartSelection selection;
    char *arguments[] = {"populous", "--code", "shisoding", NULL};
    assert(parse_start(3, arguments, &selection) && selection.kind == START_ORIGINAL && selection.number == 0);
    arguments[2] = "GENESIS"; assert(parse_start(3, arguments, &selection) && selection.number == 0);
    arguments[2] = "WEAVUSPERT"; assert(parse_start(3, arguments, &selection) && selection.number == 494);
    core_code(name, core_code_rand(25)); arguments[2] = name;
    assert(parse_start(3, arguments, &selection) && selection.number == 5);
    int beyond = -1;
    for (int n = 2475; n <= 5000 && beyond < 0; n += 5) {
        core_code(name, core_code_rand(n)); if (core_decode(name) == n) beyond = n;
    }
    assert(beyond > 0 && !parse_start(3, arguments, &selection));
    const char *bad[] = {"", "RINGOUT", "XYZ", "SHISODINGSHISODING"};
    for (int index = 0; index < 4; ++index) { arguments[2] = (char *)bad[index]; assert(!parse_start(3, arguments, &selection)); }
    assert(!parse_start(2, arguments, &selection));
}

/* _won_conquest asm:14466-14491. */
static void test_next_level(void)
{
    const struct { int level; int32_t score; int next, finished; } cases[] = {
        {0,500,5,0},{0,4999,5,0},{0,5000,5,0},{0,24999,5,0},{0,25000,10,0},{0,29999,10,0},
        {5,500,10,0},{10,515090,115,0},{2460,500,2465,0},{2465,500,2470,0},{2465,515090,2470,0},
        {2469,0,2470,0},{2470,500,0,1},{2470,515090,0,1},{1000,50000,1015,0}};
    for (size_t index = 0; index < sizeof cases / sizeof cases[0]; ++index) {
        int finished = -1;
        assert(core_next_level(cases[index].level, cases[index].score, &finished) == cases[index].next);
        assert(finished == cases[index].finished);
    }
    char lines[2][48];
    core_conquest_text(0, 5, 0, lines);
    char next[16]; core_code(next, core_code_rand(5));
    char expected[48]; snprintf(expected, sizeof expected, "GENESIS NOW BATTLE AT %s", next);
    assert(!strcmp(lines[0], "WELL DONE MORTAL YOU CONQUERED ") && !strcmp(lines[1], expected));
    core_conquest_text(2465, 2470, 0, lines);
    assert(!strcmp(lines[0], "WELL DONE ETERNAL GOD YOU CONQUERED "));
    assert(strstr(lines[1], " NOW BATTLE AT WEAVUSPERT"));
    core_conquest_text(2470, 0, 1, lines);
    assert(!strcmp(lines[0], "WELL DONE YOU HAVE CONQUERED EVIL") &&
           !strcmp(lines[1], "THE BATTLE IS OVER BUT TRY SHISODING"));
}

static const uint8_t asm_glyph_A[40] = {   /* _font_data 'A', asm:21949+ */
    0xff,0x00,0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x00,0xc3,0x1c,0x20,0x00,0x3c,0x99,0x26,0x40,0x00,0x66,
    0x81,0x3e,0x40,0x00,0x7e,0x99,0x62,0x04,0x00,0x66,0x99,0x66,0x00,0x00,0x66,0xff,0x00,0x00,0x00,0x00};

static void write_font_fixture(void)
{
    uint8_t font[4400];
    for (int glyph = 0; glyph < 110; ++glyph) {
        memcpy(font + glyph*40, asm_glyph_A, 40);
        if (!glyph) for (int row = 0; row < 8; ++row) { font[row*5] = 0xff; memset(font+row*5+1, 0, 4); }
    }
    char path[512]; snprintf(path, sizeof path, "%s/font.dat", test_data_directory);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(font, 1, sizeof font, file) == sizeof font && !fclose(file));
}

static void test_amiga_text(void)
{
    uint8_t saved[sizeof game_font]; memcpy(saved, game_font, sizeof saved);
    int loaded = game_font_loaded;
    for (int glyph = 0; glyph < 110; ++glyph) memcpy(game_font + glyph*40, asm_glyph_A, 40);
    game_font_loaded = 1;
    uint8_t pixels[sizeof game_pixels], before[sizeof game_pixels];
    for (size_t index = 0; index < sizeof pixels; ++index) pixels[index] = (uint8_t)(index * 7 % 16);
    memcpy(before, pixels, sizeof before);
    amiga_text(pixels, 13, 5, "A\nA");    /* x 13 -> byte 1 */
    for (int y = 0; y < 200; ++y) for (int x = 0; x < 320; ++x) {
        int expected = before[y*320+x];
        int glyph_row = y >= 5 && y < 13 ? y-5 : y >= 13 && y < 21 ? y-13 : -1;
        if (glyph_row >= 0 && x >= 8 && x < 16) {
            const uint8_t *g = asm_glyph_A + glyph_row*5; int b = 15 - x;
            expected = 0;
            for (int plane = 0; plane < 4; ++plane)
                expected |= ((((before[y*320+x] >> plane) & (g[0] >> b)) | (g[1+plane] >> b)) & 1) << plane;
        }
        assert(pixels[y*320+x] == expected);
    }
    /* Glyph row 2 {0xc3,0x1c,0x20,0x00,0x3c}: x 10 -> planes 1+3, x 11 -> 0+3, x 8 kept. */
    assert(pixels[7*320+10] == 10 && pixels[7*320+11] == 9 && pixels[7*320+8] == before[7*320+8]);
    /* Linear plane memory: column 40 wraps to the next line, past 8000 bytes is dropped. */
    memcpy(pixels, before, sizeof pixels);
    amiga_text(pixels, 39*8, 0, "AA");
    assert(pixels[2*320+315] == 9 && pixels[3*320+3] == 9 && pixels[2*320+3] == before[2*320+3]);
    memcpy(pixels, before, sizeof pixels);
    amiga_text(pixels, 0, 196, "A");
    assert(!memcmp(pixels+200*320, before+200*320, sizeof pixels - 200*320));
    game_font_loaded = 0; memcpy(pixels, before, sizeof pixels);
    amiga_text(pixels, 0, 0, "A"); assert(!memcmp(pixels, before, sizeof pixels));
    memcpy(game_font, saved, sizeof saved); game_font_loaded = loaded;
}

/* _pointer (asm:615-620, 2042, 2110-2131), _a_sprite frames (asm:16783-16830),
 * top-left hotspot (asm:19928-19972), HUD font and _move_mana (asm:3164-3244). */
static void test_pointer_hud_mana(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    assert(player==0 && pointer==1);
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_m;
    game_handle_event(&key); assert(core_tool==1 && pointer==2);
    player=1; game_handle_event(&key); assert(core_tool==0 && pointer==4);
    game_handle_event(&key); assert(pointer==3);
    player=0; game_handle_event(&key); assert(pointer==1);
    core_tool=2; core_set_pointer(); assert(pointer==0);
    core_tool=0; pointer=7; mousex=-100; game_render(); assert(pointer==1); /* asm:615-620 */
    A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)|=0x10; key.key.keysym.sym=SDLK_s;
    game_handle_event(&key); assert(core_swamp_selected && pointer==5);
    core_swamp_selected=0; core_set_pointer(); assert(pointer==1);
    /* HUD: original font glyphs (status and key lines). */
    game_render();
    assert(hud_text_at(0,0,"PLAY BLUE") && hud_text_at(3,0,"0 GATHER 1 SETTLE 2 JOIN 3 FIGHT R RESET"));
    assert(hud_text_at(6,0,"A ARMAGEDDON 80000 SPC PAUSE ESC QUIT"));
    /* Pointer: non-zero plane values of frame 0x4e at (mousex,mousey), mask ignored. */
    memset(game_assets->sprite_indices,0,sizeof game_assets->sprite_indices);
    memset(game_assets->sprite_keep,0,sizeof game_assets->sprite_keep);
    game_assets->sprite_indices[0x4e*256]=7; game_assets->sprite_indices[0x4e*256+15*16+15]=3;
    game_assets->sprite_indices[0x53*256+16+1]=11;
    uint32_t *off=malloc(320*256*sizeof *off); assert(off);
    mousex=-100; game_render(); memcpy(off,prg_screen->chunky_buffer,320*256*sizeof *off);
    for (int sample=0; sample<2; ++sample) {
        mousex=100; mousey=sample ? 236 : 150; game_render();
        int changed=0;
        for (int pixel=0; pixel<320*256; ++pixel) changed+=prg_screen->chunky_buffer[pixel]!=off[pixel];
        assert(prg_screen->chunky_buffer[mousey*320+100]==prg_screen->palette[7]);
        assert(prg_screen->chunky_buffer[(mousey+15)*320+115]==prg_screen->palette[3]);
        assert(changed<=2);
    }
    key.key.keysym.sym=SDLK_s; game_handle_event(&key); assert(pointer==5);
    game_render(); assert(prg_screen->chunky_buffer[(mousey+1)*320+101]==prg_screen->palette[11]);
    core_swamp_selected=0; core_set_pointer();
    mousex=-100; free(off);
    /* Mana marker: sprite 69 on the scale per _mana_values thresholds. */
    game_assets->sprite_indices[69*256]=9; game_assets->sprite_keep[69*256]=1;
    memset(game_assets->background,0,sizeof game_assets->background);
    static const struct { int32_t mana; int x, y; } marks[] = {
        {0,174,15},{10,176,16},{11,176,16},{2500,208,32},{7000,236,46},{160000,304,80},{160001,311,87},{5000000,311,87}};
    for (size_t sample=0; sample<sizeof marks/sizeof *marks; ++sample) {
        A4_LONG(0x9a02)=(uint32_t)marks[sample].mana; game_render();
        assert(game_pixels[marks[sample].y*320+marks[sample].x]==9);
        for (size_t other=0; other<sizeof marks/sizeof *marks; ++other)
            if (marks[other].x!=marks[sample].x) assert(game_pixels[marks[other].y*320+marks[other].x]!=9);
    }
    _closeall(); SDL_Quit();
}

/* Pixel (x,y) of requester icon 12..23 (_icon_data, plane words per row). */
static int icon_pixel(int icon, int x, int y)
{
    int colour = 0;
    for (int plane = 0; plane < 4; ++plane) colour |= ((core_requester_icons[icon-12][y*4+plane] >> (15-x)) & 1) << plane;
    return colour;
}
/* The whole 16x16 tile at (x,y) is icon. */
static void assert_tile(int x, int y, int icon)
{
    for (int row = 0; row < 16; ++row) for (int column = 0; column < 16; ++column)
        assert(game_pixels[(y+row)*320+x+column] == icon_pixel(icon, column, row));
}

/* Left press at screen pixel (x,y) in a 960x768 window. */
static void screen_click(int x, int y, int button)
{
    SDL_SetWindowSize(prg_screen->window,960,768);
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN};
    click.button.button=(uint8_t)button; click.button.x=x*3+1; click.button.y=y*3+1;
    game_handle_event(&click);
}
/* The _end_game button: click its centre (core_end_button_layout). */
static void end_button_click(void)
{
    core_end_button_layout(0);
    screen_click((core_rec_w(core_end_button,0)+core_rec_w(core_end_button,4))/2, core_rec_w(core_end_button,2)+8, SDL_BUTTON_LEFT);
}

static void end_kill(int owner)
{
    for (int p = 0; p < no_peeps; ++p) if (peep_owner(p) == owner) set_peep_word(p,4,0);
    for (int tick = 0; tick < 4 && !core_result; ++tick) game_tick();
    assert(!core_end_shown);
    if (core_result && !core_end_shown) core_enter_end();   /* as _animate */
    assert(core_result == (owner == player ? 2 : 1) && core_end_shown);
}

/* State outside the requester tables (_end_ok/_end_cancel, _option_text,
 * _game_text, _set_text) and _tempo matches before. */
static int a4_same_but_tables(const uint8_t *before)
{
    static uint8_t copy[sizeof prg_a4_area]; memcpy(copy,before,sizeof copy);
    static const int ranges[][2]={{0x84c6,2*46},{0x8522,14*46},{0x87ee,17*46},{0x8fc0,11*46},{0x824e,2}};
    for (int k=0;k<5;++k) {
        size_t at=(size_t)(A4_DISP(ranges[k][0])-prg_a4_area);
        memcpy(copy+at,prg_a4_area+at,(size_t)ranges[k][1]);
    }
    return a4_same(copy);
}
/* Clicks move _mousex/_mousey; take them over into before. */
static void keep_mouse(uint8_t *before)
{
    memcpy(before+(A4_DISP(0x99aa)-prg_a4_area),A4_DISP(0x99aa),2);
    memcpy(before+(A4_DISP(0xae6e)-prg_a4_area),A4_DISP(0xae6e),2);
}
static void entry_click(const uint8_t *entry, int dx, int dy)
{
    screen_click(core_rec_w(entry,0)+dx+1, core_rec_w(entry,2)+dy+1, SDL_BUTTON_LEFT);
}
/* _do_message's OK (_end_ok of _requester 0x28,0x3c,0x100,0x30). */
static void disk_ok_click(void) { screen_click(core_rec_w(core_end_ok,0)+4, core_rec_w(core_end_ok,2)+8, SDL_BUTTON_LEFT); }
static void world_ok_click(void) { screen_click(core_rec_w(core_world_ok,0)+4, core_rec_w(core_world_ok,2)+8, SDL_BUTTON_LEFT); }
static void world_new_click(void) { screen_click(core_rec_w(core_world_cancel,0)+4, core_rec_w(core_world_cancel,2)+8, SDL_BUTTON_LEFT); }
static void world_type(const char *text)
{
    for (; *text; ++text) {
        SDL_Event key = {.type = SDL_KEYDOWN};
        key.key.keysym.sym = *text == '\n' ? SDLK_RETURN : *text == '\b' ? SDLK_BACKSPACE : (SDL_Keycode)*text;
        game_handle_event(&key);
    }
}
static void ok_click(void) { screen_click(core_rec_w(core_end_ok,0)+4, core_rec_w(core_end_ok,2)+8, SDL_BUTTON_LEFT); }
static void cancel_click(void) { screen_click(core_rec_w(core_end_cancel,0)+4, core_rec_w(core_end_cancel,2)+8, SDL_BUTTON_LEFT); }

/* SAVE A GAME / LOAD A GAME: _save_game/_checksum/_load_game file format
 * (asm:17123-17703) and the _save_load requester (asm:10493-10972). */
static char save_directory[] = "/tmp/populous-save-XXXXXX";
static void save_file_path(char *path, size_t size, const char *name)
{
    snprintf(path, size, "%s/%s", save_directory, name);
}
static size_t save_read(const char *name, uint8_t *bytes, size_t size)
{
    char path[512]; save_file_path(path, sizeof path, name);
    FILE *in = fopen(path, "rb"); assert(in);
    size_t count = fread(bytes, 1, size, in); fclose(in);
    return count;
}
static void save_write(const char *name, const uint8_t *bytes, size_t size)
{
    char path[512]; save_file_path(path, sizeof path, name);
    FILE *out = fopen(path, "wb"); assert(out && fwrite(bytes, 1, size, out) == size && !fclose(out));
}
static void save_remove(const char *name)
{
    char path[512]; save_file_path(path, sizeof path, name); assert(!unlink(path));
}
static uint16_t be16_at(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be32_at(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void save_fixture(void)
{
    toolbar_fixture();
    for (int k = 0; k < 4; ++k) game_tick();
    A4_WORD(0xae86) = 0x1234; A4_WORD(0xae86 + 2*4224) = -2;   /* first and last _alt word */
    ((uint8_t *)map_alt)[0] = 0xab; ((uint8_t *)map_blk)[1] = 0xcd;
    ((uint8_t *)map_bk2)[4095] = 0xef; ((uint8_t *)map_who)[2] = 0x5a;
    core_good_pop(0) = 0x01020304; core_good_pop(1) = 0x00011000;
    A4_LONG(0x9a02) = 0x0a0b0c0d;   /* mana */
    A4_WORD(0x82b6 + 46 + 0x10) = 0x0607;
    A4_LONG(0x82b6 + 46 + 0x22) = 2;   /* _stats pointer: slot 1 */
    A4_LONG(0xae72) = 0xdeadbeef; game_turn = 0x2345; A4_WORD(0xae7a) = 3; A4_WORD(0xae7c) = 0x104;
    A4_WORD(0xae7e) = 77; conq_08_seed = 0x4321; conquest = 0x9a; A4_WORD(0x8256) = 2;
    start_seed = 0x0bad; seed = 0x0bee; A4_WORD(0xae80) = 0;
}
static void test_save_load(void)
{
    assert(mkdtemp(save_directory));
    assert(!setenv("POPULOUS_SAVE_DIR", save_directory, 1));
    static uint8_t file[CORE_SAVE_SIZE + 8], again[CORE_SAVE_SIZE + 8];
    static uint8_t snapshot[sizeof prg_a4_area], loaded[sizeof prg_a4_area];
    assert(CORE_SAVE_SIZE == 29646 && CORE_SAVE_SCALARS_AT + 42 == CORE_SAVE_SIZE);

    /* Byte layout: _Write order, big-endian, peep pointers as _peeps addresses. */
    save_fixture();
    int16_t person = (int16_t)(no_peeps - 1);
    assert(no_peeps >= 2 && set_peep_reference(person, 0));
    uint32_t reference = 1;
    memcpy(peeps + 200*22 + 14, &reference, 4);   /* beyond no_peeps: kept as an address */
    assert(core_save_game("LAYOUT"));
    assert(save_read("LAYOUT.GAM", file, sizeof file) == CORE_SAVE_SIZE);
    assert(be32_at(file) == 0xffffb615u);
    assert(be16_at(file+4) == 0x1234 && be16_at(file+4+2*4224) == 0xfffe);
    int maps = 4 + 0x2102;
    assert(file[maps] == 0xab && file[maps+0x1000+1] == 0xcd && file[maps+0x2000+4095] == 0xef &&
           file[maps+0x3000+2] == 0x5a);
    int magnet = maps + 0x4000;
    assert(be16_at(file+magnet) == (uint16_t)core_leader(0) && be16_at(file+magnet+2) == (uint16_t)core_target(0) &&
           be32_at(file+magnet+8) == 0x01020304 && be32_at(file+magnet+12) == 0x0a0b0c0d &&
           be32_at(file+magnet+16+8) == 0x00011000);
    int records = magnet + 0x20;
    assert(file[records + person*22] == peeps[person*22] && file[records + person*22 + 1] == peeps[person*22 + 1]);
    assert(be16_at(file + records + person*22 + 4) == (uint16_t)peep_word(person, 4) &&
           be16_at(file + records + person*22 + 8) == (uint16_t)peep_word(person, 8));
    assert(be32_at(file + records + person*22 + 14) == 0x53014u);
    assert(be32_at(file + records + 200*22 + 14) == 0x53014u);
    int stats_at = records + 0x1222;
    assert(be16_at(file + stats_at + 46 + 0x10) == 0x0607 && be32_at(file + stats_at + 46 + 0x22) == 0x53014u + 22);
    const uint8_t *s = file + CORE_SAVE_SCALARS_AT;
    assert(be16_at(s) == 2 && be16_at(s+2) == 0x0bee && be16_at(s+4) == 0x0bad && be16_at(s+6) == (uint16_t)player &&
           be16_at(s+8) == (uint16_t)ground_in && be16_at(s+10) == 0 && be16_at(s+12) == 0x2345);
    assert(s[14] == 0x9a && be16_at(s+22) == 0x4321 && be16_at(s+24) == (uint16_t)in_conquest &&
           be16_at(s+26) == (uint16_t)paint_map && be16_at(s+28) == 3 && be16_at(s+30) == 0x104 &&
           be16_at(s+32) == (uint16_t)game_mode && be16_at(s+34) == 77 && be32_at(s+36) == 0xdeadbeefu);
    /* _checksum: word sums of _alt and the maps plus good_pop+bad_pop. */
    uint16_t sum = 0;
    for (int at = 4; at < magnet; at += 2) sum = (uint16_t)(sum + be16_at(file+at));
    sum = (uint16_t)(sum + 0x01020304u + 0x00011000u);
    assert(be16_at(s+40) == sum && (uint16_t)A4_WORD(0x3082) == sum);
    assert(core_save_encode(again) == sum && !memcmp(again, file, CORE_SAVE_SIZE));
    /* An existing ".GAM" is not doubled. */
    assert(core_save_game("LAYOUT.GAM") && save_read("LAYOUT.GAM", again, sizeof again) == CORE_SAVE_SIZE);
    save_remove("LAYOUT.GAM");

    /* Round trip: everything saved comes back; _load_game's own steps
     * (no_peeps, cleared mailboxes, _get_heading, magnets) are applied. */
    save_fixture();
    A4_DISP(0x82b6)[0] = 7;   /* mailbox, cleared by the load */
    memcpy(snapshot, prg_a4_area, sizeof snapshot);
    StartSelection start = current_start; GraphicsAssets *assets = game_assets;
    assert(core_save_game("ROUND"));
    assert(game_reset(1)); for (int k = 0; k < 6; ++k) game_tick();
    core_tool = 1; core_set_pointer(); core_result = 1;
    assert(core_load_game("ROUND"));
    assert(!core_result && !core_tool && game_assets && current_start.kind == start.kind);
    memcpy(loaded, prg_a4_area, sizeof loaded);
    for (size_t r = 0; r < sizeof core_save_regions / sizeof *core_save_regions; ++r) {
        size_t at = (size_t)(A4_DISP(core_save_regions[r].offset) - prg_a4_area);
        for (int k = 0; k < core_save_regions[r].size; ++k) {
            int field = k % 22;
            if (core_save_regions[r].kind == CORE_SAVE_PEEPLIST && k/22 < no_peeps && field >= 14 && field < 18) continue;
            if (core_save_regions[r].kind == CORE_SAVE_STATS && k < 3) { assert(!prg_a4_area[at+k]); continue; }
            assert(prg_a4_area[at+k] == snapshot[at+k]);
        }
    }
    int16_t peeps_now = no_peeps;
    for (int i = 0; i < peeps_now; ++i) {   /* references are _get_heading's, which is stable */
        uint32_t raw; memcpy(&raw, peeps + i*22 + 14, 4);
        if (!raw) continue;
        uint8_t record[22]; memcpy(record, peeps + i*22, 22);
        core_get_heading(i); assert(!memcmp(record, peeps + i*22, 22));
    }
    assert(peep_word(peeps_now-1, 4) && not_player == (player ? 0 : 1) &&
           A4_WORD(0xae78) == core_target(0) && A4_WORD(0xae76) == core_target(1));
    /* Loading again over a changed game gives the identical state. */
    assert(game_reset(2)); for (int k = 0; k < 3; ++k) game_tick();
    assert(core_load_game("ROUND.GAM"));
    for (size_t r = 0; r < sizeof core_save_regions / sizeof *core_save_regions; ++r) {
        size_t at = (size_t)(A4_DISP(core_save_regions[r].offset) - prg_a4_area);
        assert(!memcmp(loaded + at, prg_a4_area + at, (size_t)core_save_regions[r].size));
    }
    static const int derived[] = {0xae5e, 0xae76, 0xae78, 0x825a, 0x3082, 0x84b6};   /* no_peeps.._pointer */
    for (size_t k = 0; k < sizeof derived / sizeof *derived; ++k) {
        size_t at = (size_t)(A4_DISP(derived[k]) - prg_a4_area);
        assert(!memcmp(loaded + at, prg_a4_area + at, 2));
    }
    /* Saving the loaded game reproduces the file bit for bit. */
    assert(save_read("ROUND.GAM", file, sizeof file) == CORE_SAVE_SIZE);
    assert(core_save_game("AGAIN") && save_read("AGAIN.GAM", again, sizeof again) == CORE_SAVE_SIZE);
    for (int k = 0; k < CORE_SAVE_SIZE; ++k) {
        int at = k - (magnet + 0x20);
        if (at >= 0 && at < 0x1222 && at/22 < peeps_now && at%22 >= 14 && at%22 < 18) continue;   /* _get_heading */
        if (k >= stats_at && k < stats_at + 92 && (k - stats_at) % 46 < 3) continue;           /* mailboxes */
        assert(file[k] == again[k]);
    }
    save_remove("AGAIN.GAM");

    /* Invalid files are rejected before anything changes. */
    memcpy(snapshot, prg_a4_area, sizeof snapshot); assets = game_assets;
    memcpy(again, file, CORE_SAVE_SIZE); again[0] = 0; save_write("BAD.GAM", again, CORE_SAVE_SIZE);
    assert(!core_load_game("BAD"));
    save_write("BAD.GAM", file, CORE_SAVE_SIZE - 1); assert(!core_load_game("BAD"));
    memcpy(again, file, CORE_SAVE_SIZE); again[CORE_SAVE_SIZE] = 0;
    save_write("BAD.GAM", again, CORE_SAVE_SIZE + 1); assert(!core_load_game("BAD"));
    memcpy(again, file, CORE_SAVE_SIZE); again[maps + 7] ^= 1;   /* checksum mismatch */
    save_write("BAD.GAM", again, CORE_SAVE_SIZE); assert(!core_load_game("BAD"));
    memcpy(again, file, CORE_SAVE_SIZE); again[CORE_SAVE_SCALARS_AT + 9] = 4;   /* _ground_in 4 */
    save_write("BAD.GAM", again, CORE_SAVE_SIZE); assert(!core_load_game("BAD"));
    memcpy(again, file, CORE_SAVE_SIZE); again[CORE_SAVE_SCALARS_AT + 7] = 2;   /* _player 2 */
    save_write("BAD.GAM", again, CORE_SAVE_SIZE); assert(!core_load_game("BAD"));
    assert(!core_load_game("MISSING"));
    assert(!memcmp(snapshot, prg_a4_area, sizeof snapshot) && game_assets == assets);
    save_remove("BAD.GAM");

    /* The requester: SAVE A GAME, type the name, OK. */
    save_fixture(); core_game_options_open();
    entry_click(core_game_entry(12),16,16);
    assert(core_req.kind == CORE_REQ_FILE && core_file.save && core_file.count == 1 && !strcmp(core_file.names[0], "ROUND"));
    assert(!strcmp(core_rec_text(core_file_ok), "SAVE") && core_rec_w(core_file_ok,0) == 80 &&
           core_rec_w(core_file_ok,2) == 176 && core_rec_w(core_end_cancel,2) == 176);
    core_draw_requester();
    screen_click(0x60, 0xa2, SDL_BUTTON_LEFT); assert(core_file.field == 2);
    world_type("slot\b\bved\n"); assert(!core_file.field && !strcmp(core_filename, "SLVED"));
    memcpy(snapshot, prg_a4_area, sizeof snapshot);
    char level_path[512]; snprintf(level_path, sizeof level_path, "%s/level.dat", test_data_directory);
    assert(access(level_path, F_OK));   /* no level.dat: the disk check after the save */
    screen_click(core_rec_w(core_file_ok,0)+4, core_rec_w(core_file_ok,2)+8, SDL_BUTTON_LEFT);
    assert(save_read("SLVED.GAM", file, sizeof file) == CORE_SAVE_SIZE);
    /* LAB_46110..LAB_46144 (asm:10931-10944): _saved is set, then
     * _do_message(strInsertDisk,0) until level.dat opens; only OK, keys ignored. */
    assert(core_req.kind == CORE_REQ_FILE && core_disk_message == CORE_DISK_SAVE && A4_WORD(0x3054) == 1);
    world_type("x\n"); assert(!strcmp(core_filename, "SLVED") && core_disk_message == CORE_DISK_SAVE);
    core_draw_requester(); assert_tile(32,0x3c,12);
    assert(core_rec_w(core_end_ok,0) == 144 && core_rec_w(core_end_ok,4) == 192 && core_rec_w(core_end_ok,2) == 0x5c);
    screen_click(0x100, 0x60, SDL_BUTTON_LEFT); assert(core_disk_message == CORE_DISK_SAVE);
    disk_ok_click(); assert(core_req.kind == CORE_REQ_FILE && core_disk_message == CORE_DISK_SAVE);
    write_level_fixture(1, 1);
    disk_ok_click();
    assert(core_req.kind == CORE_REQ_NONE && !core_disk_message && A4_WORD(0x3054) == 1);
    assert(!unlink(level_path));
    /* LOAD A GAME lists the drawer (upper-cased .GAM files, sorted), a row
     * picks the name; the arrows scroll while top+5 < count. */
    save_write("b.gam", file, 4); save_write("note.txt", file, 4);
    for (int k = 0; k < 4; ++k) { char name[16]; snprintf(name, sizeof name, "Z%d.GAM", k); save_write(name, file, 4); }
    char sub[512]; save_file_path(sub, sizeof sub, "DIR.GAM"); assert(!mkdir(sub, 0700));
    assert(game_reset(1)); serial_off = 1; core_game_options_open();
    entry_click(core_game_entry(13),16,16);
    assert(core_req.kind == CORE_REQ_FILE && !core_file.save && core_file.count == 7);
    assert(!strcmp(core_file.names[0], "B") && !strcmp(core_file.names[1], "ROUND") &&
           !strcmp(core_file.names[2], "SLVED") && !strcmp(core_file.names[6], "Z3"));
    screen_click(0xdc, 0x7c, SDL_BUTTON_LEFT); screen_click(0xdc, 0x7c, SDL_BUTTON_LEFT);
    screen_click(0xdc, 0x7c, SDL_BUTTON_LEFT); assert(core_file.top == 2);
    screen_click(0xdc, 0x24, SDL_BUTTON_LEFT); assert(core_file.top == 1);
    core_draw_requester();
    screen_click(0x60, 0x22 + 8 + 2, SDL_BUTTON_LEFT); assert(!strcmp(core_filename, "SLVED"));
    screen_click(core_rec_w(core_file_ok,0)+4, core_rec_w(core_file_ok,2)+8, SDL_BUTTON_LEFT);
    assert(core_req.kind == CORE_REQ_NONE && !memcmp(map_who, snapshot + (map_who - prg_a4_area), 4096));
    assert(A4_LONG(0xae72) == 0xdeadbeef && game_turn == 0x2345 && A4_WORD(0xae86) == 0x1234);
    /* A failed save shows ERROR IN SAVING until a click; a bad drawer lists
     * nothing; an empty name or CANCEL return to the game options. */
    core_game_options_open(); entry_click(core_game_entry(12),16,16);
    screen_click(0x60, 0x8f, SDL_BUTTON_LEFT); assert(core_file.field == 1);
    world_type("nodir\n"); assert(!strcmp(core_drawer, "NODIR") && !core_file.count);
    memcpy(snapshot, prg_a4_area, sizeof snapshot);
    screen_click(core_rec_w(core_file_ok,0)+4, core_rec_w(core_file_ok,2)+8, SDL_BUTTON_LEFT);
    assert(core_req.kind == CORE_REQ_FILE && core_file.error);
    core_draw_requester();
    screen_click(10, 10, SDL_BUTTON_LEFT); assert(!core_file.error && core_req.kind == CORE_REQ_FILE);
    char missing[512]; save_file_path(missing, sizeof missing, "NODIR/SLVED.GAM"); assert(access(missing, F_OK));
    screen_click(0x60, 0x8f, SDL_BUTTON_LEFT);
    /* CANCEL in _get_name empties the field and ends the requester on the same press. */
    cancel_click(); assert(core_req.kind == CORE_REQ_GAME && !core_drawer[0] && !core_file.field);
    core_filename[0] = 0; entry_click(core_game_entry(12),16,16);
    screen_click(core_rec_w(core_file_ok,0)+4, core_rec_w(core_file_ok,2)+8, SDL_BUTTON_LEFT);
    assert(core_req.kind == CORE_REQ_GAME);
    core_req.kind = CORE_REQ_NONE;

    const char *names[] = {"ROUND.GAM","SLVED.GAM","b.gam","note.txt","Z0.GAM","Z1.GAM","Z2.GAM","Z3.GAM"};
    for (size_t k = 0; k < sizeof names / sizeof *names; ++k) save_remove(names[k]);
    assert(!rmdir(sub) && !rmdir(save_directory));
    assert(!setenv("POPULOUS_SAVE_DIR", "/nonexistent-populous-saves", 1));
}

/* _requester/_check_cancel/_options/_game_options/_set_options. */
static void test_requesters(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    /* Opening through the toolbar icon: _do_action y=7, x = left button. */
    toolbar_fixture(); tempo=5;
    system_click(0,0,SDL_BUTTON_LEFT);
    int16_t turn=game_turn; game_tick();
    assert(core_req.kind==CORE_REQ_OPTIONS && core_req.side==player && game_turn==turn+1 && !tempo);
    assert(!A4_DISP(0x82b6)[0]);
    /* Geometry: _requester(16,16,0x120,0xa6,ok,cancel). */
    assert(core_rec_w(core_end_ok,0)==32 && core_rec_w(core_end_ok,2)==176 && core_rec_w(core_end_ok,4)==80);
    assert(core_rec_w(core_end_cancel,0)==208 && core_rec_w(core_end_cancel,2)==176 && core_rec_w(core_end_cancel,4)==288);
    draw_original(xoff,yoff);
    assert_tile(16,16,12); assert_tile(160,16,13); assert_tile(288,16,14);
    assert_tile(16,32,18); assert_tile(288,32,19);
    assert_tile(16,176,15); assert_tile(32,176,21); assert_tile(64,176,23); assert_tile(80,176,16);
    assert_tile(192,176,16); assert_tile(208,176,21); assert_tile(272,176,23); assert_tile(288,176,17);
    for (int x=48;x<64;++x) for (int y=176;y<179;++y) assert(game_pixels[y*320+x]==icon_pixel(22,x&15,y-176));
    /* _check_cancel edges: x..x2, y..y+16 inclusive. */
    static const int hits[][3]={{32,176,1},{80,192,1},{81,180,0},{31,180,0},{50,175,0},{50,193,0},
                                {208,176,2},{288,192,2},{207,180,0},{289,180,0}};
    for (size_t k=0;k<sizeof hits/sizeof *hits;++k) {
        mousex=(int16_t)hits[k][0]; mousey=(int16_t)hits[k][1];
        assert(core_check_cancel(core_end_ok,core_end_cancel)==hits[k][2]);
    }
    mousex=50; mousey=180; assert(core_check_cancel(core_end_ok,NULL)==1);
    mousex=220; assert(!core_check_cancel(core_end_ok,NULL) && !core_check_cancel(NULL,NULL));
    /* Paused: no tick, keys inert, other buttons and clicks off the buttons
     * leave everything as it is. */
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    for (int k=0;k<3;++k) game_tick();
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_SPACE; game_handle_event(&key);
    key.key.keysym.sym=SDLK_q; game_handle_event(&key);
    screen_click(100,100,SDL_BUTTON_LEFT); screen_click(40,184,SDL_BUTTON_RIGHT);
    grid_click(3,3,SDL_BUTTON_LEFT); grid_click(6,0,SDL_BUTTON_LEFT);
    keep_mouse(before);
    assert(a4_same(before) && core_req.kind==CORE_REQ_OPTIONS && !pause && !core_tool);
    /* A conquest game shows the options without edits. */
    entry_click(core_option_entry(1),16,16); keep_mouse(before); assert(a4_same(before));
    ok_click(); assert(!core_req.kind && left_button==2);
    /* The other side through the right button; not while its mailbox
     * also holds the options request (asm:18607-18629). */
    toolbar_fixture(); system_click(0,0,SDL_BUTTON_RIGHT); core_consume_terrain_mailboxes();
    assert(core_req.kind==CORE_REQ_OPTIONS && core_req.side==not_player);
    assert(!strcmp(core_rec_text(core_option_entry(0)),"OPTIONS FOR EVIL"));
    core_req.kind=CORE_REQ_NONE;
    toolbar_fixture(); system_click(0,0,SDL_BUTTON_LEFT);
    A4_DISP(0x82b6+46)[0]=14; A4_DISP(0x82b6+46)[2]=7;
    core_consume_terrain_mailboxes(); assert(!core_req.kind);
    /* Custom game: each power bit, the sliders, then OK stores 10-value. */
    for (int bit=0;bit<9;++bit) {
        toolbar_fixture(); in_conquest=-1;
        uint8_t *own=A4_DISP(0x82b6); core_rec_w(own,0xe)=0x15;
        core_options_open(0);
        assert(core_rec_text(core_option_entry(bit+1))[0]==(char)(0x80+!!(0x15&(1<<bit))));
        assert(!strcmp(core_rec_text(core_option_entry(0)),"OPTIONS FOR GOOD"));
        entry_click(core_option_entry(bit+1),16,16);
        assert(core_rec_w(own,0xe)==(0x15^(1<<bit)) && core_req.kind==CORE_REQ_OPTIONS);
        assert(core_rec_text(core_option_entry(bit+1))[0]==(char)(0x80+!(0x15&(1<<bit))));
        ok_click(); assert(!core_req.kind && core_rec_w(own,0xe)==(0x15^(1<<bit)));
    }
    toolbar_fixture(); in_conquest=-1;
    uint8_t *own=A4_DISP(0x82b6+46); core_rec_w(own,0xc)=2; core_rec_w(own,0x10)=7;
    core_options_open(1);
    assert(core_rec_w(own,0xc)==8 && core_rec_w(own,0x10)==3);
    const uint8_t *slider=core_option_entry(11);
    screen_click(core_rec_w(slider,0)+0x28+5*8+3,core_rec_w(slider,2)+18,SDL_BUTTON_LEFT);
    assert(core_rec_w(own,0xc)==5);
    screen_click(core_rec_w(slider,0)+0x28+10*8+3,core_rec_w(slider,2)+18,SDL_BUTTON_LEFT);
    assert(core_rec_w(own,0xc)==5);   /* 10 is out of 0..9 */
    screen_click(core_rec_w(slider,0)+0x28-3,core_rec_w(slider,2)+18,SDL_BUTTON_LEFT);
    assert(core_rec_w(own,0xc)==0);   /* DIVS truncates -3/8 to 0 */
    slider=core_option_entry(13);
    screen_click(core_rec_w(slider,0)+0x30+9*8,core_rec_w(slider,2)+18,SDL_BUTTON_LEFT);
    assert(core_rec_w(own,0x10)==9);
    ok_click(); assert(core_rec_w(own,0xc)==10 && core_rec_w(own,0x10)==1);
    /* Cancel restores the copy (_return_to_game sets _left_button 2). */
    toolbar_fixture(); in_conquest=-1; tempo=0; left_button=2; memcpy(before,prg_a4_area,sizeof before);
    core_options_open(0);
    entry_click(core_option_entry(3),16,16);
    screen_click(core_rec_w(core_option_entry(11),0)+0x28+1,core_rec_w(core_option_entry(11),2)+18,SDL_BUTTON_LEFT);
    keep_mouse(before); assert(!a4_same_but_tables(before));
    cancel_click(); keep_mouse(before); assert(!core_req.kind && a4_same_but_tables(before));

    /* _game_options via the toolbar (y=8): _new_map cleared, ticks set. */
    toolbar_fixture(); A4_WORD(0x309a)=3; system_click(1,1,SDL_BUTTON_LEFT); game_tick();
    assert(core_req.kind==CORE_REQ_GAME && !A4_WORD(0x309a) && !A4_WORD(0x3080));
    assert(core_rec_w(core_end_ok,2)==176 && core_rec_w(core_end_cancel,0)==208);
    assert(core_rec_text(core_game_entry(1))[0]==(char)0x81 && core_rec_text(core_game_entry(3))[0]==(char)0x81 &&
           core_rec_text(core_game_entry(4))[0]==(char)0x80 && core_rec_text(core_game_entry(9))[0]==(char)0x81 &&
           core_rec_text(core_game_entry(10))[0]==(char)0x80);
    draw_original(xoff,yoff); assert_tile(16,16,12); assert_tile(288,176,17);
    /* Conquest: entries 0..8 are ignored. */
    left_button=2; memcpy(before,prg_a4_area,sizeof before);
    for (int i=1;i<9;++i) entry_click(core_game_entry(i),16,16);
    keep_mouse(before); assert(a4_same(before) && core_req.kind==CORE_REQ_GAME);
    /* CUSTOM GAME sets in_conquest -1; cancel restores everything. */
    entry_click(core_game_entry(10),16,16); assert(in_conquest==-1);
    entry_click(core_game_entry(4),16,16); assert(paint_map==1);
    entry_click(core_game_entry(6),16,16); assert(player==1 && not_player==0);
    entry_click(core_game_entry(8),16,16);
    cancel_click(); keep_mouse(before); assert(!core_req.kind && a4_same_but_tables(before));
    /* Each custom entry and OK. */
    toolbar_fixture(); in_conquest=-1; core_rec_w(core_stats(0),6)=0; core_rec_w(core_stats(1),6)=1;
    core_game_options_open();
    entry_click(core_game_entry(4),16,16); assert(paint_map==1 && core_rec_text(core_game_entry(4))[0]==(char)0x81);
    entry_click(core_game_entry(3),16,16); assert(paint_map==0 && core_rec_text(core_game_entry(4))[0]==(char)0x80);
    entry_click(core_game_entry(6),16,16);
    assert(player==1 && not_player==0 && core_rec_w(core_stats(1),6)==0 && core_rec_w(core_stats(0),6)==1 && pointer==4);
    entry_click(core_game_entry(5),16,16);
    assert(player==0 && not_player==1 && core_rec_w(core_stats(0),6)==0 && core_rec_w(core_stats(1),6)==1 && pointer==1);
    entry_click(core_game_entry(8),16,16); assert(core_rec_w(core_stats(0),6)==1 && core_rec_w(core_stats(1),6)==1);
    entry_click(core_game_entry(7),16,16); assert(core_rec_w(core_stats(0),6)==0 && core_rec_w(core_stats(1),6)==1);
    /* Inert: TWO PLAYERS (CONQUEST opens _show_world, test_world_screen).
     * SAVE A GAME / LOAD A GAME open _save_load; CANCEL changes nothing
     * (test_save_load); LOAD only while _serial_off. */
    memcpy(before,prg_a4_area,sizeof before);
    entry_click(core_game_entry(2),16,16);
    entry_click(core_game_entry(12),16,16); assert(core_req.kind==CORE_REQ_FILE && core_file.save==1);
    cancel_click(); assert(core_req.kind==CORE_REQ_GAME);
    entry_click(core_game_entry(13),16,16);
    assert(core_req.kind==(serial_off ? CORE_REQ_FILE : CORE_REQ_GAME));
    if (serial_off) { assert(!core_file.save); cancel_click(); assert(core_req.kind==CORE_REQ_GAME); }
    /* _save_load waits with _left_button 2 (asm:10833, 10907). */
    keep_mouse(before); assert(left_button==2); memcpy(before+(A4_DISP(0xae62)-prg_a4_area),A4_DISP(0xae62),2);
    assert(a4_same_but_tables(before) && core_req.kind==CORE_REQ_GAME);
    entry_click(core_game_entry(1),16,16); assert(serial_off==1 && core_rec_text(core_game_entry(1))[0]==(char)0x81);
    /* GAME OPTIONS: _set_options over the setup screen, toggles _game_mode. */
    entry_click(core_game_entry(11),16,16);
    assert(core_req.kind==CORE_REQ_SET && core_rec_w(core_end_ok,0)==48 && core_rec_w(core_end_ok,2)==120 &&
           core_rec_w(core_end_ok,4)==96 && core_rec_w(core_end_cancel,0)==192 && core_rec_w(core_end_cancel,4)==272);
    draw_original(xoff,yoff); assert_tile(32,24,12); assert_tile(272,24,14); assert_tile(32,120,15); assert_tile(272,120,17);
    assert_tile(16,16,12);   /* the setup frame stays around it */
    game_mode=0;
    for (int bit=0;bit<5;++bit) { entry_click(core_set_entry(bit*2+1),0x20,0x18); assert(game_mode==((2<<bit)-1)); }
    ok_click(); assert(core_req.kind==CORE_REQ_GAME && game_mode==0x1f && core_rec_w(core_end_ok,0)==32);
    entry_click(core_game_entry(11),16,16); entry_click(core_set_entry(5),0x20,0x18); assert(game_mode==0x1b);
    cancel_click(); assert(core_req.kind==CORE_REQ_GAME && game_mode==0x1f);
    ok_click(); assert(!core_req.kind && paint_map==0 && player==0 && in_conquest==-1 && game_mode==0x1f);
    /* Conquest: _set_options shows but does not toggle. */
    toolbar_fixture(); core_game_options_open(); entry_click(core_game_entry(11),16,16);
    int16_t modes=game_mode; entry_click(core_set_entry(1),0x20,0x18); assert(game_mode==modes);
    ok_click(); ok_click(); assert(!core_req.kind);
    /* SURRENDER: _surender = player, the next tick loses. */
    toolbar_fixture(); core_game_options_open(); entry_click(core_game_entry(16),16,16);
    assert(!core_req.kind && A4_WORD(0x8258)==player);
    game_tick(); assert(core_result==2);
    /* RESTART / NEXT MAP: _setup_display rebuilds the conquest world. */
    for (int entry=14;entry<16;++entry) {
        toolbar_fixture(); for (int k=0;k<5;++k) game_tick();
        core_game_options_open(); entry_click(core_game_entry(entry),16,16);
        assert(!core_req.kind && !game_turn && !core_result);
        /* in_conquest -1: a custom landscape (test_custom_game has the seeds). */
        toolbar_fixture(); for (int k=0;k<5;++k) game_tick(); in_conquest=-1;
        core_game_options_open(); entry_click(core_game_entry(entry),16,16);
        assert(!core_req.kind && !game_turn && current_start.kind==START_CUSTOM && in_conquest==-1);
    }
    _closeall(); SDL_Quit();
}

/* lord.pic: colour (x/8+y)&15, palette lord_fixture_rgb(i); mouths.pic
 * frame k all colour k+1. gwords/gmusic1 copied from the export if present. */
static uint16_t lord_fixture_rgb(int i) { return (uint16_t)((i*7&15) << 8 | (15-i) << 4 | i); }
static int copy_raw_sound(const char *name)
{
    char path[4160]; snprintf(path,sizeof path,"%s/raw/%s",POPULOUS_DEFAULT_ASSET_DIR,name);
    FILE *in = fopen(path,"rb"); if (!in) return 0;
    static uint8_t bytes[0x40000]; size_t count = fread(bytes,1,sizeof bytes,in); fclose(in);
    snprintf(path,sizeof path,"%s/raw",test_data_directory); mkdir(path,0700);
    snprintf(path,sizeof path,"%s/raw/%s",test_data_directory,name);
    FILE *out = fopen(path,"wb"); assert(out && fwrite(bytes,1,count,out) == count && !fclose(out));
    return 1;
}
static int write_lord_fixture(void)
{
    static uint8_t lord[32032], mouths[5040];
    for (int p = 0; p < 4; ++p) for (int b = 0; b < 8000; ++b) lord[p*8000+b] = (((b%40 + b/40) & 15) >> p & 1) ? 0xff : 0;
    for (int i = 0; i < 16; ++i) { lord[32000+i*2] = (uint8_t)(lord_fixture_rgb(i) >> 8); lord[32001+i*2] = (uint8_t)lord_fixture_rgb(i); }
    for (int k = 0; k < 6; ++k) for (int r = 0; r < 35; ++r) for (int p = 0; p < 4; ++p)
        memset(mouths + k*840 + r*24 + p*6, ((k+1) >> p & 1) ? 0xff : 0, 6);
    const struct { const char *name; uint8_t *data; size_t size; } files[] = {{"lord.pic",lord,sizeof lord},{"mouths.pic",mouths,sizeof mouths}};
    for (int f = 0; f < 2; ++f) {
        char path[512]; snprintf(path,sizeof path,"%s/%s",test_data_directory,files[f].name);
        FILE *out = fopen(path,"wb"); assert(out && fwrite(files[f].data,1,files[f].size,out) == files[f].size && !fclose(out));
    }
    return copy_raw_sound("gwords") & copy_raw_sound("gmusic1");
}
static void remove_lord_fixture(void)
{
    const char *names[] = {"lord.pic","mouths.pic","raw/gwords","raw/gmusic1","raw"};
    for (int n = 0; n < 5; ++n) {
        char path[512]; snprintf(path,sizeof path,"%s/%s",test_data_directory,names[n]);
        if (n < 4) unlink(path); else rmdir(path);
    }
}

/* Result -> stats once -> click/key -> next world (asm:14386-14402, 14460-14491). */
static void test_end_transition(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    write_level_fixture(1, 1);
    write_font_fixture();
    write_lord_fixture();
    SDL_Event key = {.type = SDL_KEYDOWN}, click = {.type = SDL_MOUSEBUTTONDOWN};
    key.key.keysym.sym = SDLK_x; click.button.button = SDL_BUTTON_LEFT;
    /* Conquest win. */
    assert(game_reset_start((StartSelection){START_ORIGINAL, 2}) && game_font_loaded);
    assert(A4_WORD(0xae7e) == 10 && serial_off == 1 && !core_end_shown);
    end_kill(1);
    int32_t score = (int32_t)A4_LONG(0xae72);
    assert(score >= 5000 && !strcmp(core_end_row(0), "GAME WON"));
    int finished, next = core_next_level(10, score, &finished);
    assert(core_end_next == next && !finished && next > 10 && !(next % 5));
    char code[16]; core_code(code, core_code_rand(next));
    uint8_t stopped[sizeof prg_a4_area]; memcpy(stopped, prg_a4_area, sizeof stopped);
    game_tick(); assert(!memcmp(stopped, prg_a4_area, sizeof stopped));   /* finished: no tick */
    key.key.repeat = 1; game_handle_event(&key); key.key.repeat = 0;
    assert(core_result == 1 && !memcmp(stopped, prg_a4_area, sizeof stopped));
    SDL_Event escape = key; escape.key.keysym.sym = SDLK_ESCAPE;
    game_handle_event(&escape); assert(prg_quit && core_result == 1); prg_quit = 0;
    game_handle_event(&key);
    /* _won_conquest: the lord screen first (asm:14503-14765, test_lord_screen);
     * keys other than Escape keep it, a left click leaves it. */
    assert(core_lord.shown && !core_req.kind && core_result == 1 && !memcmp(core_message, code, strlen(code)+1));
    game_handle_event(&key); game_handle_event(&escape); assert(prg_quit && core_lord.shown); prg_quit = 0;
    screen_click(160,100,SDL_BUTTON_RIGHT); assert(core_lord.shown);
    screen_click(160,100,SDL_BUTTON_LEFT); assert(!core_lord.shown);
    /* Then _show_world with the next world's code (asm:14492-14501, 14766-14772). */
    assert(core_req.kind == CORE_REQ_WORLD && core_world.origin == CORE_WORLD_WON && core_world.shown &&
           !strcmp(core_message, code) && core_world.level == next && core_result == 1);
    world_ok_click();
    assert(!core_req.kind && !core_result && !core_end_shown && current_start.kind == START_ORIGINAL && current_start.number == next/5);
    assert(A4_WORD(0xae7e) == next && in_conquest == next && A4_LONG(0xae72) == 10 && !A4_WORD(0x82bc));
    /* Conquest loss: the same world again, by a click on the requester
     * button (_check_cancel(button,0), asm:14370-14385); a click elsewhere
     * or with another button keeps the end screen. */
    end_kill(player);
    assert(!strcmp(core_end_row(0), "GAME LOST") && core_end_next == -1);
    game_handle_event(&click); assert(core_result && core_end_shown);
    core_end_button_layout(0);
    assert(!strcmp(core_rec_text(core_end_button), "TRY IT AGAIN") && core_rec_w(core_end_button,0) == 96 &&
           core_rec_w(core_end_button,2) == 176 && core_rec_w(core_end_button,4) == 224);
    screen_click(95,184,SDL_BUTTON_LEFT); screen_click(225,184,SDL_BUTTON_LEFT);
    screen_click(160,175,SDL_BUTTON_LEFT); screen_click(160,193,SDL_BUTTON_LEFT);
    screen_click(160,184,SDL_BUTTON_RIGHT);
    assert(core_result && core_end_shown);
    end_button_click();
    assert(!core_result && current_start.number == next/5 && A4_WORD(0xae7e) == next);
    /* The last world wraps to 0 (asm:14482-14491). */
    assert(game_reset_start((StartSelection){START_ORIGINAL, 494}));
    end_kill(1);
    assert(core_end_next == 0 && core_end_finished);
    end_button_click(); assert(core_lord.shown && core_lord.finished);
    screen_click(160,100,SDL_BUTTON_LEFT);
    assert(core_req.kind == CORE_REQ_WORLD && !core_world.level); world_ok_click();
    assert(current_start.kind == START_ORIGINAL && !current_start.number && !A4_WORD(0xae7e));
    /* Synthetic scenarios are not conquest: restart them. */
    assert(game_reset(3));
    end_kill(1);
    assert(core_end_next == -1);
    game_handle_event(&key);
    assert(!core_result && current_start.kind == START_SYNTHETIC && current_start.number == 3);
    /* A result set without the end path keeps the old gating (no transition). */
    core_result = 1; memcpy(stopped, prg_a4_area, sizeof stopped);
    game_handle_event(&key); game_handle_event(&click);
    assert(core_result == 1 && !memcmp(stopped, prg_a4_area, sizeof stopped));
    /* End screen pixels: the _requester frame, its button and the font text
     * appear only with the result. */
    assert(game_reset_start((StartSelection){START_ORIGINAL, 2}));
    draw_original(xoff, yoff);
    uint8_t playing[sizeof game_pixels]; memcpy(playing, game_pixels, sizeof playing);
    end_kill(1);
    draw_original(xoff, yoff);
    /* "GAME WON" at _end_text (0x70,0x0a)+16 over the title tiles (icon 13, then 20 below y 32):
     * per plane (tile & mask) | glyph. */
    int text = 0;
    for (int y = 26; y < 34; ++y) for (int x = 128; x < 192; ++x) {
        const uint8_t *g = asm_glyph_A + (y-26)*5; int b = 7 - (x & 7), colour = 0, tile = y < 32 ? icon_pixel(13, x & 15, y-16) : icon_pixel(20, x & 15, y-32);
        if (x >= 128+32 && x < 128+40) { assert(game_pixels[y*320+x] == tile); continue; }   /* the space glyph keeps the tile */
        for (int plane = 0; plane < 4; ++plane)
            colour |= ((((tile >> plane) & (g[0] >> b)) | (g[1+plane] >> b)) & 1) << plane;
        assert(game_pixels[y*320+x] == colour);
        text |= colour != tile;
    }
    assert(text && memcmp(playing, game_pixels, sizeof playing));
    /* The original end screen only: no _won_conquest lines (they are on
     * the lord screen); the strip they used matches a lost result's. */
    uint8_t won_pixels[sizeof game_pixels]; memcpy(won_pixels, game_pixels, sizeof won_pixels);
    assert(core_end_next >= 0);
    core_result = 2; draw_original(xoff, yoff); core_result = 1;
    int nonzero = 0;
    for (int p = 0x96*320; p < 0xaa*320; ++p) { assert(won_pixels[p] == game_pixels[p]); nonzero |= won_pixels[p]; }
    assert(nonzero);
    draw_original(xoff, yoff);
    /* Corners and edges of the 16,16,0x120,0xa6 frame; "NEW GAME" button
     * tiles 21,22..,23 at x 112..207 (x2 = 112+(8+4)*8) on the bottom row y 176. */
    assert_tile(16,16,12); assert_tile(32,16,13); assert_tile(288,16,14);
    assert_tile(16,128,18); assert_tile(48,128,20); assert_tile(288,128,19);
    assert_tile(16,176,15); assert_tile(96,176,16); assert_tile(288,176,17);
    assert_tile(112,176,21); assert_tile(192,176,23); assert_tile(208,176,16);
    for (int y = 176; y < 179; ++y) for (int x = 128; x < 192; ++x) assert(game_pixels[y*320+x] == icon_pixel(22, x & 15, y-176));
    assert(core_rec_w(core_end_button,0) == 112 && core_rec_w(core_end_button,4) == 208);
    char path[512]; snprintf(path, sizeof path, "%s/font.dat", test_data_directory); assert(!unlink(path));
    snprintf(path, sizeof path, "%s/level.dat", test_data_directory); assert(!unlink(path));
    assert(game_reset(0) && !game_font_loaded);
    copy_real_font();
    remove_lord_fixture();
    _closeall(); SDL_Quit();
}

/* _show_world asm:12190-12773 with _get_name asm:10307-10488. */
/* The shared conquest generator (original_generate) entered with the
 * _seed the custom path builds from: one ___newrand when nonzero (asm:430-436,
 * computed here with core_code_rand), and the custom people counts. */
static uint8_t custom_reference[4096*3 + 208*22];
static int16_t custom_reference_seed;
static void custom_reference_build(int16_t from, int you, int him)
{
    static _Alignas(4) uint8_t scratch[sizeof prg_a4_area];
    memset(scratch, 0, sizeof scratch);
    uint8_t *live = prg_a4; prg_a4 = scratch + 32768;
    LevelStart start = {0};
    start.record = (LevelRecord){5, 3, 0, 0, 0, 0, (uint8_t)you, (uint8_t)him, 0};
    start.initial_seed = from ? core_code_rand((uint16_t)from) : 0;
    assert(original_generate(&start));
    memcpy(custom_reference, map_alt, 4096); memcpy(custom_reference+4096, map_blk, 4096);
    memcpy(custom_reference+8192, map_who, 4096); memcpy(custom_reference+12288, peeps, 208*22);
    custom_reference_seed = seed;
    prg_a4 = live;
}
static int custom_matches_reference(void)
{
    return !memcmp(custom_reference, map_alt, 4096) && !memcmp(custom_reference+4096, map_blk, 4096) &&
           !memcmp(custom_reference+8192, map_who, 4096) && !memcmp(custom_reference+12288, peeps, 208*22) &&
           seed == custom_reference_seed;
}

/* _setup_display with in_conquest -1 (asm:430-500): seeds, ground, people,
 * _new_map's next/restart (asm:18917-18932), the end of a game (asm:14410-
 * 14432), the _options/_set_options inputs and the synthetic fixtures. */
static void test_custom_game(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    core_custom_live = 0;
    /* Boot data: landscape 0 (no draw), 1 + 3 people (ratings 1/5), score 10. */
    assert(core_custom_start(0, 1));
    assert(current_start.kind==START_CUSTOM && in_conquest==-1 && !start_seed && !ground_in);
    assert(game_mode==0x10 && (uint16_t)A4_WORD(0x82c4)==0xffff && A4_WORD(0x82c6+46)==3 && A4_WORD(0x82f0)==5);
    assert(no_peeps==4 && A4_LONG(0xae72)==10 && A4_LONG(0x9a02)==399 && A4_LONG(0x9a12)==399);
    custom_reference_build(0, 1, 3); assert(custom_matches_reference());
    /* Seeds 1234 and 0x69bc: one draw, then the shared generator; arg 1 keeps ground 2. */
    static const int16_t seeds[] = {1234, 0x69bc, 77};
    for (int k=0;k<3;++k) {
        ground_in = 2; assert(core_custom_start(seeds[k], 1));
        assert(start_seed==seeds[k] && ground_in==2 && no_peeps==4);
        custom_reference_build(seeds[k], 1, 3); assert(custom_matches_reference());
        /* arg -1 steps ground on an odd draw; the same inputs give the same land. */
        ground_in = 3; assert(core_custom_start(seeds[k], -1));
        assert(ground_in == ((core_code_rand((uint16_t)seeds[k]) & 1) ? 0 : 3) && custom_matches_reference());
        static uint8_t first[sizeof prg_a4_area]; memcpy(first, prg_a4_area, sizeof first);
        ground_in = 3; assert(core_custom_start(seeds[k], -1) && !memcmp(first, prg_a4_area, sizeof first));
    }
    /* The tutorial is _setup_display(0,1) on 0x69bc with 15/3 people. */
    assert(game_reset_start((StartSelection){START_TUTORIAL, 0}));
    custom_reference_build(0x69bc, 15, 3); assert(custom_matches_reference());
    /* A4 is never reloaded: its _stats (ratings 10, powers $3f) stay for custom. */
    assert(core_custom_start(5, 1) && no_peeps==6 && A4_WORD(0x82c4)==0x3f && game_mode==1);
    custom_reference_build(5, 3, 3); assert(custom_matches_reference());
    core_custom_live = 0;   /* back to the boot data */

    /* MOVE TO NEXT MAP: _new_map 1 keeps the live _seed, arg -1. */
    ground_in = 2; assert(core_custom_start(1234, 1));
    for (int k=0;k<40;++k) game_tick();
    int16_t live = seed; assert(live != 1234);
    core_game_options_open(); entry_click(core_game_entry(14),16,16);
    assert(!core_req.kind && current_start.kind==START_CUSTOM && start_seed==live && !game_turn);
    assert(ground_in == ((core_code_rand((uint16_t)live) & 1) ? 3 : 2));
    custom_reference_build(live, 1, 3); assert(custom_matches_reference());
    /* RESTART THIS MAP: _start_seed, arg ground_in: the same landscape and ground. */
    static uint8_t lands[4096*3 + 208*22]; memcpy(lands, custom_reference, sizeof lands);
    int16_t ground = ground_in;
    for (int k=0;k<40;++k) game_tick();
    core_game_options_open(); entry_click(core_game_entry(15),16,16);
    assert(!core_req.kind && start_seed==live && ground_in==ground && !game_turn && custom_matches_reference());
    /* The host R key restarts the same landscape. */
    for (int k=0;k<5;++k) game_tick();
    SDL_Event reset = {.type = SDL_KEYDOWN}; reset.key.keysym.sym = SDLK_r; game_handle_event(&reset);
    assert(start_seed==live && !game_turn && custom_matches_reference());

    /* _set_options bit 0, an _options power, a rating > 4, the speed and
     * EVIL carry into the next map; no conq_ byte is read. */
    int16_t modes = game_mode;
    core_game_options_open(); entry_click(core_game_entry(11),16,16);
    entry_click(core_set_entry(1),0x20,0x18); ok_click();
    entry_click(core_game_entry(6),16,16); ok_click();
    assert(!core_req.kind && game_mode==(modes ^ 1) && player==1 && start_seed==live);  /* no _new_map */
    core_options_open(0); entry_click(core_option_entry(4),16,16); ok_click();
    assert(!core_req.kind && (uint16_t)A4_WORD(0x82c4)==0xfff7);
    A4_WORD(0x82c2) = 7; A4_WORD(0x82c6+46) = 9; conq_04_mode = 0x12; conq_02_enemypow = 0;
    live = seed;
    core_game_options_open(); entry_click(core_game_entry(14),16,16);
    assert(start_seed==live && game_mode==(modes ^ 1) && (uint16_t)A4_WORD(0x82c4)==0xfff7 && (uint16_t)A4_WORD(0x82c4+46)==0xffff);
    assert(A4_WORD(0x82c2)==7 && A4_WORD(0x82c6+46)==9 && player==1 && not_player==0 && pointer==4);
    assert(no_peeps==6 && A4_LONG(0xae72)==30);   /* 3 + 3, score from the enemy count as _player 1 */
    custom_reference_build(live, 3, 3); assert(custom_matches_reference());
    core_game_options_open(); entry_click(core_game_entry(5),16,16); ok_click(); assert(player==0);

    /* End of a custom game: _setup_display(0,-1) from the live _seed. */
    for (int k=0;k<10;++k) game_tick();
    live = seed; A4_WORD(0x8258) = player; game_tick(); assert(core_result==2);
    core_enter_end(); assert(core_end_next==-1 && core_leave_end());
    assert(!core_result && current_start.kind==START_CUSTOM && start_seed==live && in_conquest==-1);

    /* The scenario numbers stay port fixtures. */
    assert(game_reset(3) && current_start.kind==START_SYNTHETIC && in_conquest==15 && current_start_pop_you==10);
    assert(game_reset(0) && num_demo_peeps==22);
    _closeall(); SDL_Quit();
}

static void test_world_screen(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    /* Fixture: base record (startable), records 1 and 7 differ for display. */
    uint8_t bytes[LEVEL_FILE_SIZE];
    for (int index = 0; index < LEVEL_RECORD_COUNT; ++index) {
        uint8_t record[] = {7, 9, 135, 63, 19, 2, 1, 1, 0xab, 0xcd};
        if (index == 1) { uint8_t other[] = {1, 10, 0x15, 0x2a, 0x0a, 1, 12, 30, 0x01, 0x23}; memcpy(record, other, 10); }
        if (index == 7) { uint8_t other[] = {10, 2, 0x3f, 0x00, 0x10, 3, 5, 6, 0x12, 0x34}; memcpy(record, other, 10); }
        memcpy(bytes + 10 * index, record, 10);
    }
    char path[512]; snprintf(path, sizeof path, "%s/level.dat", test_data_directory);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, sizeof bytes, file) == sizeof bytes && !fclose(file));
    /* --conquest: _start_of_game == 2 (asm:536-551). */
    char *arguments[] = {"populous", "--conquest", NULL};
    StartSelection selection;
    assert(parse_start(2, arguments, &selection) && requested_world && selection.kind == START_ORIGINAL && !selection.number);
    arguments[1] = "--conquests"; assert(!parse_start(2, arguments, &selection) && !requested_world);
    /* Start: GENESIS by default, record 0 into _conquest, the lines shown. */
    assert(game_reset_start((StartSelection){START_ORIGINAL, 2}));
    core_message[0] = 0; core_world_open(CORE_WORLD_START);
    assert(core_req.kind == CORE_REQ_WORLD && core_world.shown && !core_world.entry && !core_world.level &&
           !strcmp(core_message, "GENESIS") && !memcmp(A4_DISP(0x84bc), bytes, 8) && (uint16_t)conq_08_seed == 0xabcd);
    assert(core_rec_w(core_world_ok,0) == 32 && core_rec_w(core_world_ok,2) == 168 && core_rec_w(core_world_cancel,2) == 168);
    char line[48], you[8], him[8];
    static const char *const genesis[15] = {NULL, "BATTLE NUMBER IS 0", "WORLDS LANDSCAPE IS SNOW AND ICE",
        "HIS REACTIONS ARE VERY SLOW", "HIS RATING IS POOR", "LAND BUILT JUST ON TOWNS", "THE SWAMPS ARE BOTTOMLESS",
        "WATER IS FATAL", "               YOU    HIM", "POPULATION"};
    for (int i = 1; i < 10; ++i) { core_world_line(i, line, you, him); assert(!strcmp(line, genesis[i])); }
    core_world_line(9, line, you, him); assert(!strcmp(you, "1") && !strcmp(him, "1"));
    for (int i = 10; i < 15; ++i) {   /* yourpow 63, enemypow 135 = bits 0,1,2 (+7) */
        core_world_line(i, line, you, him);
        assert(!strcmp(line, core_world_template[i].text) && !strcmp(you, "YES") && !strcmp(him, i < 13 ? "YES" : "NO"));
    }
    /* Paused while open; keys typed now are ignored (no entry). */
    uint8_t before[sizeof prg_a4_area]; memcpy(before, prg_a4_area, sizeof before);
    game_tick(); world_type("ab"); assert(!memcmp(before, prg_a4_area, sizeof before));
    draw_original(xoff, yoff);
    assert_tile(16,24,12); assert_tile(288,24,14); assert_tile(16,168,15); assert_tile(32,168,21); assert_tile(288,168,17);
    assert(!game_pixels[0] && !game_pixels[319]);   /* ___clr_wsc */
    /* NEW GAME: an empty entry; a world of record 1 (worlds 25..49). */
    world_new_click();
    assert(core_world.entry && !core_world.shown && !core_world.length && (uint8_t)core_message[0] == 0x82 && !core_message[16]);
    char name[16]; core_code(name, core_code_rand(30));
    for (char *c = name; *c; ++c) *c = (char)tolower((unsigned char)*c);
    world_type("X\b"); assert(!core_world.length && (uint8_t)core_message[0] == 0x82);
    world_type(name); world_type("\n");
    core_code(name, core_code_rand(30));
    assert(!strcmp(core_message, name) && core_world.shown && core_world.level == 30 && !memcmp(A4_DISP(0x84bc), bytes+10, 8));
    static const char *const second[8] = {NULL, "BATTLE NUMBER IS 6", "WORLDS LANDSCAPE IS DESERT",
        "HIS REACTIONS ARE VERY SLOW", "HIS RATING IS VERY GOOD", "LAND ONLY BUILT UP", "THE SWAMPS ARE BOTTOMLESS", "WATER IS HARMFUL"};
    for (int i = 1; i < 8; ++i) { core_world_line(i, line, you, him); assert(!strcmp(line, second[i])); }
    core_world_line(9, line, you, him); assert(!strcmp(you, "12") && !strcmp(him, "30"));
    core_world_line(10, line, you, him); assert(!strcmp(you, "NO") && !strcmp(him, "YES"));
    core_world_line(11, line, you, him); assert(!strcmp(you, "YES") && !strcmp(him, "NO"));
    /* A world of record 7 (175..199). */
    world_new_click(); core_code(name, core_code_rand(180)); world_type(name); world_type("\n");
    assert(core_world.level == 180 && !memcmp(A4_DISP(0x84bc), bytes+70, 8) && (uint16_t)conq_08_seed == 0x1234);
    core_world_line(3, line, you, him); assert(!strcmp(line, "HIS REACTIONS ARE VERY FAST"));
    core_world_line(4, line, you, him); assert(!strcmp(line, "HIS RATING IS VERY POOR"));
    core_world_line(5, line, you, him); assert(!strcmp(line, "LAND BUILT JUST ON TOWNS"));
    core_world_line(14, line, you, him); assert(!strcmp(you, "NO") && !strcmp(him, "YES"));
    /* Invalid: "NO SUCH WORLD", the entry stays; Escape keeps the host quit. */
    world_new_click(); world_type("NOWHERE\n");
    assert(core_req.kind == CORE_REQ_WORLD && core_world.error && core_world.entry && !core_world.shown && core_world.level < 0);
    draw_original(xoff, yoff); memcpy(before, prg_a4_area, sizeof before);
    SDL_Event escape = {.type = SDL_KEYDOWN}; escape.key.keysym.sym = SDLK_ESCAPE;
    game_handle_event(&escape); assert(prg_quit && core_req.kind == CORE_REQ_WORLD); prg_quit = 0;
    /* GENESIS by name, started with START GAME after the entry ends. */
    /* _get_name resumes after the rejected name (strlen, asm:10308-10312). */
    assert(core_world.length == 7 && !strcmp(core_message, "NOWHERE\x82\x82\x82\x82\x82\x82\x82\x82\x82"));
    world_type("\b\b\b\b\b\b\bgenesis"); assert(core_world.length == 7);
    world_ok_click(); assert(core_world.shown && !core_world.error && !core_world.level);
    world_ok_click();
    assert(!core_req.kind && current_start.kind == START_ORIGINAL && !current_start.number && !A4_WORD(0xae7e) && !in_conquest);
    /* A valid name starts its world. */
    core_message[0] = 0; core_world_open(CORE_WORLD_START); world_new_click();
    core_code(name, core_code_rand(10)); world_type(name); world_type("\n"); world_ok_click();
    assert(!core_req.kind && current_start.number == 2 && A4_WORD(0xae7e) == 10 && in_conquest == 10 && !player && serial_off == 1);
    /* A number: _start_seed/_seed, -1 -> the start loop shows it again. */
    core_message[0] = 0; core_world_open(CORE_WORLD_START); world_new_click(); world_type("123\n");
    assert(core_req.kind == CORE_REQ_WORLD && start_seed == 123 && seed == 123 && !strcmp(core_message, "GENESIS") && core_world.shown);
    /* Game options CONQUEST: _show_world with an empty _message; START GAME
     * sets _new_map and starts the world (asm:13221-13239). */
    toolbar_fixture(); in_conquest = -1;
    core_game_options_open(); entry_click(core_game_entry(9),16,16);
    assert(core_req.kind == CORE_REQ_WORLD && core_world.origin == CORE_WORLD_OPTIONS && !strcmp(core_message, "GENESIS") &&
           core_rec_text(core_game_entry(9))[0] == (char)0x81 && core_rec_text(core_game_entry(10))[0] == (char)0x80);
    draw_original(xoff, yoff); assert_tile(16,24,12);   /* over the setup frame, its bottom row below */
    for (int x = 0; x < 16; ++x) assert(game_pixels[188*320+16+x] == icon_pixel(15, x, 12));
    world_new_click(); core_code(name, core_code_rand(10)); world_type(name); world_type("\n"); world_ok_click();
    assert(!core_req.kind && current_start.kind == START_ORIGINAL && current_start.number == 2 && in_conquest == 10);
    /* A number there: -1 with _seed set, _new_map 1 builds that landscape
     * (asm:12262-12272, 13236-13239, 18917-18932). */
    toolbar_fixture(); in_conquest = -1;
    core_game_options_open(); entry_click(core_game_entry(9),16,16); world_new_click(); world_type("4321\n");
    assert(!core_req.kind && current_start.kind == START_CUSTOM && in_conquest == -1 && start_seed == 4321 && !game_turn);
    assert(ground_in == (core_code_rand(4321) & 1));   /* from ground 0, arg -1 */
    /* ___atoi's low word: 65536 is not a number but a (bad) name. */
    toolbar_fixture(); in_conquest = -1;
    core_game_options_open(); entry_click(core_game_entry(9),16,16); world_new_click(); world_type("65536\n");
    assert(core_req.kind == CORE_REQ_WORLD && core_world.error && current_start.kind == START_SYNTHETIC);
    core_req.kind = CORE_REQ_NONE;
    /* _animate with --conquest opens the screen before the first frame. */
    requested_world = 1; requested_start = (StartSelection){START_ORIGINAL, 0};
    SDL_Event quit = {.type = SDL_QUIT}; assert(SDL_PushEvent(&quit) == 1);
    _animate(); assert(core_req.kind == CORE_REQ_WORLD && core_world.origin == CORE_WORLD_START);
    requested_world = 0; core_req.kind = CORE_REQ_NONE; prg_quit = 0;
    assert(!unlink(path));
    _closeall(); SDL_Quit();
}

/* diskerr_47222 (asm:12369-12375): without level.dat _show_world shows
 * _do_message(strInsertDisk,0) (asm:11098-11141, text asm:21250-21256);
 * its OK re-evaluates the name (LAB_47064 with -22 clear), so it repeats
 * until the file is there. Also drawn in the diagnostic view. */
static void test_insert_disk(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    char path[512]; snprintf(path, sizeof path, "%s/level.dat", test_data_directory);
    unlink(path);
    assert(game_reset(0));
    core_message[0] = 0; core_world_open(CORE_WORLD_START);
    assert(core_req.kind == CORE_REQ_WORLD && core_disk_message == CORE_DISK_WORLD &&
           !core_world.shown && !core_world.entry && !strcmp(core_message, "GENESIS"));
    /* Frame 0x28,0x3c,0x100,0x30: title 12 at x 32 (0x28/16 words), the lone OK at 144..192. */
    draw_original(xoff, yoff);
    assert_tile(32,0x3c,12); assert_tile(0x110,0x3c,14); assert_tile(32,0x5c,15);
    assert(core_rec_w(core_end_ok,0) == 144 && core_rec_w(core_end_ok,4) == 192 && core_rec_w(core_end_ok,2) == 0x5c && !strcmp(core_rec_text(core_end_ok), "OK"));
    static uint8_t shown[sizeof game_pixels]; memcpy(shown, game_pixels, sizeof shown);
    core_disk_layout(1);   /* the frame alone: both text lines differ */
    int line1 = 0, line2 = 0;
    for (int x = 0x38; x < 0x38 + 21*8; ++x) for (int y = 0; y < 8; ++y) {
        line1 |= shown[(0x48+y)*320+x] != game_pixels[(0x48+y)*320+x];
        line2 |= shown[(0x50+y)*320+x] != game_pixels[(0x50+y)*320+x];
    }
    assert(line1 && line2);
    core_disk_layout(1); amiga_text(game_pixels, 0x38, 0x48, "INSERT THE ORIGINAL\nPOPULOUS DISK IN DF0:");
    assert(!memcmp(shown, game_pixels, sizeof shown));   /* exactly strInsertDisk at 0x38,0x48 */
    /* Keys are ignored, a click off OK keeps it, OK without the file repeats it. */
    world_type("ABC\n"); assert(!strcmp(core_message, "GENESIS") && core_disk_message == CORE_DISK_WORLD);
    screen_click(0x100, 0x48, SDL_BUTTON_LEFT); assert(core_disk_message == CORE_DISK_WORLD);
    disk_ok_click(); assert(core_req.kind == CORE_REQ_WORLD && core_disk_message == CORE_DISK_WORLD && !core_world.shown);
    /* Diagnostic view: the requester and the message over the map. */
    diagnostic_view = 1; xoff = yoff = 0;
    static uint32_t frame[320*256];
    _draw_it(yoff, xoff);
    memcpy(frame, prg_screen->chunky_buffer, sizeof frame);
    for (int row = 0; row < 16; ++row) for (int column = 0; column < 16; ++column)
        assert(frame[(0x3c+row)*320+32+column] == prg_screen->palette[icon_pixel(12,column,row)]);
    core_disk_message = CORE_DISK_NONE; core_req.kind = CORE_REQ_NONE;
    _draw_it(yoff, xoff);
    /* The start world screen clears d_screen (as in the original view);
     * the diagnostic rows below y 200 stay. */
    assert(frame[2*320+2] == prg_screen->palette[0] && frame[2*320+2] != prg_screen->chunky_buffer[2*320+2]);
    assert(!memcmp(frame + 200*320, prg_screen->chunky_buffer + 200*320, 56*320*sizeof *frame));
    core_disk_message = CORE_DISK_WORLD; core_req.kind = CORE_REQ_WORLD;
    /* The file is back: OK reads record 0 and shows the world lines. */
    write_level_fixture(1, 1);
    disk_ok_click();
    assert(core_req.kind == CORE_REQ_WORLD && !core_disk_message && core_world.shown && !core_world.level &&
           (uint16_t)conq_08_seed == 0xabcd);
    world_ok_click(); assert(!core_req.kind && current_start.kind == START_ORIGINAL && in_conquest == 0);
    assert(!unlink(path));
    _closeall(); SDL_Quit();
}

/* The _end_game screen in the diagnostic view: the same requester, rows and
 * button over the map, the pointer drawn; its button continues. */
static void test_diagnostic_end_screen(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    write_font_fixture();
    assert(game_reset(0) && game_font_loaded);
    end_kill(1);
    draw_original(6, 6);
    static uint8_t original[sizeof game_pixels]; memcpy(original, game_pixels, sizeof original);
    diagnostic_view = 1; xoff = yoff = 0; mousex = 300; mousey = 230;
    _draw_it(yoff, xoff);
    static uint32_t frame[320*256]; memcpy(frame, prg_screen->chunky_buffer, sizeof frame);
    /* Every pixel of the 16,16,0x120,0xa6 box matches the original view. */
    for (int y = 16; y < 16+0xa6; ++y) for (int x = 16; x < 16+0x120; ++x)
        assert(frame[y*320+x] == prg_screen->palette[original[y*320+x]]);
    assert(SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE);   /* the drawn pointer */
    core_end_shown = 0; _draw_it(yoff, xoff); core_end_shown = 1;
    assert(frame[2*320+2] == prg_screen->chunky_buffer[2*320+2]);   /* map outside the box */
    int differs = 0;
    for (int x = 32; x < 300; ++x) differs |= frame[32*320+x] != prg_screen->chunky_buffer[32*320+x];
    assert(differs);
    end_button_click();
    assert(!core_end_shown && !core_result && current_start.kind == START_SYNTHETIC);
    diagnostic_view = 0;
    copy_real_font();
    _closeall(); SDL_Quit();
}

/* Channel ch plays drumkit entry `sound` from its start (one-shot request). */
static int sound_playing(int ch, int sound)
{
    AudioWrite w;
    if (!audio_current(ch,&w)) return 0;
    const AudioKit *d = &core_bank.kit[sound];
    return w.data == core_bank.samples + core_bank.sams[d->sample].offset &&
           w.length == d->length && w.period == d->period && w.volume == d->volume && w.cycles == 1;
}

/* Sound triggers against the extracted gmusic1 (asm:19620-19672,20307-20518). */
/* The launcher's $42 word becomes _start_of_game without arguments (prg
 * asm:94-96): 1 the default (custom landscape 0), 2 the world screen, 3 the tutorial
 * (_start_game asm:13975-13999). Arguments always win (CLI bypass). */
static void test_launcher_start(void)
{
    char *arguments[] = {"populous", "--conquest", NULL};
    char saved[4096]; snprintf(saved, sizeof saved, "%s", getenv("POPULOUS_DATA_DIR"));
    char raw[4160]; snprintf(raw, sizeof raw, "%s/raw", POPULOUS_DEFAULT_ASSET_DIR);
    assert(!setenv("POPULOUS_DATA_DIR", raw, 1));   /* the world screen reads level.dat */
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    g_argv = arguments;
    for (int run = 0; run < 5; ++run) {
        g_argc = run < 3 ? 1 : 2;
        g_start_of_game = run < 3 ? run + 1 : 3;
        if (run == 4) { arguments[1] = "--tutorial"; g_start_of_game = 0; }
        ensure_prg_screen();
        SDL_Event quit = {.type = SDL_KEYDOWN};
        quit.key.keysym.sym = SDLK_ESCAPE;
        assert(SDL_PushEvent(&quit) == 1);
        prg_main();
        assert(prg_exit_status() == 0 && prg_screen == NULL);
        int world = run == 1 || run == 3, tutorial = run == 2 || run == 4;
        assert(requested_world == world);
        assert(current_start.kind == (tutorial ? START_TUTORIAL : world ? START_ORIGINAL : START_CUSTOM));
        if (world) assert(current_start.number == 0 && core_req.kind == CORE_REQ_WORLD);
        /* 1: _setup_display(0,1) on the boot data, custom landscape 0. */
        if (!run) assert(in_conquest == -1 && !start_seed && !ground_in && game_mode == 0x10 && no_peeps == 4);
        if (!tutorial) continue;
        /* seed 0x69bc, _tutorial_game_mode, _pause, LAB_5188C mana, the
         * copied _stats powers/speeds and conq_06/07 people (score 15*10). */
        assert(in_conquest == -1 && start_seed == 0x69bc && game_mode == 1 && pause == 1);
        assert(A4_LONG(0x9a02) == 10000 && A4_LONG(0xae72) == 150 && no_peeps == 18);
        assert(A4_WORD(0x82c4) == 0x3f && A4_WORD(0x82c4 + 46) == 3);
        assert(A4_WORD(0x82c2) == 10 && A4_WORD(0x82c6 + 46) == 10 && A4_WORD(0x82be + 46) == 1);
        assert(!core_conquest_game());
    }
    char *tutorial[] = {"populous", "--tutorial", NULL};
    StartSelection selected;
    assert(parse_start(2, tutorial, &selected) && selected.kind == START_TUTORIAL && !requested_world);
    assert(!game_reset_start((StartSelection){START_TUTORIAL, 1}));
    g_argc = 0; g_argv = NULL; g_start_of_game = 0;
    assert(!setenv("POPULOUS_DATA_DIR", saved, 1));
    SDL_Quit();
}

static void test_sound(void)
{
    char saved[4096] = ""; const char *old = getenv("POPULOUS_ASSET_DIR");
    if (old) snprintf(saved,sizeof saved,"%s",old);
    assert(!setenv("POPULOUS_ASSET_DIR",POPULOUS_DEFAULT_ASSET_DIR,1));
    audio_close();   /* no device thread: the mixer only moves when asked */
    int loaded = core_load_sound("gmusic1") == 0;
    if (old) setenv("POPULOUS_ASSET_DIR",saved,1); else unsetenv("POPULOUS_ASSET_DIR");
    if (!loaded) { puts("sound: extracted gmusic1 absent, skipped"); return; }
    assert(no_sounds == 104 && seqlen == 8 && !curseq && !curbeat && !curtime);
    for (int ch = 0; ch < 4; ++ch) assert(!Soundpri(ch) && !audio_busy(ch));

    /* Every _effect id (powers asm:4075-8529, battle/fire asm:3434-3454)
     * plays its own drumkit entry on channels 2 and 3, delays music to 40. */
    static const int effects[][3] = {          /* id, period, sample */
        {0x42,654,3},{0x43,701,4},{0x44,715,4},{0x45,714,5},{0x46,480,6},{0x49,3367,8},
        {0x4a,759,9},{0x4b,1704,10},{0x4c,357,11},{0x4d,952,1}};
    music_off = 0; effect_off = 1; tempo = 0;
    for (size_t k = 0; k < sizeof effects/sizeof effects[0]; ++k) {
        core_kill_effect(0,4); Soundpri(2) = Soundpri(3) = 0;
        effect = (int16_t)effects[k][0]; music = 10;
        core_vbi_audio();
        assert(!effect && music == 0x28 && !audio_busy(0) && !audio_busy(1));
        for (int ch = 2; ch < 4; ++ch) {
            assert(sound_playing(ch,effects[k][0]));
            assert(core_bank.kit[effects[k][0]].period == effects[k][1]);
            assert(core_bank.kit[effects[k][0]].sample == effects[k][2]);
            assert(Soundpri(ch) == core_bank.kit[effects[k][0]].volume);
        }
    }
    /* A cast end to end: _do_war's 0x49 (asm:8529). */
    core_kill_effect(0,4); Soundpri(2) = Soundpri(3) = 0;
    no_peeps = 0; pause = 0; core_result = 0; A4_WORD(0xae80) = 0;
    A4_LONG(0x84a6) = 1; A4_LONG(0x9a02) = 1; A4_WORD(0x82c4) = 0x100;
    assert(core_war(0) && effect == 0x49);
    core_vbi_audio();
    assert(sound_playing(2,0x49) && sound_playing(3,0x49));
    A4_WORD(0xae80) = 0;

    /* The quake rumble queues its repeat request: offset sams.+2:d+6 = 5. */
    core_kill_effect(0,4); Soundpri(2) = 0;
    assert(core_PlaySound(0x4c,2) && audio_current(2,NULL) == 2);
    /* Priority: a busy channel keeps against a lower d+4, not an equal one. */
    core_kill_effect(0,4); Soundpri(2) = 0;
    assert(core_PlaySound(0x4a,2) && sound_playing(2,0x4a));          /* 64 */
    assert(!core_PlaySound(0x43,2) && sound_playing(2,0x4a));         /* 51 */
    assert(core_PlaySound(0x44,2) && sound_playing(2,0x44));          /* 64 */
    core_kill_effect(2,3);
    assert(core_PlaySound(0x43,2) && sound_playing(2,0x43) && Soundpri(2) == 51);
    /* Unsigned bounds: negative notes and channels above 3 are ignored. */
    assert(!core_PlaySound(-1,0) && !core_PlaySound(104,0) && !core_PlaySound(0x42,4));

    /* _effect_off == 0 mutes: nothing plays and _effect stays pending. */
    core_kill_effect(0,4);
    effect_off = 0; effect = 0x4a; music = 10; music_now = 3;
    core_vbi_audio();
    for (int ch = 0; ch < 4; ++ch) assert(!audio_busy(ch));
    assert(effect == 0x4a && music_now == 3 && music == 10);
    effect = 0;

    /* Heartbeat: 0x48 at _tempo-_beat_two, 0x47 at _tempo, channel 0. */
    effect_off = 1; tempo = 9; beat_two = 3; tempo_now = 5; Soundpri(0) = 0;
    core_vbi_audio();
    assert(tempo_now == 6 && sound_playing(0,0x48));
    core_kill_effect(0,1);
    core_vbi_audio(); core_vbi_audio();
    assert(tempo_now == 8 && !audio_busy(0));
    core_vbi_audio();
    assert(tempo_now == 0 && sound_playing(0,0x47));
    sound_in = 1; core_kill_effect(0,4); tempo_now = 8;
    core_vbi_audio();
    assert(tempo_now == 8 && !audio_busy(0));
    sound_in = 0;

    /* _tempo from enemy/own population (asm:564-605) and the _toggle flip. */
    player = 0; core_good_pop(0) = 100; core_good_pop(1) = 300; toggle = 0; tempo_now = 60;
    core_tempo_pass();
    assert(tempo == 44 && beat_two == 14 && tempo_now == 44 && toggle == 1);
    core_good_pop(1) = 10000; core_tempo_pass();
    assert(tempo == 7 && beat_two == 2 && toggle == 0);
    player = 1; core_tempo_pass();
    assert(tempo == 0x32 && beat_two == 16);
    core_good_pop(1) = 0; core_tempo_pass();
    assert(tempo == 0 && beat_two == 1 && tempo_now == 0);
    player = 0;

    /* Music: a beat every _music frames; measure 2 beat 0 is 'A' on track 0. */
    core_kill_effect(0,4); for (int ch = 0; ch < 4; ++ch) Soundpri(ch) = 0;
    music_off = 1; effect_off = 0; music = 10; music_now = 8;
    core_vbi_audio();
    assert(music_now == 9 && !audio_busy(0));
    core_vbi_audio();
    assert(!music_now && music == 10 && curbeat == 1 && sound_playing(0,0));
    assert(audio_current(0,NULL) == 2 && !audio_busy(1) && !audio_busy(2) && !audio_busy(3));
    /* 64 beats end measure 2; sequence entry 0 repeats once (curtime 1). */
    for (int beat = 1; beat < 64; ++beat) { music_now = 9; core_vbi_audio(); }
    assert(!curbeat && curtime == 1 && curseq == 1 && core_curmeas == 3u*256);
    music_off = 0;

    /* Toolbar: switching sound on stops all channels (asm:1744-1781). */
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    toolbar_fixture(); music_off = effect_off = 0;
    assert(core_PlaySound(0x47,0) && core_PlaySound(0x4a,2));
    system_click(0,4,SDL_BUTTON_LEFT);
    assert(effect_off == 1 && !audio_busy(0) && !audio_busy(2));
    assert(core_PlaySound(0x4a,2)); Soundpri(2) = 0;
    system_click(0,4,SDL_BUTTON_LEFT);
    assert(!effect_off && audio_busy(2));      /* switching off keeps playing */
    system_click(0,3,SDL_BUTTON_LEFT);
    assert(music_off == 1 && !audio_busy(2));

    /* _move_sprite's flags reach _effect via _move_peeps (asm:3434-3454). */
    A4_WORD(0x82be) = A4_WORD(0x82be + 46) = 1; no_peeps = 0; pause = 0;
    effect = 0; last_fire = 0; toggle = 1; sound_effect = 8;
    _move_peeps(); assert(effect == 0x44 && !last_fire);
    effect = 0; sound_effect = 0x80;
    _move_peeps(); assert(effect == 0x46 && last_fire == 3);
    effect = 0; _move_peeps(); assert(!effect && last_fire == 3);
    sound_effect = 0; _move_peeps(); assert(last_fire == 2);
    effect = last_fire = 0; music_off = effect_off = 0;
    core_free_all_sounds();
    assert(!core_PlaySound(0x4a,2) && !audio_busy(2));
    SDL_Quit();
}

/* Four _waittof per loop pass (asm:14740-14743), then the mouth and the next pass. */
static void lord_pass(void) { for (int v = 0; v < 4; ++v) core_lord_vblank(); }
/* Let channel 2's word finish in the silent mixer. */
static void lord_drain(void)
{
    static int16_t out[2*4096];
    for (int k = 0; k < 64 && audio_busy(2); ++k) audio_mix(out,4096,8000);
    assert(!audio_busy(2));
}
static int lord_pixel(int x, int y) { return core_lord.screen[y*320+x]; }
static uint32_t lord_argb(int i)
{
    uint16_t w = lord_fixture_rgb(i);
    return 0xff000000u | (uint32_t)((w>>8)&15)*17 << 16 | (uint32_t)((w>>4)&15)*17 << 8 | (uint32_t)(w&15)*17;
}

/* _won_conquest lord screen asm:14503-14765: _read_lord/_read_mouth,
 * the _con_text lines, _draw_mouth and the gwords speech on channel 2. */
static void test_lord_screen(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    write_level_fixture(1, 1);
    write_font_fixture();
    int sounds = write_lord_fixture();
    SDL_Event key = {.type = SDL_KEYDOWN}; key.key.keysym.sym = SDLK_x;
    assert(game_reset_start((StartSelection){START_ORIGINAL, 2}));
    audio_close();   /* the mixer moves only in lord_drain */
    end_kill(1);
    int finished, next = core_next_level(10, (int32_t)A4_LONG(0xae72), &finished);
    char code[16]; core_code(code, core_code_rand(next));
    int16_t was_music = music_off, was_effect = effect_off;
    game_handle_event(&key);
    assert(core_lord.shown && !core_req.kind && !core_lord.finished && core_lord.rank == next/250 && core_lord.speak_step == 1);
    assert(!music_off && !effect_off && left_button == 2 && A4_WORD(0xae7e) == next && !strcmp(core_message, code));
    assert(seed == core_code_rand(10));   /* asm:14518: _seed = old world, then ___newrand */
    if (sounds) assert(no_sounds == 104 && !audio_busy(2));
    /* Picture and palette: plane-major colour (x/8+y)&15 away from text and mouth. */
    for (int y = 0; y < 0xb2; ++y) for (int x = 0; x < 320; ++x)
        if (!(x >= 144 && x < 192 && y >= 0x87 && y < 0x87+35)) assert(lord_pixel(x,y) == ((x/8 + y) & 15));
    for (int i = 0; i < 16; ++i) assert(core_lord.palette[i] == lord_fixture_rgb(i));
    /* The lines at x 8, y 0xb2/0xba (asm:14531-14582): "WELL DONE <rank> YOU CONQUERED " and
     * "<old world> NOW BATTLE AT <next>", over the picture as by ___text. */
    static uint8_t expected[320*200];
    for (int pixel = 0; pixel < 320*200; ++pixel) expected[pixel] = (uint8_t)((pixel%320/8 + pixel/320) & 15);
    char line[2][48], old[16]; core_code(old, core_code_rand(10));
    snprintf(line[1], 48, "%s NOW BATTLE AT %s", old, code);
    snprintf(line[0], 48, "WELL DONE %s YOU CONQUERED ", "MORTAL");
    if (next/250 == 0) {
        amiga_text(expected, 8, 0xb2, line[0]); amiga_text(expected, 8, 0xba, line[1]);
        assert(!memcmp(core_lord.screen + 0xb2*320, expected + 0xb2*320, 16*320));
        uint8_t plain[16*320];
        for (int pixel = 0; pixel < 16*320; ++pixel) plain[pixel] = (uint8_t)((pixel%320/8 + 0xb2 + pixel/320) & 15);
        assert(memcmp(core_lord.screen + 0xb2*320, plain, sizeof plain));
    }
    /* The render: lord _tcmap for the picture, gameplay colours kept for the pointer. */
    game_render();
    assert(prg_screen->chunky_buffer[20*320+300] == lord_argb(9) && prg_screen->chunky_buffer[0] == lord_argb(0));
    for (int i = 0; i < 16; ++i) { uint16_t w = graphics_palette[i];
        assert(prg_screen->palette[i] == (0xff000000u | (uint32_t)((w>>8)&15)*17 << 16 | (uint32_t)((w>>4)&15)*17 << 8 | (uint32_t)(w&15)*17)); }
    /* No simulation while shown. */
    uint8_t stopped[sizeof prg_a4_area]; memcpy(stopped, prg_a4_area, sizeof stopped);
    game_tick(); assert(!memcmp(stopped, prg_a4_area, sizeof stopped));
    /* Passes 1..9 idle at frame 0 (mouth frame 0 = colour 1 at byte 0x12, y 0x87). */
    core_lord_vblank(); core_lord_vblank(); core_lord_vblank();
    assert(lord_pixel(144,0x87) == ((18 + 0x87) & 15));
    core_lord_vblank();
    assert(lord_pixel(144,0x87) == 1 && lord_pixel(191,0x87+34) == 1 && lord_pixel(192,0x87) == ((24+0x87)&15) && core_lord.speak_step == 2);
    while (core_lord.speak_step < 10) { lord_pass(); assert(lord_pixel(150,0x90) == 1 && core_lord.mouth_frame == 0); }
    lord_pass();   /* c 10: word 0 on channel 2 unconditionally */
    assert(core_lord.speak_step == 11);
    if (!sounds) { puts("lord: extracted gwords absent, speech skipped"); goto leave; }
    assert(sound_playing(2,0) && !audio_busy(0) && !audio_busy(1) && !audio_busy(3));
    /* While channel 2 talks the mouth bounces 0..4 (asm:14748-14757). */
    static const int frames[] = {0,1,2,3,4,3,2,1,0,1};
    for (size_t k = 0; k < sizeof frames/sizeof frames[0]; ++k) {
        lord_pass();
        assert(lord_pixel(160,0x90) == frames[k]+1 && core_lord.speak_step == 11);
    }
    /* Then _words[rank*2], _words[rank*2+1] when channel 2 is idle (asm:14670-14705). */
    lord_drain(); int f = core_lord.mouth_frame;
    lord_pass();
    assert(core_lord.mouth_frame == f && core_lord.speak_step == 12);
    int16_t first = core_words[next/250*2], second = core_words[next/250*2+1];
    assert(next/250 != 0 || (first == 9 && second == -1));
    assert(sound_playing(2,first));
    lord_drain(); lord_pass();
    assert(core_lord.speak_step == 13 && (second < 0 ? !audio_busy(2) : sound_playing(2,second)));
    lord_drain();
    lord_pass();   /* draws the last speech frame, then resets -108(A5) to 0 */
    for (int k = 0; k < 237; ++k) { lord_pass(); assert(lord_pixel(160,0x90) == 1 && !audio_busy(2)); }
    assert(core_lord.speak_step == 251); lord_pass(); assert(core_lord.speak_step == 1);   /* > 250: again from 0 */
leave:
    /* The click: gmusic1 back, sound switches restored, _show_world with the next world. */
    screen_click(10,10,SDL_BUTTON_LEFT);
    assert(!core_lord.shown && core_req.kind == CORE_REQ_WORLD && core_world.origin == CORE_WORLD_WON &&
           !strcmp(core_message, code) && core_world.level == next && left_button == 2);
    assert(music_off == was_music && effect_off == was_effect);
    for (int ch = 0; ch < 4; ++ch) assert(!audio_busy(ch));
    if (sounds) assert(no_sounds == 104 && seqlen == 8);
    world_ok_click(); assert(!core_req.kind && A4_WORD(0xae7e) == next && current_start.number == next/5);

    /* The last world (finished, asm:14482-14491): "CONQUERED EVIL", _end_words
     * 0,1,3,5,6 at passes 10..14, mouth frame 5 at byte 0x10, y 0x4b at pass 16. */
    assert(game_reset_start((StartSelection){START_ORIGINAL, 494}));
    end_kill(1); game_handle_event(&key);
    assert(core_lord.shown && core_lord.finished && !A4_WORD(0xae7e) && !strcmp(core_message, "SHISODING") && !seed);
    memcpy(expected, core_lord.screen, sizeof expected);
    for (int pixel = 0xb2*320; pixel < 0xc2*320; ++pixel) expected[pixel] = (uint8_t)((pixel%320/8 + pixel/320) & 15);
    amiga_text(expected, 8, 0xb2, "WELL DONE YOU HAVE CONQUERED EVIL");
    amiga_text(expected, 8, 0xba, "THE BATTLE IS OVER BUT TRY SHISODING");
    assert(!memcmp(core_lord.screen, expected, sizeof expected));
    while (core_lord.speak_step < 10) lord_pass();
    for (int w = 0; w < 5 && sounds; ++w) {
        lord_pass(); assert(core_lord.speak_step == 11+w && sound_playing(2,core_end_words[w]));
        if (w < 4) { lord_pass(); assert(core_lord.speak_step == 11+w); }   /* busy: waits (c 15 no longer does) */
        lord_drain();
    }
    if (sounds) {
        assert(lord_pixel(128,0x4b) == ((16+0x4b)&15));
        lord_pass(); assert(core_lord.speak_step == 16 && lord_pixel(128,0x4b) == ((16+0x4b)&15));
        lord_pass(); assert(core_lord.speak_step == 17 && lord_pixel(128,0x4b) == 6 && lord_pixel(175,0x4b+34) == 6);
        while (core_lord.speak_step <= 200) lord_pass();
        lord_pass(); assert(core_lord.speak_step == 1);
    }
    screen_click(10,10,SDL_BUTTON_LEFT);
    assert(core_req.kind == CORE_REQ_WORLD && !core_world.level); world_ok_click();

    /* _read_lord fails: _level_number back, empty _message (asm:14762-14765),
     * then _show_world directly ("GENESIS" for the empty name). */
    char path[512]; snprintf(path, sizeof path, "%s/lord.pic", test_data_directory); assert(!unlink(path));
    assert(game_reset_start((StartSelection){START_ORIGINAL, 2}));
    end_kill(1); game_handle_event(&key);
    assert(!core_lord.shown && core_req.kind == CORE_REQ_WORLD && A4_WORD(0xae7e) == 10 &&
           !strcmp(core_message, "GENESIS") && music_off == was_music && effect_off == was_effect);
    world_ok_click();
    /* A short lord.pic counts as failed too (port: no stale w_screen). */
    FILE *file = fopen(path, "wb"); assert(file && fwrite(path, 1, 100, file) == 100 && !fclose(file));
    assert(game_reset_start((StartSelection){START_ORIGINAL, 2}));
    end_kill(1); game_handle_event(&key);
    assert(!core_lord.shown && core_req.kind == CORE_REQ_WORLD && A4_WORD(0xae7e) == 10);
    world_ok_click();
    /* Without mouths.pic the mouth is not drawn. */
    write_lord_fixture();
    snprintf(path, sizeof path, "%s/mouths.pic", test_data_directory); assert(!unlink(path));
    assert(game_reset_start((StartSelection){START_ORIGINAL, 2}));
    end_kill(1); game_handle_event(&key);
    assert(core_lord.shown && !core_lord.mouths_loaded);
    lord_pass(); assert(lord_pixel(144,0x87) == ((18 + 0x87) & 15));
    screen_click(10,10,SDL_BUTTON_LEFT); world_ok_click();
    remove_lord_fixture();
    /* Extracted data: mouth frame 0 is the picture's own closed mouth at
     * _draw_mouth's byte 0x12, y 0x87 (layout check of both loaders). */
    char saved[4096]; snprintf(saved, sizeof saved, "%s", getenv("POPULOUS_DATA_DIR"));
    char raw[4160]; snprintf(raw, sizeof raw, "%s/raw", POPULOUS_DEFAULT_ASSET_DIR);
    assert(!setenv("POPULOUS_DATA_DIR", raw, 1));
    if (core_read_lord()) {
        core_read_mouth(); assert(core_lord.mouths_loaded);
        assert(core_lord.palette[0] == 0x000);
        memcpy(expected, core_lord.screen, sizeof expected);
        core_draw_mouth(0x12,0x87,0x23,0); assert(!memcmp(expected, core_lord.screen, sizeof expected));
        core_draw_mouth(0x12,0x87,0x23,4); assert(memcmp(expected, core_lord.screen, sizeof expected));
    } else puts("lord: extracted lord.pic absent, layout check skipped");
    assert(!setenv("POPULOUS_DATA_DIR", saved, 1));
    snprintf(path, sizeof path, "%s/level.dat", test_data_directory); assert(!unlink(path));
    copy_real_font();
    _closeall(); SDL_Quit();
}

/* _paint_map (editor/free mode): each power prologue skips the mana, _war,
 * _pause and permission gates and the payment (asm:7104-7206, 7569, 7865,
 * 8048, 8297, 8404, 8484); _do_magnet has no bypass. Cleared, the very same
 * state is rejected without any A4 change. */
static void paint_starve(int owner)
{
    A4_LONG(0x9a02+owner*16)=0; A4_WORD(0x82c4+owner*46)=0;
    pause=1; A4_WORD(0xae80)=1;
}
static void test_paint_map_powers(void)
{
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint) {
        uint8_t before[sizeof prg_a4_area];
        /* Quake: score stays (asm:7899-7904). */
        quake_fixture(1); paint_starve(owner); A4_LONG(0xae72)=0; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_quake(owner,20,30,quake_random)==paint);
        if (paint) assert(core_mana(owner)==0 && A4_LONG(0xae72)==(owner==player ? 25u : 0u) && A4_WORD(0x2fac)==76);
        else assert(a4_same(before) && !quake_draws);
        /* Volcano: score stays (asm:8077-8083). */
        volcano_fixture(0); paint_starve(owner); A4_LONG(0xae72)=0; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_volcano(owner,20,30,volcano_random)==paint);
        if (paint) assert(core_mana(owner)==0 && A4_LONG(0xae72)==(owner==player ? 100u : 0u) && volcano_draws==229);
        else assert(a4_same(before) && !volcano_draws);
        /* Swamp: score stays (asm:8326-8332). */
        swamp_fixture(); paint_starve(owner); A4_LONG(0xae72)=0; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_swamp(owner,20,20,swamp_random)==paint);
        if (paint) assert(core_mana(owner)==0 && A4_LONG(0xae72)==(owner==player ? 50u : 0u) && swamp_draws==60);
        else assert(a4_same(before) && !swamp_draws);
        /* Flood: score stays (asm:7598-7604). */
        flood_fixture(2); paint_starve(owner); A4_LONG(0xae72)=0; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_flood(owner)==paint);
        if (paint) assert(core_mana(owner)==0 && A4_LONG(0xae72)==(owner==player ? 250u : 0u) && core_vertex(10,10)==1);
        else assert(a4_same(before));
        /* Knight: still needs a leader (asm:8394-8402), score stays. */
        knight_fixture(owner); paint_starve(owner); A4_LONG(0xae72)=0; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_knight(owner)==paint);
        if (paint) assert(core_mana(owner)==0 && !core_leader(owner) && A4_LONG(0xae72)==(owner==player ? 150u : 0u));
        else assert(a4_same(before));
        knight_fixture(owner); paint_starve(owner); paint_map=1; core_leader(owner)=0;
        memcpy(before,prg_a4_area,sizeof before);
        assert(!core_knight(owner) && a4_same(before));
        /* Armageddon: no payment and no score (asm:8484-8516). */
        armageddon_fixture(); paint_starve(owner); A4_WORD(0xae80)=0; A4_LONG(0xae72)=0; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_war(owner)==paint);
        if (paint) assert(core_mana(owner)==0 && !A4_LONG(0xae72) && A4_WORD(0xae80)==1 && A4_WORD(0x2fac)==73);
        else assert(a4_same(before));
        /* Raise/lower: admission and cost skipped (asm:7104-7126,7189-7219). */
        quake_fixture(1); A4_LONG(0x848a)=10; A4_LONG(0x9a02+owner*16)=3; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_raw_raise(owner,20,30)==paint);
        if (paint) assert(core_vertex(20,30)==2 && core_mana(owner)==3);
        else assert(a4_same(before));
        quake_fixture(1); A4_LONG(0x848a)=10; A4_LONG(0x9a02+owner*16)=3; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        core_do_lower_point(owner,20,30);
        if (paint) assert(core_vertex(20,30)==0 && core_mana(owner)==3);
        else { assert(core_vertex(20,30)==1 && core_mana(owner)==3); }
        /* Admitted with paint clear, the original price is still paid. */
        quake_fixture(1); A4_LONG(0x848a)=10; A4_LONG(0x9a02+owner*16)=100; paint_map=0;
        assert(core_raw_raise(owner,20,30) && core_mana(owner)==100-(int32_t)(A4_WORD(0x2f88)*4+10));
        /* _do_magnet (asm:6769-6800) has no editor bypass. */
        armageddon_fixture(); A4_LONG(0x848e)=100; A4_LONG(0x9a02+owner*16)=99; core_leader(owner)=1;
        A4_WORD(0xae80)=0; paint_map=paint;
        memcpy(before,prg_a4_area,sizeof before);
        assert(!core_do_magnet(owner,5,5) && a4_same(before));
    }
    paint_map=0;
}

static void paint_key(SDL_Keycode sym, int shift)
{
    SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=sym;
    key.key.keysym.mod=shift ? KMOD_LSHIFT : KMOD_NONE;
    game_handle_event(&key);
}
/* _animate asm:676-738 freezes people; _paint_the_map asm:15123-15274 posts
 * the editor opcodes that _get_message (asm:18017-18183) and _do_action
 * y=9..13 (asm:18943-18999) execute. */
static void test_paint_map_editor(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    /* No _move_peeps: _game_turn, peeps and the outcome stay put. */
    for (int paint=0;paint<2;++paint) {
        assert(game_reset(0)); paint_map=paint;
        int16_t turn=game_turn; uint8_t people[208*22]; memcpy(people,peeps,sizeof people);
        for (int tick=0;tick<8;++tick) game_tick();
        assert(game_turn==(int16_t)(turn+(paint ? 0 : 8)));
        if (paint) assert(!memcmp(people,peeps,sizeof people));
        assert(game_reset(0)); paint_map=paint;
        for (int person=0;person<no_peeps;++person) set_peep_word(person,4,0);
        no_peeps=0; game_tick();
        assert(core_result==(paint ? 0 : 2));
    }
    /* Outside the editor the keys post nothing (digits keep the host tendency). */
    assert(game_reset(0)); paint_map=0; A4_WORD(0x3028)=10; A4_WORD(0x3026)=12;
    paint_key(SDLK_F1,0); paint_key(SDLK_F8,0); paint_key(SDLK_DELETE,0);
    assert(!A4_DISP(0x82b6)[0] && !A4_DISP(0x82b6)[1] && !A4_DISP(0x82b6)[2]);
    paint_key(SDLK_2,0); assert(core_tendency(0)==2 && !A4_DISP(0x82b6)[0]);
    /* F1/F2 men, Shift leaders, at _cur_x/_cur_y; repeats are ignored. */
    assert(game_reset(0)); paint_map=1; A4_WORD(0x825c)=0x32; A4_WORD(0x825e)=0x2f;
    A4_WORD(0x3028)=10; A4_WORD(0x3026)=12;
    int cell=12*64+10, count=no_peeps;
    map_who[cell]=0; map_alt[cell]=2; map_blk[cell]=15; map_bk2[cell]=0;
    paint_key(SDLK_F1,0);
    assert(A4_DISP(0x82b6)[0]==7 && A4_DISP(0x82b6)[1]==10 && A4_DISP(0x82b6)[2]==12);
    game_tick();
    assert(no_peeps==count+1 && map_who[cell]==count+1 && peep_owner(count)==0);
    assert(peeps[count*22]==2 && peep_word(count,4)==45 && peep_word(count,8)==cell && !peep_word(count,12));
    { SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_F2; key.key.repeat=1;
      game_handle_event(&key); assert(!A4_DISP(0x82b6)[0]); }
    paint_key(SDLK_F2,0); game_tick(); assert(no_peeps==count+2 && peep_owner(count+1)==1);
    /* Without a leader Shift-F1 takes a new slot; with one the leader's
     * slot is reused (_zero_population), the old square released. */
    core_leader(0)=0; A4_WORD(0x3028)=11;
    paint_key(SDLK_F1,1); assert(A4_DISP(0x82b6)[0]==9); game_tick();
    int leader=core_leader(0);
    assert(no_peeps==count+3 && leader==count+3 && peep_word(leader-1,8)==12*64+11);
    A4_WORD(0x3028)=13; paint_key(SDLK_F1,1); game_tick();
    assert(no_peeps==count+3 && core_leader(0)==leader && peep_word(leader-1,8)==12*64+13);
    assert(map_who[12*64+13]==leader && map_who[12*64+11]!=leader);
    A4_WORD(0x3028)=14; core_leader(1)=0;
    paint_key(SDLK_F2,1); assert(A4_DISP(0x82b6)[0]==10); game_tick();
    assert(core_leader(1)==count+4 && peep_word(count+3,8)==12*64+14 && peep_owner(count+3)==1);
    /* F3 trees, F4 rocks cycle from _last_tree/_last_rock; not at height 7. */
    A4_WORD(0x3028)=10;
    paint_key(SDLK_F3,0); game_tick(); assert(map_bk2[cell]==0x32);
    paint_key(SDLK_F3,0); game_tick(); assert(map_bk2[cell]==0x33);
    paint_key(SDLK_F3,0); game_tick(); paint_key(SDLK_F3,0); game_tick();
    assert(map_bk2[cell]==0x32 && A4_WORD(0x825c)==0x32);
    paint_key(SDLK_F4,0); game_tick(); assert(map_blk[cell]==0x2f);
    paint_key(SDLK_F4,0); game_tick(); assert(map_blk[cell]==0x30 && A4_WORD(0x825e)==0x30);
    map_alt[cell+1]=7; map_bk2[cell+1]=0; A4_WORD(0x3028)=11;
    paint_key(SDLK_F3,0); game_tick(); assert(!map_bk2[cell+1]);
    /* F5: the rock becomes grass, objects and the occupant go. */
    A4_WORD(0x3028)=10; int occupant=map_who[cell]-1;
    assert(occupant>=0);
    paint_key(SDLK_F5,0); game_tick();
    assert(map_blk[cell]!=0x30 && !map_bk2[cell] && !map_who[cell] && peep_word(occupant,4)==0);
    /* F6/F7: x2+500 below 100000, Shift halves (DIVS). */
    A4_LONG(0x9a02)=1000; A4_LONG(0x9a12)=99999;
    paint_key(SDLK_F6,0); game_tick(); assert(A4_LONG(0x9a02)==2500);
    paint_key(SDLK_F6,1); game_tick(); assert(A4_LONG(0x9a02)==1250);
    paint_key(SDLK_F7,0); game_tick(); assert(A4_LONG(0x9a12)==200498);
    paint_key(SDLK_F7,0); game_tick(); assert(A4_LONG(0x9a12)==200498);
    A4_LONG(0x9a12)=(uint32_t)-7; paint_key(SDLK_F7,1); game_tick(); assert((int32_t)A4_LONG(0x9a12)==-3);
    /* F8 mirrors: both halves take the higher height, trees copy over. */
    for (int i=0;i<4225;++i) A4_WORD(0xae86+i*2)=1;   /* land: _make_map keeps objects */
    A4_WORD(0xae86+5*2)=4; A4_WORD(0xae86+(0x1080-7)*2)=3;
    memset(map_bk2,0,4096); map_bk2[100]=51; map_bk2[0xfff-200]=55; map_bk2[300]=42;
    paint_key(SDLK_F8,0); game_tick();
    assert(A4_WORD(0xae86+(0x1080-5)*2)==4 && A4_WORD(0xae86+5*2)==4);
    assert(A4_WORD(0xae86+7*2)==3 && A4_WORD(0xae86+(0x1080-7)*2)==3);
    assert(map_bk2[0xfff-100]==51 && map_bk2[200]==55 && !map_bk2[0xfff-300]);
    /* 1..4 load landscapes 0..3; 5..9 are not available in the port. */
    paint_key(SDLK_2,0); assert(A4_DISP(0x82b6)[0]==14 && A4_DISP(0x82b6)[1]==1 && A4_DISP(0x82b6)[2]==13);
    game_tick(); assert(ground_in==1 && game_assets);
    paint_key(SDLK_7,0); game_tick(); assert(ground_in==1);
    paint_key(SDLK_1,0); game_tick(); assert(ground_in==0);
    /* DEL clears the whole land and every person. */
    paint_key(SDLK_DELETE,0); game_tick();
    assert(no_peeps==0 && !core_leader(0) && !core_leader(1));
    for (int i=0;i<4225;++i) assert(!A4_WORD(0xae86+i*2));
    for (int i=0;i<4096;++i) assert(!map_who[i] && !map_blk[i] && !map_bk2[i]);
    paint_map=0; _closeall(); SDL_Quit();
}

/* Entry/exit: GAME SETUP PAINT MAP / PLAY GAME (asm:13044-13056), kept on
 * OK and restored on CANCEL (asm:13326); START GAME clears it (asm:12769). */
static void test_paint_map_options(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    toolbar_fixture(); in_conquest=-1; paint_map=0;
    core_game_options_open(); assert(core_rec_text(core_game_entry(4))[0]==(char)0x80);
    entry_click(core_game_entry(4),16,16); assert(paint_map==1);
    ok_click(); assert(!core_req.kind && paint_map==1);
    /* In the editor Q is free and the click sculpt ignores the build limits. */
    A4_LONG(0x9a02)=0; A4_WORD(0x82c4)=0; game_mode=GAME_MODE_NO_BUILD;
    xoff=20; yoff=30; memset(A4_DISP(0x82b6),0,3);
    paint_key(SDLK_q,0); assert(A4_WORD(0x2fac)==76 && core_mana(0)==0);
    for (int i=0;i<4225;++i) A4_WORD(0xae86+i*2)=1;
    SDL_SetWindowSize(prg_screen->window,640,512); iso_click_vertex(24,34,SDL_BUTTON_LEFT);
    assert(core_vertex(24,34)==2 && core_mana(0)==0);
    assert(A4_WORD(0x3028)==24 && A4_WORD(0x3026)==34);
    SDL_SetWindowSize(prg_screen->window,640,512); iso_click_vertex(24,34,SDL_BUTTON_RIGHT); assert(core_vertex(24,34)==1 && core_mana(0)==0);
    /* Reopened, PLAY GAME leaves the editor. */
    core_game_options_open(); assert(core_rec_text(core_game_entry(4))[0]==(char)0x81);
    entry_click(core_game_entry(3),16,16); assert(paint_map==0);
    ok_click(); assert(!core_req.kind && paint_map==0);
    A4_WORD(0x2fac)=0; paint_key(SDLK_q,0); assert(!A4_WORD(0x2fac));
    SDL_SetWindowSize(prg_screen->window,640,512); iso_click_vertex(24,34,SDL_BUTTON_LEFT); assert(core_vertex(24,34)==1);
    /* CANCEL restores the value it was opened with. */
    core_game_options_open(); entry_click(core_game_entry(4),16,16); assert(paint_map==1);
    cancel_click(); assert(!core_req.kind && paint_map==0);
    paint_map=1; core_game_options_open(); entry_click(core_game_entry(3),16,16);
    cancel_click(); assert(paint_map==1);
    /* The host P key is not an editor switch. */
    paint_map=0; paint_key(SDLK_p,0); assert(!paint_map);
    _closeall(); SDL_Quit();
}

int main(int argc, char **argv)
{
    /* Hermetic save drawer; test_save_load uses its own temporary one. */
    setenv("POPULOUS_SAVE_DIR", "/nonexistent-populous-saves", 1);
    /* Shake frames are checked in test_quake_shake; elsewhere skip the 20 ms vblanks. */
    core_quake_delay = no_quake_delay;
    if (argc==2 && !strcmp(argv[1],"--set-devil-magnet")) { test_set_devil_magnet(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--devil-effect")) { test_devil_effect(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--good-pop")) { test_good_pop(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--start-reset")) {
        prepare_rule_data(); test_start_reset(); cleanup_rule_data(); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--computer-effect")) { test_computer_effect(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--ai-level-parameters")) { test_ai_level_parameters(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--devil-magnet")) { test_set_devil_magnet(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--make-level")) { test_make_level(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--opcode-magnet")) { test_ai_opcode_magnet(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--opcode-action")) { test_ai_opcode_action(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--opcode-effects")) { test_ai_opcode_effects(); return 0; }

    if (argc==2 && !strcmp(argv[1],"--swamp-rescue")) {
        test_swamp_rescue(); puts("focused swamp rescue: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--magnet-rescue")) {
        test_magnet_rescue(); test_water_rescue(); test_war_terrain(); test_magnet();
        puts("focused magnet rescue: PASS"); return 0;
    }
    if (argc==3 && !strcmp(argv[1],"--rescue-every-tick")) {
        assert(getenv("POPULOUS_TEST_REAL_DATA"));
        if (!strcmp(argv[2],"isometric")) test_isometric_match();
        else { assert(!strcmp(argv[2],"events")); test_match_events(); }
        return 0;
    }
    /* Former default (cadence 1) and legacy (cadence 8) scripts, start 6001. */
    if (argc==2 && !strcmp(argv[1],"--rescue-legacy-every-tick")) {
        assert(getenv("POPULOUS_TEST_REAL_DATA"));
        test_isometric_match_cadence(1,6001,&late_rescue_defeat);
        test_match_events_cadence(1,6001,&late_rescue_defeat);
        puts("historical every-tick rescue match: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--rescue-legacy-match")) {
        assert(getenv("POPULOUS_TEST_REAL_DATA"));
        test_isometric_match_cadence(8,6001,&late_rescue_defeat);
        test_match_events_cadence(8,6001,&late_rescue_defeat);
        puts("historical rescue match cadences: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--water-rescue")) {
        test_water_rescue(); puts("focused water rescue: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--match-isometric")) {
        prepare_rule_data(); test_isometric_match(); cleanup_rule_data(); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--match-events")) {
        prepare_rule_data(); test_match_events(); cleanup_rule_data(); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--water-town")) {
        test_water_town(); test_water_loss(); test_town_capacity();
        puts("focused dry water town: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--town-capacity")) {
        test_town_capacity();
        puts("focused town capacity regression: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--town-split")) {
        test_town_split(); test_town_economy(); test_town_footprint();
        puts("focused ordinary town split: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--town-footprint")) {
        test_town_footprint(); test_town_economy(); test_war_state();
        puts("focused ordinary town footprint: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--town-economy")) {
        test_town_economy(); test_war_state();
        puts("focused ordinary town economy: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--water-remnant")) {
        test_water_remnant(); test_water_remnant_rescue();
        test_remnant_timer(); test_remnant_occupancy_and_state(); test_remnant_tick_and_water();
        test_water_wait(); test_water_celebration(); test_water_recovery(); test_water_loss();
        puts("focused dry water remnant: PASS (15360 state, 40 frame, 6 expiry, 144 edge, 96 gate/skip/tick, 120 mixed, 14 deferred)"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--walker-extras")) {
        test_walker_extras();
        puts("focused walker extras: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--phased-move")) {
        test_phased_move();
        puts("focused phased movement: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--collision-wait")) {
        test_collision_wait_mark(); test_collision_wait_gates(); test_collision_wait_tick();
        test_blocked_retry(); test_wait_boundaries(); test_wait_water_and_death();
        puts("focused collision-wait production: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--water-wait")) {
        test_water_wait(); test_water_wait_rescue();
        test_water_celebration(); test_water_recovery(); test_water_loss(); test_water_entry();
        test_blocked_entry(); test_blocked_retry(); test_wait_boundaries(); test_wait_water_and_death();
        test_war_state(); test_heading(); test_war_terrain_old_water(); test_war_terrain_order_and_death();
        puts("focused dry water waiting: PASS (9504 state, 1584 retry, 36 encounter, 108 gates, 64 mixed, 24 rescue)"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--water-celebration")) {
        test_water_celebration(); test_water_celebration_rescue();
        test_water_recovery(); test_water_recovery_rescue(); test_water_loss(); test_water_entry(); test_celebration();
        puts("focused dry water celebration: PASS (6720 state, 312 town, 72 gate/skip/tick, 12 rescue cases)"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--water-recovery")) {
        test_water_recovery(); test_water_recovery_rescue(); test_water_loss(); test_water_loss_rescue(); test_water_entry();
        puts("dry exact-18 water recovery tests passed"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--water-loss")) {
        test_water_loss(); test_water_loss_rescue(); test_water_entry();
        test_celebration_dispatch(); test_flood_delayed_hazards();
        test_war_terrain_old_water(); test_war_terrain_order_and_death();
        puts("focused already-wet loss: PASS (960 state cases, 131072 word cases, 128 edge cases)"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--water-entry")) {
        test_water_entry(); puts("focused newly submerged entry: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--battle-loss")) {
        test_battle_loss(); test_heading_town_casualty_controls(); test_battle_frame_deaths();
        puts("focused battle loss: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--battle-over")) {
        test_heading_battle(); test_battle_entry_town_reward();
        puts("focused battle completion: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--battle-entry")) {
        test_battle_entry_state(); test_battle_entry_wait_retry(); test_battle_entry_town_reward();
        test_heading_town(); test_battle_reinforcement(); test_blocked_reinforcement_retry();
        prepare_rule_data(); test_battle_frame_render_and_entry(); cleanup_rule_data();
        puts("focused battle entry: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--battle-frames")) {
        test_battle_frame_helper(); test_battle_frame_integration(); test_battle_frame_deaths();
        test_heading_battle();
        prepare_rule_data(); test_battle_frame_render_and_entry(); cleanup_rule_data();
        puts("focused battle frames: PASS"); return 0;
    }
    if (argc==3 && !strcmp(argv[1],"--knight")) { test_knight_original(argv[2]); return 0; }
    if (argc==2 && !strcmp(argv[1],"--knight")) {
        test_knight_input(); test_knight_guards(); test_knight_cast();
        prepare_rule_data(); test_knight_render(); test_knight_lifecycle(); test_knight_ordinary_actions(); cleanup_rule_data();
        puts("focused knight: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--special-sprites")) {
        prepare_rule_data(); test_special_sprites(); cleanup_rule_data();
        puts("focused special sprites: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--heading-town")) {
        test_heading_town(); puts("focused heading town completion: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--heading-battle")) {
        test_heading_battle(); puts("focused heading battle completion: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--celebration")) {
        test_celebration(); puts("focused celebration consumer: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--heading-navigation")) {
        test_heading_navigation();
        prepare_rule_data(); test_heading_seeded_encounter(); cleanup_rule_data();
        puts("focused heading navigation: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--heading")) {
        test_heading(); puts("focused heading/reference regression: PASS"); return 0;
    }
    if (argc==3 && !strcmp(argv[1],"--armageddon")) {
        test_armageddon_original(argv[2]); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--armageddon")) {
        test_armageddon_input_smoke(); test_armageddon_cast(); test_armageddon_delayed(); test_armageddon_islands();
        prepare_rule_data(); test_armageddon_locks(); test_armageddon_render(); cleanup_rule_data();
        puts("focused Armageddon: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--war-terrain")) {
        test_war_terrain(); puts("focused war terrain producers: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--war-water")) {
        test_war_terrain_old_water(); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--war-obstacle")) {
        test_war_terrain_obstacle(); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--terrain-mailbox")) {
        test_terrain_mailbox();
        prepare_rule_data(); test_terrain_defaults(); cleanup_rule_data();
        puts("focused terrain mailbox/defaults: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--magnet")) {
        test_magnet();
        puts("focused magnet regression: PASS");
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--war-state")) {
        test_war_state();
        puts("focused war-state regression: PASS");
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--volcano")) {
        test_volcano_input_smoke(); test_volcano_cast(); test_volcano_lava();
        prepare_rule_data(); test_volcano_input(); test_volcano_render(); cleanup_rule_data();
        puts("focused volcano regression: PASS");
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--flood")) {
        test_flood_input_smoke(); test_flood_cast(); test_flood_delayed_hazards();
        prepare_rule_data(); test_flood_input(); cleanup_rule_data();
        puts("focused flood regression: PASS");
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--lower")) { test_original_lower(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--quake-core")) { test_quake_cast(); return 0; }
    if (argc==2 && !strcmp(argv[1],"--quake")) {
        test_original_lower(); test_quake_cast();
        prepare_rule_data(); test_quake_input(); cleanup_rule_data();
        puts("focused quake regression: PASS");
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--swamp")) {
        test_swamp_cast();
        test_swamp_hazard();
        prepare_rule_data(); test_swamp_input(); test_swamp_render(); cleanup_rule_data();
        puts("focused swamp regression: PASS");
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "--levels")) {
        test_all_original_levels(argv[2]);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "--overlap")) {
        prepare_rule_data();
        if (!strcmp(argv[2], "placement")) test_original_placement();
        else if (!strcmp(argv[2], "encounter")) test_colocated_hostile_encounter();
        else if (!strcmp(argv[2], "town")) test_no_hidden_town();
        else if (!strcmp(argv[2], "death")) test_colocated_battle_death();
        else if (!strcmp(argv[2], "friendly")) test_friendly_merge_state();
        else if (!strcmp(argv[2], "friendly-reject")) test_friendly_merge_metadata_rejection();
        else if (!strcmp(argv[2], "friendly-dead")) test_friendly_merge_dead_target();
        else if (!strcmp(argv[2], "friendly-guards")) test_friendly_merge_helper_guards();
        else if (!strcmp(argv[2], "friendly-tick")) test_friendly_merge_tick_totals();
        else if (!strcmp(argv[2], "friendly-town")) test_friendly_merge_town_and_split();
        else if (!strcmp(argv[2], "reinforcement")) test_battle_reinforcement();
        else if (!strcmp(argv[2], "invalid-battle")) test_invalid_battle_blocks_reinforcement();
        else if (!strcmp(argv[2], "reinforcement-guards")) test_reinforcement_helper_guards();
        else if (!strcmp(argv[2], "reinforcement-tick")) test_reinforcement_tick_and_death();
        else if (!strcmp(argv[2], "reinforcement-retry")) test_blocked_reinforcement_retry();
        else assert(0);
        cleanup_rule_data();
        puts("focused overlap regression: PASS");
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "--waiting")) {
        if (!strcmp(argv[2], "entry")) test_blocked_entry();
        else if (!strcmp(argv[2], "retry")) test_blocked_retry();
        else if (!strcmp(argv[2], "boundaries")) test_wait_boundaries();
        else if (!strcmp(argv[2], "death")) test_wait_water_and_death();
        else assert(0);
        puts("focused waiting regression: PASS");
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--remnant")) {
        test_remnant_timer(); test_remnant_occupancy_and_state();
        test_remnant_tick_and_water(); test_remnant_collision();
        puts("focused remnant regression: PASS");
        return 0;
    }
    if (argc==3 && !strcmp(argv[1],"--remnant")) {
        assert(!strcmp(argv[2],"collision")); test_remnant_collision(); return 0;
    }
    assert(argc == 1);
    test_computer_ai();
    test_swamp_rescue();
    test_magnet_rescue();
    test_water_rescue();
    test_water_town();
    test_town_capacity();
    test_town_split();
    test_town_footprint(); test_town_economy();
    test_water_remnant(); test_water_remnant_rescue();
    test_water_wait(); test_water_wait_rescue();
    test_water_celebration(); test_water_celebration_rescue();
    test_water_recovery(); test_water_recovery_rescue(); test_water_loss(); test_water_loss_rescue();
    test_water_entry();
    test_battle_loss();
    test_battle_entry_state(); test_battle_entry_wait_retry(); test_battle_entry_town_reward();
    test_battle_frame_helper(); test_battle_frame_integration(); test_battle_frame_deaths();
    test_knight_input(); test_knight_guards(); test_knight_cast();
    test_heading_battle(); test_celebration();
    test_heading(); test_heading_navigation();
    test_armageddon_input_smoke(); test_armageddon_cast(); test_armageddon_delayed(); test_armageddon_islands();
    test_war_terrain();
    test_terrain_mailbox();
    test_paint_map_powers(); test_paint_map_editor(); test_paint_map_options();
    test_magnet();
    test_war_state();
    test_remnant_timer(); test_remnant_occupancy_and_state();
    test_remnant_tick_and_water(); test_remnant_collision();
    test_flood_input_smoke(); test_flood_cast(); test_flood_delayed_hazards();
    test_original_lower();
    test_quake_cast();
    test_quake_shake();
    test_volcano_input_smoke(); test_volcano_cast(); test_volcano_lava();
    test_swamp_cast();
    test_swamp_hazard();
    test_blocked_entry();
    test_blocked_retry();
    test_wait_boundaries();
    test_wait_water_and_death();
    test_collision_wait_mark(); test_collision_wait_gates(); test_collision_wait_tick();
    test_phased_move();
    test_walker_extras();
    test_original_selection();
    test_end_stats(); test_level_codes(); test_next_level(); test_amiga_text();
    test_pointer_hud_mana();
    test_original_generator();
    test_original_placement();
    prepare_rule_data();
test_knight_render(); test_knight_lifecycle(); test_knight_ordinary_actions();
    test_heading_seeded_encounter();
    test_armageddon_locks(); test_armageddon_render();
    test_terrain_defaults();
    test_volcano_input(); test_volcano_render();
    test_flood_input();
    test_quake_input();
    test_swamp_input();
    test_power_toolbar();
    test_toolbar_icons();
    test_swamp_render();
    test_original_reset();
    test_start_reset();
    test_end_transition();
    test_lord_screen();
    test_world_screen();
    test_insert_disk();
    test_diagnostic_end_screen();
    test_requesters();
    test_save_load();
    test_custom_game();
    test_original_graphics_output();
    test_toolbar_icon_highlight();
    test_query_and_shield();
    test_shield_icons();
    test_icon_flashes();
    test_special_sprites(); test_battle_frame_render_and_entry(); test_walker_interpolation();
    test_funny_objects();
    test_isometric_match();
    test_colocated_hostile_encounter();
    test_no_hidden_town();
    test_colocated_battle_death();
    test_friendly_merge_state();
    test_friendly_merge_metadata_rejection();
    test_friendly_merge_dead_target();
    test_friendly_merge_helper_guards();
    test_friendly_merge_tick_totals();
    test_friendly_merge_town_and_split();
    test_battle_reinforcement();
    test_invalid_battle_blocks_reinforcement();
    test_reinforcement_helper_guards();
    test_reinforcement_tick_and_death();
    test_blocked_reinforcement_retry();
    test_battle_casualties_and_rewards();
    test_near_town_sculpt();
    test_split_leader_and_occupancy();
    test_settlement_occupancy();
    test_sculpt_navigation_locality();
    test_load_failure_and_magnet();
    test_sculpt_projection_and_rejection();
    test_match_events();
    test_original_rules();
    test_memory();
    test_initial_world();
    test_cli();
    test_reset_and_input();
    test_lifecycle();
    test_sound();
    test_launcher_start();
    cleanup_rule_data();
    puts("game memory/start/render/input/lifecycle: PASS");
    return 0;
}
