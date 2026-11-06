#include "../pop_log.h"
/*
 * modules/04_render_input.c
 * -------------------------
 * Unity-included from populous_prg.c (do not compile alone).
 * Original populous_prg.c lines 4764-5339.
 *
 * Frame draw, _move_peeps tick, Amiga/SDL shims, and input handling.
 * Prefer a4_state.h names over raw A4_WORD(0x…).
 */

/* ---- Peep framing / move_peeps ----------------------------------------- */

static int core_frame_walker(int person)
{
    if (!peep_word(person,4)) return 0;
    int flags = peeps[person*22], previous = peep_word(person,12);
    if (flags == 2) {
        set_peep_word(person,12,previous >= 7 ? 0 : previous+1);
        return previous >= 7;
    }
    /* asm:5833-5845: exact-5/6 simulation consumers, not general mask 0x04.
     * Keep the signed incremented word even on completion; rendering selects
     * the corresponding original sprites separately. */
    if (flags == 5 || flags == 6) {
        int frame = (int16_t)(previous+1);
        set_peep_word(person,12,frame);
        return frame < 85 || frame >= 88;
    }
    if ((flags&0x60) && !(flags&0x1c)) {
        int frame = (int16_t)(previous+1);
        int complete = frame < 101 || frame > 102;
        set_peep_word(person,12,complete ? 101 : frame);
        return complete;
    }
    return 0;
}
/* _move_peeps asm:4058-4067,4145-4184: an exact-2 walker still alive after
 * framing (and any move) marks the peep map_who names at its own cell. No
 * owner, liveness or no_peeps test. With the port's immediate destination
 * occupancy a successful move names the walker itself, so this fires only
 * when it stays on a cell represented by another peep. */
static void core_collision_wait(int person)
{
    if (peep_word(person,4) <= 0) return;
    int other = map_who[peep_word(person,8)]-1;
    if (other == person || other == -1 || peep_word(other,12) || other >= 208 ||
        (peeps[other*22]&0x10)) return;
    peeps[other*22] |= 0x20;
    set_peep_word(other,12,101); set_peep_word(other,6,0);
}
static void _move_peeps(void)
{
    /* asm:3254-3270: the computer player acts on last pass's pointers while
     * its latch is clear; the latch is re-read between the two calls. */
    for (int owner = 0; owner < 2; ++owner) {
        if (!stats_build_latch(owner)) core_set_devil_magnet(owner);
        if (!stats_build_latch(owner)) core_devil_effect(owner);
    }
    /* asm:3271-3335: per-pass AI bookkeeping, including the good_pop reset. */
    for (int owner = 0; owner < 2; ++owner) {
        core_town_strength[owner] = (int16_t)(stats_city_count(owner)*3 + stats_town_tally(owner));
        core_town_age(owner) = 0;
        core_good_pop(owner) = 0;
        core_city_count[owner] = 0; core_youngest_town_age[owner] = 0x4e1f;
        stats_best_town_ref(owner) = stats_oldest_town_ref(owner) = stats_youngest_town_ref(owner) = 0;
        core_oldest_town_age[owner] = 0; core_best_town_capacity[owner] = 0;
    }
    /* asm:3357-3424: release heading-bearing leaders before war recenters.
     * Presence, not liveness or reference validity, controls this prepass. */
    if (no_peeps >= 0 && no_peeps <= 208) for (int owner=0;owner<2;++owner) {
        int leader = core_leader(owner)-1;
        if (leader < 0 || leader >= no_peeps || leader >= 208) continue;
        if (peep_reference(leader) == -1) continue;
        int position = peep_word(leader,8);
        if (!pause && position >= 0 && position < 4096) {
            core_target(owner) = position;
            owner_magnet(owner) = position;
        }
        core_leader(owner) = 0;
    }
    /* asm:3425-3432: consume war state even with no living residents. */
    if (war_flag) {
        core_target(0) = core_target(1) = 0x820;
        god_magnet = devil_magnet = 0x820;
    }
    /* asm:3434-3454: sounds flagged by the last _move_sprite pass. */
    if (sound_effect & 8) effect = (int16_t)(toggle + 0x43);
    if (sound_effect & 0x80) { if (!last_fire) effect = 0x46; last_fire = 3; }
    else if (last_fire) --last_fire;
    /* Existing walker/town/battle/remnant consumption; nonwar AI remains disabled. */
    for (int person = 0; person < no_peeps; ++person) {
        if (peep_word(person,4) <= 0) continue;
        /* asm:3459-3474: sum the pre-movement word (EXT.L) by owner byte. */
        core_good_pop(peeps[person*22+1]) += (uint32_t)(int32_t)peep_word(person,4);
        int flags = peeps[person*22];
        int position = peep_word(person,8);
        /* Entry flag, before water mutation or land recovery (asm:3481-3543).
         * Retain the host battle exemption and existing recovery behavior. */
        if ((flags&16) && !(flags&8) && !(game_mode&GAME_MODE_WATER_FATAL))
            core_queue_terrain_raise(peep_owner(person),position);
        if (!map_blk[position] && !(flags&8)) {
            if (flags&16) {
                /* asm:3484-3490,3544-3559,4308-4315: fatal removal sees
                 * original flags; otherwise store the word before signed death.
                 * Still wet skips framing and all lower-priority consumers. */
                if (game_mode&GAME_MODE_WATER_FATAL) { core_remove(person); continue; }
                uint16_t loss = (uint16_t)(2u*(uint16_t)a4_walk_death);
                set_peep_word(person,4,(uint16_t)((uint16_t)peep_word(person,4)-loss));
                if (peep_word(person,4)<=0) core_remove(person);
                continue;
            }
            if (flags&1) core_set_town(person,1);
            peeps[person*22] = 18;
            /* asm:3562-3580: new water entry only reframes; exact 18 then
             * reaches the common positive-population check without other work. */
            if (peep_word(person,12) >= 8) set_peep_word(person,12,0);
            continue;
        }
        if ((flags == 0x11 || flags == 18 || flags == 0x15 || flags == 0x16 ||
             flags == 0x32 || flags == 0x52 || flags == 0x72 || flags == 0x90) && map_blk[position]) {
            /* asm:3484-3490,3544-3557: dry recovery still loses first.
             * Frame even a negative word, but never zero; ignore completion.
             * Exact-2 dies before navigation; 5/6 finish celebration first;
             * 0x22/42/62 finish waiting loss and any retry before cleanup.
             * Exact-0x80 framing is inert; even nonpositive loss reaches its timer. */
            if (game_mode&GAME_MODE_WATER_FATAL) { core_remove(person); continue; }
            uint16_t loss = (uint16_t)(2u*(uint16_t)a4_walk_death);
            set_peep_word(person,4,(uint16_t)((uint16_t)peep_word(person,4)-loss));
            peeps[person*22] = flags &= 0xef;
            /* asm:5728,5810-5832: zero skips this first frame, negative does
             * not. Exact-1 still enters the ordinary town consumer below:
             * evacuation bypasses death; eligible growth can revive the word.
             * This does not enable town AI, candidate selection or funny work. */
            if (flags == 1) {
                if (peep_word(person,4)!=0)
                    core_frame_town(person,core_check_life(peep_owner(person),position));
            } else core_frame_walker(person);
            if (flags == 2 && peep_word(person,4)<=0) { core_remove(person); continue; }
        } else if (flags&16) { peeps[person*22] = 2; set_peep_word(person,12,0); flags = 2; }
        /* asm:3584-3599, scoped to exact 5/6 after existing water handling.
         * Completion clears only mask 0x04 and reframes without town maintenance,
         * evacuation or walking movement. Always end this person's tick. */
        if (flags == 5 || flags == 6) {
            if (core_frame_walker(person)) {
                peeps[person*22] &= 0xfb;
                /* Exact-1 _set_frame branch, asm:5810-5832, not _set_town. */
                if (peeps[person*22] == 1)
                    core_frame_town(person,core_check_life(peep_owner(person),position));
                else core_frame_walker(person);
            }
            if (peep_word(person,4)<=0) core_remove(person);
            continue;
        }
        if (flags&8) {
            int other = peep_word(person,6);
            if (other > person && other < no_peeps && peep_word(other,4)>0) core_battle(person,other);
        } else if (flags&0x60) {
            /* _move_peeps asm:4213-4268: loss every tick, unsigned old counter. */
            core_frame_walker(person);
            set_peep_word(person,4,peep_word(person,4)-a4_walk_death);
            unsigned previous = (uint16_t)peep_word(person,6);
            set_peep_word(person,6,(uint16_t)(previous+1));
            if (previous > 14) {
                peeps[person*22] &= 0x9f;
                core_frame_walker(person);
                if (peeps[person*22] == 2) core_move(person);
            }
            /* A retry can create a reciprocal battle before attrition kills us. */
            if (peep_word(person,4)<=0) core_remove(person);
        } else if (flags == 1) core_settle_tick(person);
        else if (flags == 0x80) {
            /* asm:4283-4315: ordinary remnant, not a knight flag. Water and
             * waiting/battle dispatch stay ahead of the heading-winner remnant. */
            if (!map_who[position]) map_who[position] = person+1;
            unsigned previous = (uint16_t)peep_word(person,6);
            set_peep_word(person,6,(uint16_t)(previous-1u));
            if (!previous || peep_word(person,4)<=0) core_remove(person);
        } else {
            /* asm:4040 (CMPI.B #2; BNE LAB_411C0): only exact 2 walks. Other
             * residual flags reach only the signed death check, and are
             * positive here. */
            if (flags != 2) continue;
            if (!core_frame_walker(person)) { core_collision_wait(person); continue; }
            int position = peep_word(person,8);
            /* asm:4061-4081: ordinary frame completion, before move/loss.
             * Water cells never get here: they end the turn at the top. */
            if (map_blk[position] == 53) {
                core_remove(person);
                effect = 66;
                if (!(game_mode&2)) map_blk[position] = 15;
                continue;
            }
            /* asm:4092-4122: first walker of the pass for the enemy's AI
             * (host slot+1 encoding), then terrain flattening. */
            int owner = peep_owner(person);
            if (!stats_oldest_town_ref(owner ? 0 : 1)) stats_oldest_town_ref(owner ? 0 : 1) = (uint32_t)(person+1);
            if (!stats_build_latch(owner)) core_one_block_flat(position,owner);
            core_move(person);
            /* asm:4100-4117: drop a stale +0x12 cell, then this path's loss. */
            if (peep_word(person,18) &&
                (int16_t)(peep_word(person,8)-peep_word(person,10)) != peep_word(person,18))
                set_peep_word(person,18,0);
            set_peep_word(person,4,peep_word(person,4)-a4_walk_death);
            core_collision_wait(person);
            if (peep_word(person,4)<=0) core_remove(person);
        }
    }
    /* asm:4349-4370: publish this pass's town and city counts. */
    for (int owner = 0; owner < 2; ++owner) {
        stats_town_tally(owner) = core_town_age(owner);
        stats_city_count(owner) = core_city_count[owner];
    }
}
static void _show_the_shield(void) { core_shield_state(); }
static void _sculpt(void) {}
static void _zoom_map(int16_t d) {}
/* _set_temp_view asm:3151-3160; restored by the countdown in game_tick. */
static void core_set_temp_view(int16_t who)
{
    if (!view_timer) old_view_who = view_who;
    view_timer = 10;
    view_who = who;
}
/* _interogate asm:2719-2765: first sprite entry with mx in [x,x+12] and
 * my in [y,y+8]; query mode only. Right keeps the view, else temporary. */
static void core_interogate(int mx, int my, int right)
{
    for (int i = 0; i < no_sprites && i < 64; ++i) {
        int16_t x = sprite_x(i), y = sprite_y(i), who = sprite_who(i);
        if (mx < x || mx > x+12 || my < y || my > y+8) continue;
        if (core_tool == 2) {
            if (right) { view_timer = 0; view_who = who; }
            else core_set_temp_view(who);
        }
        return;
    }
}
static void _interogate(void) { core_interogate(mousex,mousey,right_button); }
/* xoff/yoff = cell column/row - 3; ASR.W #6 floors negative rows. */
static void core_view_cell(int16_t cell)
{
    xoff = (int16_t)((cell & 63) - 3);
    yoff = (int16_t)((cell >= 0 ? cell/64 : -((-cell+63)/64)) - 3);
}
/* Battle cycle, asm:2229-2290. The original never stops when no battle exists
 * and view_fight+1 == no_peeps (the wrap skips the end test); bound the scan. */
static void core_next_battle(void)
{
    if (!no_peeps) return;
    if (view_fight >= no_peeps) view_fight = 0;
    int start = view_fight + 1, person = start;
    for (int seen = 0; seen <= no_peeps; ++seen, ++person) {
        if (person >= no_peeps) person = 0;
        if (person == start && seen) return;
        if ((peeps[person*22] & 0x08) && peep_word(person,4)) {
            core_view_cell(peep_word(person,8));
            view_fight = (int16_t)person;
            core_set_temp_view((int16_t)(person+1));
            return;
        }
    }
}
/* Own-walker cycle, asm:2291-2395: left picks peeps with a heading reference,
 * right picks ordinary walkers (type 1); both need positive population. */
static void core_next_person(int left)
{
    if (!no_peeps) return;
    if (view_people >= no_peeps) view_people = 0;
    int start = view_people + 1, person = start;
    for (int seen = 0; ; ++person) {
        if (person == start && seen) return;
        if (person == no_peeps) person = 0;
        seen = 1;
        if (peep_owner(person) != player || peep_word(person,4) <= 0) continue;
        uint32_t reference; memcpy(&reference,peeps+person*22+14,sizeof reference);
        if (left ? reference != 0 : peeps[person*22] == 1) {
            core_view_cell(peep_word(person,8));
            view_people = (int16_t)person;
            core_set_temp_view((int16_t)(person+1));
            return;
        }
    }
}
/* Toolbar clicks of _zoom_map, asm:1700-2451 (power grid jump table asm:2406-2424).
 * left is the original argument (left button held). Mailbox posts happen only
 * when post is set; costs, permissions, war and pause stay with the consumer
 * and the power routines. Swamp alone checks mana > cost and permission bit 4
 * before arming the map target. Camera, tool and audio-flag icons always act.
 * Returns whether (sx,sy) hits a handled icon. */
static int core_toolbar(int sx, int sy, int left, int post)
{
    int c = sx/2 + sy - 32, r = sy - sx/2 + 32;
    if (sy >= 200) return 0;
    int owner_ok = player >= 0 && player <= 1;
    uint8_t *mailbox = owner_ok ? stats_bytes(player) : NULL;
    post = post && owner_ok;
    if (c > 0x112) {
        /* System icons asm:1700-1829: options, game options, pause, sound. */
        int icon = (c-0x110)/16, line = (r-0x20)/16;
        /* d_screen flashes asm:1717-1727,1783-1793,1810-1818; sound icons
         * toggle _back_scr instead (asm:1733-1781), pause none. */
        if ((icon == 0 && line == 0) || (icon == 1 && line == 1) || (icon == 2 && line == 2)) core_flash_icon(0x17e4,icon,line);
        if (icon == 0 && line == 0) { if (post) { mailbox[0] = 14; mailbox[1] = (uint8_t)left; mailbox[2] = 7; } }
        /* asm:1744-1781: switching on also stops all four channels. */
        else if (icon == 0 && line == 3) { if ((music_off = !music_off)) core_kill_effect(0,4); }
        else if (icon == 0 && line == 4) { if ((effect_off = !effect_off)) core_kill_effect(0,4); }
        else if (icon == 1 && line == 1) { if (post) { mailbox[0] = 14; mailbox[2] = 8; } }
        /* _do_action y=6 asm:18591-18608. The port's pause also stops the
         * mailbox consumer, so this icon toggles directly rather than posting. */
        else if (icon == 1 && line == 3) { pause = !pause; toggle = 0; }
        else if (icon == 2 && line == 2) { if (post) { mailbox[0] = 14; mailbox[2] = 2; } }
        else return 0;
        return 1;
    }
    if (r < 0x92) return 0;
    int icon = (c-0x60)/16, line = (r-0x90)/16;
    if (icon < 0 || icon > 8) return 0;
    if (icon >= 3 && icon <= 5 && line <= 2) {
        /* Scroll pad asm:1838-1902; its centre recentres on view_who. All
         * nine cells flash first (asm:1842-1851). */
        core_flash_icon(0x12c0,icon,line);
        if (icon == 4 && line == 1) {
            if (view_who > 0 && view_who <= no_peeps) core_view_cell(peep_word(view_who-1,8));
        } else {
            xoff = (int16_t)(xoff + (icon == 5) - (icon == 3));
            yoff = (int16_t)(yoff + (line == 2) - (line == 0));
        }
    } else if (icon <= 2) {
        if (line > 2) return 0;
        /* Flashes asm:1930-1940,1952-1975,1984-2017: all but icon 0 lines
         * 1,2 and the papal icon under _paint_map; swamp flashes even unarmed. */
        if (!(icon == 0 && line) && !(icon == 1 && line == 2) && !(icon == 1 && line == 0 && paint_map)) core_flash_icon(0x12c0,icon,line);
        if (!post) return 1;
        if (icon == 0 && line == 0) { mailbox[0] = 14; mailbox[2] = 4; }
        else if (icon == 1 && line == 0) { if (!paint_map) { mailbox[0] = 14; mailbox[2] = 3; } }
        else if (icon == 1 && line == 1) { mailbox[0] = 6; mailbox[1] = (uint8_t)xoff; mailbox[2] = (uint8_t)yoff; }
        else if (icon == 2 && line == 0) { mailbox[0] = 3; mailbox[1] = (uint8_t)xoff; mailbox[2] = (uint8_t)yoff; }
        else if (icon == 2 && line == 1) { mailbox[0] = 14; mailbox[2] = 5; }
        else if (icon == 2 && line == 2) {
            if ((core_mana(player) > (int32_t)mana_swamp_cost && (stats_power_mask(player)&0x10)) || paint_map)
                { core_swamp_selected = 1; core_set_pointer(); }
        }
    } else if (icon <= 5) {
        /* Tendency icons asm:2043-2089: _do_action y=1 with x = tendency. */
        static const int8_t tendency[3][2] = {{0,-1},{1,3},{2,-1}};
        if (line < 3 || line > 4 || tendency[icon-3][line-3] < 0) return 0;
        if (post) { mailbox[0] = 14; mailbox[1] = (uint8_t)tendency[icon-3][line-3]; mailbox[2] = 1; }
    } else if (icon == 6) {
        /* Mode icons asm:2090-2133: 0 query, 1 terrain, 2 papal magnet; each
         * clears the swamp bit. core_tool: 0 terrain, 1 magnet, 2 query. */
        if (line > 2) return 0;
        core_tool = line == 0 ? 2 : line == 2; core_swamp_selected = 0; core_set_pointer();
    } else if (icon == 7) {
        /* asm:2134-2228: left views the leader (else the magnet), right the magnet. */
        if (line <= 1) core_flash_icon(0x12c0,icon,line);   /* asm:2134-2142,2226-2235 */
        if (line == 0) {
            if (!owner_ok) return 1;
            int16_t leader = core_leader(player);
            if (left && leader > 0 && leader <= no_peeps) {
                core_view_cell(peep_word(leader-1,8));
                core_set_temp_view(leader);
            } else if (left || core_target(player)) core_view_cell(core_target(player));
        } else if (line == 1) core_next_battle();
        else return 0;
    } else {
        if (line != 0) return 0;
        core_flash_icon(0x12c0,icon,line);   /* asm:2296-2305 */
        core_next_person(left);
    }
    /* Common tail asm:2425-2440. */
    if (xoff < 0) xoff = 0;
    if (xoff > 56) xoff = 56;
    if (yoff < 0) yoff = 0;
    if (yoff > 56) yoff = 56;
    return 1;
}
static void _do_place_funny(int16_t a) { core_place_funny(a,0); }

/* ___ shims (SDL / stdlib backed) */
static void *amiga_alloc_mem(uint32_t s,uint32_t a){(void)a;return calloc(1,s);}
static void amiga_free_mem(void *p){free(p);}
static void ___open_screen(void)
{
    ensure_prg_screen();
    POP_LOGD("SDL", "open_screen (real window created)");
}
static void ___read_sprites(int16_t n){POP_LOGD("PRG", "read_sprites %d",n);}
static void ___sprite_to_amiga(void){POP_LOGD("PRG", "sprite_to_amiga");}
static void ___start_game(void){POP_LOGD("PRG", "start_game"); core_sound_start();}
static void ___set_serial(void){POP_LOGD("PRG", "set_serial");}
static void ___load_ground(int16_t a, int16_t terrain)
{
    /* Rule tables and indexed graphics are loaded transactionally by game_reset. */
    (void)a; (void)terrain;
}
static void ___Setscreen(int16_t m,void *s1,void *s2){POP_LOGD("SDL", "Setscreen");}
static void ___copy_screen(void *d,void *s){POP_LOGD("SDL", "copy_screen");}
static void ___toggle_icon(int16_t i,int16_t a,int16_t b,void *scr){POP_LOGD("SDL", "toggle_icon 0x%04x",(uint16_t)i);}
static void ___draw_it(int16_t y,int16_t x){_draw_it(y, x);}
static void ___draw_s_32(int16_t x,int16_t y,void *scr){POP_LOGD("SDL", "draw_s_32");}
static void ___draw_sprite(int16_t f,int16_t x,int16_t y,void *scr){POP_LOGD("SDL", "draw_sprite");}
static void ___clr_wsc(void)
{
    /* Called every frame before main draw in the _animate path.
     * For the demo, we can use it to occasionally "age" or reset minor overlay state
     * (e.g. help the shield timer or dim a temp effect). Keeps the transcribed loop active.
     */
    if (demo_shield_timer > 0 && (growth_tick & 1)) {
        demo_shield_timer--;  /* gentle decay if not refreshed */
    }
    /* printf removed for clean demo runs */
}
static void ___swap_screens(void)
{
    ensure_prg_screen();
    if (prg_screen) amiga_present(prg_screen);
}
static int  ___show_world(void){POP_LOGD("PRG", "show_world");return 0;}
static void ___return_to_game(void){POP_LOGD("PRG", "return_to_game");}
static void ___display_debug(void){POP_LOGD("PRG", "display_debug (1993)");}
static void ___paint_the_map(void *p){POP_LOGD("PRG", "paint_the_map");}
static void ___text(const char *s,int16_t y,int16_t x,void *scr){POP_LOGD("SDL", "text %s",s?s:"");}
static void ___a_putpixel(int16_t c,int16_t pos){POP_LOGD("SDL", "a_putpixel");}
static void ___keyboard(void)
{
    SDL_Event event;
    while (!prg_quit && SDL_PollEvent(&event)) game_handle_event(&event);
}

/* ---- Input / SDL events --------------------------------------------- */

static void game_handle_event(const SDL_Event *event)
{
    if (event->type == SDL_QUIT) { prg_quit = 1; return; }
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_ESCAPE) { prg_quit = 1; return; }
        if (core_req.kind && core_disk_message) return;   /* _do_message reads only the mouse */
        /* _show_world's _get_name reads ___keyboard (_asckey). */
        if (core_req.kind == CORE_REQ_WORLD) {
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER) core_world_key(13);
            else if (key >= 0 && key < 0x80) core_world_key((int)key);
            return;
        }
        if (core_req.kind == CORE_REQ_FILE) {   /* _save_load's _get_name */
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER) core_file_key(13);
            else if (key >= 0 && key < 0x80) core_file_key((int)key);
            return;
        }
        if (core_req.kind || core_lord.shown) return;   /* the requester and lord loops read only the mouse */
        /* _end_game waits for the requester (asm:14370-14385): any other key. */
        if (core_end_shown && core_result) { if (!event->key.repeat) core_leave_end(); return; }
        if (!event->key.repeat) {
            /* _animate asm:917: unhandled keys go to _paint_the_map, which
             * acts only with _paint_map set (Amiga raw codes). */
            int raw = key >= SDLK_F1 && key <= SDLK_F8 ? 0x50 + (int)(key - SDLK_F1) :
                      key == SDLK_DELETE ? 0x46 : key >= SDLK_1 && key <= SDLK_9 ? (int)(key - SDLK_0) : -1;
            if (raw >= 0 && core_paint_the_map(raw, (event->key.keysym.mod & KMOD_SHIFT) != 0)) return;
            if (key == SDLK_SPACE) pause = !pause;
            if (key == SDLK_d) { diagnostic_view = !diagnostic_view; xoff = yoff = diagnostic_view ? 0 : 6; }
            if (key == SDLK_r && !game_reset_start(current_start))
            POP_LOG_ERROR("Reset failed; keeping the current game.");
            if (!pause && !core_result && key >= SDLK_0 && key <= SDLK_3) core_tendency(0) = key-SDLK_0;
            if (key == SDLK_m) { core_tool = core_tool != 1; core_set_pointer(); }
            if (key == SDLK_q) _do_quake();
            if (key == SDLK_f) _do_flood();
            if (key == SDLK_v) _do_volcano();
            if (key == SDLK_k) _do_knight();
            /* Host binding, not original A (which was a debug win). */
            if (key == SDLK_a) _do_war();
            /* Host S mapping for source selection asm:2021-2042 (> cost). */
            if (key == SDLK_s && player >= 0 && player <= 1 &&
                ((core_mana(player) > (int32_t)mana_swamp_cost &&
                  (stats_power_mask(player)&0x10)) || paint_map)) { core_swamp_selected = 1; core_set_pointer(); }
        }
        if ((key == SDLK_LEFT || key == SDLK_KP_4 || key == SDLK_KP_7 || key == SDLK_KP_1) && xoff > 0) --xoff;
        if ((key == SDLK_RIGHT || key == SDLK_KP_6 || key == SDLK_KP_9 || key == SDLK_KP_3) && xoff < 56) ++xoff;
        if ((key == SDLK_UP || key == SDLK_KP_8 || key == SDLK_KP_7 || key == SDLK_KP_9) && yoff > 0) --yoff;
        if ((key == SDLK_DOWN || key == SDLK_KP_2 || key == SDLK_KP_1 || key == SDLK_KP_3) && yoff < 56) ++yoff;

    }
    if (event->type == SDL_MOUSEMOTION && prg_screen) {
        int width, height;
        SDL_GetWindowSize(prg_screen->window, &width, &height);
        if (width > 0 && height > 0) {
            int x = event->motion.x, y = event->motion.y;
            if (x < 0) x = 0;
            if (x >= width) x = width - 1;
            if (y < 0) y = 0;
            if (y >= height) y = height - 1;
            mousex = (int16_t)((int64_t)x * prg_screen->width / width);
            mousey = (int16_t)((int64_t)y * prg_screen->height / height);
            big_mousex = mousex;
            big_mousey = mousey;
            /* _sculpt asm:7406-7416 keeps _cur_x/_cur_y on the vertex under
             * the pointer; _paint_the_map posts there. */
            int column, row;
            if (!diagnostic_view && !core_req.kind && game_pick(mousex,mousey,&column,&row)) {
                cur_x = (int16_t)column; cur_y = (int16_t)row;
            }
            /* continue_3EC20 asm:963-975 also interrogates while hovering. */
            if (!event->motion.state && !diagnostic_view && !core_req.kind && !core_lord.shown) core_interogate(mousex,mousey,0);
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && prg_screen) {
        int width,height; SDL_GetWindowSize(prg_screen->window,&width,&height);
        if (width <= 0 || height <= 0 || event->button.x < 0 || event->button.y < 0 ||
            event->button.x >= width || event->button.y >= height) return;
        int sx = (int64_t)event->button.x*320/width, sy = (int64_t)event->button.y*256/height;
        /* Requesters take the left press at the pointer (_left_button,
         * _mousex/_mousey); _end_game continues only on its button. */
        if (core_req.kind || core_lord.shown || (core_end_shown && core_result)) {
            if (event->button.button != SDL_BUTTON_LEFT) return;
            mousex = (int16_t)sx; mousey = (int16_t)sy;
            if (core_lord.shown) core_lord_leave();   /* _left_button cleared, asm:14503 loop */
            else if (core_req.kind && core_disk_message) core_disk_click();
            else if (core_req.kind) core_requester_click();
            else { core_end_button_layout(0); if (core_check_cancel(core_end_button,NULL)) core_leave_end(); }
            return;
        }
        int column,row;
        if (!diagnostic_view && sy<64 && sx>=1 && sx<=127) {
            column=sy+(sx-64)/2; row=sy-(sx-64)/2;
            if (column>=0 && column<64 && row>=0 && row<64) {
                if (event->button.button==SDL_BUTTON_LEFT) {
                    xoff=column<4 ? 0 : column>60 ? 56 : column-4;
                    yoff=row<4 ? 0 : row>60 ? 56 : row-4;
                }
                return;
            }
        }
        /* A paused or finished game never consumes mailboxes; drop the post.
         * _zoom_map runs only for the left or right button (asm:976-989). */
        if (!diagnostic_view && (event->button.button == SDL_BUTTON_LEFT ||
            event->button.button == SDL_BUTTON_RIGHT) &&
            core_toolbar(sx,sy,event->button.button == SDL_BUTTON_LEFT,
                         !pause && !core_result)) return;
        /* Query mode skips _sculpt: clicks only interrogate (asm:963-975). */
        if (core_tool == 2 && !core_swamp_selected) {
            if (!diagnostic_view && (event->button.button == SDL_BUTTON_LEFT ||
                event->button.button == SDL_BUTTON_RIGHT))
                core_interogate(sx,sy,event->button.button == SDL_BUTTON_RIGHT);
            return;
        }
        /* _sculpt asm:7295 / _do_magnet asm:6784: keep camera usable,
         * but reject world input before consuming an armed target. */
        if (pause || core_result || war_flag) return;
        if (diagnostic_view) {
            if (sy>=240) return;
            column=(sx+xoff)/4; row=(sy+yoff)/4;
            if (column>=64 || row>=64) return;
        } else if (!game_pick(sx,sy,&column,&row)) return;
        if (core_swamp_selected) {
            if (event->button.button == SDL_BUTTON_LEFT) {
                /* asm:7451-7489 clears selection before command execution.
                 * Diagnostic picks use this same vertex-to-byte mapping. */
                core_swamp_selected = 0; core_set_pointer();
                core_swamp(player,(column-1)&255,(row-1)&255,___newrand);
            }
            return;
        }
        if (core_tool == 1) {
            if (column>=64 || row>=64) return;
            if (event->button.button == SDL_BUTTON_LEFT && core_leader(0) && core_mana(0)>=200) {
                core_add_mana(0,-200); core_target(0)=row*64+column; core_tendency(0)=0;
            }
        } else if (paint_map && (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)) {
            /* _sculpt asm:7307-7312,7417-7438: the editor skips _ok_to_build
             * and the game-mode build limits; the raise/lower opcodes then
             * run free (_do_raise_point/_do_lower_point). */
            cur_x = (int16_t)column; cur_y = (int16_t)row;
            if (event->button.button == SDL_BUTTON_LEFT) core_raw_raise(player,column,row);
            else if (column <= 64 && row <= 64) core_do_lower_point(player,column,row);
        } else if (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)
            core_sculpt(0,column,row,event->button.button == SDL_BUTTON_LEFT ? 1 : -1);
    }
    /* Unsupported special powers remain inert. */
}
static void ___setup_mouse(void){POP_LOGD("SDL", "setup_mouse");}
static void ___AddIntServer(int16_t l,void *is){POP_LOGD("PRG", "AddIntServer %d",l);}
static void ___RemIntServer(int16_t l,void *is){POP_LOGD("PRG", "RemIntServer %d",l);}
static void ___Delay(int16_t t){/*SDL_Delay*/;}
static void ___reset_screen(void *v){POP_LOGD("SDL", "reset");}
static void ___free_amiga_sprites(void){POP_LOGD("PRG", "free sprites");}
static void ___free_all_sounds(void){core_free_all_sounds();}
static void ___close_channels(void){audio_close();}
static void *___FindTask(void *t){return (void*)1;}
static void ___exit(int16_t c){POP_LOGD("PRG", "exit");exit(c);}
static int32_t ___mulu(int32_t a,int32_t b){return a*b;}
static int32_t ___divs(int32_t a,int32_t b){return b?a/b:0;}
/* Correct implementation of the game's _newrand (from the disassembly BANNER and tetracorp analysis).
 * It is a 16-bit LCG that only produces values 0..32767.
 * Uses the emulated global 'seed' variable (A4 offset 0x99e2).
 */
static uint16_t ___newrand(void)
{
    uint16_t d0 = seed;

    /* MULU #$24a1 */
    uint32_t temp = (uint32_t)d0 * 0x24a1;
    d0 = (uint16_t)temp;

    /* ADDI.W #$24df */
    d0 += 0x24df;

    /* BCLR #$F  (clear bit 15) */
    d0 &= 0x7fff;

    seed = d0;   /* write back to the global the game uses */
    return d0;
}

/* When you paste the next chunk (more of _animate, _move_peeps, _draw_map, any _do_* etc.)
 * we will replace the corresponding stub or extend the logic above with the real 68k translation,
 * keeping A4 + SDL low-level + all the 1993 notes.
 */

