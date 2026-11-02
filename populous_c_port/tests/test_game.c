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
        assert(memcmp(before, prg_a4_area, sizeof before) == 0);
        key.key.keysym.sym = SDLK_SPACE;
        assert(SDL_PushEvent(&key) == 1);
        ___keyboard();
        assert(pause);
        memcpy(before, prg_a4_area, sizeof before);
        game_tick();
        game_render();
        assert(memcmp(before, prg_a4_area, sizeof before) == 0);
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
    assert(memcmp(initial, prg_a4_area, sizeof initial) == 0);
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
    assert(memcmp(before,prg_a4_area,sizeof before)==0); /* HUD must not sculpt hidden terrain. */
    LAB_516B2=0; memcpy(before,prg_a4_area,sizeof before);
    assert(!core_sculpt(0,10,10,1));
    assert(memcmp(before,prg_a4_area,sizeof before)==0);
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
    assert(memcmp(before,prg_a4_area,sizeof before)==0);
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
        game_render(); assert(!memcmp(saved,prg_a4_area,sizeof saved));
    }
    /* Keep the existing eight-tick dispatch, not the original frame-101 handshake. */
    setup_colocated_walkers(0);
    for (int tick=0; tick<7; ++tick) _move_peeps();
    assert(!core_battles && peeps[0]==2 && peeps[22]==2);
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
        /* Even an empty/self representative cannot conceal an existing town.
         * A one-tile town keeps core_check_life positive at its own center. */
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
            assert(!(peeps[0]&1) && peeps[22]==1 && !core_settlements);
            assert(map_bk2[position]==35 && map_who[position]==representative);
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

static void setup_reinforcement(int newcomer, int owner, int colocated, int representative)
{
    memset(prg_a4_area,0,sizeof prg_a4_area);
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
    set_peep_word(newcomer,4,45); set_peep_word(newcomer,8,colocated ? 650 : 649);
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
        assert(peep_word(newcomer,8)==(colocated ? 650 : 649));
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
        uint8_t saved[sizeof prg_a4_area]; memcpy(saved,prg_a4_area,sizeof saved);
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
        assert(A4_LONG(0x99fe + owner*16)==135 && A4_LONG(0x99fe + (1-owner)*16)==90);
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
    memset(prg_a4_area,0,sizeof prg_a4_area);
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
     * index-dependent totals, lost references and unrelated occupancy cleanup. */
    const int flags[]={2,0x22,0x42,0x62,0x61,1};
    const int populations[][3]={{100,200,300},{16000,16000,32000},{32000,1,32000}};
    for (int source=0; source<2; ++source) for (int owner=0; owner<2; ++owner)
    for (unsigned state=0; state<sizeof flags/sizeof flags[0]; ++state)
    for (int variant=0; variant<3; ++variant) {
        setup_friendly_merge(source,owner);
        int target=1-source;
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
        uint8_t wanted[sizeof prg_a4_area]; memcpy(wanted,prg_a4_area,sizeof wanted);
        memcpy(prg_a4_area,expected,sizeof expected);
        core_move(source);
        assert(!memcmp(wanted,prg_a4_area,sizeof wanted));
        assert(!core_battles && !core_settlements);
    }
}

static void test_friendly_merge_metadata_rejection(void)
{
    for (int source=0; source<2; ++source) for (int owner=0; owner<2; ++owner)
    for (int byte=14; byte<18; ++byte) {
        setup_friendly_merge(source,owner);
        peeps[(1-source)*22]=1;
        peeps[source*22+byte]=0x80; peeps[source*22+3]=200;
        uint8_t saved[sizeof prg_a4_area]; memcpy(saved,prg_a4_area,sizeof saved);
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
        setup_friendly_merge(source,owner);
        set_peep_word(1-source,4,populations[index]);
        uint8_t saved[sizeof prg_a4_area]; memcpy(saved,prg_a4_area,sizeof saved);
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
        setup_friendly_merge(source,owner);
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
        map_who[position]=2; core_tendency(owner)=0; core_leader(owner)=1; core_target(owner)=position+1;
        /* A split child sharing its town's cell must depart, not merge back. */
        core_move(0);
        assert(peep_word(0,4)==100 && peep_word(1,4)==100 && peeps[22]==1);
        assert(peep_word(0,8)==position+1 && map_bk2[position]==42);
        map_who[position]=2;
        /* A nearby return uses the existing destination encounter. */
        core_target(owner)=position; peeps[22]=0x61; set_peep_word(1,12,101);
        uint8_t terrain[4096], buildings[4096], split[22];
        memcpy(terrain,map_blk,sizeof terrain); memcpy(buildings,map_bk2,sizeof buildings);
        memcpy(split,peeps+44,sizeof split);
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
    memset(prg_a4_area,0,sizeof prg_a4_area);
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
    assert(!map_who[650] && map_who[586]==1 && !core_settlements && seed==123);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(map_who[peep_word(0,8)]==1 && !map_who[650]);
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
        assert(!memcmp(before,prg_a4_area,sizeof before)); /* Only command/latch/history changed. */
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
        assert(map_who[positions[corner]+expected[corner]]==1);
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
        assert(!map_who[650] && map_who[586]==1 && peeps[0]==2 && seed==123);
        assert(!core_settlements && A4_WORD(0xff88+586*2)==1);
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
    assert(!map_who[650] && map_who[651]==1 && (int8_t)peeps[21]==1);
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
    assert(!map_who[650] && map_who[651]==1 && peep_word(0,12)==6);
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
            assert(peep_word(0,4)==100-ticks && !map_who[650] && map_who[651]==1);
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
    seed=123; /* Rolls 1 and 3: lesser strength 100, loss 11, not wait loss 1. */
    _move_peeps();
    assert(peeps[0]==8 && peeps[16*22]==10);
    assert(peep_word(0,6)==16 && peep_word(16,6)==0);
    assert(peep_word(0,4)==89 && peep_word(16,4)==89);
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
            _move_peeps();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
            assert(!core_battles && !core_settlements && !core_result);
        }
    /* Direct tick retains its stricter nonpositive-population skip. */
    const int skipped[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int flags=5; flags<=6; ++flags)
        for (unsigned sample=0; sample<sizeof skipped/sizeof skipped[0]; ++sample) {
            setup_celebration(owner); peeps[0]=flags; set_peep_word(0,4,skipped[sample]);
            uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
            _move_peeps();
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
            _move_peeps();
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
            _move_peeps();
            assert(!memcmp(expected,prg_a4_area,sizeof expected));
        }
        _move_peeps();
        assert(peeps[0]==2 && peep_word(0,12)==0 && peep_word(0,8)==651);
        assert(peep_word(0,4)==1 && peep_word(0,10)==1 && !map_who[650] && map_who[651]==1);
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
                    _move_peeps();
                    /* Includes all records, footprint, map history, scratch,
                     * mana, growth/split tables, metadata, RNG and turn eight. */
                    assert(!memcmp(expected,prg_a4_area,sizeof expected));
                    assert(!core_battles && !core_settlements && !core_result);
                }
                game_turn=9; _move_peeps();
                assert(peep_word(0,4)==(overflow ? 3200 : 1) && no_peeps==1);
                assert(core_mana(owner)==(owner ? 23456 : 12345) && seed==123);
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
            _move_peeps();
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
            _move_peeps();
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
                    assert(map_who[650]==survives && peep_word(0,8)==650 && seed==123);
                }
    for (int owner=0; owner<2; ++owner) {
        setup_celebration(owner); peeps[0]=0x16; set_peep_word(0,12,87);
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,0);
        int16_t population=98; memcpy(expected+(peeps-prg_a4_area)+4,&population,2);
        _move_peeps(); /* Loss, two celebration frames, then walker reframe only. */
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
        _move_peeps();
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==123);
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        /* Battle exemption: no valid partner, so no combat or water side effects. */
        const int combat[]={8,9,10,0x88};
        for (unsigned state=0; state<sizeof combat/sizeof combat[0]; ++state) {
            setup_water_entry(owner,combat[state],87,1); game_mode=fatal;
            memcpy(expected,prg_a4_area,sizeof expected);
            _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_result && !core_battles && !core_settlements);
        game_tick();
        assert(game_turn==2 && core_result==(owner ? 1 : 2));
        assert(!peep_word(0,4) && peep_word(0,6)==123 && peep_word(0,12)==7);
        assert(peep_word(17,12)==2 && peep_word(17,4)==100 && map_who[900]==18);
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_leader(owner)==1 && view_who==1);
        expected[peeps-prg_a4_area]=0;
        int16_t zero=0;
        memcpy(expected+(peeps-prg_a4_area)+4,&zero,2);
        if (representative==1) expected[map_who-prg_a4_area+650]=0;
        celebration_expected_word(expected,0x99f6+owner*16,0);
        celebration_expected_word(expected,0xae68,0);
        celebration_expected_word(expected,0x99f8+owner*16,650);
        celebration_expected_word(expected,owner ? 0xae76 : 0xae78,650);
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps();
        assert(peep_word(0,4)==(remaining>0 ? remaining : 0));
        assert(peeps[0]==(remaining>0 ? flags[state] : 0));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==123);
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
            _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        int32_t friendly=owner ? 100 : !fatal, enemy=owner ? !fatal : 100;
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_result==(fatal ? (owner ? 1 : 2) : 0));
        assert(peep_word(17,12)==1 && peep_word(17,4)==100 && map_who[900]==18);
    }
    /* Mixed combat+water intentionally retains host recovery, not wet loss. */
    const int combat[]={0x18,0x19,0x1e,0x98};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (unsigned sample=0; sample<sizeof combat/sizeof combat[0]; ++sample) {
        setup_water_entry(owner,combat[sample],32767,1); game_mode=fatal;
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,1);
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps();
        assert(peep_word(0,4)==remaining && peep_word(0,12)==frame);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==123);
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
        _move_peeps();
        assert(!peeps[0] && !peep_word(0,4) && peep_word(0,12)==7);
        assert(peeps[22]==2 && peep_word(1,4)==100 && peep_word(1,12)==1);
        assert(peep_word(0,6)==123 && peep_word(1,6)==29);
        assert(peep_word(0,10)==-64 && peep_word(1,10)==-63 && map_who[650]==2);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==123);
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
            expected[map_who-prg_a4_area+650]=0;
            expected[map_who-prg_a4_area+715]=low ? 0 : 1;
            celebration_expected_word(expected,0xff88+715*2,733);
            if (low) {
                celebration_expected_word(expected,0x99f8+owner*16,715);
                celebration_expected_word(expected,owner ? 0xae76 : 0xae78,715);
            }
        }
        _move_peeps();
        assert(peep_word(0,4)==(dead ? 0 : 97));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==123);
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
            _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        int32_t friendly=owner ? 100 : 0, enemy=owner ? 0 : 100;
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_result==(owner ? 1 : 2) && peep_word(17,12)==1);
        assert(!core_battles && !core_settlements);
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps();
        assert(peep_word(0,4)==(remaining>0 ? remaining : 0));
        assert(peep_word(0,12)==frame && peeps[0]==expected[peeps-prg_a4_area]);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==123);
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
        _move_peeps();
        assert(peep_word(0,12)==frame);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==123);
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
            _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        int32_t friendly=owner ? 100 : 0, enemy=owner ? 0 : 100;
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_result==(owner ? 1 : 2) && peep_word(17,12)==1);
        assert(!core_battles && !core_settlements);
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
        _move_peeps();
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
                expected[map_who-prg_a4_area+650]=0; expected[map_who-prg_a4_area+651]=1;
                celebration_expected_word(expected,0xff88+651*2,669);
            }
        }
        /* Blocked exploration visits the random ray once: seed 123 -> 15930. */
        if (blocked) celebration_expected_word(expected,0x99e2,15930);
        _move_peeps();
        assert(peep_word(0,4)==(remaining>0 ? remaining : 0) && peep_word(0,12)==frame);
        assert(peep_word(0,6)==counter && peeps[0]==expected[peeps-prg_a4_area]);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && !core_result && seed==(blocked ? 15930 : 123));
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
        _move_peeps();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==colocated && !core_settlements && !core_result && seed==123);
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
            _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        int32_t friendly=owner ? 100 : 0, enemy=owner ? 0 : 100;
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_result==(owner ? 1 : 2) && !core_battles && !core_settlements);
    }
    const int mixed[]={0x30,0x31,0x33,0x36,0x50,0x51,0x53,0x56,0x70,0x71,0x73,0x76,0x92,0xb2,0xd2,0xf2};
    for (unsigned sample=0; sample<sizeof mixed/sizeof mixed[0]; ++sample)
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal) {
        setup_water_entry(owner,mixed[sample],5,1); map_blk[650]=53; game_mode=fatal;
        uint8_t expected[sizeof prg_a4_area]; celebration_expected(expected,2,1);
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        /* Preserve a previous-cell occupant even on death: the host deliberately
         * differs from original _zero_population's previous-cell clearing.
         * Separately, host death clears flags; original retains the flag byte. */
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
        int16_t later=1; memcpy(expected+(peeps-prg_a4_area)+22+12,&later,2);
        _move_peeps();
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        /* Ordinary positive 0x80 expires only when the NEXT old timer is zero,
         * with no second water loss, frame change or swamp consumption. */
        water_remnant_expected(expected,owner,650,0,-1,1);
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==123);
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
            _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        int32_t friendly=owner ? 100 : 0, enemy=owner ? 0 : 100;
        memcpy(expected+(A4_DISP(0x99fe)-prg_a4_area),&friendly,4);
        memcpy(expected+(A4_DISP(0x9a0e)-prg_a4_area),&enemy,4);
        game_tick(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_result==(owner ? 1 : 2) && !core_battles && !core_settlements);
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
            assert(seed==123 && peep_word(0,8)==650 && peep_word(0,10)==-64);
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
        _move_peeps();
        assert(map_who[650]==(representative ? representative : 1));
        assert(peep_word(0,6)==0);
        /* Apart from timer and empty-slot claim, consumption is inert. */
        set_peep_word(0,6,1); map_who[650]=representative;
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
            assert(!memcmp(before,prg_a4_area,sizeof before));
        }
        pause=0; core_result=0;
        for (int tick=1; tick<=40; ++tick) {
            game_tick();
            assert(game_turn==tick && peep_word(0,6)==40-tick && !core_result);
            assert(A4_LONG(0x99fe)==1 && A4_LONG(0x9a0e)==1);
        }
        game_tick();
        assert(game_turn==41 && !peep_word(0,4) && !map_who[650]);
        assert(A4_LONG(owner ? 0x9a0e : 0x99fe)==0);
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
                    assert(map_who[650]==(destination ? 0 : 2) && map_who[651]==1);
                    assert(A4_WORD(0xff88+651*2)==1);
                    /* Exact consumer preserves the last writer, including at death. */
                    if (remnant_flags[state]==0x80) {
                        _move_peeps();
                        assert(peep_word(1,6)==39 && peep_word(1,4)==1 && map_who[651]==1);
                        set_peep_word(1,6,0); _move_peeps();
                        assert(!peep_word(1,4) && !map_who[650] && map_who[651]==1);
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
                assert(peep_word(1,12)==0 && map_who[651]==2);
                assert(map_who[650]==(!destination && counter ? 1 : 0));
                assert(core_population(0)==100+(friendly ? counter : 0));
                assert(core_population(1)==(friendly ? 0 : counter));
                assert(!core_battles && !core_settlements && A4_WORD(0xff88+651*2)==1);
                _move_peeps();
                assert(!peeps[0] && !peep_word(0,4) && !map_who[650] && map_who[651]==2);
                assert(peeps[22]==2 && peep_word(1,4)==100 && peep_word(1,8)==651);
                assert(core_population(0)==100 && !core_population(1));
                assert(!core_battles && !core_settlements);
            }
    /* Ignoring combat must not bypass occupancy and hide a new town. */
    for (int friendly=0; friendly<2; ++friendly) {
        setup_blocked_walker(); memset(map_blk,15,4096); no_peeps=2;
        peeps[22]=0x80; peeps[23]=friendly ? 0 : 1;
        set_peep_word(1,4,1); set_peep_word(1,6,40); set_peep_word(1,8,650);
        map_who[650]=2;
        assert(core_explore(0)==0 && core_check_life(0,650)>0);
        core_move(0);
        assert(peeps[0]==2 && peeps[22]==0x80 && peep_word(1,4)==1);
        assert(map_who[650]==2 && !map_bk2[650] && !core_battles && !core_settlements);
    }
}

/* Consumer-only fixtures: no Armageddon producer or war navigation enabled. */
static void setup_war_town(int owner, int position, int city)
{
    memset(prg_a4_area,0,sizeof prg_a4_area);
    memset(map_blk,15,4096);
    no_peeps=1; core_result=prg_quit=core_battles=core_settlements=0;
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
                assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        if (direct) core_settle_tick(0); else _move_peeps();
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
        if (!trigger) { _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected)); }
    }
}

static void test_town_economy_split(void)
{
    for (int owner=0; owner<2; ++owner) for (int full=0; full<=1; ++full) {
        setup_town_economy(owner,3100);
        if (full) {
            no_peeps=208;
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
        /* A later parent turn reclaims the cell vacated by a lower child. */
        assert(map_who[650]==(child<parent ? parent+1 : 0));
        assert(map_who[history ? 715 : 587]==child+1);
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
            celebration_expected_word(expected,0xae6a,8);
            celebration_expected_word(expected,0x9c26+22+12,86);
            town_expected_long(expected,0x99fe + owner*16,0);
            town_expected_long(expected,0x99fe + (1-owner)*16,77);
        }
        game_tick();
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_result==(gate ? (gate>=3 ? gate-2 : 0) : (owner ? 1 : 2)));
        assert(!core_battles && !core_settlements);
        if (!gate) {
            assert(!peeps[0] && !peep_word(0,4) && peep_word(1,12)==86);
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
            if (turn==8) {
                celebration_expected_word(expected,0x9c26+4,111);
                expected[peeps-prg_a4_area+3]=9;
                town_expected_long(expected,0x9a02+owner*16,owner ? 23439 : 12328);
            }
            town_footprint_map(expected,owner,position,frame,cases[sample].refresh,0);
            if (direct) core_settle_tick(0); else _move_peeps();
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
        no_peeps=full ? 208 : 2;
        for (int person=1; person<no_peeps; ++person) {
            peeps[person*22]=0x80; peeps[person*22+1]=1-owner;
            set_peep_word(person,4,77); set_peep_word(person,8,650);
        }
        if (!full) { memset(peeps+2*22,0xa5,22); set_peep_word(2,4,-1); }
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        celebration_expected_word(expected,0x2ff8,9); celebration_expected_word(expected,0x2fb2,0);
        celebration_expected_word(expected,0x9c26+12,41);
        if (!occupant) expected[map_who-prg_a4_area+650]=1;
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
        _move_peeps();
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
                if (turn==9) expected[peeps-prg_a4_area+22]=2;
                town_expected_long(expected,0x99fe + owner*16,turn==7 ? 1 : fatal ? 0 : 12);
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
            }
            game_tick();
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
        if (direct) core_settle_tick(0); else _move_peeps();
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
    assert(memcmp(before,prg_a4_area,sizeof before)==0);
    for (int tick=0; tick<16; ++tick) game_tick();
    int position=peep_word(0,8);
    assert(peeps[0]==1);
    int32_t mana_before=core_mana(0);
    assert(core_sculpt(0,position%64,position/64,1));
    assert(core_vertex(position%64,position/64)==2 && core_mana(0)==mana_before-14);
    memcpy(before,prg_a4_area,sizeof before);
    assert(!core_sculpt(0,60,60,1));
    assert(memcmp(before,prg_a4_area,sizeof before)==0);
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

static void test_match_events_cadence(int interval, int legacy)
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
        if (tick > 6000 && tick%interval == 0) {
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
    if (legacy) {
        assert(getenv("POPULOUS_TEST_REAL_DATA"));
        assert(!core_result && game_turn==12000 && core_battles==316);
        assert(core_population(0)==26818 && core_population(1)==177062);
        _closeall(); SDL_Quit(); return;
    }
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
    key.key.keysym.sym=SDLK_ESCAPE;
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    assert(prg_quit);
    _closeall(); SDL_Quit();
}

static void test_match_events(void)
{
    /* Counter continuous rescue with legal input; retain cadence 8 separately. */
    test_match_events_cadence(1,0);
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

static void test_isometric_match_cadence(int interval, int legacy)
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
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    game_tick(); game_render(); assert(!memcmp(before,prg_a4_area,sizeof before));
    game_handle_event(&event); assert(!core_result && xoff==6 && yoff==6);
    event.key.keysym.sym=SDLK_3; game_handle_event(&event);
    for (int tick=0;tick<12000 && !core_result;++tick) {
        game_tick();
        if (tick>6000 && tick%interval==0) for (int person=0;person<no_peeps;++person) {
            if (!peep_owner(person) || peep_word(person,4)<=0) continue;
            int position=peep_word(person,8);
            for (int dy=0;dy<2;++dy) for (int dx=0;dx<2;++dx)
                iso_click_vertex(position%64+dx,position/64+dy,SDL_BUTTON_RIGHT);
        }
    }
    fprintf(stderr,"isometric fight result=%d turns=%u battles=%d towns=%d pop=%d/%d\n",core_result,(uint16_t)game_turn,core_battles,core_settlements,core_population(0),core_population(1));
    if (legacy) {
        assert(getenv("POPULOUS_TEST_REAL_DATA"));
        assert(!core_result && game_turn==12000 && core_battles==225);
        assert(core_population(0)==65387 && core_population(1)==9594);
        _closeall(); SDL_Quit(); return;
    }
    assert(core_result && core_battles>0 && core_settlements>0);
    if (getenv("POPULOUS_TEST_REAL_DATA")) assert(core_result==1);
    event.key.keysym.sym=SDLK_ESCAPE; game_handle_event(&event); assert(prg_quit);
    _closeall(); SDL_Quit();
}

static void test_isometric_match(void)
{
    test_isometric_match_cadence(1,0);
}

static void test_original_graphics_output(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    GraphicsAssets *assets_before=game_assets;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    char *directory=strdup(getenv("POPULOUS_ASSET_DIR") ? getenv("POPULOUS_ASSET_DIR") : POPULOUS_DEFAULT_ASSET_DIR);
    assert(directory && !setenv("POPULOUS_ASSET_DIR","/nonexistent-populous-assets",1));
    assert(!game_reset(0) && game_assets==assets_before && !memcmp(before,prg_a4_area,sizeof before));
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
    game_render();
    assert(game_pixels[72*320+192]==3); /* first flat tile */
    assert(game_pixels[88*320+184]==9); /* first walker at (8,8), no interpolation */
    assert(!memcmp(before,prg_a4_area,sizeof before));
    for (int tick=0;tick<16;++tick) game_tick();
    int settlements=0;
    for (int cell=0;cell<4096;++cell) settlements+=map_bk2[cell]==42;
    assert(settlements>0);
    game_assets->sprite_indices[64*256]=8;
    game_render();
    assert(game_pixels[96*320+192]==5); /* town overlay, tile_top-8 */
    assert(game_pixels[88*320+192]==9); /* settled occupant uses frame 64 */
    /* _move_sprite compares flags to exactly 1: flags 9 is a town defending
     * a battle, not a peaceful occupant. Distinct frame pixels catch precedence. */
    assert(peep_word(0,8)==8*64+8 && map_who[8*64+8]==1);
    for (int owner=0;owner<2;++owner) for (int phase=0;phase<2;++phase) {
        peeps[0]=9; peeps[1]=owner; game_turn=16+phase*4;
        game_assets->sprite_indices[(64+phase+owner*2)*256]=8;
        game_assets->sprite_indices[(138+phase)*256]=2+phase*2;
        memcpy(before,prg_a4_area,sizeof before);
        game_render();
        assert(game_pixels[88*320+192]==(phase ? 5 : 3));
        assert(prg_screen->chunky_buffer[88*320+192]==prg_screen->palette[phase ? 5 : 3]);
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
    xoff=16; yoff=16; mousex=-100; mousey=-100;
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
    game_turn=turn; map_blk[cell]=block; map_alt[cell]=altitude;
    uint8_t expected[64000],before[sizeof prg_a4_area];
    memcpy(before,prg_a4_area,sizeof before);
    draw_original(16,16);
    assert(!memcmp(before,prg_a4_area,sizeof before));
    memcpy(expected,game_pixels,sizeof expected);
    sprite_mask_oracle(expected,want_frame,want_x,want_y);
    map_who[cell]=1;
    memcpy(before,prg_a4_area,sizeof before);
    draw_original(16,16);
    assert(!memcmp(before,prg_a4_area,sizeof before));
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
                sprite_case(18,owner,7,headings[raw],phase*4+turn,directions[direction],block ? 0 : 15,0,0,0,
                    water[owner][headed][turn],184,block ? 60 : 56);
        }
        for (int direction=0;direction<8;++direction) for (int block=0;block<2;++block)
            sprite_case(2,owner,7,headings[raw],phase*4,directions[direction],block ? 0 : 15,0,0,0,
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
    /* Unsupported flags retain the old battle-before-water-before-walker cascade. */
    const int fallback[][3]={{0,23,0},{4,23,0},{0x86,23,0},{0x88,139,1},
        {0x16,100,0},{0x28,139,1},{0x62+1,23,0},{0x90,100,0},{0xff,139,1}};
    for (unsigned sample=0;sample<sizeof fallback/sizeof fallback[0];++sample)
        sprite_case(fallback[sample][0],1,32767,UINT32_MAX,7,1,0,0,0,0,
            fallback[sample][1],fallback[sample][2] ? 192 : 184,fallback[sample][2] ? 56 : 60);
    /* Current-cell coordinates, height lift, top clipping and far visible corners. */
    sprite_case(6,1,42,1,0,1,15,0,0,8,78,192,-8);
    sprite_case(2,1,7,1,4,1,0,0,0,8,55,184,-4);
    sprite_case(0x80,0,0,0,3,1,15,7,0,0,108,296,112);
    sprite_case(5,0,85,0,0,1,15,7,7,0,85,192,168);
    sprite_case(0x22,0,101,0,0,1,15,3,2,3,101,208,72);
    sprite_case(5,1,42,1,0,1,0,0,0,0,78,192,56);
    sprite_case(0x80,1,0,1,3,1,0,0,0,0,108,184,56);
    sprite_case(0x62,1,102,1,4,1,0,0,0,0,128,192,56);
    for (int sample=0;sample<5;++sample) {
        sprite_case(2,1,stored[sample],1,4,1,0,0,0,0,55,184,60);
        sprite_case(18,1,stored[sample],1,7,1,0,0,0,0,116,184,60);
    }
}
static void test_special_sprite_order_and_guards(void)
{
    sprite_scene(); game_turn=4;
    uint8_t expected[64000],before[sizeof prg_a4_area];
    draw_original(16,16); memcpy(expected,game_pixels,sizeof expected);
    /* Reverse slot order: the earlier cell's celebration overlaps the later
     * cell's walker. Hidden colocated slot 2 must not be drawn. */
    no_peeps=3;
    for (int person=0;person<3;++person) {
        peeps[person*22]=person ? 5 : 2;
        set_peep_word(person,4,100); set_peep_word(person,8,16*64+16+(person==0));
        set_peep_word(person,10,1); set_peep_word(person,12,person==2 ? 86 : 85);
    }
    map_who[16*64+16]=2; map_who[16*64+17]=1;
    sprite_mask_oracle(expected,85,192,56);
    sprite_mask_oracle(expected,7,200,64);
    memcpy(before,prg_a4_area,sizeof before); draw_original(16,16);
    assert(!memcmp(before,prg_a4_area,sizeof before)); assert_sprite_pixels(expected);
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
        assert(!memcmp(before,prg_a4_area,sizeof before)); assert_sprite_pixels(expected);
    }
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
        assert(person<no_peeps && peep_word(person,4)>0 && peep_word(person,8)==cell);
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
    memset(prg_a4_area,0,sizeof prg_a4_area);
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
        assert(swamp_draws==0 && !memcmp(before,prg_a4_area,sizeof before));
    }
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int mana=5000;mana<=5001;++mana) for (int paint=0;paint<2;++paint) {
        swamp_fixture(); player=local; paint_map=paint;
        A4_LONG(0x9a02+owner*16)=mana;
        assert(core_swamp(owner,255,255,swamp_random));
        assert(core_mana(owner)==mana-5000 && swamp_draws==60);
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
    assert(!memcmp(before,prg_a4_area,sizeof before));
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
     * and accidental clearing of another occupant or the previous cell. */
    for (int owner=0;owner<2;++owner) for (int bottomless=0;bottomless<2;++bottomless)
    for (int fatal=0;fatal<2;++fatal) for (int owned=0;owned<2;++owned) {
        swamp_walker(owner,2,0);
        game_mode=(bottomless ? 2 : 0) | (fatal ? GAME_MODE_WATER_FATAL : 0);
        core_leader(owner)=1; core_target(owner)=777; view_who=1;
        A4_WORD(owner ? 0xae76 : 0xae78)=777;
        map_who[1300]=owned ? 1 : 2; map_who[1299]=1;
        for (int frame=0;frame<7;++frame) {
            _move_peeps();
            assert(peep_word(0,4)==100 && peep_word(0,8)==1300);
            assert(peep_word(0,12)==frame+1 && map_blk[1300]==53);
            assert(A4_WORD(0x2fac)==0 && seed==123);
        }
        _move_peeps();
        assert(peep_word(0,4)==0 && peeps[0]==0 && peep_word(0,8)==1300);
        assert(map_blk[1300]==(bottomless ? 53 : 15) && A4_WORD(0x2fac)==66);
        assert(map_who[1300]==(owned ? 0 : 2) && map_who[1299]==1);
        assert(core_leader(owner)==0 && core_target(owner)==1300 && view_who==0);
        assert(A4_WORD(owner ? 0xae76 : 0xae78)==1300);
        assert(core_target(1-owner)==0 && seed==123);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
    /* Repeats/missing spell cannot arm; P never permits a free cast. */
    for (int paint=0;paint<2;++paint) for (int missing=0;missing<2;++missing) {
        assert(game_reset(0)); SDL_SetWindowSize(prg_screen->window,640,512);
        paint_map=paint; A4_LONG(0x9a02)=6000; A4_WORD(0x82c4)=missing ? 1 : 0x11;
        key.key.repeat=!missing; game_handle_event(&key); key.key.repeat=0;
        iso_click_vertex(20,20,SDL_BUTTON_LEFT);
        assert(core_vertex(20,20)==2 && core_mana(0)==5986 && A4_LONG(0xae72)==0);
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
    assert(!memcmp(before,prg_a4_area,sizeof before));
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
    memset(prg_a4_area,0,sizeof prg_a4_area);
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
    assert(!memcmp(before,prg_a4_area,sizeof before));
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
static void test_quake_cast(void)
{
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint)
    for (int gate=0;gate<13;++gate) {
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
        assert(!quake_draws && !memcmp(before,prg_a4_area,sizeof before));
    }
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int paint=0;paint<2;++paint) for (int height=0;height<2;++height) {
        quake_fixture(height); player=local; paint_map=paint;
        A4_LONG(0x9a02+owner*16)=2500; A4_LONG(0xae72)=17;
        assert(core_quake(owner,20,30,quake_random));
        assert(core_mana(owner)==0 && core_mana(1-owner)==10000);
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
     * Exact-cost admission and P must not become an editor bypass. */
    for (int diagnostic=0;diagnostic<2;++diagnostic) for (int tool=0;tool<2;++tool)
    for (int owner=0;owner<2;++owner) for (int paint=0;paint<2;++paint) {
        assert(game_reset(0)); diagnostic_view=diagnostic; core_tool=tool;
        player=owner; paint_map=paint; core_swamp_selected=1;
        xoff=20; yoff=30; mousex=mousey=0;
        memset(A4_DISP(0xae86),0,65*65*2);
        A4_LONG(0x9a02+owner*16)=2500; A4_WORD(0x82c4+owner*46)=8;
        uint16_t old_seed=seed;
        game_handle_event(&key);
        assert(core_mana(owner)==0 && A4_LONG(0xae72)==25 && A4_WORD(0x2fac)==76);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
        game_render(); assert(!memcmp(before,prg_a4_area,sizeof before));
    }
    /* Every rejected key preserves the entire emulated state and armed tool. */
    for (int diagnostic=0;diagnostic<2;++diagnostic) for (int paint=0;paint<2;++paint)
    for (int gate=0;gate<9;++gate) {
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before) && seed==321);
        assert(core_result==(gate>=6 ? gate-5 : 0));
    }
    /* An empty world still pays, scores, signals effect, and clears history. */
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int paint=0;paint<2;++paint) for (int height=0;height<3;++height)
    for (int extra=0;extra<2;++extra) {
        flood_fixture(height); player=local; paint_map=paint;
        A4_LONG(0x9a02+owner*16)=40000+extra; A4_LONG(0xae72)=17;
        assert(core_flood(owner));
        assert(core_mana(owner)==extra && core_mana(1-owner)==40000);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
        assert(core_tool==1 && core_swamp_selected==1);
    }
    assert(game_reset(0)); game_render();
    assert(strstr(SDL_GetWindowTitle(prg_screen->window),"F flood 40000"));
    /* Verify visible text rather than only the window title; four-pixel glyph
     * pitch leaves the entire flood label inside the 320-pixel HUD. */
    uint32_t line[320*7]; memcpy(line,prg_screen->chunky_buffer+231*320,sizeof line);
    hud_text(64,231,"F FLOOD 40000");
    assert(!memcmp(line,prg_screen->chunky_buffer+231*320,sizeof line));
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
        assert(!volcano_draws && seed==321 && !memcmp(before,prg_a4_area,sizeof before));
    }
    for (int owner=0;owner<2;++owner) for (int local=0;local<2;++local)
    for (int paint=0;paint<2;++paint) for (int height=0;height<=8;height+=8)
    for (int extra=0;extra<2;++extra) {
        volcano_fixture(height); player=local; paint_map=paint;
        A4_LONG(0x9a02+owner*16)=10000+extra; A4_LONG(0xae72)=17;
        for (int draw=0;draw<165;++draw) volcano_values[draw]=height ? 1 : draw%2 ? 3 : 0;
        assert(core_volcano(owner,20,30,volcano_random));
        assert(core_mana(owner)==extra && core_mana(1-owner)==10000);
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
        assert(core_mana(owner)==0 && seed==expected_seed && A4_LONG(0xae72)==100);
        assert(A4_WORD(0x2fac)==75 && core_swamp_selected==1 && core_tool==tool);
        assert(xoff==(view ? 40 : 10) && yoff==(view ? 40 : 20));
        assert(mousex==319 && mousey==255 && core_vertex(xoff+4,yoff+4)>0);
        assert(core_vertex(view ? 14 : 44,view ? 24 : 44)==0);
    }
    for (int gate=0;gate<11;++gate) for (int paint=0;paint<2;++paint) {
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
        assert(core_tool==1 && core_swamp_selected==1);
    }
    assert(game_reset(0)); game_render();
    assert(strstr(SDL_GetWindowTitle(prg_screen->window),"V volcano 10000"));
    uint32_t line[320*7]; memcpy(line,prg_screen->chunky_buffer+231*320,sizeof line);
    hud_text(128,231,"V VOLCANO 10000");
    assert(!memcmp(line,prg_screen->chunky_buffer+231*320,sizeof line));
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
    assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before) && assets==game_assets);
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
        game_tick();
        int raised=opcode==1 && !(cases[index].controller==1 && !cases[index].divisor);
        assert(core_vertex(20,30)==1+raised && core_mana(owner)==10000-14*raised);
        assert(A4_WORD(0x82be + owner*46)==cases[index].expected);
        assert(A4_WORD(0x82bc+owner*46)==cases[index].controller);
        assert((uint16_t)A4_WORD(0x82c6+owner*46)==cases[index].divisor);
        assert_mailbox_empty();
    }
    for (int opcode=2;opcode<=255;++opcode) {
        terrain_mailbox_fixture(1); terrain_mailbox_write(0,opcode,20,30);
        game_tick();
        assert(core_vertex(20,30)==1 && core_mana(0)==10000 && A4_WORD(0x2f88)==123);
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
        game_tick(); assert(!memcmp(before,prg_a4_area,sizeof before));
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
            assert(core_result==2 && !peep_word(0,4) && A4_LONG(0x99fe)==0 && A4_LONG(0x9a0e)==45);
            assert(core_vertex(16,6)==0 && core_mana(0)==10000 && A4_WORD(0x2f88)==123);
            assert(A4_WORD(0x82be)==17 && A4_WORD(0x82be + 46)==17);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(core_vertex(20,30)==1+(war || admitted));
        assert(core_mana(owner)==(admitted ? (war ? 20 : 6) : -1));
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
        assert(A4_LONG(0x99fe)==(old_water ? 39u : 45u) && !core_result);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
    }
    const int corners[]={0,63,4032,4095};
    for (int owner=0;owner<2;++owner) for (int corner=0;corner<4;++corner) {
        war_terrain_fixture(); core_queue_terrain_raise(owner,corners[corner]);
        assert_war_command(owner,corners[corner]); assert(seed==123);
    }
    war_terrain_fixture(); A4_WORD(0xae80)=0;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    core_queue_terrain_raise(0,650); assert(!memcmp(before,prg_a4_area,sizeof before));
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
static void test_war_terrain_old_water(void)
{
    war_terrain_fixture(); map_blk[650]=0;
    _move_peeps();
    assert(peep_word(0,4)==100 && peeps[0]==18); assert_mailbox_empty();
    _move_peeps();
    assert(peep_word(0,4)==98 && peeps[0]==18); assert_war_command(0,650);
    assert(core_vertex(10,10)==0 && seed==123 && core_mana(0)==100);
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
        if (!fatal && (war || (controller==1 && !no_build))) assert_war_command(owner,650); else assert_mailbox_empty();
        assert(peep_word(0,4)==(fatal ? 0 : 98));
        if (land) assert(peeps[0]==(fatal ? 0 : 2) && peep_word(0,12)==(fatal ? 7 : 1));
        assert(core_vertex(10,10)==0 && seed==123 && core_mana(owner)==-1);
    }
    /* Existing host battle exemption and recovery stay unchanged, even with
     * flag16: no new source fatal-on-land or return-to-land attrition imported. */
    war_terrain_fixture(); peeps[0]=24; map_blk[650]=0;
    _move_peeps(); assert_mailbox_empty(); assert(peep_word(0,4)==100);
    /* Land primary-water queues despite fatal mode; old wet fatal never queues. */
    war_terrain_fixture(); game_mode=1;
    _move_peeps(); assert_war_command(0,650); assert(peep_word(0,4)==99);
    war_terrain_fixture(); game_mode=1; peeps[0]=18; map_blk[650]=0;
    _move_peeps(); assert_mailbox_empty(); assert(!peep_word(0,4));
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
        else assert(!A4_DISP(0x82b6+46)[0]);
        assert(peep_word(0,8)==first+1 && peep_word(1,8)==last+1);
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
            assert(core_vertex(column,row)==0); /* Full movement finishes before execution. */
        assert(seed==123 && core_mana(0)==100 && core_mana(1)==100);
        core_consume_terrain_mailboxes();
        assert(core_vertex(last%64,last/64)==1);
        assert(core_vertex(first%64,first/64)==separate);
        /* Source consumer executes owner 0 then 1, regardless of walker cells. */
        assert(A4_WORD(0x2f8a)==last%64 && A4_WORD(0x2f8e)==last/64);
        assert_mailbox_empty(); assert(seed==123);
    }
    /* Earlier producer is discarded if the last enemy dies later in the pass. */
    war_terrain_fixture(); no_peeps=2;
    peeps[22]=18; peeps[23]=1; set_peep_word(1,4,1); set_peep_word(1,8,900);
    map_blk[900]=0; map_who[900]=2;
    game_tick();
    assert(core_result==1 && !peep_word(1,4) && peep_word(0,4)==99);
    assert(core_vertex(10,10)==0 && core_vertex(4,14)==0 && seed==123);
    assert_mailbox_empty();
    /* A dying old-water producer belongs to the owner, not the person. */
    war_terrain_fixture(); no_peeps=3; peeps[0]=18; map_blk[650]=0; set_peep_word(0,4,1);
    for (int person=1;person<3;++person) {
        peeps[person*22]=0x80; peeps[person*22+1]=person-1;
        set_peep_word(person,4,100); set_peep_word(person,6,100);
        set_peep_word(person,8,900+person); map_who[900+person]=person+1;
    }
    game_tick();
    assert(!peep_word(0,4) && !core_result && core_population(0)==100 && core_population(1)==100);
    assert(core_vertex(10,10)==1 && map_blk[650]==17 && core_mana(0)==100 && seed==123);
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
        assert(!core_result && !core_battles && !core_settlements && seed==123);
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
        assert(!core_result && !core_battles && !core_settlements && seed==123);
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
        game_tick();
        assert(peeps[0]==18 && peep_word(0,4)==100 && peep_word(0,12)==7);
        assert(map_blk[650]==17 && core_vertex(10,10)==1 && !core_result);
        assert(core_population(0)==100 && core_population(1)==100); assert_mailbox_empty();
        game_tick();
        assert(!peeps[0] && !peep_word(0,4) && peep_word(0,12)==7 && !map_who[650]);
        assert(core_result==(owner ? 1 : 2) && peep_word(1,4)==100);
        assert(core_vertex(10,10)==1 && seed==123 && !core_battles); assert_mailbox_empty();
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
        assert(!core_result && !core_battles && !core_settlements && seed==123);
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
        assert(!core_result && !core_battles && !core_settlements && seed==123); assert_mailbox_empty();
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
        assert(core_result==(terminal ? (owner ? 1 : 2) : 0));
        /* Terminal discards; a dying nonterminal producer still raises for its owner. */
        assert(core_vertex(10,10)==!terminal && map_blk[650]==(terminal ? (dry ? 17 : 0) : 17));
        assert(peep_word(1,12)==1 && !core_battles && !core_settlements && seed==123);
        assert_mailbox_empty();
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
        assert(core_population(1-owner)==100 && seed==123);
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
        if (!core_battles) assert(seed==123);
        assert_mailbox_empty();
        for (int row=0;row<=64;++row) for (int column=0;column<=64;++column)
            assert(core_vertex(column,row)>=0 && core_vertex(column,row)<=8);
    }
    printf("war obstacle: raised=%d battle_tick=%d outcome=%d tick=%d populations=%d/%d\n",
           raised,first_battle,core_result,game_turn,core_population(0),core_population(1));
    assert(raised && first_battle==41 && core_result==3 && game_turn==45);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!core_war(side)); assert(!memcmp(before,prg_a4_area,sizeof before));
        SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_a;
        player=side; memcpy(before,prg_a4_area,sizeof before);
        game_handle_event(&key); assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(core_mana(owner)==0 && core_mana(1-owner)==80000);
        assert(A4_LONG(0xae72)==(owner==local ? 5017u : 17u));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
    game_render(); assert(!memcmp(before,prg_a4_area,sizeof before));
    const char *title=SDL_GetWindowTitle(prg_screen->window);
    assert(strstr(title,"WAR") && strstr(title,"A Armageddon 80000") && strstr(title,"Esc quit"));
    uint32_t hud[320*56]; memcpy(hud,prg_screen->chunky_buffer+320*200,sizeof hud);
    /* Check glyph presence using the existing renderer; separately prove the
     * literal strings fit its four-pixel advance rather than trusting clipping. */
    const char *lines[]={
        "Q QUAKE 2500 / F FLOOD 40000 / V VOLCANO 10000 / A ARMAGEDDON 80000",
        "WAR - TERRAIN AND MAGNET INPUT LOCKED / ARROWS SCROLL / MAP CLICK",
        "SPACE PAUSE / R RESET / ESC QUIT / D VIEW",
        "0 GATHER 1 SETTLE 2 JOIN 3 FIGHT / M MAGNET / S SWAMP 5000 / K KNIGHT 7500"
    };
    const int rows[]={231,213,249,222};
    for (unsigned line=0;line<sizeof lines/sizeof lines[0];++line) {
        assert(4+4*strlen(lines[line])<=320 && rows[line]+5<=256);
        hud_text(4,rows[line],lines[line]);
    }
    assert(!memcmp(hud,prg_screen->chunky_buffer+320*200,sizeof hud));
    game_render(); assert(!memcmp(before,prg_a4_area,sizeof before));
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
        war_terrain_fixture(); A4_WORD(0xae80)=0; player=local;
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
            if (!core_battles) assert(seed==123);
            assert_mailbox_empty();
            for (int vertex=0;vertex<4225;++vertex)
                assert(core_vertex(vertex%65,vertex/65)>=0 && core_vertex(vertex%65,vertex/65)<=8);
        }
        printf("Armageddon cast islands: caster=%d raised=%d battle=%d outcome=%d tick=%d populations=%d/%d\n",
               local,raised,first_battle,core_result,game_turn,core_population(0),core_population(1));
        assert(raised && first_battle && core_result==1 && core_population(0)>0 && !core_population(1));
    }
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
    assert(!memcmp(before,prg_a4_area,sizeof before));
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
    memset(prg_a4_area,0xa5,sizeof prg_a4_area);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
    }
    const int targets[]={INT_MIN,-2,1,207,208,INT_MAX};
    heading_fixture(1);
    for (unsigned sample=0;sample<sizeof targets/sizeof targets[0];++sample) {
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(!set_peep_reference(0,targets[sample]));
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
    }
    for (int invalid=0;invalid<6;++invalid) {
        heading_navigation_fixture(0);
        if (invalid<4) set_peep_word(0,8,bad_cells[invalid]);
        else peeps[1]=invalid==4 ? 2 : 255;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==999);
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
        core_target(owner)=-1;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==999);
        assert(!memcmp(before,prg_a4_area,sizeof before));
        core_target(owner)=4096;
        memcpy(before,prg_a4_area,sizeof before);
        assert(core_magnet_direction(0)==999);
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(map_who[651]==1 && !map_who[650] && !core_settlements && seed==123);
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
        _move_peeps(); assert(!memcmp(before,prg_a4_area,sizeof before));
    }
    setup_magnet_walker(0); assert(set_peep_reference(0,0));
    set_peep_word(0,4,0); set_peep_word(0,8,-1); core_leader(0)=1;
    core_target(0)=1234; A4_WORD(0xae78)=2345;
    _move_peeps();
    assert(!core_leader(0) && core_target(0)==1234 && A4_WORD(0xae78)==2345);
    setup_magnet_walker(0); assert(set_peep_reference(0,0)); core_leader(0)=1; pause=1;
    uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
    game_tick(); assert(!memcmp(before,prg_a4_area,sizeof before));
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
    memset(prg_a4_area,0,sizeof prg_a4_area);
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
    /* Host cleanup does not clear the previous cell; keep that distinction. */
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
        assert(seed==28505);
        /* Dry baseline: keep existing walking and zero loss, not water timing.
         * Source accumulates population BEFORE each resident (asm:3462-3474),
         * so expiry's source total still includes 1 until the following pass.
         * Host totals are post-pass: zero and terminal on invocation 41. */
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
            assert(map_who[peep_word(winner,8)]==winner+1);
        }
        game_tick();
        assert(game_turn==41 && !peep_word(loser,4));
        assert((uint16_t)peep_word(loser,6)==0xffff);
        assert(A4_LONG(owner ? 0x99fe : 0x9a0e)==0 && core_result==(owner ? 2 : 1));
        assert(A4_LONG(owner ? 0x9a0e : 0x99fe)>0);
        assert(!core_leader(1-owner) && core_target(1-owner)==650);
        assert(A4_WORD(owner ? 0xae78 : 0xae76)==650);
        assert(view_who==winner+1 && map_who[peep_word(winner,8)]==winner+1);
        assert(!memcmp(winner_heading,peeps+winner*22+14,4));
        assert(!memcmp(loser_heading,peeps+loser*22+14,4));
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
        assert(seed==28505);
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
        assert(peep_word(winner,6)==loser && seed==28505);
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
            assert(seed==28505 && core_battles==17 && !core_settlements);
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
    A4_LONG(0x9a02+owner*16)-=A4_LONG(0x849a);
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
        assert(!core_knight(side)); assert(!memcmp(before,prg_a4_area,sizeof before));
        /* The core takes int; player is a signed word, so use representative
         * invalid words for extremes which would truncate to a valid owner. */
        player=side==INT_MIN || side==INT_MAX ? -1 : side;
        memcpy(before,prg_a4_area,sizeof before);
        SDL_Event key={.type=SDL_KEYDOWN}; key.key.keysym.sym=SDLK_k;
        game_handle_event(&key); assert(!memcmp(before,prg_a4_area,sizeof before));
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
    const char *line="0 GATHER 1 SETTLE 2 JOIN 3 FIGHT / M MAGNET / S SWAMP 5000 / K KNIGHT 7500";
    /* Four-pixel advance and three-pixel glyph: remain inside the HUD. */
    assert(4+4*strlen(line)<=302);
    for (int state=0; state<6; ++state) {
        core_result=state>=3 ? state-2 : 0; pause=state==1; A4_WORD(0xae80)=state==2;
        core_swamp_selected=core_tool=1;
        uint8_t before[sizeof prg_a4_area]; memcpy(before,prg_a4_area,sizeof before);
        game_render(); assert(!memcmp(before,prg_a4_area,sizeof before));
        const char *title=SDL_GetWindowTitle(prg_screen->window);
        assert(strstr(title,"K knight 7500") && strstr(title,"Esc quit") && strlen(title)<512);
        uint32_t hud[320*56]; memcpy(hud,prg_screen->chunky_buffer+320*200,sizeof hud);
        hud_text(4,222,line);
        assert(!memcmp(hud,prg_screen->chunky_buffer+320*200,sizeof hud));
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
    assert(!memcmp(before,prg_a4_area,sizeof before));
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
                assert(map_blk[654]==66 && map_bk2[654]>=53 && map_bk2[654]<=63);
            }
            if (peeps[0]==6) { assert(remnant); celebrated=1; }
            if (celebrated && peeps[0]==2) resumed=1;
            if (remnant && !peep_word(1,4)) expired=1;
            assert(peep_word(0,4)>0 && core_tool==1 && core_swamp_selected==1);
            assert_mailbox_empty(); assert_live_occupancy();
            if (!(tick%10)) {
                memcpy(expected,prg_a4_area,sizeof expected); game_render();
                assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        assert(!memcmp(before,prg_a4_area,sizeof before));
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
        memset(prg_a4_area,0,sizeof prg_a4_area); no_peeps=counts[count];
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
        assert(seed==(route && winner==1 ? 2976 : 28505));
        assert(game_turn==tick && !core_result && core_battles==17);
        if (!route) {
            assert(peep_word(win,4)==15926 && peep_word(lose,4)==15926);
            assert(peeps[win*22]==8 && peeps[lose*22]==9);
            int frame=headed ? (owner ? 134 : 130) : 70;
            assert(peep_word(win,12)==frame && peep_word(lose,12)==frame);
            assert(map_bk2[650]==35 && core_mana(owner)==1000);
        } else {
            assert(peep_word(win,4)==29990 && peep_word(win,12)==(winner==2 ? 86 : 85));
            assert(peep_word(lose,12)==(route==1 ? 35 : 34));
            assert(peeps[win*22]==(headed ? 6 : 5));
            assert(peeps[lose*22]==(headed ? 0x80 : 0));
            assert(peep_word(lose,4)==headed);
            assert(peep_word(lose,6)==(headed ? (winner==1 ? 39 : 40) : 0));
            assert(core_mana(owner)==(route==1 ? 1104 : 1103));
            assert(map_bk2[650]==(headed ? 56 : 34));
            assert(map_who[650]==win+1 && view_who==win+1);
        }
        if (tick) {
            assert(A4_LONG(owner ? 0x9a0e : 0x99fe)==(route ? 29990u : 15926u));
            assert(A4_LONG(owner ? 0x99fe : 0x9a0e)==(route ? 10u+headed : 15936u));
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
        peeps[attacker*22]=8;
        peeps[defender*22]=destination ? flags[state]|8 : (flags[state]&7)|8;
        set_peep_word(attacker,6,defender); set_peep_word(defender,6,attacker);
        peeps[attacker*22+21]=1;
        if (destination) { map_who[649]=0; set_peep_word(attacker,8,650); }
        else map_who[650]=attacker+1;
        set_peep_word(attacker,12,base+successors[frame]);
        int defender_frame=destination && flags[state]==0x12 ? old_second : base+successors[reverse];
        set_peep_word(defender,12,defender_frame);
        memcpy(expected,prg_a4_area,sizeof expected); memcpy(prg_a4_area,before,sizeof before);
        core_move(attacker);
        assert(peep_word(attacker,12)==base+successors[frame]);
        assert(peep_word(defender,12)==defender_frame);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(core_battles==18 && !core_settlements && !core_result);
    }
    /* Current-cell admission precedes the nonpositive-attacker guard; destination
     * admission still follows it. Destination defenders retain the host's lack
     * of a population gate, but zero alone must skip frame initialization. */
    for (int destination=0; destination<2; ++destination)
    for (int population=-1; population<=0; ++population) {
        setup_battle_entry(1,0,destination); set_peep_word(1,4,population);
        core_move(1);
        assert(peep_word(1,12)==(!destination && population ? 70 : 35));
        assert(peep_word(2,12)==(destination ? 35 : 70));
        assert(peep_word(1,4)==population && peep_word(2,4)==100 && seed==123);
        assert(core_battles==(destination ? 17 : 18));
        setup_battle_entry(1,0,1); set_peep_word(2,4,population);
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
        _move_peeps();
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
        core_move(attacker);
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

static void test_battle_frame_render_and_entry(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO)==0); assert(game_reset(0));
    const int frames[]={70,71,72,130,131,132,134,135,136,138,139,140};
    const int invalid[]={0,35,73,129,133,137,141,32767,-32768,-1};
    uint32_t headings[]={0,1,UINT32_MAX,0x80000000u,0x00008000u};
    const int flags[]={8,9,0x18,0xff};
    for (int owner=0; owner<2; ++owner) for (int turn=0; turn<8; ++turn)
    for (unsigned raw=0; raw<sizeof headings/sizeof headings[0]; ++raw) {
        for (unsigned frame=0; frame<sizeof frames/sizeof frames[0]; ++frame)
            sprite_case(flags[frame%4],owner,frames[frame],headings[raw],turn,1,15,0,0,0,
                        frames[frame],192,56);
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
        seed=123; core_move(0);
        assert((peeps[0]&8) && (peeps[22]&8) && seed==123);
        assert(peep_word(0,12)==bases[owner][heads] && peep_word(1,12)==bases[owner][heads]);
        assert(peep_word(0,4)==100 && peep_word(1,4)==100);
        assert(peep_word(0,8)==cell+destination);
        assert(map_who[cell+destination]==(destination ? 2 : 1));
        uint8_t expected[64000], before[sizeof prg_a4_area];
        int occupant=map_who[cell+destination];
        map_who[cell+destination]=0;
        draw_original(16,16); memcpy(expected,game_pixels,sizeof expected);
        sprite_mask_oracle(expected,bases[owner][heads],destination ? 208 : 192,destination ? 64 : 56);
        map_who[cell+destination]=occupant;
        memcpy(before,prg_a4_area,sizeof before); draw_original(16,16);
        assert(!memcmp(before,prg_a4_area,sizeof before)); assert_sprite_pixels(expected);
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
        _move_peeps();
        if (memcmp(expected,prg_a4_area,sizeof expected))
            fprintf(stderr,"water town owner=%d fatal=%d war=%d heading=%d plot=%u pop=%u turn=%d flags=%x pop=%d frame=%d\n",
                    owner,fatal,war,heading,plot,sample,turn,peeps[0],peep_word(0,4),peep_word(0,12));
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(!core_battles && !core_settlements && seed==123);
        if (!fatal && evacuate && population<=0) {
            memcpy(expected,prg_a4_area,sizeof expected);
            _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    const int dead[]={0,-1,-32768};
    for (int owner=0; owner<2; ++owner) for (int fatal=0; fatal<2; ++fatal)
    for (unsigned sample=0; sample<3; ++sample) {
        setup_water_town(owner,5,650); game_mode=fatal; set_peep_word(0,4,dead[sample]);
        uint8_t expected[sizeof prg_a4_area]; memcpy(expected,prg_a4_area,sizeof expected);
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        }
        _move_peeps();
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
        assert(core_result==(!fertile && terminal && initial<5 ? (owner ? 1 : 2) : 0));
        assert(!core_battles && !core_settlements && seed==123); assert_mailbox_empty();
        if (!fertile && initial<5) {
            uint8_t record[22]; memcpy(record,peeps,22);
            game_tick(); assert(!memcmp(record,peeps,22));
            assert(map_who[650]==1 && view_who==1 && core_leader(owner)==1);
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
        _move_peeps();
        if (memcmp(expected,prg_a4_area,sizeof expected))
            fprintf(stderr,"water rescue owner=%d controller=%d war=%x mode=%x case=%u\n",
                    owner,controllers[controller],wars[war],(uint16_t)game_mode,sample);
        assert(!memcmp(expected,prg_a4_area,sizeof expected));
        assert(seed==123 && !core_battles && !core_settlements); ++checked;
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
            assert(core_result==(terminal ? (owner ? 1 : 2) : 0));
            assert(core_vertex(10,10)==!terminal && core_mana(owner)==(terminal ? 100 : 86));
            assert(core_population(1-owner)==100 && core_population(owner)==(terminal ? 0 : 100));
            assert_mailbox_empty();
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
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
    }
    /* Fatal old-water cannot erase an earlier pending owner command. */
    for (int owner=0; owner<2; ++owner) for (int war=0; war<2; ++war) {
        water_rescue_fixture(owner,1,0); game_mode=1; A4_WORD(0xae80)=war;
        terrain_mailbox_write(owner,1,27,31); A4_WORD(0x82be + owner*46)=7;
        uint8_t expected[sizeof prg_a4_area]; water_loss_expected(expected,owner,650,0);
        _move_peeps(); assert(!memcmp(expected,prg_a4_area,sizeof expected));
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
        }
        _move_peeps();
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
        for (int person=0; person<2; ++person) {
            int walker=person==reverse, side=separate && !walker ? 1-owner : owner;
            int position=person ? 780 : 650;
            int removed=walker ? dying : old && (fatal || dying);
            expected[peeps-prg_a4_area+person*22]=removed ? 0 : walker ? 0x42 : 18;
            celebration_expected_word(expected,0x9c26+person*22+4,
                                      removed ? 0 : walker ? 99 : old ? 98 : dying ? 1 : 100);
            if (walker) {
                celebration_expected_word(expected,0x9c26+person*22+6,7);
                celebration_expected_word(expected,0x9c26+person*22+12,0);
                expected[peeps-prg_a4_area+person*22+21]=(uint8_t)-1;
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
            assert(A4_WORD(0x82be + owner*46)==(tick<3)); assert_mailbox_empty();
        }
        game_tick();
        assert(peeps[0]==2 && peep_word(0,8)==651 && peep_word(0,10)==1);
        assert(peep_word(0,4)==90 && core_mana(owner)==54 && !core_result);
        assert(!map_who[650] && map_who[651]==1); assert_mailbox_empty();
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
            assert(A4_WORD(0x82be + owner*46)==(turn<3)); assert_mailbox_empty();
            assert(peeps[0]==(turn<10 ? 0x42 : 2));
            assert(peep_word(0,8)==(turn<10 ? 650 : 651));
        }
        assert(peep_word(0,10)==1 && map_who[651]==1 && !map_who[650] && !core_result);
        swamp_rescue_fixture(owner,100,0); A4_LONG(0x9a02+owner*16)=9;
        for (int turn=1; turn<=10; ++turn) {
            game_tick();
            assert(peep_word(0,8)==650 && peeps[0]==0x42 && peep_word(0,4)==100-turn);
            assert(map_blk[651]==53 && core_vertex(11,10)==0 && core_mana(owner)==9);
            assert(A4_WORD(0x82be + owner*46)==(turn<3 || turn==10)); assert_mailbox_empty();
        }
        for (int terminal=0; terminal<2; ++terminal) {
            swamp_rescue_fixture(owner,1,terminal); game_tick();
            assert(!peeps[0] && !peep_word(0,4) && !map_who[650]);
            assert(core_result==(terminal ? (owner ? 1 : 2) : 0));
            assert(map_blk[651]==(terminal ? 53 : 17));
            assert(core_vertex(11,10)==!terminal && core_mana(owner)==(terminal ? 100 : 86));
            assert_mailbox_empty();
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
        else assert(!A4_DISP(0x82b6+(1-owner)*46)[0]);
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

int main(int argc, char **argv)
{
    if (argc==2 && !strcmp(argv[1],"--swamp-rescue")) {
        test_swamp_rescue(); puts("focused swamp rescue: PASS"); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--magnet-rescue")) {
        test_magnet_rescue(); test_water_rescue(); test_war_terrain(); test_magnet();
        puts("focused magnet rescue: PASS"); return 0;
    }
    if (argc==3 && !strcmp(argv[1],"--rescue-every-tick")) {
        assert(getenv("POPULOUS_TEST_REAL_DATA"));
        if (!strcmp(argv[2],"isometric")) test_isometric_match_cadence(1,0);
        else { assert(!strcmp(argv[2],"events")); test_match_events_cadence(1,0); }
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--rescue-legacy-match")) {
        assert(getenv("POPULOUS_TEST_REAL_DATA"));
        test_isometric_match_cadence(8,1); test_match_events_cadence(8,1);
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
    test_magnet();
    test_war_state();
    test_remnant_timer(); test_remnant_occupancy_and_state();
    test_remnant_tick_and_water(); test_remnant_collision();
    test_flood_input_smoke(); test_flood_cast(); test_flood_delayed_hazards();
    test_original_lower();
    test_quake_cast();
    test_volcano_input_smoke(); test_volcano_cast(); test_volcano_lava();
    test_swamp_cast();
    test_swamp_hazard();
    test_blocked_entry();
    test_blocked_retry();
    test_wait_boundaries();
    test_wait_water_and_death();
    test_original_selection();
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
    test_swamp_render();
    test_original_reset();
    test_original_graphics_output();
    test_special_sprites(); test_battle_frame_render_and_entry();
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
    cleanup_rule_data();
    puts("game memory/start/render/input/lifecycle: PASS");
    return 0;
}
