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
    assert(peeps[0]==1 && !peep_word(1,4) && map_who[position]==1);
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
            core_tendency(owner)=0; core_leader(owner)=1; core_target(owner)=position;
            /* Flat center offers settlement without the surrounding town exclusion. */
            map_blk[position]=15;
            assert(core_check_life(owner,position)>0);
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
        assert(peeps[winner*22]==2 && map_who[50*64+50]==winner+1);
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
        assert(peeps[recipient*22]==2 && map_who[650]==recipient+1);
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
        assert(peep_word(0,4)==(fatal ? 0 : 98));
        assert(peeps[0]==(fatal ? 0 : 18) && map_who[650]==(fatal ? 0 : 1));
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

/* Synthetic consumer fixtures only: heading-winner production is not enabled. */
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
                assert(core_leader(owner)==1 && view_who==1 && core_target(owner)==1234);
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
    /* Keep the port's existing immediate water loss/fatal priority, not the
     * original's delayed newly-submerged loss/frame behavior (asm:3562-3580). */
    for (int fatal=0; fatal<=1; ++fatal)
        for (int population=1; population<=3; population+=2) {
            setup_remnant(0,40); set_peep_word(0,4,population);
            map_blk[650]=0; game_mode=fatal;
            _move_peeps();
            int survives=!fatal && population==3;
            assert(peeps[0]==(survives ? 0x12 : 0) && peep_word(0,4)==survives);
            assert(peep_word(0,6)==40 && map_who[650]==survives && peep_word(0,12)==42);
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
        /* Flat capacity 3050: original split leaves half in town, releases
         * 3100-1525, and transfers only that owner's leader (asm:3901-3929). */
        core_settle_tick(0);
        assert(no_peeps==3 && peep_word(2,4)==1575 && peeps[44]==2);
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

static void test_match_events(void)
{
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
        if (tick > 6000 && tick%8 == 0) {
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

/* Synthetic rule files exercise mechanics, never pretend to be commercial LAND. */
static char test_data_directory[] = "/tmp/populous-game-test-XXXXXX";
static void fixture_pixels(const char *name,size_t size,int keep)
{
    char path[512]; snprintf(path,sizeof path,"%s/pixels/%s",test_data_directory,name);
    FILE *file=fopen(path,"wb"); assert(file);
    for (size_t pixel=0;pixel<size;++pixel) assert(fputc(keep ? 1 : (pixel%15)+1,file)!=EOF);
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
    fixture_pixels("sprites0.dat.indices.u8",GFX_SPRITE_PIXELS,0);
    fixture_pixels("sprites0.dat.keep.u8",GFX_SPRITE_PIXELS,1);
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

static void test_isometric_match(void)
{
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
        if (tick>6000 && tick%8==0) for (int person=0;person<no_peeps;++person) {
            if (!peep_owner(person) || peep_word(person,4)<=0) continue;
            int position=peep_word(person,8);
            for (int dy=0;dy<2;++dy) for (int dx=0;dx<2;++dx)
                iso_click_vertex(position%64+dx,position/64+dy,SDL_BUTTON_RIGHT);
        }
    }
    fprintf(stderr,"isometric fight result=%d turns=%u battles=%d towns=%d pop=%d/%d\n",core_result,(uint16_t)game_turn,core_battles,core_settlements,core_population(0),core_population(1));
    assert(core_result && core_battles>0 && core_settlements>0);
    if (getenv("POPULOUS_TEST_REAL_DATA")) assert(core_result==1);
    event.key.keysym.sym=SDLK_ESCAPE; game_handle_event(&event); assert(prg_quit);
    _closeall(); SDL_Quit();
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
    /* No direct damage: existing next-tick water handling owns loss/removal,
     * leader/magnet cleanup, and town teardown. */
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

int main(int argc, char **argv)
{
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
    test_volcano_input(); test_volcano_render();
    test_flood_input();
    test_quake_input();
    test_swamp_input();
    test_swamp_render();
    test_original_reset();
    test_original_graphics_output();
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
