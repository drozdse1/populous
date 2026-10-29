#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
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
    for (int step=0;step<8;++step) { assert(SDL_PushEvent(&key)==1); }
    ___keyboard();
    game_render();
    uint32_t pixels[320*256]; memcpy(pixels,prg_screen->chunky_buffer,sizeof pixels);
    SDL_Event click={.type=SDL_MOUSEBUTTONDOWN};
    click.button.button=SDL_BUTTON_LEFT; click.button.x=99; click.button.y=123;
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
    click.button.x=82; click.button.y=82;
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
    assert(core_result==2 && core_population(0)==0 && core_population(1)>0);
    key.key.keysym.sym=SDLK_ESCAPE;
    assert(SDL_PushEvent(&key)==1); ___keyboard();
    assert(prg_quit);
    _closeall(); SDL_Quit();
}

/* Synthetic rule files exercise mechanics, never pretend to be commercial LAND. */
static char test_data_directory[] = "/tmp/populous-game-test-XXXXXX";
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
}
static void cleanup_rule_data(void)
{
    if (getenv("POPULOUS_TEST_REAL_DATA")) return;
    for (int land=0; land<4; ++land) {
        char path[256]; snprintf(path,sizeof path,"%s/land%d",test_data_directory,land);
        assert(unlink(path)==0);
    }
    assert(rmdir(test_data_directory)==0);
}

int main(void)
{
    prepare_rule_data();
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
