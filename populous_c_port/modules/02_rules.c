#include "../pop_log.h"
/*
 * modules/02_rules.c
 * ------------------
 * Unity-included from populous_prg.c (do not compile alone).
 * Original populous_prg.c lines 798-3187.
 *
 * Map vertices, population / battles / AI, divine powers, world generation,
 * animate / reset. Prefer a4_state.h names over raw A4_WORD(0x…).
 */

/* ---- Map vertices / make_map ------------------------------------------- */

static int core_vertex(int column, int row)
{
    return map_vertex_word(column, row);
}
static void core_set_vertex(int column, int row, int height)
{
    map_vertex_word(column, row) = height;
}
static void core_make_map_region(int left, int top, int right, int bottom)
{
    for (int row = top; row <= bottom; ++row) {
        for (int column = left; column <= right; ++column) {
            int corners[] = {core_vertex(column,row), core_vertex(column+1,row),
                             core_vertex(column+1,row+1), core_vertex(column,row+1)};
            int altitude = (corners[0]+corners[1]+corners[2]+corners[3]) / 4;
            int block = 0, cell = row * 64 + column;
            for (int corner = 0; corner < 4; ++corner)
                if (corners[corner] > altitude) block |= 1 << corner;
            /* asm:1475-1517: preserve rock only while any land remains;
             * protected flat rocks keep the unadjusted average altitude. */
            int protected_rock = map_blk[cell] == 47 && (block || altitude);
            if (protected_rock) block = 47;
            if (altitude && !block) { --altitude; block = 15; }
            if (!altitude && block && block != 15) block += 16;
            map_alt[cell] = altitude;
            if (protected_rock) block = 47;
            map_blk[cell] = block;
            if (!block) map_bk2[cell] = 0;
            map_step_word(cell) = 0;
        }
    }
}

static void _make_map(void)
{
    core_make_map_region(0,0,63,63);
}

static int core_result, core_battles, core_settlements, core_tool;
static int core_swamp_selected;
static int core_shake_dy; /* terrain-only vertical offset, quake shake */
static const int16_t core_offsets[25] = {
    0,-64,1,64,-1,-63,65,63,-65,-128,2,128,-2,-126,130,126,-130,
    -127,-62,66,129,127,62,-66,-129
}; /* asm:24274 */
static const uint16_t core_city[9] = {42,44,43,44,43,41,41,41,41};
static const int32_t core_mana_values[11] = {-250,10,200,2500,5000,7500,10000,40000,80000,160000,1999999};
/* ---- Population / mana ----------------------------------------------- */

static int core_population(int owner)
{
    int total = 0;
    for (int person = 0; person < no_peeps; ++person)
        if (peep_owner(person) == owner && peep_word(person,4) > 0) total += peep_word(person,4);
    return total;
}
/* good_pop/bad_pop: signed longs at A4+$99fe, stride 16 (asm:25331). The
 * index is the raw owner byte or word, scaled without an owner range check. */
static void core_add_mana(int owner, int amount)
{
    int64_t value = (int64_t)core_mana(owner) + amount;
    if (value < -250) value = -250;
    if (value > 1999999) value = 1999999;
    core_mana_long(owner) = (uint32_t)(int32_t)value;
}
/* _valid_move asm:20912-20954. */
static int core_valid_move(int position, int delta)
{
    if (position < 0 || position >= 4096) return 1;
    if (!delta) return 0;
    int destination = position + delta, horizontal = delta & 63;
    if (horizontal > 3) horizontal -= 64;
    int column = (position & 63) + horizontal;
    if (destination < 0 || destination >= 4096 || column < 0 || column > 63) return 1;
    if (map_blk[destination] == 0) return 3;
    return map_blk[destination] == 47 ? 2 : 0;
}
/* _check_life asm:20958-21042, including neighbouring settlement exclusion. */
static int core_check_life(int owner, int position)
{
    int life = 0;
    life_swamp_count = life_water_flag = 0;
    for (int index = 0; index < 17; ++index) {
        int valid = core_valid_move(position,core_offsets[index]);
        if (valid) { if (valid == 2) life -= 15; continue; }
        int cell = position+core_offsets[index];
        if (map_blk[cell] == 31+owner || map_blk[cell] == 15) {
            if (map_blk[cell] == 15) life_water_flag = 1;
            if (!life) life = 50;
            life += 15;
        } else if (!index) return 0;
        int building = map_bk2[cell];
        if (index < 9 && map_bk2[position] == 42 && building >= 41 && building <= 44) ++life_swamp_count;
        else if (index && building > 32 && building <= 44) return 0;
    }
    if (life < 35) return 0;
    return life == 305 ? 3050 : life;
}
/* _set_town asm:5865-6062. Original block IDs, simplified drawing only. */
static void core_set_town(int person, int remove)
{
    int position = peep_word(person,8), owner = peep_owner(person);
    int old_city = map_bk2[position] == 42, city = !remove && peep_word(person,12) == 42;
    if (remove || (old_city && !city)) {
        for (int index = 0; index < (old_city ? 25 : 17); ++index) {
            if (core_valid_move(position,core_offsets[index]) == 1) continue;
            int cell = position+core_offsets[index];
            if (old_city || index < 9) map_bk2[cell] = 0;
            if (map_blk[cell] == 31+owner) map_blk[cell] = 15;
        }
        map_bk2[position] = 0;
        if (remove) return;
    }
    for (int index = 0; index < (city ? 25 : 17); ++index) {
        if (core_valid_move(position,core_offsets[index]) == 1) continue;
        int cell = position+core_offsets[index];
        if (index < 9) map_bk2[cell] = city ? core_city[index] : 0;
        if (map_blk[cell] == 15) map_blk[cell] = 31+owner;
    }
    if (!city && map_blk[position] == 31+owner) map_bk2[position] = peep_word(person,12);
}
static void core_remove(int person)
{
    int owner = peep_owner(person), position = peep_word(person,8);
    if (peeps[person*22]&1) core_set_town(person,1);
    if (map_who[position] == person+1) map_who[position] = 0;
    /* _zero_population asm:5671-5680: also the previous departure's claim. */
    int previous = (int16_t)(position-peep_word(person,10));
    if (previous >= 0 && previous < 4096 && map_who[previous] == person+1) map_who[previous] = 0;
    if (peeps[person*22]&8) {
        int partner = peep_word(person,6);
        /* Walking attrition can remove an attacker immediately after entry,
         * outside core_battle's winner path. Release only its reciprocal link. */
        if (partner >= 0 && partner < no_peeps && partner != person &&
            peep_word(partner,4) > 0 && (peeps[partner*22]&8) &&
            peep_word(partner,6) == person) {
            peeps[partner*22] = (peeps[partner*22]&1) ? 1 : 2;
            set_peep_word(partner,6,0); set_peep_word(partner,10,0);
            if (!(peeps[partner*22]&1)) set_peep_word(partner,12,0);
            int cell = peep_word(partner,8);
            if (!map_who[cell]) map_who[cell] = partner+1;
        }
        set_peep_word(person,6,0);
    }
    if (core_leader(owner) == person+1) {
        core_leader(owner) = 0;
        /* _zero_population / _set_magnet_to asm:5691-5716,8271-8291. */
        if (!pause) {
            core_target(owner) = position;
            owner_magnet(owner) = position;
        }
    }
    if (view_who == person+1) view_who = 0;
    set_peep_word(person,4,0); peeps[person*22] = 0;
}
/* "Funny" objects: peep slots $D1/$D2 (_peeps+$11F6 = LAB_5420A, A4 0xae1c),
 * the only users of the 32x32 sheet. _funny asm:24161-24171 (A4 0x8292),
 * 12 bytes per type: step, first frame (low byte of +2), last frame (low
 * byte of +4), then three trail offsets read at LAB_51686. */
#define CORE_FUNNY_FIRST 0xd1
#define CORE_FUNNY_END   0xd3
static const int16_t core_funny[3][6] = {
    {-64, 0, 4,  0,-1, 1},
    {-65, 5, 8,  0,-64,-1},
    { 65, 9,12,  0, 64, 1},
};
static void core_zero_funny(int slot)
{
    set_peep_word(slot,4,0);
    int position = peep_word(slot,8), previous = (int16_t)(position-peep_word(slot,10));
    if (position >= 0 && position < 4096 && map_who[position] == slot+1) map_who[position] = 0;
    if (previous >= 0 && previous < 4096 && map_who[previous] == slot+1) map_who[previous] = 0;
    if (view_who == slot+1) view_who = 0;
}
static void core_place_funny(int type, int edge)
{
    if (type < 0 || type > 2) return;
    for (int slot = CORE_FUNNY_FIRST; slot < CORE_FUNNY_END; ++slot) {
        if (peep_word(slot,4)) continue;
        set_peep_word(slot,4,1); peeps[slot*22] = 2;
        int position = peep_word(slot,8);
        if (edge == 0) position = ((int16_t)___newrand()%125)/2 + 0xfc0;
        else if (edge == 1 || edge == 2) {
            int odd = ___newrand() & 1, value = (int16_t)___newrand()%43;
            if (edge == 1) position = (value+20)*64 + (odd ? 63 : 0xfc0);
            else position = odd ? value*64 : value;
        }
        set_peep_word(slot,8,(int16_t)position);
        if (position >= 0 && position < 4096) map_who[position] = (uint8_t)(slot+1);
        set_peep_word(slot,10,core_funny[type][0]);
        peeps[slot*22+2] = (uint8_t)core_funny[type][2];
        peeps[slot*22+3] = (uint8_t)core_funny[type][1];
        peeps[slot*22+21] = 1;
        set_peep_word(slot,6,core_funny[type][1]);
        peeps[slot*22+20] = (uint8_t)type;
        return;
    }
}
static void core_do_funny(void)
{
    for (int slot = CORE_FUNNY_FIRST; slot < CORE_FUNNY_END; ++slot) {
        uint8_t *record = peeps+slot*22;
        if (!peep_word(slot,4)) continue;
        set_peep_word(slot,12,(int16_t)(peep_word(slot,12)+1));
        int step = (int8_t)record[21];
        set_peep_word(slot,6,(int16_t)(peep_word(slot,6)+step));
        uint16_t frame = (uint16_t)peep_word(slot,6);
        if (frame >= record[2] || frame <= record[3]) record[21] = (uint8_t)-step;
        if (peep_word(slot,12) <= 7) continue;
        set_peep_word(slot,12,0);
        int position = peep_word(slot,8), type = (int8_t)record[20];
        if (position >= 0 && position < 4096 && map_who[position] == slot+1) map_who[position] = 0;
        if (type < 0 || type > 2) continue;
        if (!peep_word(slot,10)) set_peep_word(slot,10,core_funny[type][0]);
        if (core_valid_move(position,peep_word(slot,10)) == 1) { set_peep_word(slot,4,0); continue; }
        position += peep_word(slot,10);
        set_peep_word(slot,8,(int16_t)position);
        for (int index = 0; index < 3; ++index) {
            int offset = core_funny[type][3+index];
            if (core_valid_move(position,offset) == 1) continue;
            int cell = position+offset, block = map_blk[cell];
            if (type == 0) { if (block && block != 16) map_bk2[cell] = (uint8_t)(50+index); }
            else if (type == 1) { if (block == 15 || block == 31 || block == 32 || block == 66) map_blk[cell] = 53; }
            else if (block && block != 16) map_blk[cell] = (uint8_t)(47+index);
            int victim = map_who[cell]-1;
            if (victim < 0) continue;
            if (victim < 208) core_remove(victim);
            else if (victim < CORE_FUNNY_END) core_zero_funny(victim);
        }
        map_who[position] = (uint8_t)(slot+1);
    }
}
static void core_frame_town(int person, int capacity)
{
    set_peep_word(person,12,capacity >= 3050 ? 42 : 32+capacity*10/305);
}
/* _where_do_i_go asm:4622-4901: rank five choices over nine directional rays. */
static int core_explore(int person)
{
    int distance[5] = {5,5,5,5,5}, choices[5] = {999,999,999,999,999};
    int least_steps = 9999, direction = -1, owner = peep_owner(person);
    for (int ray = 0; ray < 9; ++ray) {
        if (direction == 0 && ray == 1) direction = (___newrand()&7)+1;
        else if (++direction == 9) direction = 0;
        int position = peep_word(person,8), delta = core_offsets[direction];
        for (int depth = 0; depth < peeps[person*22+2]; ++depth) {
            if (core_valid_move(position,delta)) break;
            position += delta;
            if (map_blk[position] == 15 && depth < distance[0]) {
                int build = direction == 0 ? core_check_life(owner,position)>0 : 1;
                if (direction) for (int index = 9; index < 17; ++index) {
                    if (core_valid_move(position,core_offsets[index])) continue;
                    int block = map_bk2[position+core_offsets[index]];
                    if (block > 32 && block <= 44) build = 0;
                }
                if (build) { distance[0] = depth; choices[0] = delta; continue; }
            }
            if (!direction) continue;
            int other = map_who[position]-1, choice = -1;
            if (other >= 0 && other < no_peeps && other != person) {
                if (peeps[other*22]&8) choice = 1;
                else if (peep_owner(other) != owner) choice = 2;
                else if (peeps[other*22]&2) choice = 3;
            }
            if (choice >= 0 && depth < distance[choice]) { distance[choice] = depth; choices[choice] = delta; continue; }
            int steps = (uint16_t)map_step_word(position);
            if (delta != peep_word(person,10) && (steps < least_steps || (steps == least_steps && depth < distance[4]))) {
                least_steps = steps; distance[4] = depth; choices[4] = delta;
            }
        }
    }
    if (core_tendency(owner) == 3 && distance[2] != 5) return choices[2];
    if (core_tendency(owner) == 2 && distance[3] != 5) return choices[3];
    for (int index = 0; index < 5; ++index) if (distance[index] != 5) return choices[index];
    return 999;
}
/* _move_magnet_peeps asm:5277-5327 and _move_peeps asm:3491-3543.
 * Owner mailboxes overwrite unconditionally; the tick consumer owns execution.
 * Only the old-water caller gates fatal mode, not the blocked walker. */
static void core_queue_terrain_raise(int owner, int position)
{
    if (owner < 0 || owner > 1 || position < 0 || position >= 4096) return;
    int displacement = owner * 46;
    if (!war_flag &&
        (stats_build_mode(displacement / 46) != 1 || (game_mode&GAME_MODE_NO_BUILD))) return;
    stats_bytes(displacement / 46)[0] = 1;
    (stats_bytes(displacement / 46) + 1)[0] = position & 63;
    (stats_bytes(displacement / 46) + 2)[0] = position / 64;
    stats_build_latch(displacement / 46) = 1;
}
/* _move_magnet_peeps asm:4912-5465. Existing host headings override magnets;
 * invalid opaque references retain their bytes and use only a bounded magnet. */
static int core_magnet_direction(int person)
{
    if (no_peeps < 0 || no_peeps > 208 || person < 0 || person >= no_peeps) return 999;
    int owner = peep_owner(person), position = peep_word(person,8);
    if (owner > 1 || position < 0 || position >= 4096) return 999;
    int reference = peep_reference(person), present = reference != -1;
    int target;
    if (reference >= 0) {
        target = peep_word(reference,8);
        if (target == position || peep_word(reference,4) <= 0 ||
            peep_owner(reference) == owner || (peeps[reference*22]&0x80) ||
            target < 0 || target >= 4096) {
            core_get_heading(person);
            reference = peep_reference(person);
            target = peep_word(reference,8);
        }
        /* The exact selector intentionally admits all signed word positions.
         * Validate once here, without retrying or changing selector semantics. */
        if (target < 0 || target >= 4096) return 999;
    } else {
        target = core_target(owner);
        if (!present) {
            int leader = core_leader(owner)-1;
            if (!core_leader(owner) && position == target) {
                core_leader(owner) = person+1;
                if (!view_who) view_who = person+1;
            }
            /* Invalid/dead/foreign leaders use the magnet without repair. */
            if (leader >= 0 && leader < no_peeps && leader < 208 && leader != person &&
                peep_word(leader,4) > 0 && peep_owner(leader) == owner &&
                peep_word(leader,8) >= 0 && peep_word(leader,8) < 4096)
                target = peep_word(leader,8);
        } else if (target < 0 || target >= 4096) return 999;
    }
    int dx = (target%64 > position%64)-(target%64 < position%64);
    int dy = (target/64 > position/64)-(target/64 < position/64);
    static const int to_delta[9] = {7,6,5,0,0,4,1,2,3};
    static const int directions[8] = {-64,-63,1,65,64,63,-1,-65};
    static const int opposite[8] = {4,5,6,7,0,1,2,3};
    int primary = to_delta[(dx+1)*3+dy+1], delta = directions[primary];
    int valid = core_valid_move(position,delta);
    if ((!valid && (!present || map_blk[position+delta] != 53)) ||
        (valid == 2 && war_flag)) {
        peeps[person*22+21] = (uint8_t)delta;
        return delta;
    }
    if (valid == 3 && reference != -2)
        core_queue_terrain_raise(owner,position);
    /* asm:5328-5383 queues the rejected primary swamp, then tries fallback.
     * Retain host geometric safety and the bounded opaque-reference exclusion. */
    if (valid == 0 && reference != -2 && map_blk[position+delta] == 53 &&
        !war_flag && !(game_mode&GAME_MODE_BUILD_ONLY_UP))
        core_queue_terrain_raise(owner,position+delta);
    int direction = primary-1;
    for (int attempt = 0; attempt < 8; ++attempt, ++direction) {
        if (direction < 0) direction = 7;
        if (direction > 7) direction = 0;
        int candidate = directions[direction];
        if (!core_valid_move(position,candidate) && candidate != (int8_t)peeps[person*22+21] &&
            (!present || map_blk[position+candidate] != 53)) {
            peeps[person*22+21] = (uint8_t)directions[opposite[direction]];
            return candidate;
        }
    }
    /* asm:5445-5461 increments AFTER rejection, without a final wrap. Index 8
     * reads adjacent _palette[0] == 0; reproduce that north choice safely. */
    peeps[person*22+21] = (uint8_t)directions[direction == 8 ? 0 : opposite[direction]];
    return 999;
}
/* _battle_over asm:6207-6564. Casualties and winner selection stay in core_battle. */
static int core_battle_over(int winner, int loser, bool raw_loser_nonzero)
{
    if (no_peeps < 0 || no_peeps > 208 || winner < 0 || winner >= no_peeps ||
        loser < 0 || loser >= no_peeps || winner == loser ||
        peep_word(winner,4) <= 0 || peep_word(loser,4) > 0) return 0;
    int owner=peep_owner(winner), enemy=peep_owner(loser);
    int position=peep_word(winner,8), loser_position=peep_word(loser,8);
    if (owner > 1 || enemy > 1 || owner == enemy || position < 0 || position >= 4096 ||
        loser_position < 0 || loser_position >= 4096) return 0;
    uint32_t winner_heading, loser_heading;
    memcpy(&winner_heading,peeps+winner*22+14,sizeof winner_heading);
    memcpy(&loser_heading,peeps+loser*22+14,sizeof loser_heading);
    int32_t reward=A4_WORD(loser_heading ? 0x3012 : core_leader(enemy)==loser+1 ? 0x3014 : 0x3010);
    battle_won(owner)=(uint16_t)((uint16_t)battle_won(owner)+1u);
    if (peeps[loser*22]&1) {
        peeps[loser*22]=1;
        /* _do_battle's preliminary combat frames are overwritten here and by
         * winner frame 85. Exact-zero losers skip both frames and life scratch;
         * negative host casualties must still reframe before any terrain edit. */
        if (raw_loser_nonzero) core_frame_town(loser,core_check_life(enemy,loser_position));
        int level=(int16_t)(peep_word(loser,12)-32);
        reward=level>=0 && level<=10 ? land_battle_add1(level) : 100;
        if (winner_heading) {
            peeps[loser*22]=0x80; set_peep_word(loser,4,1); set_peep_word(loser,6,40);
            peeps[winner*22]=2;
            int city=map_bk2[loser_position]==42;
            for (int index=0; index<(city ? 25 : 17); ++index) {
                int cell=loser_position+core_offsets[index];
                /* Source city loop has no valid_move gate: retain row wrap,
                 * guarding only accesses outside the host's flat map array. */
                if (city ? (cell<0 || cell>=4096) : core_valid_move(loser_position,core_offsets[index])!=0) continue;
                if (city && index>0 && index<9 && map_bk2[cell]>=41 && map_bk2[cell]<=44)
                    map_bk2[cell]+=21;
                if (map_blk[cell]==31+enemy) map_blk[cell]=66;
            }
            if (map_bk2[loser_position]>=32 && map_bk2[loser_position]<=42) map_bk2[loser_position]+=21;
            if (map_who[loser_position]==loser+1) map_who[loser_position]=0;
        } else {
            core_set_town(loser,1);
            int capacity=core_check_life(owner,position);
            if (capacity>0) {
                /* Prevent later loser cleanup from erasing the new town. */
                peeps[loser*22]=8; peeps[winner*22]=1;
                set_peep_word(winner,6,game_turn);
                core_frame_town(winner,capacity); core_set_town(winner,0);
            }
        }
    }
    /* Normalize before core_remove so reciprocal host cleanup cannot undo it. */
    peeps[winner*22]=((peeps[winner*22]&1) ? 1 : 2)|4;
    set_peep_word(winner,10,0); set_peep_word(winner,12,85);
    if (!map_who[position] || map_who[position]==loser+1) map_who[position]=winner+1;
    if (peeps[winner*22+20]) {
        stats_go_to_pos(owner)=position;
        stats_go_to_mode(owner)=2;
        --peeps[winner*22+20];
    }
    if (peeps[loser*22+20]) stats_go_to_mode(enemy)=0;
    /* ADD.L/SUB.L wrap before the signed loser-only comparison, not core_add_mana. */
    core_mana_long(owner)+=(uint32_t)reward;
    core_mana_long(enemy)-=(uint32_t)reward;
    if ((core_mana_long(enemy)^0x80000000u) < (mana_floor^0x80000000u))
        core_mana_long(enemy)=mana_floor;
    if (view_who==loser+1) view_who=winner+1;
    if (peep_word(loser,4)<=0) core_remove(loser);
    return 1;
}
/* _set_frame asm:5759-5809: actual combat partner, opaque heading presence.
 * The caller preserves the pre-clamp population test; water has source priority. */
static int core_frame_battle(int person, int partner, bool raw_nonzero)
{
    if (no_peeps < 0 || no_peeps > 208 || person < 0 || person >= no_peeps ||
        partner < 0 || partner >= no_peeps || person == partner || !raw_nonzero ||
        !(peeps[person*22]&8) || (peeps[person*22]&0x10)) return 0;
    uint32_t heading, partner_heading;
    memcpy(&heading,peeps+person*22+14,sizeof heading);
    memcpy(&partner_heading,peeps+partner*22+14,sizeof partner_heading);
    int base=heading && partner_heading ? 138 : !heading && !partner_heading ? 70 :
        peeps[(heading ? person : partner)*22+1]==0 ? 130 : 134;
    int frame=(int16_t)((uint16_t)peep_word(person,12)+1u);
    int reset=frame < base || frame >= base+3;
    set_peep_word(person,12,reset ? base : frame);
    return reset;
}
static void core_battle(int first, int second)
{
    /* _do_battle:6082-6158: both casualties use the lesser rolled strength. */
    int second_strength = (___newrand()%3+1)*peep_word(second,4);
    int first_strength = (___newrand()%3+1)*peep_word(first,4);
    int lesser = first_strength < second_strength ? first_strength : second_strength;
    /* SUB.W wraps before TST.W and before either raw-nonzero frame test.
     * Keep host storage normalized without losing the signed source residual. */
    uint16_t first_bits = (uint16_t)((uint32_t)peep_word(first,4)-
        ((uint32_t)(lesser/100)*peeps[second*22+3]+10u));
    uint16_t second_bits = (uint16_t)((uint32_t)peep_word(second,4)-
        ((uint32_t)(lesser/100)*peeps[first*22+3]+10u));
    int first_pop = first_bits <= INT16_MAX ? (int)first_bits : (int)first_bits-65536;
    int second_pop = second_bits <= INT16_MAX ? (int)second_bits : (int)second_bits-65536;
    set_peep_word(first,4,first_pop > 0 ? first_pop : 0);
    set_peep_word(second,4,second_pop > 0 ? second_pop : 0);
    core_frame_battle(first,second,first_pop!=0);
    core_frame_battle(second,first,second_pop!=0);
    if (first_pop > 0 && second_pop > 0) return;
    if (first_pop <= 0 && second_pop <= 0) { core_remove(first); core_remove(second); return; }
    int winner = first_pop > 0 ? first : second, loser = winner == first ? second : first;
    bool raw_loser_nonzero=(loser==first ? first_pop : second_pop)!=0;
    core_battle_over(winner,loser,raw_loser_nonzero);
}
/* _join_battle asm:6663-6765. Keep the pair's combat state intact. */
static void core_join_battle(int newcomer, int encountered)
{
    if (newcomer < 0 || newcomer >= no_peeps || newcomer >= 208 ||
        encountered < 0 || encountered >= no_peeps || encountered >= 208 ||
        newcomer == encountered || peep_word(newcomer,4) <= 0 ||
        peeps[newcomer*22] != 2 || peep_owner(newcomer) > 1 ||
        peep_word(encountered,4) <= 0 || !(peeps[encountered*22]&8) ||
        peep_owner(encountered) > 1) return;
    int partner = peep_word(encountered,6);
    if (partner < 0 || partner >= no_peeps || partner >= 208 ||
        partner == encountered || partner == newcomer ||
        peep_word(partner,4) <= 0 || !(peeps[partner*22]&8) ||
        peep_word(partner,6) != encountered ||
        peep_owner(partner) != 1-peep_owner(encountered)) return;
    int owner = peep_owner(newcomer);
    int recipient = peep_owner(encountered) == owner ? encountered : partner;
    int population = peep_word(recipient,4)+peep_word(newcomer,4);
    set_peep_word(recipient,4,population > 32000 ? 32000 : population);
    if (core_leader(owner) == newcomer+1) core_leader(owner) = recipient+1;
    if (view_who == newcomer+1) view_who = recipient+1;
    /* This is an opaque four-byte Amiga field, not a native host pointer. */
    uint32_t metadata;
    memcpy(&metadata,peeps+newcomer*22+14,sizeof metadata);
    if (metadata) memcpy(peeps+recipient*22+14,&metadata,sizeof metadata);
    if (peeps[newcomer*22+3] > peeps[recipient*22+3])
        peeps[recipient*22+3] = peeps[newcomer*22+3];
    /* asm:6725-6740: a not-yet-visited recipient will count the merged word. */
    if (recipient > newcomer)
        core_good_pop(peeps[newcomer*22+1]) -= (uint32_t)(int32_t)peep_word(newcomer,4);
    core_remove(newcomer);
}
/* _join_forces asm:5540-5640: ordinary friendly merge, not battle joining. */
static void core_join_forces(int source, int target)
{
    if (source < 0 || source >= no_peeps || source >= 208 ||
        target < 0 || target >= no_peeps || target >= 208 || source == target ||
        peep_word(source,4) <= 0 || peeps[source*22] != 2 ||
        peep_owner(source) > 1 || peep_word(target,4) <= 0 ||
        peep_owner(target) != peep_owner(source) || (peeps[target*22]&8)) return;
    /* Opaque Amiga metadata: copy four bytes, never dereference as a pointer. */
    uint32_t metadata;
    memcpy(&metadata,peeps+source*22+14,sizeof metadata);
    if (metadata && peeps[target*22] == 1) return;
    if (metadata) memcpy(peeps+target*22+14,&metadata,sizeof metadata);
    int population = peep_word(source,4)+peep_word(target,4);
    set_peep_word(target,4,population > 32000 ? 32000 : population);
    int owner = peep_owner(source);
    if (core_leader(owner) == source+1) core_leader(owner) = target+1;
    if (view_who == source+1) view_who = target+1;
    if (peeps[source*22+3] > peeps[target*22+3])
        peeps[target*22+3] = peeps[source*22+3];
    peeps[target*22] &= 0x9f;
    set_peep_word(target,12,0);
    /* asm:5605-5620: a not-yet-visited target will count the merged word. */
    if (target > source)
        core_good_pop(peeps[source*22+1]) -= (uint32_t)(int32_t)peep_word(source,4);
    /* Host cleanup clears only source-owned occupancy; transfer refs first. */
    core_remove(source);
}
static void core_move(int person)
{
    int owner = peep_owner(person), position = peep_word(person,8);
    int present = peep_reference(person) != -1;
    int use_magnet = present || war_flag || !core_tendency(owner);
    int delta = use_magnet ? core_magnet_direction(person) : core_explore(person);
    /* _move_explorer asm:4425-4437: entry does not change the frame. */
    if (delta == 999) {
        peeps[person*22] |= 0x40;
        set_peep_word(person,6,7);
        return;
    }
    peeps[person*22] &= 0xbf;
    /* asm:4438-4449: release the cell claimed by the previous departure. */
    int previous = (int16_t)(position-peep_word(person,10));
    if (previous >= 0 && previous < 4096 && map_who[previous] == person+1) map_who[previous] = 0;
    /* asm:4450-4509: a computer-controlled walker on block 0x42 asks for its
     * own cell to be lowered: command byte, x, ASR.W #6 row; no latch write. */
    int request = 0x82b6 + owner*46;
    if (A4_WORD(request+6) == 1 && map_blk[position] == 0x42 && !A4_WORD(request+8) &&
        !(game_mode&(GAME_MODE_NO_BUILD|GAME_MODE_BUILD_ONLY_UP))) {
        int16_t cell = (int16_t)position;
        A4_DISP(request)[0] = 2;
        A4_DISP(request+1)[0] = (uint8_t)(cell & 63);
        A4_DISP(request+2)[0] = (uint8_t)(cell >= 0 ? cell/64 : -((-cell+63)/64));
    }
    /* asm:4510-4559: the encounter is with the peep named at the walker's
     * own cell. Only -1, self, slot >= 208 and bit 7 are excluded. */
    int occupant = map_who[position]-1;
    if (occupant != -1 && occupant != person && occupant < 208 && !(peeps[occupant*22]&0x80)) {
        if (peeps[occupant*22]&8) core_join_battle(person,occupant);
        else if (peep_owner(occupant) == owner) core_join_forces(person,occupant);
        else {
            peeps[person*22] = 8;
            peeps[occupant*22] = (peeps[occupant*22]&7)|8;
            set_peep_word(person,6,occupant); set_peep_word(occupant,6,person);
            core_frame_battle(person,occupant,peep_word(person,4)!=0);
            core_frame_battle(occupant,person,peep_word(occupant,4)!=0);
            /* _set_battle asm:6604-6656 (its view_who store is a no-op):
             * release the defender's claims, then a town keeps its cell
             * while a walker is pulled back onto the attacker's cell. */
            int cell = peep_word(occupant,8);
            int left = (int16_t)(cell-peep_word(occupant,10));
            if (left >= 0 && left < 4096 && map_who[left] == occupant+1) map_who[left] = 0;
            if (cell >= 0 && cell < 4096 && map_who[cell] == occupant+1) map_who[cell] = 0;
            if (peeps[occupant*22]&1) {
                if (cell >= 0 && cell < 4096) map_who[cell] = person+1;
            } else {
                set_peep_word(occupant,8,position);
                map_who[position] = person+1;
            }
            ++core_battles;
            return;
        }
    }
    if (peep_word(person,4) <= 0) return;
    /* asm:4564-4589: settle on a zero heading without metadata. */
    if (!delta && !present) {
        if (!map_who[position]) map_who[position] = person+1;
        peeps[person*22] = 1; set_peep_word(person,6,game_turn);
        core_frame_town(person,core_check_life(owner,position)); core_set_town(person,0); ++core_settlements;
        return;
    }
    /* asm:4590-4617: count and claim the departure cell, then advance. The
     * destination is claimed only by the next departure. */
    map_step_word(position) = (uint16_t)map_step_word(position)+1;
    if (!map_who[position]) map_who[position] = person+1;
    set_peep_word(person,8,(int16_t)(position+delta)); set_peep_word(person,10,delta);
}
/* _move_peeps frame local -28 (asm:3331-3335): per-owner oldest town age. */
static uint16_t core_oldest_town_age[2];
/* _move_peeps frame local -24 (asm:3336-3340): per-owner best town capacity. */
static int16_t core_best_town_capacity[2];
/* _one_block_flat asm:9148-9310: queue one terrain command that would flatten
 * the walker's cell. Corner sums 4q+3 raise the low corner, 4q+1 lower the
 * high one; scan is x-major over the cell's four vertices. Writes _stats only. */
static void core_one_block_flat(int position, int owner)
{
    if (stats_build_latch(owner) || (int32_t)core_mana_long(owner) < 20 ||
        (int16_t)core_town_age(owner) > 50 || !(stats_power_mask(owner)&1) ||
        (game_mode&GAME_MODE_NO_BUILD)) return;
    int column = position & 63, row = (int16_t)position >= 0 ? (int16_t)position/64 : -((-(int16_t)position+63)/64);
    int base = row*65 + column;
    A4Word *verts = (A4Word *)map_vertices;
    int16_t sum = (int16_t)(verts[base].value + verts[base+1].value +
                            verts[base+66].value + verts[base+65].value);
    if (sum == 1) return;
    int remainder = sum % 4, level = sum / 4; /* DIVS: truncating quotient/remainder */
    for (int x = column; x <= column+1; ++x) for (int y = row; y <= row+1; ++y) {
        int16_t height = map_vertex_word(x, y);
        int command = remainder == 3 ? (height == level ? 1 : 0) :
                      remainder == 1 && height > level && !(game_mode&GAME_MODE_BUILD_ONLY_UP) ? 2 : 0;
        if (command) {
            stats_cmd_byte(owner) = command;
            stats_cmd_x(owner) = x;
            stats_cmd_y(owner) = y;
            stats_build_latch(owner) = 1;
            return;
        }
    }
}
/* _move_peeps frame local -36 (asm:3331-3335 init $4e1f, asm:3738-3765):
 * per-owner youngest town age; -40 (asm:3640-3645): per-owner city count;
 * -32 (asm:3276-3289): good_castles*3+good_towns read at pass start. */
static uint16_t core_youngest_town_age[2];
static int16_t core_city_count[2], core_town_strength[2];
/* _a_flat asm:25174-25184 (A4+$9704): 81 signed (dx,dy) byte pairs. */
static const int8_t core_a_flat[162] = {
    -4,-4,-4,-3,-4,-2,-4,-1,-4,0,-4,1,-4,2,-4,3,-4,4,
    -3,-4,-3,-3,-3,-2,-3,-1,-3,0,-3,1,-3,2,-3,3,-3,4,
    4,-4,4,-3,4,-2,4,-1,4,0,4,1,4,2,4,3,4,4,
    3,-4,3,-3,3,-2,3,-1,3,0,3,1,3,2,3,3,3,4,
    -2,-4,-2,-3,-2,-2,-2,-1,-2,0,-2,1,-2,2,-2,3,-2,4,
    2,-4,2,-3,2,-2,2,-1,2,0,2,1,2,2,2,3,2,4,
    -1,-4,-1,-3,-1,-2,-1,-1,-1,0,-1,1,-1,2,-1,3,-1,4,
    1,-4,1,-3,1,-2,1,-1,1,0,1,1,1,2,1,3,1,4,
    0,-4,0,-3,0,-2,0,-1,0,0,0,1,0,2,0,3,0,4
};
/* _make_level's early exit (asm:9017-9031) returns D0 untouched, which still
 * holds the _stats element address A4+$82b6+owner*46; the town pass stores
 * its low byte in peep +20 (asm:3793-3797). These are the low bytes at the
 * disassembly base (A4 = $493ee: $516a4 and $516d2). The real value depends
 * on the load address and has not been measured on real hardware. It is not
 * just a flag: peep +20 is decremented in battle (asm:6477-6498), used as a
 * signed table index (asm:8689-8737) and compared with exactly 1 (asm:8771). */
#define MAKE_LEVEL_EXIT_BYTE_OWNER0 ((uint8_t)(0x516a4 + 0*46))
#define MAKE_LEVEL_EXIT_BYTE_OWNER1 ((uint8_t)(0x516a4 + 1*46))
/* _make_level asm:9017-9147: queue one raise/lower toward the town vertex
 * height over the 9x9 a_flat neighbourhood. Returns D0: 0 when a command was
 * queued, 1 when already level, else the exit byte above. */
static uint8_t core_make_level(int position, int owner)
{
    int s = 0x82b6 + owner*46;
    if (!(A4_WORD(s+14)&1) || (game_mode&GAME_MODE_NO_BUILD)) return owner ? MAKE_LEVEL_EXIT_BYTE_OWNER1 : MAKE_LEVEL_EXIT_BYTE_OWNER0;
    int16_t x = (int16_t)(position & 63), y = (int16_t)((int16_t)position >> 6);
    int16_t height = map_vertex_word(x, y);
    for (int index = 0; index < 162; index += 2) {
        int16_t cx = (int16_t)(core_a_flat[index] + x), cy = (int16_t)(core_a_flat[index+1] + y);
        if (cx < 0 || cx > 64 || cy < 0 || cy > 64) continue;
        /* ASL.W #6 + ADD.W: x == 64 or y == 64 reads past row/map, as the original. */
        int16_t cell = (int16_t)(cy*64 + cx);
        int command = 0;
        if (map_blk[cell] == 0x2f && !(game_mode&GAME_MODE_BUILD_ONLY_UP)) {
            ++map_blk[cell];
            command = 2;
        } else {
            int16_t diff = (int16_t)(height - map_vertex_word(cx, cy));
            if (diff > 0) command = 1;
            else if ((diff < 0 || map_blk[cell] == 0x42 || map_blk[cell] == 0x35) &&
                     !(game_mode&GAME_MODE_BUILD_ONLY_UP)) command = 2;
        }
        if (command) {
            A4_DISP(s)[0] = (uint8_t)command;
            A4_DISP(s+1)[0] = (uint8_t)cx; A4_DISP(s+2)[0] = (uint8_t)cy;
            A4_WORD(s+8) = 1;
            return 0;
        }
    }
    return 1;
}
/* ---- Computer AI ---------------------------------------------------- */

/* Host guard: the AI dereferences _magnet leaders and _stats pointers as
 * peep records without validation. Outside the 208-slot table the original
 * reads unrelated memory; the port then skips that AI routine for the pass. */
static int core_ai_slot(int slot) { return slot >= 0 && slot < 208; }
/* Signed CMP.L of mana against (cost + bonus), ADD.L wrapping. */
static int core_ai_mana_above(int owner, uint32_t cost, uint32_t bonus)
{
    return (int32_t)core_mana_long(owner) > (int32_t)(cost + bonus);
}
static void core_ai_put(int s, int command, int16_t cell)
{
    A4_DISP(s)[0] = (uint8_t)command;
    A4_DISP(s+1)[0] = (uint8_t)(cell & 63);
    A4_DISP(s+2)[0] = (uint8_t)(cell >> 6); /* ASR.W #6, low byte */
}
/* _do_computer_effect asm:9526-9600: aim at the computer's own magnet cell
 * when it has NO leader (TST.W _magnet; BNE LAB_450B8, asm:9530-9536) and an
 * enemy stands there; else 3 cells above and left of the _stats+$26 peep
 * (slot+1), each clamped at zero only. */
static void core_do_computer_effect(int owner)
{
    int s = stats_base(owner);
    if (!core_leader(owner)) {
        int16_t target = core_target(owner);
        int who = target >= 0 && target < 4096 ? map_who[target] : 0; /* host bound */
        if (who && peep_owner(who-1) != owner) {
            core_ai_put(s, stats_cmd_byte(owner), target);
            return;
        }
    }
    int subject = (int)stats_oldest_town_ref(owner) - 1;
    int16_t cell = peep_word(subject, 8);
    int16_t x = (int16_t)((cell & 63) - 3), y = (int16_t)((cell >> 6) - 3);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    stats_cmd_x(owner) = (uint8_t)x;
    stats_cmd_y(owner) = (uint8_t)y;
}
/* _devil_effect asm:9300-9525. Called per owner from _move_peeps asm:3266-3270
 * while the build latch is clear. The permission word is copied to a local:
 * BTST on its high byte is bit 8 (war), on its low byte bits 0..7. */
static void core_devil_effect(int owner)
{
    int s = stats_base(owner), enemy = owner ? 0 : 1;
    if (stats_build_latch(owner)) return;
    if ((core_leader(owner) && !core_ai_slot(core_leader(owner)-1)) ||
        (core_leader(enemy) && !core_ai_slot(core_leader(enemy)-1)) ||
        stats_oldest_town_ref(owner) > 208) return;
    uint16_t flags = (uint16_t)stats_power_mask(owner);
    /* asm:9317-9349: armageddon when richer by >999 and more populous. */
    if ((flags & 0x100) && core_ai_mana_above(owner, mana_war_cost, 999) &&
        (int32_t)core_good_pop(owner) > (int32_t)core_good_pop(enemy)) {
        stats_cmd_byte(owner) = 14;
        stats_cmd_y(owner) = 3;
        stats_build_latch(owner) = 1;
        return;
    }
    /* asm:9350-9365: flood. */
    if ((flags & 0x80) && core_ai_mana_above(owner, mana_flood_cost, 1999)) {
        stats_cmd_byte(owner) = 14;
        stats_cmd_y(owner) = 4;
        stats_build_latch(owner) = 1;
        return;
    }
    /* asm:9366-9396: knight for a leader above 3000; returns either way. */
    if ((flags & 0x20) && core_leader(owner) && peep_word(core_leader(owner)-1, 4) > 3000) {
        if (core_ai_mana_above(owner, mana_knight_cost, 500)) {
            stats_cmd_byte(owner) = 14;
            stats_cmd_y(owner) = 5;
            stats_build_latch(owner) = 1;
        }
        return;
    }
    if (!stats_oldest_town_ref(owner)) return;
    /* asm:9398-9419: volcano. */
    if (core_ai_mana_above(owner, mana_volcano_cost, 500) && (flags & 0x40)) {
        stats_cmd_byte(owner) = 6;
        core_do_computer_effect(owner);
        stats_build_latch(owner) = 1;
        stats_send_flag(owner) = 0;
        return;
    }
    /* asm:9420-9482: swamp on the enemy leader; no latch write. */
    if (core_ai_mana_above(owner, mana_swamp_cost, 500) && (flags & 0x10)) {
        int16_t count = stats_send_flag(owner);
        if (!(count < stats_ai_delay(owner) && (flags & 8)) &&
            !(count > stats_ai_seed(owner) && (flags & 0x40))) {
            int16_t theirs = (int16_t)(core_leader(enemy) - 1);
            int16_t ours = (int16_t)(core_leader(owner) - 1);
            if (theirs != -1 && (ours == -1 || peep_word(theirs, 4) > peep_word(ours, 4))) {
                stats_send_flag(owner) = (int16_t)(count + 1);
                core_ai_put(s, 4, peep_word(theirs, 8));
                return;
            }
        }
    }
    /* asm:9483-9524: earthquake on the target town. */
    if (core_ai_mana_above(owner, mana_quake_cost, 500) &&
        peeps[((int)stats_oldest_town_ref(owner) - 1) * 22] == 1 && (flags & 8)) {
        if (stats_send_flag(owner) >= stats_ai_delay(owner) && (flags & 0x50)) return;
        stats_cmd_byte(owner) = 3;
        core_do_computer_effect(owner);
        stats_build_latch(owner) = 1;
        stats_send_flag(owner) = (int16_t)(stats_send_flag(owner) + 1);
    }
}
static void core_ai_tendency(int s, uint8_t value)
{
    A4_DISP(s)[0] = 14; A4_DISP(s+2)[0] = 1; A4_DISP(s+1)[0] = value; A4_WORD(s+8) = 1;
}
/* _stats+$22/$2a are Amiga pointers (slot+1 here). With none, the original
 * dereferences address 8 (an OS exception vector); that compare is not
 * modelled: the magnet branch is skipped and the fallback runs. */
static int core_ai_town_cell(int s, int field, int16_t *cell)
{
    uint32_t reference = A4_LONG(s+field);
    if (!reference || reference > 208) return 0;
    *cell = peep_word((int)reference-1,8);
    return 1;
}
/* _set_devil_magnet asm:9601-9800. Called per owner from _move_peeps
 * asm:3257-3261 while the latch is clear. One newrand draw on the early
 * tendency path only (asm:9628-9633). */
static void core_set_devil_magnet(int owner)
{
    int s = 0x82b6 + owner*46, enemy = owner ? 0 : 1;
    int16_t towns = (int16_t)(A4_WORD(s+0x16)+A4_WORD(s+0x14));
    int16_t level = A4_WORD(s+0xc);
    if (towns < (int16_t)(level*2+15) ||
        (uint16_t)((uint16_t)game_turn % 90u) < (uint16_t)(level+10)) {
        if (!core_tendency(owner)) {
            A4_DISP(s)[0] = 14; A4_DISP(s+2)[0] = 1;
            A4_DISP(s+1)[0] = (uint8_t)((int16_t)___newrand()%3 + 1);
            A4_WORD(s+8) = 1;
        }
        return;
    }
    int theirs = core_leader(enemy) ? core_leader(enemy)-1 : -1;
    int ours = core_leader(owner) ? core_leader(owner)-1 : -1;
    if ((theirs != -1 && !core_ai_slot(theirs)) || (ours != -1 && !core_ai_slot(ours))) return;
    if (ours < 0) {
        if (core_tendency(owner)) { core_ai_tendency(s,0); A4_WORD(s+0x1c) = 0; }
        return;
    }
    if (peep_word(ours,8) == A4_WORD(s+0x1e) && A4_WORD(s+0x1c))
        A4_WORD(s+0x1c) = (int16_t)(A4_WORD(s+0x1c)-1);
    int16_t cell;
    if (peep_word(ours,4) < 6000) {
        if (A4_WORD(s+0x1c)) {
            if (core_target(owner) != A4_WORD(s+0x1e)) {
                core_ai_put(s,5,A4_WORD(s+0x1e)); A4_WORD(s+8) = 1;
                return;
            }
        } else if (core_ai_town_cell(s,0x2a,&cell) && core_target(owner) != cell) {
            core_ai_put(s,5,cell); A4_WORD(s+0x1e) = cell; A4_WORD(s+0x1c) = 2; A4_WORD(s+8) = 1;
            return;
        }
        if (core_tendency(owner)) core_ai_tendency(s,0);
        return;
    }
    /* asm:9784-9864: a leader 500 above the enemy's, enemy tendency 0. */
    if (theirs >= 0 && peep_word(ours,4) > (int16_t)(peep_word(theirs,4)+500) && !core_tendency(enemy)) {
        if (A4_WORD(s+14)&4) {
            if (core_tendency(owner)) core_ai_tendency(s,0);
            else if (core_target(owner) != core_target(enemy)) {
                core_ai_put(s,5,core_target(enemy)); A4_WORD(s+8) = 1;
            }
        }
        return;
    }
    /* asm:9865-9898: magnet onto the enemy's best town. */
    if (A4_WORD(s+14)&2) {
        if (core_ai_town_cell(s,0x22,&cell) && core_target(owner) != cell && !A4_WORD(s+0x1c)) {
            core_ai_put(s,5,cell); A4_WORD(s+0x1e) = cell; A4_WORD(s+0x1c) = 2; A4_WORD(s+8) = 1;
        } else if (core_tendency(owner)) core_ai_tendency(s,0);
    }
}
static void core_settle_tick(int person)
{
    int owner = peep_owner(person), position = peep_word(person,8), capacity = core_check_life(owner,position);
    /* asm:3600-3633: caller dispatches exact peaceful towns. Metadata is an
     * opaque four-byte Amiga field, never a native pointer. */
    uint32_t metadata;
    memcpy(&metadata,peeps+person*22+14,sizeof metadata);
    if (capacity <= 0 || metadata || war_flag) {
        peeps[person*22] = (peeps[person*22]&0xfe)|2;
        set_peep_word(person,12,0); set_peep_word(person,10,0);
        core_set_town(person,1);
        return;
    }
    int old_frame = peep_word(person,12); /* asm:3634-3636, before reframing. */
    core_frame_town(person,capacity);
    /* asm:3658-3661: LAB_52DEA counts non-city frames during the pass. */
    if (capacity < 3050) core_town_age(owner) = (uint16_t)core_town_age(owner)+1;
    else if (owner < 2) ++core_city_count[owner]; /* asm:3637-3645 */
    /* asm:3667-3677: reclaim a departed walker's slot, never another occupant. */
    if (!map_who[position]) map_who[position] = person+1;
    /* asm:3684-3705: a capacity strictly above the pass maximum (signed
     * word, BGE skips ties) becomes the enemy-indexed _stats+0x22 pointer. */
    if (owner < 2 && !(core_best_town_capacity[owner] >= (int16_t)capacity)) {
        core_best_town_capacity[owner] = (int16_t)capacity;
        stats_best_town_ref(owner ? 0 : 1) = (uint32_t)(person+1);
    }
    /* asm:3706-3737: the oldest-so-far town (unsigned, ties replace) becomes
     * the enemy-indexed _stats+0x26 pointer (host slot+1 encoding). */
    uint16_t age = (uint16_t)((uint16_t)game_turn-(uint16_t)peep_word(person,6));
    if (owner < 2 && !(core_oldest_town_age[owner] > age)) {
        core_oldest_town_age[owner] = age;
        stats_oldest_town_ref(owner ? 0 : 1) = (uint32_t)(person+1);
    }
    /* asm:3738-3765: the strictly youngest town (unsigned) is this owner's
     * own _stats+0x2a pointer. */
    if (owner < 2 && core_youngest_town_age[owner] > age) {
        core_youngest_town_age[owner] = age;
        stats_youngest_town_ref(owner) = (uint32_t)(person+1);
    }
    /* asm:3766-3817: town levelling while this owner's latch is clear. Flat
     * cells keep the result in peep +20 and skip an unchanged, unflagged frame. */
    if (owner < 2 && !stats_build_latch(owner)) {
        if (!map_alt[position]) {
            if (!peeps[person*22+20] || peep_word(person,12) != old_frame || life_water_flag)
                peeps[person*22+20] = core_make_level(position,owner);
        } else if (core_town_strength[owner] < 3 && (uint16_t)game_turn > 250)
            core_make_level(position,owner);
    }
    if (!((uint16_t)game_turn&7)) {
        int level = peep_word(person,12)-32;
        /* asm:3818-3862: local split limits, never reframe or reindex economy. */
        if (peep_word(person,12) == 42 && stats_build_mode(owner) == 1 &&
            peep_word(person,4) > 305 && !stats_build_latch(owner)) {
            capacity = 305;
            stats_build_latch(owner) = 1;
        }
        uint16_t cheat = (uint16_t)land_cheat_owner;
        if (cheat && (uint16_t)owner == (uint16_t)(cheat-1u)) capacity = 50;
        /* asm:3863-3882: EXT.L signed word then ADD.L, without host mana bounds. */
        core_mana_long(owner)+=(uint32_t)(int32_t)(int16_t)land_mana_add(level);
        peeps[person*22+3] = land_weapons_add(level)&255;
        if (peep_word(person,4) > capacity) {
            int child;
            for (child = 0; child < 208 && peep_word(child,4)>0; ++child) {}
            if (child < 208) {
                if (child >= no_peeps) no_peeps = child+1;
                /* asm:3950-3963 leaves this slot's counter and nav history intact. */
                set_peep_word(child,10,0); set_peep_word(child,12,0);
                memset(peeps+child*22+14,0,4); peeps[child*22+20] = 0;
                peeps[child*22] = 2; peeps[child*22+1] = owner;
                peeps[child*22+2] = peeps[person*22+2]; peeps[child*22+3] = peeps[person*22+3];
                set_peep_word(child,4,peep_word(person,4)-capacity/2); set_peep_word(person,4,capacity/2);
                /* asm:3913-3929: the released walker carries the leader. */
                if (core_leader(owner) == person+1) core_leader(owner) = child+1;
                if (view_who == person+1) view_who = child+1; /* asm:3931-3937 */
                set_peep_word(child,8,position); set_peep_word(child,18,position);
                /* asm:3964-3983: preserve a co-located walker or battle occupant. */
                if (!map_who[position] || map_who[position] == person+1) map_who[position] = child+1;
                if (peeps[person*22+2] < 4) ++peeps[person*22+2];
            } else if (!funny_done) {
                /* asm:3996-4006: no free slot of 208, once per game. */
                core_place_funny(1,0);
                funny_done = 1;
            }
        }
        /* asm:4007-4014: ADD.W can revive or kill; store before maintenance. */
        set_peep_word(person,4,(uint16_t)((uint16_t)peep_word(person,4)+(uint16_t)land_population_add(level)));
    }
    /* asm:4037-4055: use this resident's life-scan scratch after economy.
     * Evacuation bypasses both maintenance and signed cleanup (asm:3633). */
    int frame = peep_word(person,12);
    if (frame != old_frame || life_water_flag || (frame == 42 && (int16_t)life_swamp_count < 9))
        core_set_town(person,0);
    if (peep_word(person,4)<=0) core_remove(person);
}
/* _raise_point/_lower_point: the same 8-neighbour slope constraint, bounded work. */
/* The admission tests of core_sculpt, also used before a two-player post. */
static int core_sculpt_admit(int owner, int column, int row, int direction)
{
    if (owner < 0 || owner > 1 || pause || core_result || war_flag || column < 0 || column > 64 || row < 0 || row > 64 ||
        (direction != 1 && direction != -1) || (game_mode&GAME_MODE_NO_BUILD) ||
        (direction < 0 && (game_mode&GAME_MODE_BUILD_ONLY_UP)) || core_mana(owner) < 10 ||
        !(stats_power_mask(owner)&1)) return 0;
    int original = core_vertex(column,row), target_height = original+direction;
    if (target_height < 0 || target_height > 8) return 0;
    if ((game_mode&GAME_MODE_BUILD_NEAR_TOWNS) && original > 0) {
        int near = 0;
        for (int person = 0; person < no_peeps; ++person) {
            int position = peep_word(person,8);
            if (peep_word(person,4)>0 && peep_owner(person)==owner && (peeps[person*22]&1) &&
                abs(position%64-column)<=2 && abs(position/64-row)<=2) near = 1;
        }
        if (!near) return 0;
    }
    return 1;
}
static int core_sculpt(int owner, int column, int row, int direction)
{
    if (!core_sculpt_admit(owner, column, row, direction)) return 0;
    int target_height = core_vertex(column,row)+direction;
    int changed = 0, xmin = column, xmax = column, ymin = row, ymax = row;
    for (int y = 0; y <= 64; ++y) for (int x = 0; x <= 64; ++x) {
        int distance = abs(x-column)>abs(y-row) ? abs(x-column) : abs(y-row);
        int height = core_vertex(x,y), bound = target_height-direction*distance;
        if ((direction > 0 && height < bound) || (direction < 0 && height > bound)) {
            changed += abs(bound-height); core_set_vertex(x,y,bound);
            if (x < xmin) xmin = x;
            if (x > xmax) xmax = x;
            if (y < ymin) ymin = y;
            if (y > ymax) ymax = y;
        }
    }
    core_add_mana(owner,-(10+changed*4));
    /* Original map-update bounds include the cells on both sides of changed
     * vertices; remote terrain and navigation history must remain untouched. */
    core_make_map_region(xmin > 0 ? xmin-1 : 0, ymin > 0 ? ymin-1 : 0,
                         xmax < 64 ? xmax : 63, ymax < 64 ? ymax : 63);
    /* Host footprint refresh. A town in battle reaches only _do_battle
     * (_move_peeps asm:4269-4281); _sculpt/_make_map (asm:7271-7420,
     * 1420-1593) never call _set_town. */
    for (int person = 0; person < no_peeps; ++person)
        if (peep_word(person,4)>0 && (peeps[person*22]&1) && !(peeps[person*22]&8))
            core_set_town(person,0);
    return 1;
}

static void _place_first_people(void)
{
    memset(peeps, 0, 208 * 22);
    memset(map_who, 0, 4096);
    no_peeps = current_start_pop_you + current_start_pop_enemy;
    /* Explicit synthetic, legal flat starting plots, not original level maps. */
    for (int index = 0; index < no_peeps; ++index) {
        int owner = index >= current_start_pop_you;
        int local = owner ? index-current_start_pop_you : index;
        int position = (8+(local/5)*7)*64 + 8+(local%5)*8+owner*3;
        peeps[index * 22] = 2;
        peeps[index * 22 + 1] = index >= current_start_pop_you;
        peeps[index * 22 + 2] = 1;
        peeps[index * 22 + 3] = 1;
        set_peep_word(index, 4, 45);
        set_peep_word(index, 8, position);
        set_peep_word(index, 12, 255);
        map_who[position] = index + 1;
    }
}

/* Raw raise for generation and powers: no mana or settlement restrictions.
 * asm:1274-1416; discard out-of-map neighbours before reading their heights. */
static int original_raise_point(int column, int row)
{
    if (column < 0 || column > 64 || row < 0 || row > 64) return 0;
    int height = core_vertex(column, row);
    if (height >= 8) return height;
    core_set_vertex(column, row, ++height);
    build_count = (int16_t)((uint16_t)build_count + 1u);
    static const int neighbours[8][2] = {
        {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}
    };
    for (int index = 0; index < 8; ++index) {
        int next_column = column + neighbours[index][0];
        int next_row = row + neighbours[index][1];
        if (next_column >= 0 && next_column <= 64 && next_row >= 0 && next_row <= 64 &&
            height - core_vertex(next_column, next_row) > 1)
            original_raise_point(next_column, next_row);
    }
    if (column < raise_min_col) raise_min_col = column;
    if (column > raise_max_col) raise_max_col = column;
    if (row < raise_min_row) raise_min_row = row;
    if (row > raise_max_row) raise_max_row = row;
    return core_vertex(column, row);
}

/* _do_raise_point asm:7180-7246: _paint_map (asm:7189,7206) or _war skips
 * both the mana admission and the payment. */
static int core_raw_raise(int owner, int column, int row)
{
    if (owner < 0 || owner > 1 || column < 0 || column > 64 || row < 0 || row > 64)
        return 0;
    /* Convert the signed long comparison without out-of-range signed casts. */
    uint32_t mana = core_mana_long(owner), threshold = mana_raise_cost;
    if (!paint_map && !war_flag && (mana ^ 0x80000000u) < (threshold ^ 0x80000000u)) return 0;
    raise_min_col = raise_max_col = column;
    raise_min_row = raise_max_row = row;
    build_count = 0;
    original_raise_point(column, row);
    if (!paint_map && !war_flag) {
        unsigned word_cost = ((uint16_t)build_count * 4u + 10u) & 0xffffu;
        int32_t cost = word_cost < 0x8000u ? (int32_t)word_cost : (int32_t)word_cost - 65536;
        /* SUB.L wraps in 32 bits. Unlike core_sculpt/core_add_mana, no host cap. */
        core_mana_long(owner) = mana - (uint32_t)cost;
    }
    if (raise_min_col < 1) raise_min_col = 1;
    if (raise_max_col > 63) raise_max_col = 63;
    if (raise_min_row < 1) raise_min_row = 1;
    if (raise_max_row > 63) raise_max_row = 63;
    core_make_map_region(raise_min_col-1, raise_min_row-1,
                         raise_max_col, raise_max_row);
    return 1;
}

static int original_lower_point(int column, int row);
/* _do_lower_point asm:7091-7180: bounds before the mana gate; unlike raise,
 * war does not bypass it. Charge asm:7121-7126: ASL.W #2, ADD.W #10, then
 * EXT.L sign-extends the word before SUB.L. */
static void core_do_lower_point(int owner, int column, int row)
{
    raise_min_col = raise_max_col = column;
    raise_min_row = raise_max_row = row;
    if ((int32_t)core_mana_long(owner) < (int32_t)mana_raise_cost && !paint_map) return;
    build_count = 0;
    original_lower_point(column,row);
    if (!paint_map && !war_flag)
        core_mana_long(owner) -= (uint32_t)(int32_t)(int16_t)(uint16_t)((uint16_t)build_count*4u+10u);
    if ((int16_t)raise_min_col < 1) raise_min_col = 1;
    if ((int16_t)raise_max_col > 63) raise_max_col = 63;
    if ((int16_t)raise_min_row < 1) raise_min_row = 1;
    if ((int16_t)raise_max_row > 63) raise_max_row = 63;
    core_make_map_region(raise_min_col-1, raise_min_row-1, raise_max_col, raise_max_row);
}
/* _place_people(side,x,y,leader) asm:6982-7087: a new leader replaces the
 * current one (_zero_population), else the next free slot. Byte map
 * coordinates past 63 would write outside _map_who; the port drops them. */
static void core_place_people(int side, int x, int y, int leader)
{
    if (no_peeps >= 208 || side < 0 || side > 1 || x < 0 || x > 63 || y < 0 || y > 63) return;
    int person, magnet = core_leader(side);
    if (leader && magnet > 0 && magnet <= no_peeps) { person = magnet-1; core_remove(person); }
    else person = no_peeps++;
    int cell = y*64+x;
    set_peep_word(person,6,0); set_peep_word(person,4,45); set_peep_word(person,8,cell);
    map_who[cell] = person+1;
    peeps[person*22+1] = (uint8_t)side; peeps[person*22] = 2;
    set_peep_word(person,12,255); peeps[person*22+2] = 1;
    set_peep_word(person,10,0); peeps[person*22+3] = 1;
    memset(peeps+person*22+14,0,4);
    if (leader) core_leader(side) = person+1;
    set_peep_word(person,12,0);   /* _set_frame on frame 255, asm:5728-5743 */
}
/* _rotate_all_map asm:6823-6922: "mirror the landscape". Both halves of
 * _alt take the higher (signed) height, trees 50..55 are copied across. */
static void core_rotate_all_map(void)
{
    for (int i = 0; i < 0x1080/2; ++i) {
        int mirror = 0xae86+(0x1080-i)*2;
        if (A4_WORD(0xae86+i*2) < A4_WORD(mirror)) A4_WORD(0xae86+i*2) = A4_WORD(mirror);
        A4_WORD(mirror) = A4_WORD(0xae86+i*2);
    }
    for (int i = 0; i < 0xfff/2; ++i) {
        if (map_bk2[i] >= 50 && map_bk2[i] <= 55) map_bk2[0xfff-i] = map_bk2[i];
        else if (map_bk2[0xfff-i] >= 50 && map_bk2[0xfff-i] <= 55) map_bk2[i] = map_bk2[0xfff-i];
    }
    _make_map();
}
/* _clear_all_map asm:6926-6978: flat sea, no objects, no people. */
static void core_clear_all_map(void)
{
    for (int i = 0; i < 0x1081; ++i) {
        A4_WORD(0xae86+i*2) = 0;
        if (i < 0x1000) map_who[i] = map_bk2[i] = map_blk[i] = 0;
    }
    for (int person = 0; person < no_peeps; ++person) core_remove(person);
    no_peeps = 0;
    _make_map();
}
static int core_load_ground(int terrain);
/* _paint_the_map asm:15123-15274, editor keys into the _player mailbox at
 * _cur_x/_cur_y (low bytes): F1/F2 man (Shift: leader) for either side,
 * F3 tree, F4 rock, F5 delete, F6/F7 mana (x = _shift), F8 mirror, DEL
 * clear, 1..9 terrain (16-(inkey-0xed))/2 = digit-1. raw is the Amiga key. */
static int core_paint_the_map(int raw, int shifted)
{
    if (!paint_map || player < 0 || player > 1) return 0;
    if (core_serial_live() && !core_serial_can_post()) return 1;   /* no input during the exchange */
    uint8_t *box = stats_bytes(player);
    box[1] = (uint8_t)cur_x; box[2] = (uint8_t)cur_y;
    switch (raw) {
    case 0x50: box[0] = shifted ? 9 : 7; return 1;
    case 0x51: box[0] = shifted ? 10 : 8; return 1;
    case 0x52: box[0] = 11; return 1;
    case 0x53: box[0] = 12; return 1;
    case 0x54: box[0] = 13; return 1;
    case 0x55: box[0] = 14; box[1] = (uint8_t)shifted; box[2] = 9; return 1;
    case 0x56: box[0] = 14; box[1] = (uint8_t)shifted; box[2] = 10; return 1;
    case 0x57: box[0] = 14; box[2] = 11; return 1;
    case 0x46: box[0] = 14; box[2] = 12; return 1;
    default:
        if (raw >= 1 && raw <= 9) { box[0] = 14; box[1] = (uint8_t)(raw-1); box[2] = 13; return 1; }
        return 0;
    }
}
/* _get_message opcodes 11/12 asm:18049-18122: cycle a tree (bk2 50..52) or
 * rock (blk 47..49) on a cell below height 7, starting from the last one. */
static void core_paint_cycle(int x, int y, int rock)
{
    if (x < 0 || x > 63 || y < 0 || y > 63) return;
    int cell = y*64+x, low = rock ? 47 : 50, high = low+2;
    if (map_alt[cell] >= 7) return;
    uint8_t *slot = rock ? &map_blk[cell] : &map_bk2[cell];
    int value = *slot;
    if (value < low || value > high) value = (rock ? last_rock : last_tree);
    else if (value == high) value = low;
    else ++value;
    if (rock) last_rock = (int16_t)value; else last_tree = (int16_t)value;
    *slot = (uint8_t)value;
}
/* Opcode 13 asm:18123-18183: a rock becomes grass, then objects and the
 * occupant go. */
static void core_paint_delete(int x, int y)
{
    if (x < 0 || x > 63 || y < 0 || y > 63) return;
    int cell = y*64+x;
    if (map_blk[cell] >= 47 && map_blk[cell] <= 49) {
        map_blk[cell] = 15;
        core_make_map_region(x,y,x+1 > 63 ? 63 : x+1,y+1 > 63 ? 63 : y+1);
    }
    map_bk2[cell] = 0;
    if (map_who[cell]) {
        int person = map_who[cell]-1;
        if (person < no_peeps) core_remove(person);
        map_bk2[cell] = 0;
        map_who[cell] = 0;
    }
}
/* _do_action y=9/10 asm:18943-18976: x (= _shift) halves the blue/red mana
 * (DIVS), else below 100000 it doubles plus 500. */
static void core_paint_mana(int owner, int halve)
{
    int32_t mana = (int32_t)core_mana_long(owner);
    if (halve) mana /= 2;
    else if (mana < 100000) mana = mana*2+500;
    core_mana_long(owner) = (uint32_t)mana;
}
/* _clear_send asm:18234-18254: only command/x/y, never the controller state. */
static void core_clear_terrain_mailboxes(void)
{
    for (int owner = 0; owner < 2; ++owner) memset(stats_bytes(owner), 0, 3);
}

/* _get_message asm:17929-17995,18218-18229. No serial/editor/UI producers yet.
 * Cadence clears the latch independently of the opcode, including empty slots. */
static int core_quake(int owner, int column, int row, uint16_t (*random)(void));
static int core_swamp(int owner, int column, int row, uint16_t (*random)(void));
static int core_volcano(int owner, int column, int row, uint16_t (*random)(void));
static int core_war(int owner);
static int core_flood(int owner);
static int core_knight(int owner);
static int core_do_magnet(int owner, int x, int y);
static void core_consume_terrain_mailboxes(void) { core_consume_from(0); }
/* From owner first; 2 when a two-player _do_action suspended the loop. */
static int core_consume_from(int first)
{
    core_consume_next = -1; core_consume_abort = 0;
    for (int owner = first; owner < 2; ++owner) {
        int displacement = owner * 46;
        if (stats_build_mode(displacement / 46) == 1) {
            unsigned divisor = (uint16_t)stats_build_period(displacement / 46);
            if (!divisor) {
                POP_LOG_ERROR("Terrain mailbox owner %d: zero cadence divisor; command discarded, latch preserved", owner);
                continue;
            }
            if ((uint16_t)game_turn % divisor == 0) stats_build_latch(displacement / 46) = 0;
        }
        int x = (stats_bytes(displacement / 46) + 1)[0], y = (stats_bytes(displacement / 46) + 2)[0];
        /* Jump table asm:18200-18216; 7..13 come from _paint_the_map. */
        switch (stats_bytes(displacement / 46)[0]) {
        case 1: core_raw_raise(owner, x, y); break;
        case 2: core_do_lower_point(owner, x, y); break;
        case 3: core_quake(owner, x, y, ___newrand); break;
        case 4: core_swamp(owner, x, y, ___newrand); break;
        case 5: core_do_magnet(owner, x, y); break;
        case 6: core_volcano(owner, x, y, ___newrand); break;
        case 7: core_place_people(0, x, y, 0); break;
        case 8: core_place_people(1, x, y, 0); break;
        case 9: core_place_people(0, x, y, 1); break;
        case 10: core_place_people(1, x, y, 1); break;
        case 11: core_paint_cycle(x, y, 0); break;
        case 12: core_paint_cycle(x, y, 1); break;
        case 13: core_paint_delete(x, y); break;
        case 14:
            /* With the link up y=2/6/7/8 talk to the other side (05_serial.c). */
            if (core_serial_live() && (y == 2 || y == 6 || y == 7 || y == 8)) {
                int result = core_serial_do_action(owner, x, y);
                if (result == 1) { core_clear_terrain_mailboxes(); return 1; }
                if (result == 2) { core_consume_next = owner + 1; return 2; }
                break;
            }
            /* _do_action asm:18406-19032, switch on y; these cases return 0. */
            if (y == 1) core_tendency(owner) = (int16_t)x; /* _set_tend_icons is UI only */
            else if (y == 2) { if (!serial_off) POP_LOGW("MSG", "Message requester needs the serial link; dropped"); }
            else if (y == 3) core_war(owner);
            else if (y == 4) core_flood(owner);
            else if (y == 5) core_knight(owner);
            else if (y == 9) core_paint_mana(0, x);
            else if (y == 10) core_paint_mana(1, x);
            else if (y == 11) core_rotate_all_map();
            else if (y == 12) core_clear_all_map();
            else if (y == 13) core_load_ground(x);
            /* y=7 asm:18607-18650: not while the other side's mailbox also
             * holds options; our side shows _options(x ? player : not_player).
             * y=8 asm:18729-18733: _new_map = 0, _game_options. The serial
             * exchange that follows for two players is not ported. */
            else if (y == 7 && owner == player && !core_req.kind) {
                const uint8_t *other = stats_bytes(owner ? 0 : 1);
                if (!(other[2] == 7 && other[0] == 14)) core_options_open(x ? player : not_player);
            } else if (y == 8 && owner == player && !core_req.kind) {
                options_new_map = 0; core_game_options_open();
            }
            break;
        default: break;
        }
    }
    core_clear_terrain_mailboxes();
    return 0;
}
/* _set_magnet_to asm:8271-8293. */
static void core_set_magnet_to(int owner, int16_t cell)
{
    if (pause) return;
    core_target(owner) = cell;
    owner_magnet(owner) = cell;
}
/* _do_magnet asm:6769-6820: signed CMP.L, leader and war gates, then
 * payment before the 0..63 clamps. */
static int core_do_magnet(int owner, int x, int y)
{
    if ((int32_t)core_mana_long(owner) < (int32_t)mana_magnet_cost ||
        !core_leader(owner) || war_flag) return 0;
    effect = 77;
    core_mana_long(owner) -= mana_magnet_cost;
    if (x >= 64) x = 63;
    if (x < 0) x = 0;
    if (y >= 64) y = 63;
    if (y < 0) y = 0;
    core_set_magnet_to(owner,(int16_t)(y*64+x));
    return 1;
}

/* _lower_point asm:2571-2713: ADDQ.W build_count, then SUBQ.W height.
 * Keep the source recursive neighbour order, but bounds-check before reads. */
static int original_lower_point(int column, int row)
{
    if (column < 0 || column > 64 || row < 0 || row > 64) return 0;
    int height = core_vertex(column, row);
    if (!height) return 0;
    build_count = (int16_t)((uint16_t)build_count + 1u);
    core_set_vertex(column, row, --height);
    static const int neighbours[8][2] = {
        {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}
    };
    for (int index = 0; index < 8; ++index) {
        int next_column = column + neighbours[index][0];
        int next_row = row + neighbours[index][1];
        if (next_column >= 0 && next_column <= 64 && next_row >= 0 && next_row <= 64 &&
            core_vertex(next_column, next_row) - height > 1)
            original_lower_point(next_column, next_row);
    }
    if (column < raise_min_col) raise_min_col = column;
    if (column > raise_max_col) raise_max_col = column;
    if (row < raise_min_row) raise_min_row = row;
    if (row > raise_max_row) raise_max_row = row;
    return core_vertex(column, row);
}

/* _do_flood asm:7567-7648: _paint_map (asm:7569) skips the mana, _war,
 * _pause and permission gates and the payment, not the score. The signed BLE guard leaves negative heights unchanged. This is not a
 * recursive lower/sculpt operation; water and town consequences wait for tick. */
static int core_flood(int owner)
{
    if (owner < 0 || owner > 1 || core_result) return 0;
    if (!paint_map) {
        if (pause || war_flag || core_mana(owner) < (int32_t)mana_flood_cost ||
            !(stats_power_mask(owner)&0x80)) return 0;
        core_mana_long(owner) -= mana_flood_cost;
    }
    if (owner == player) score_long += 250;
    effect = 74;
    for (int index = 0; index < 4225; ++index)
        if (((A4Word *)map_vertices)[index].value > 0) --((A4Word *)map_vertices)[index].value;
    _make_map();
    return 1;
}

/* ---- Divine powers --------------------------------------------------- */

/* _do_knight asm:8392-8478: the leader test comes first; _paint_map
 * (asm:8404) then skips the mana, _war, _pause and permission gates and the
 * payment, not the score. */
static int core_knight(int owner)
{
    if (owner < 0 || owner > 1 || no_peeps < 0 || no_peeps > 208 ||
        core_result || prg_quit) return 0;
    if (!paint_map && (pause || war_flag)) return 0;
    int leader=core_leader(owner);
    if (leader < 1 || leader > no_peeps) return 0;
    int subject=leader-1, position=peep_word(subject,8);
    if (peep_word(subject,4) <= 0 || peep_owner(subject) != owner ||
        position < 0 || position >= 4096) return 0;
    uint32_t mana=core_mana_long(owner), cost=mana_knight_cost;
    /* Signed CMP.L admission, unsigned SUB.L/ADD.L wrap, as in core_war. */
    if (!paint_map) {
        if ((mana ^ 0x80000000u) < (cost ^ 0x80000000u) ||
            !(stats_power_mask(owner)&0x20)) return 0;
        core_mana_long(owner)=mana-cost;
    }
    if (owner == player) score_long+=150;
    peeps[subject*22+20]=1;
    core_get_heading(subject);
    core_target(owner)=position;
    owner_magnet(owner)=position;
    core_leader(owner)=0;
    effect=69;
    return 1;
}

/* _do_war asm:8482-8530: _paint_map (asm:8484) jumps past the gates, the
 * payment and the score straight to the peep clear. */
static int core_war(int owner)
{
    if (owner < 0 || owner > 1 || no_peeps < 0 || no_peeps > 208 ||
        core_result || prg_quit) return 0;
    if (!paint_map) {
        uint32_t mana = core_mana_long(owner), cost = mana_war_cost;
        /* Signed CMP.L without an out-of-range signed conversion. BTST targets
         * the high byte of the original big-endian permission word. */
        if (pause || (mana ^ 0x80000000u) < (cost ^ 0x80000000u) ||
            !(stats_power_mask(owner)&0x100)) return 0;
        core_mana_long(owner) = mana-cost;
        if (owner == player) score_long += 5000;
    }
    for (int person = 0; person < no_peeps; ++person)
        memset(peeps+person*22+14,0,4);
    effect = 73;
    war_flag = 1;
    return 1;
}

typedef uint16_t (*WorldRandom)(void);
/* Sound bank (gmusic1 in play) and _curmeas as an offset into _measures. */
static AudioBank core_bank;
static uint32_t core_curmeas;
/* _kill_effect(from,to) asm:20876-20894: AbortIO on from..to-1. */
static void core_kill_effect(int from, int to) { for (int ch = from; ch < to; ++ch) audio_abort(ch); }
/* _check_effect asm:20898-20908: CheckIO, nonzero once the channel is idle. */
static int core_check_effect(int ch) { return !audio_busy(ch); }
/* _setsound asm:20390-20429: CMD_WRITE of sams[d+5]+offset, d+2 - offset
 * bytes, period d+0, volume d+4 (ADIOF_PERVOL), cycles. */
static void core_setsound(int ch, const AudioKit *d, uint32_t offset, uint16_t cycles)
{
    AudioWrite w = {core_bank.samples + core_bank.sams[d->sample].offset + offset,
                    d->length - offset, d->period, (uint16_t)(d->volume < 0 ? 0 : d->volume), cycles};
    audio_write(ch,&w);
}
/* _PlaySound(sound,ch) asm:20307-20386: unsigned bounds; a busy channel
 * keeps its sound against a lower d+4 (_Soundpri). The second request's
 * long offset is built from two pushed words, sams[].+2:d+6 (asm:20371-20380);
 * its cycles slot lies past the pushed arguments, so the port uses d+6. */
static int core_PlaySound(int sound, int ch)
{
    if ((uint16_t)sound >= (uint16_t)no_sounds || (uint16_t)ch > 3 ||
        !core_bank.kit || (uint16_t)sound >= core_bank.kit_count) return 0;
    const AudioKit *d = &core_bank.kit[(uint16_t)sound];
    if (!core_check_effect(ch) && d->volume < Soundpri(ch)) return 0;
    audio_abort(ch);
    Soundpri(ch) = d->volume;
    core_setsound(ch,d,0,1);
    if (d->loop) core_setsound(ch,d,(uint32_t)core_bank.sams[d->sample].loop << 16 | d->loop,d->loop);
    return 1;
}
/* _PlayMeas asm:20433-20518: one beat on all four tracks ('A' = sound 0,
 * below 'A' is out of range), then the measure/sequence advance. _curtime
 * is never reset after _load_sound, as in the original. */
static void core_PlayMeas(void)
{
    if (!core_bank.kit || !core_bank.seq_count) return;
    const uint8_t *m = core_bank.measures + core_curmeas + (uint16_t)curbeat;
    for (int track = 0; track < 4; ++track) core_PlaySound((int16_t)(m[track*0x40] - 0x41),track);
    curbeat = (int16_t)(curbeat+1);
    if (core_bank.measln[core_bank.sequn[(uint16_t)curseq][0]] != (uint16_t)curbeat) return;
    curtime = (int16_t)(curtime+1); curbeat = 0;
    if ((uint16_t)curtime >= core_bank.sequn[(uint16_t)curseq][1] && ++curseq == seqlen) curseq = 0;
    core_curmeas = (uint32_t)core_bank.sequn[(uint16_t)curseq][0] << 8;
}
/* _free_all_sounds asm:20522-20565. */
static void core_free_all_sounds(void) { core_kill_effect(0,4); audio_bank_free(&core_bank); }
/* _load_sound asm:20712-20872, from <assets>/raw/<name>; 0 or -1. */
static int core_load_sound(const char *name)
{
    const char *directory = getenv("POPULOUS_ASSET_DIR");
    if (!directory) directory = POPULOUS_DEFAULT_ASSET_DIR;
    char path[1024], error[128];
    snprintf(path,sizeof path,"%s/raw/%s",directory,name);
    FILE *file = fopen(path,"rb");
    if (!file) { POP_LOG_ERROR("Sound %s: %s",path,strerror(errno)); return -1; }
    static uint8_t data[0x40000];
    size_t size = fread(data,1,sizeof data,file);
    int big = fgetc(file) != EOF;
    fclose(file);
    AudioBank bank;
    if (big || !audio_bank_parse(&bank,data,size,error,sizeof error)) {
        POP_LOG_ERROR("Sound %s: %s",path,big ? "file too large" : error);
        return -1;
    }
    core_free_all_sounds();
    core_bank = bank;
    no_sounds = (int16_t)bank.kit_count; seqlen = (int16_t)bank.seq_count;
    core_curmeas = (uint32_t)bank.sequn[0][0] << 8;
    curtime = curseq = curbeat = 0;
    for (int ch = 0; ch < 4; ++ch) Soundpri(ch) = 0;
    return 0;
}
/* Vertical blank sound part, asm:19620-19672: music every _music frames,
 * a pending _effect on channels 2 and 3 (delaying music to 40 frames),
 * else the population heartbeat 0x47/0x48 on channel 0. */
static void core_vbi_audio(void)
{
    if (music_off) {
        int16_t now = (int16_t)(music_now+1);
        if (now == music) { music = 10; core_PlayMeas(); now = 0; }
        music_now = now;
    }
    if (!effect_off) return;
    if (effect) {
        music = 0x28;
        core_PlaySound(effect,2); core_PlaySound(effect,3);
        effect = 0;
    } else if (!sound_in && tempo) {
        tempo_now = (int16_t)(tempo_now+1);
        if (tempo_now >= tempo) { core_PlaySound(0x47,0); tempo_now = 0; }
        else if ((int16_t)(tempo-beat_two) == tempo_now) core_PlaySound(0x48,0);
    }
}
/* _animate asm:564-605: heartbeat rate from enemy/own population, then
 * the _toggle flip (asm:606-612). Run once per main-loop pass. */
static void core_tempo_pass(void)
{
    if (core_good_pop(player)) {
        int32_t own = (int32_t)core_good_pop(player), other = (int32_t)core_good_pop(player ? 0 : 1);
        tempo = (int16_t)(0x32 - 2*(other/own));
        if (tempo < 7) tempo = 7;
        beat_two = (int16_t)(tempo/3);
    } else { tempo = 0; beat_two = 1; }
    if (tempo_now > tempo) tempo_now = tempo;
    toggle = !toggle;
}
/* _start_game asm:13966-13972 (_open_channels, _load_sound "gmusic1")
 * plus the data defaults _tempo $32, _music_off/_effect_off 1
 * (asm:24129-24134) and _music 10 (_setup_mouse asm:19687). */
static void core_sound_start(void)
{
    tempo = 0x32; music_off = effect_off = 1; music = 10;
    audio_open();
    core_load_sound("gmusic1");
}
/* _do_quake asm:7895-7925: screen_origin -40, then per frame -120/+120
 * bytes; at 40 bytes a row that is -4 lines on even c, -1 on odd c. */
static int core_quake_shake_dy(int c) { return c&1 ? -1 : -4; }
/* The vbi keeps running during the synchronous shake: _effect 0x4c plays. */
static void core_quake_delay_sdl(int ms) { core_vbi_audio(); SDL_Delay((Uint32)ms); }
static void (*core_quake_delay)(int ms) = core_quake_delay_sdl;
/* 20 synchronous frames of pre-quake terrain (no ticks), unconditional on
 * owner; each frame is presented and waits about one vblank. */
static void core_quake_shake(void)
{
    if (!prg_screen || !game_assets) return;
    for (int c = 0; c < 20; ++c) {
        core_shake_dy = core_quake_shake_dy(c);
        game_render();
        core_quake_delay(20);
    }
    core_shake_dy = 0;
}
/* _do_quake asm:7863-8042: _paint_map (asm:7865) skips the mana, _war,
 * _pause and permission gates and the payment, not the score. */
static int core_quake(int owner, int column, int row, WorldRandom random)
{
    if (owner < 0 || owner > 1 || column < 0 || column > 56 || row < 0 || row > 56 ||
        core_result) return 0;
    if (!paint_map) {
        if (pause || war_flag || core_mana(owner) < (int32_t)mana_quake_cost ||
            !(stats_power_mask(owner)&8)) return 0;
        core_mana_long(owner) -= mana_quake_cost;
    }
    raise_min_col = raise_max_col = column;
    raise_min_row = raise_max_row = row;
    effect = 76;
    if (owner == player) score_long += 25;
    core_quake_shake();
    for (int pass = 0; pass < 2; ++pass)
        for (int x = column; x < column+9; ++x)
            for (int y = row; y < row+9; ++y) {
                if (!core_vertex(x,y)) continue;
                int action = random()%5;
                if (action == 1) original_raise_point(x,y);
                else if (action >= 2) original_lower_point(x,y);
            }
    if (raise_min_col < 1) raise_min_col = 1;
    if (raise_max_col > 63) raise_max_col = 63;
    if (raise_min_row < 1) raise_min_row = 1;
    if (raise_max_row > 63) raise_max_row = 63;
    core_make_map_region(raise_min_col-1,raise_min_row-1,
                         raise_max_col,raise_max_row);
    core_kill_effect(2,4);   /* asm:8037-8040: cut the rumble */
    return 1;
}
/* _do_volcano asm:8046-8267: _paint_map (asm:8048) skips the mana, _war,
 * _pause and permission gates and the payment, not the score. */
static int core_volcano(int owner, int column, int row, WorldRandom random)
{
    if (owner < 0 || owner > 1 || column < 0 || column > 56 || row < 0 || row > 56 ||
        core_result) return 0;
    if (!paint_map && (pause || war_flag || core_mana(owner) < (int32_t)mana_volcano_cost ||
        !(stats_power_mask(owner)&0x40))) return 0;
    /* asm:8141-8157 reads blue twice. Zero protects cell zero, not no cell.
     * Reject unsupported stale references before payment or terrain mutation. */
    int leader = core_leader(0), protected_cell = 0;
    if (leader) {
        if (leader < 1 || leader > 208 || leader > no_peeps ||
            peep_word(leader-1,4) <= 0) return 0;
        protected_cell = peep_word(leader-1,8);
        if (protected_cell < 0 || protected_cell >= 4096) return 0;
    }
    if (!paint_map) core_mana_long(owner) -= mana_volcano_cost;
    if (owner == player) score_long += 100;
    raise_min_col = raise_max_col = column;
    raise_min_row = raise_max_row = row;
    effect = 75;
    for (int pass = 0; pass < 5; ++pass)
        for (int x = column+pass; x <= column+8-pass; ++x)
            for (int y = row+pass; y <= row+8-pass; ++y) {
                int action = random()%5;
                if (action == 1 || action == 2 || action == 4) original_raise_point(x,y);
            }
    for (int x = column; x < column+8; ++x)
        for (int y = row; y < row+8; ++y) {
            if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
            int cell = y*64+x;
            if (random()%5 || cell == protected_cell ||
                cell == core_target(0) || cell == core_target(1)) continue;
            map_blk[cell] = 47;
            map_bk2[cell] = 0;
        }
    /* Lava never expands these source bounds, even when no point was raised. */
    if (raise_min_col < 1) raise_min_col = 1;
    if (raise_max_col > 63) raise_max_col = 63;
    if (raise_min_row < 1) raise_min_row = 1;
    if (raise_max_row > 63) raise_max_row = 63;
    core_make_map_region(raise_min_col-1,raise_min_row-1,
                         raise_max_col,raise_max_row);
    effect = 75;
    return 1;
}

/* _do_swamp asm:8295-8387: _paint_map (asm:8297) skips the mana, _war,
 * _pause and permission gates and the payment, not the score. Byte target coordinates can be outside the map; still consume all 60 draws. */
static int core_swamp(int owner, int column, int row, WorldRandom random)
{
    if (owner < 0 || owner > 1 || column < 0 || column > 255 || row < 0 || row > 255 ||
        core_result) return 0;
    if (!paint_map) {
        if (pause || war_flag || core_mana(owner) < (int32_t)mana_swamp_cost ||
            !(stats_power_mask(owner)&0x10)) return 0;
        core_mana_long(owner) -= mana_swamp_cost;
    }
    if (owner == player) score_long += 50;
    for (int attempt = 0; attempt < 30; ++attempt) {
        int x = column + random()%7 - 3;
        int y = row + random()%7 - 3;
        /* Unlike the original, bounds must precede even the block read. */
        if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
        int cell = y*64+x, block = map_blk[cell];
        if ((block == 15 || block == 31 || block == 32 || block == 66) && !map_who[cell])
            map_blk[cell] = 53;
    }
    return 1;
}
static void original_make_thing(int spread_x, int spread_y, WorldRandom random)
{
    int column = random() % 64;
    int row = random() % 64;
    while (original_raise_point(column, row) != 6) {
        column += (int)(random() % (2 * spread_x + 1)) - spread_x;
        row += (int)(random() % (2 * spread_y + 1)) - spread_y;
        if (column < 0) column = 0;
        if (column > 64) column = 64;
        if (row < 0) row = 0;
        if (row > 64) row = 64;
    }
}

/* asm:8535-8626: rejected attempts consume no variant random value. */
static void original_woods(WorldRandom random)
{
    for (int cluster = 0; cluster < 22; ++cluster) {
        int base_column = random() % 59;
        int base_row = random() % 59;
        for (int attempt = 0; attempt < 30; ++attempt) {
            int column = base_column + random() % 9;
            int row = base_row + random() % 9;
            if (column >= 64 || row >= 64) continue;
            int cell = row * 64 + column;
            if (!map_blk[cell] || map_blk[cell] == 47) continue;
            int variant = random() % 3;
            if (cluster < 7) map_blk[cell] = 47 + variant;
            else map_bk2[cell] = 50 + variant;
        }
    }
}

static int original_place_person(int owner, int cell, int leader)
{
    if (cell < 0 || cell >= 4096 || no_peeps >= 208) return 0;
    int person = no_peeps++;
    peeps[person * 22] = 2;
    peeps[person * 22 + 1] = owner;
    peeps[person * 22 + 2] = peeps[person * 22 + 3] = 1;
    set_peep_word(person, 4, 45);
    set_peep_word(person, 8, cell);
    /* _place_people starts frame 255 then calls _set_frame (asm:5732-5736). */
    set_peep_word(person, 12, 0);
    map_who[cell] = person + 1;
    if (leader) core_leader(owner) = person + 1;
    return 1;
}

/* Conquest searches, asm:7688-7856. Caller stages the whole world so any
 * capacity/insufficient-land failure cannot publish a partial start. */
static int original_place_people(int friendly_count, int enemy_count)
{
    /* Zero counts enter the original decrement-before-test loop and underflow.
     * Reject that unsupported input rather than manufacture a different start. */
    if (friendly_count <= 0 || enemy_count <= 0 || friendly_count + enemy_count > 208)
        return 0;
    for (int owner = 0; owner < 2; ++owner) {
        int remaining = owner ? enemy_count : friendly_count;
        int step = owner ? -1 : 1;
        for (int cell = owner ? 3968 : 128;
             remaining && cell >= 128 && cell < 4096; cell += step) {
            if (map_blk[cell] != 15) continue;
            --remaining;
            /* Preferred scans do NOT filter occupancy in the original. */
            if (!original_place_person(owner, cell, remaining <= 0)) return 0;
        }
        /* Original enemy fallback probes logical 4096 (outside the map).
         * Safety exception: skip that probe; retain 4095..1, excluding zero. */
        for (int cell = owner ? 4095 : 0;
             remaining && cell >= (owner ? 1 : 0) && cell < 4096; cell += step) {
            if (!map_blk[cell] || map_who[cell]) continue;
            if (!owner) --remaining;
            if (!original_place_person(owner, cell, remaining <= 0)) return 0;
            if (owner) --remaining; /* enemy fallback never marks its last man leader */
        }
        if (remaining) return 0;
    }
    score_long = friendly_count * 10;
    return 1;
}

/* ---- World generation / clear ---------------------------------------- */

static void original_clear(WorldRandom random, unsigned internal_level)
{
    _clear_map();
    memset(map_steps, 0, 4096 * 2);
    memset(peeps, 0, 208 * 22);
    for (int slot = 208; slot < CORE_FUNNY_END; ++slot) set_peep_word(slot,4,0);
    funny_done = 0;
    no_peeps = 0;
    for (int owner = 0; owner < 2; ++owner) {
        core_tendency(owner) = 1;
        core_target(owner) = 0x820;
        core_leader(owner) = 0;
        core_mana_long(owner) = 399;
        int value = random() % 3;
        stats_ai_delay(owner) = value;
        stats_ai_seed(owner) = value + random() % 5 + 1;
    }
    if (internal_level > 0x4d3) mana_high_bonus = 2700; /* mana_values[3] + 200 */
    devil_magnet = god_magnet = 0x820;
    pointer = 1; mode = 2; surrender_side = -1;
    right_button = left_button = 2;
}

static int original_generate(const LevelStart *start)
{
    const LevelRecord *record = &start->record;
    in_conquest = level_number = start->internal_level;
    conquest = record->rating; conq_01_speed = record->speed;
    conq_02_enemypow = record->enemy_powers; conq_03_yourpow = record->your_powers;
    conq_04_mode = record->mode_flags; conq_05_terrain = record->terrain;
    conq_06_your_count = record->your_count; conq_07_enemy_count = record->enemy_count;
    conq_08_seed = (int16_t)record->seed_offset;
    seed = start_seed = (int16_t)start->initial_seed;
    ground_in = record->terrain;
    original_clear(___newrand, start->internal_level);
    LAB_516DE = record->rating; LAB_516E2 = record->speed;
    LAB_516E0 = (record->enemy_powers << 3) | 7;
    LAB_516B2 = (record->your_powers << 3) | 7;
    game_mode = record->mode_flags;
    original_make_thing(2, 4, ___newrand);
    original_make_thing(4, 2, ___newrand);
    original_make_thing(3, 3, ___newrand);
    _make_map();
    original_woods(___newrand);
    if (!original_place_people(record->your_count, record->enemy_count)) return 0;
    seed = (int16_t)((uint16_t)seed + 1u);
    return 1;
}

/* _tutorial / LAB_5172E (asm:24233-24247): the 46-byte _stats records
 * _start_game copies over _stats and strABC (asm:13976-13987). */
static const uint8_t tutorial_stats[2][46] = {
    {'a','b','c',0, 0,0,0,0, 0,1,0,0, 0,10,0,0x3f, 0,10,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,5,0,0},
    {'a','b','c',0, 0,0,0,1, 0,1,0,0, 0,10,0,3, 0,10,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0}};

/* _start_of_game == 3: _start_game asm:13975-13999, then _setup_display's
 * custom path (in_conquest == -1, asm:418-433 with argument 1: one
 * ___newrand, ground 0 kept) and _clear_map's 10000 mana (asm:1134-1136).
 * _place_first_people takes conq_06/07 (15/3, asm:24343-24346; asm:7656). */
static int tutorial_generate(void)
{
    /* Bytes +0..+3 (command, x, y), then BE words into the host-endian A4. */
    for (int owner = 0; owner < 2; ++owner) {
        memcpy(stats_bytes(owner), tutorial_stats[owner], 4);
        for (int field = 4; field < 46; field += 2)
            A4_WORD(stats_base(owner) + field) = (int16_t)(tutorial_stats[owner][field] << 8 | tutorial_stats[owner][field+1]);
    }
    in_conquest = -1;
    conquest = 1; conq_01_speed = 1; conq_02_enemypow = 0; conq_03_yourpow = 0x3f;
    conq_04_mode = 3; conq_05_terrain = 0; conq_08_seed = 0;
    conq_06_your_count = 15; conq_07_enemy_count = 3;
    seed = start_seed = 0x69bc;
    game_mode = 1;                  /* _tutorial_game_mode */
    ground_in = 0;
    ___newrand();
    original_clear(___newrand, 0);
    core_mana_long(0) = 10000;        /* LAB_5188C -> LAB_52DF0 */
    original_make_thing(2, 4, ___newrand);
    original_make_thing(4, 2, ___newrand);
    original_make_thing(3, 3, ___newrand);
    _make_map();
    original_woods(___newrand);
    if (!original_place_people(15, 3)) return 0;
    seed = (int16_t)((uint16_t)seed + 1u);
    return 1;
}

/* Boot values of _stats/strABC (asm:24171-24232) and _game_mode with
 * LAB_518A7 (asm:24310-24312): rating 1/5, powers $ffff, speed 1/3. */
static const uint8_t core_boot_stats[2][46] = {
    {'a','b','c',0, 0,0,0,0, 0,1,0,0, 0,1,0xff,0xff, 0,1,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,5},
    {'a','b','c',0, 0,0,0,1, 0,1,0,0, 0,5,0xff,0xff, 0,3,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1}};
#define CORE_BOOT_GAME_MODE 0x0010

/* _setup_display(0, arg) with in_conquest -1, asm:430-457 then 458-500:
 * _start_seed = _seed; a nonzero _seed draws one ___newrand and, for
 * arg -1 and an odd draw, steps _ground_in 0..3 (___load_ground is the
 * caller's). No conq_ byte is read: _stats powers/rating/speed and
 * _game_mode stay as _options/_set_options left them. _clear_map
 * (asm:1023-1188) skips the level mana; _make_alt, _make_map(0,0,63,63),
 * _make_woods_rocks, then _place_first_people's custom counts
 * (asm:7664-7678, 7766-7780): 1, or 3 when _serial_off and the side's
 * _stats+$c rating > 4. Returns 0 if the people cannot be placed. */
static int custom_generate(int16_t arg, int counts[2])
{
    in_conquest = -1;
    start_seed = seed;
    if (seed && (___newrand() & 1) && arg == -1 && ++ground_in > 3) ground_in = 0;
    for (int owner = 0; owner < 2; ++owner) {   /* LAB_516B6/LAB_516C0, asm:1122-1129 */
        stats_send_flag(owner) = 0; stats_go_to_mode(owner) = 0;
    }
    original_clear(___newrand, 0);
    original_make_thing(2, 4, ___newrand);
    original_make_thing(4, 2, ___newrand);
    original_make_thing(3, 3, ___newrand);
    _make_map();
    original_woods(___newrand);
    for (int owner = 0; owner < 2; ++owner)
        counts[owner] = serial_off ? (stats_rating(owner) > 4 ? 3 : 1) : 1;
    if (!original_place_people(counts[0], counts[1])) return 0;
    score_long = (uint32_t)counts[player ? 1 : 0] * 10;   /* asm:7679-7686, 7781-7788 */
    seed = (int16_t)((uint16_t)seed + 1u);
    return 1;
}

/* _end_text asm:24790-24843 (A4 0x8e7e): 7 rows x 46 bytes, word x, word y,
 * word (0x48/0x18, unused here), then text. strLOST is text+5 and
 * strScoreboard text+19 of each row (asm:14025,14102). */
static const struct { int16_t x, y, w; const char *text; } core_end_template[7] = {
    {0x70,0x0a,0x48,"GAME LOST"},
    {0x10,0x14,0x18,"                   YOU  HIM"},
    {0x10,0x23,0x18,"BATTLES WON        0000 0000"},
    {0x10,0x2d,0x18,"NUMBER OF KNIGHTS  0000 0000"},
    {0x10,0x37,0x18,"NUMBER OF TOWNS    0000 0000"},
    {0x10,0x41,0x18,"NUMBER OF CASTLES  0000 0000"},
    {0x10,0x50,0x18,"YOUR SCORE         000000000"},
};
/* end_row_* macros live in a4_state.h */
static int core_end_shown, core_end_next, core_end_finished;
/* _won_conquest lord screen, asm:14503-14765. The original loops inside
 * _won_conquest, so the game stays frozen (core_result is kept) until the
 * click. w_screen is core_lord.screen, _map_steps' mouth copy is
 * core_lord.mouths (the port keeps the A4 _map_steps intact). */
static struct {
    int shown, finished, rank, wait;
    int16_t c, f, d, saved_music, saved_effect;   /* -120,-108,-110,-116,-118(A5) */
    int mouths_loaded;
    uint16_t palette[16];                         /* _tcmap[0..15] */
    uint8_t screen[320*200], mouths[0x13b0];
} core_lord;

/* ___long_asc/___word_asc asm:21046-21110: signed decimal, no padding. */
static void core_long_asc(char *out, int32_t value) { sprintf(out, "%ld", (long)value); }

/* _end_game asm:14019-14316: fill the seven rows and the final _score.
 * Row 1 keeps its template; rows 2-5 pad "you" to 5 then append "him". */
static int32_t core_end_stats(int won)
{
    for (int r = 0; r < 7; ++r) {
        end_row_x(r) = core_end_template[r].x;
        end_row_y(r) = core_end_template[r].y;
        end_row_w(r) = core_end_template[r].w;
        memset(end_row_text(r), 0, 40);
        strcpy(end_row_text(r), core_end_template[r].text);
    }
    strcpy(end_row_text(0) + 5, won ? "WON" : "LOST");  /* asm:14022-14044 */
    int you = player, him = not_player;
    for (int r = 2; r <= 5; ++r) {
        int16_t mine = 0, theirs = 0;
        if (r == 2) { mine = battle_won(you); theirs = battle_won(him); } /* asm:14046-14061 */
        else {
            /* asm:14063-14202 counts low words; the port clamps the unsigned
             * no_peeps loop to the 208-record pool. */
            int n = (uint16_t)no_peeps < 208 ? (uint16_t)no_peeps : 208;
            for (int p = 0; p < n; ++p) {
                uint32_t heading; memcpy(&heading, peeps + p*22 + 14, 4);
                int hit = r == 3 ? heading != 0 && peep_word(p,4) != 0 :
                    peeps[p*22] == 1 && peep_word(p,4) != 0 &&
                    (r == 4 ? peep_word(p,12) != 0x2a : peep_word(p,12) == 0x2a);
                if (hit) { if (peep_owner(p) == you) ++mine; else ++theirs; }
            }
        }
        char a[16], b[16];
        core_long_asc(a, mine); core_long_asc(b, theirs);
        size_t length = strlen(a);
        while (length < 5) a[length++] = ' ';
        a[length] = 0;
        strcpy(end_row_text(r) + 19, a); strcat(end_row_text(r), b);   /* asm:14280-14316 */
    }
    /* Score row asm:14204-14272. */
    uint32_t score = score_long;
    if (battle_won(you) > battle_won(him)) score += 5000;
    for (uint16_t bit = 8; bit <= 0x100; bit <<= 1) {
        if (!(stats_power_mask(you) & bit)) score += 1000;
        if (stats_power_mask(him) & bit) score += 1000;
    }
    /* asm:14239-14247: set for every single-player start (asm:12768). */
    if (serial_off) score += (uint32_t)(int32_t)(int16_t)((10 - stats_build_period(him)) * 15);
    if (won) score *= 10u;
    if ((int32_t)score < 500) score = 500;
    if ((int32_t)score > 555555) score = 515090;
    score_long = score;
    core_long_asc(end_row_text(6) + 19, (int32_t)score);
    return (int32_t)score;
}

/* _start/_mid/_end syllables asm:25075-25173. */
static const char *const core_code_start[32] = {
    "RING","VERY","KILL","SHAD","HURT","WEAV","MIN","EOA","COR","JOS","ALP","HAM","BUR","BIN","TIM","BAD",
    "FUT","MOR","SAD","CAL","IMM","SUZ","NIM","LOW","SCO","HOB","DOU","BIL","QAZ","SWA","BUG","SHI"};
static const char *const core_code_end[32] = {
    "HILL","TORY","HOLE","PERT","MAR","CON","LOW","DOR","LIN","ING","HAM","OLD","PIL","BAR","MET","END",
    "LAS","OUT","LUG","ILL","ICK","PAL","DON","ORD","OND","BOY","JOB","ER","ED","ME","AL","T"};
static const char *const core_code_mid[32] = {
    "OUT","QAZ","ING","OGO","QUE","LOP","SOD","HIP","KOP","WIL","IKE","DIE","IN","AS","MP","DI",
    "OZ","EA","US","GB","CE","ME","DE","PE","OX","A","E","I","O","U","T","Y"};

/* ___newrand of a given seed, without touching the A4 seed. */
static uint16_t core_code_rand(int level) { return (uint16_t)((uint16_t)level * 0x24a1u + 0x24dfu) & 0x7fff; }

/* _code asm:14793-14895: buffer needs 13 bytes. */
static void core_code(char *out, uint16_t n)
{
    strcpy(out, core_code_start[n & 31]);
    strcat(out, core_code_mid[(n >> 5) & 31]);
    strcat(out, core_code_end[(n >> 10) & 31]);
}

/* _decode asm:14900-15118: first matching syllable wins, then the world
 * whose seed n=0,5,..,5000 yields the code. The original leaves _seed=n;
 * the port does not. "GENESIS" is _show_world's case (asm:12280-12288). */
static int core_decode(const char *name)
{
    if (!strcmp(name, "GENESIS")) return 0;
    const char *const *tables[3] = {core_code_start, core_code_mid, core_code_end};
    size_t used = 0;
    int code = 0;
    for (int t = 0; t < 3; ++t) {
        int s = 0;
        for (; s < 32; ++s) {
            size_t k = 0;
            while (tables[t][s][k] && name[used+k] == tables[t][s][k]) ++k;
            if (!tables[t][s][k]) { used += k; break; }
        }
        if (s == 32) return -1 - t;
        code |= s << (5*t);
    }
    if (strlen(name) != used) return -4;
    for (int n = 0; n <= 5000; n += 5) if (core_code_rand(n) == code) return n;
    return -5;
}

/* _won_conquest asm:14460-14491: skip score/5000 worlds, round up to a
 * multiple of 5, cap at 2470; winning 2470 again wraps to 0 (finished). */
static int core_next_level(int level, int32_t score, int *finished)
{
    int16_t next = (int16_t)(level + (int16_t)(score / 5000 + 1));
    if (next % 5) next = (int16_t)(next + 5 - next % 5);
    *finished = 0;
    if (next > 0x9a6) {
        if (level == 0x9a6) { next = 0; *finished = 1; }
        else next = 0x9a6;
    }
    return next;
}

/* _won_conquest message asm:14520-14582 (_con_text ranks asm:24751-24760). */
static void core_conquest_text(int old_level, int level, int finished, char line[2][48])
{
    static const char *const ranks[10] = {"MORTAL","IMMORTAL","ETERNAL","DEVA","GREATER BEING",
        "DEITY","GREATER DEITY","MORTAL GOD","GREATER GOD","ETERNAL GOD"};
    char next[16], old[16];
    core_code(next, core_code_rand(level));
    if (finished) {
        strcpy(line[0], "WELL DONE YOU HAVE CONQUERED EVIL");
        snprintf(line[1], 48, "THE BATTLE IS OVER BUT TRY %s", next);
        return;
    }
    snprintf(line[0], 48, "WELL DONE %s YOU CONQUERED ", ranks[level / 250 % 10]);
    if (old_level) core_code(old, core_code_rand(old_level)); else strcpy(old, "GENESIS");
    snprintf(line[1], 48, "%s NOW BATTLE AT %s", old, next);
}

static int core_conquest_game(void) { return current_start.kind == START_ORIGINAL && in_conquest != -1; }

/* _end_game entry: stats once after the result appears. A conquest win
 * precomputes _won_conquest's next world from the final score. */
static void core_enter_end(void)
{
    int won = core_result == 1;
    int32_t score = core_end_stats(won);
    core_end_shown = 1; core_end_next = -1; core_end_finished = 0;
    if (won && core_conquest_game())
        core_end_next = core_next_level(level_number, score, &core_end_finished);
}

/* After the click (asm:14386-14402): won conquest -> _level_number and
 * _message = the next world's code (asm:14492-14501), the lord screen
 * (core_lord_open), then _show_world until != -1 (asm:14766-14772); a
 * lost conquest keeps in_conquest, i.e. the same world; in_conquest -1
 * -> _setup_display(0,-1) from the live _seed, the next custom landscape
 * (asm:14410-14432); synthetic fixtures restart. */
static int core_leave_end(void)
{
    if (!core_end_shown) return 0;
    stats_build_mode(player) = 0;
    if (core_end_next >= 0) {
        int old = level_number;
        level_number = (int16_t)core_end_next;
        core_code(core_message, core_code_rand(core_end_next));
        if (!core_lord_open(old, core_end_next, core_end_finished)) core_world_open(CORE_WORLD_WON);
        return 1;
    }
    StartSelection next = current_start;
    if (in_conquest == -1 && current_start.kind != START_SYNTHETIC ? !core_custom_start(seed, -1) : !game_reset_start(next)) {
        POP_LOG_ERROR("End of game: next world could not be started.");
        return 0;
    }
    return 1;
}

static void _animate(void)
{
    if (!game_reset_start(requested_start)) { prg_status = EXIT_FAILURE; return; }
    /* _start_of_game == 2 (asm:536-551): the world screen first. */
    if (requested_world) { core_message[0] = 0; core_world_open(CORE_WORLD_START); }
    /* --host/--join: straight into _two_players(0), as TWO PLAYERS would. */
    if (core_link) core_two_open(0, CORE_TWO_FROM_CLI);
    game_render();
    Uint32 next_tick = SDL_GetTicks() + 50, next_vbi = SDL_GetTicks() + 20;
    while (!prg_quit) {
        ___keyboard();
        if (prg_quit) break;
        core_serial_frame();
        Uint32 now = SDL_GetTicks();
        int catch_up = 0;
        /* 50 Hz vertical blank sound interrupt (asm:19620-19672). */
        while ((Sint32)(now-next_vbi) >= 0 && catch_up++ < 10) { core_vbi_audio(); core_lord_vblank(); next_vbi += 20; }
        if ((Sint32)(now-next_vbi) >= 0) next_vbi = now+20;
        catch_up = 0;
        while ((Sint32)(now-next_tick) >= 0 && catch_up++ < 5) {
            if (core_req.kind == CORE_REQ_WORLD) ++core_world.blink;
            if (core_req.kind == CORE_REQ_FILE) ++core_file.blink;
            /* Modal requesters and the end screen leave the main loop. */
            if (!core_req.kind && !core_result) core_tempo_pass();
            game_tick(); next_tick += 50;
        }
        if ((Sint32)(now-next_tick) >= 0) next_tick = now+50;
        /* ___end_game, called by _move_peeps (asm:4323-4347); deferred to
         * after the tick so game_tick's own A4 effects stay unchanged. */
        if (core_result && !core_end_shown) core_enter_end();
        game_render();
        SDL_Delay(10);
    }
}

/* A custom start from the live _ground_in; a failure keeps the old land. */
static int core_custom_start(int16_t from, int16_t arg)
{
    CustomLand old = core_land;
    core_land = (CustomLand){from, arg, ground_in};
    if (game_reset_start((StartSelection){START_CUSTOM, 0})) return 1;
    core_land = old;
    return 0;
}

/* ---- Reset / tick / animate ------------------------------------------ */

static int game_reset(int scenario)
{
    return game_reset_start((StartSelection){START_SYNTHETIC, scenario});
}

/* The LAND rule tables and _map_colour of ___load_ground, into the A4. */
static void core_install_land(const LandRules *loaded)
{
    /* LAND tables land in fixed A4 slots (see land_* macros in a4_state.h). */
    for (int i = 0; i < 11; ++i) {
        land_population_add(i) = loaded->population_add[i];
        land_mana_add(i) = loaded->mana_add[i];
        land_weapons_add(i) = loaded->weapons_add[i];
        land_battle_add1(i) = loaded->battle_add1[i];
        if (i < 3) land_battle_add2(i) = loaded->battle_add2[i];
        land_weapons_order(i) = loaded->weapons_order[i];
    }
    a4_walk_death = loaded->walk_death;
    memcpy(map_colour_data, loaded->map_colour, 32);
}

static int game_reset_start(StartSelection selection)
{
    int scenario = selection.number;
    if (scenario < 0 || (selection.kind != START_SYNTHETIC && selection.kind != START_ORIGINAL &&
        selection.kind != START_TUTORIAL && selection.kind != START_CUSTOM) ||
        ((selection.kind == START_TUTORIAL || selection.kind == START_CUSTOM) && scenario) ||
        scenario >= (selection.kind == START_ORIGINAL ? LEVEL_COUNT : 8)) return 0;
    ensure_prg_screen();
    if (!prg_screen) return 0;
    LandRules loaded;
    char error[160];
    const char *asset_directory = getenv("POPULOUS_ASSET_DIR");
    if (!asset_directory) asset_directory = POPULOUS_DEFAULT_ASSET_DIR;
    char default_data[4096];
    int path_length = snprintf(default_data,sizeof default_data,"%s/raw",asset_directory);
    if (path_length < 0 || (size_t)path_length >= sizeof default_data) return 0;
    const char *directory = getenv("POPULOUS_DATA_DIR");
    if (!directory) directory = default_data;
    LevelStart original = {0};
    unsigned terrain;
    _Alignas(4) uint8_t staged[sizeof prg_a4_area] = {0};
    if (selection.kind == START_CUSTOM) {
        /* The original keeps all of A4 between maps; the port carries the
         * custom inputs: _stats (both records), _game_mode, _player. */
        uint8_t kept_stats[92]; int16_t modes = game_mode, side = player, line = serial_off;
        if (core_custom_live) memcpy(kept_stats, stats, 92);
        uint8_t *live = prg_a4;
        prg_a4 = staged + 32768;
        if (core_custom_live) memcpy(stats, kept_stats, 92);
        else {
            for (int owner = 0; owner < 2; ++owner) {
                memcpy(stats_bytes(owner), core_boot_stats[owner], 4);
                for (int field = 4; field < 46; field += 2)
                    A4_WORD(stats_base(owner) + field) = (int16_t)(core_boot_stats[owner][field] << 8 | core_boot_stats[owner][field+1]);
            }
            modes = CORE_BOOT_GAME_MODE; side = 0;
        }
        /* asm:24135-24138; with the link up _serial_off stays (the A4 persists). */
        game_mode = modes; player = side; serial_off = core_custom_live && core_link ? line : 1;
        seed = core_land.from; ground_in = core_land.ground;
        int counts[2];
        int generated = custom_generate(core_land.arg, counts);
        terrain = (unsigned)ground_in;
        int16_t speed = stats_build_period(1);   /* LAB_516E2 */
        original.record.speed = speed > 0 && speed < 256 ? (uint8_t)speed : 0;
        original.record.your_count = (uint8_t)counts[0]; original.record.enemy_count = (uint8_t)counts[1];
        prg_a4 = live;
        if (!generated || terrain > 3 || !original.record.speed) {
            POP_LOG_ERROR("Custom landscape %d rejected: unsupported starting population or speed", (int)core_land.from);
            return 0;
        }
    } else if (selection.kind == START_ORIGINAL) {
        LevelTable levels;
        if (!level_data_load(directory, &levels, error, sizeof error) ||
            !level_data_select(&levels, (unsigned)scenario, &original)) {
            POP_LOG_ERROR("Original level data: %s", error);
            return 0;
        }
        terrain = original.record.terrain;
    } else if (selection.kind == START_TUTORIAL) {
        /* strABC+$10 speed 10, ground 0, conq_06/07 populations. */
        original.record = (LevelRecord){10, 10, 0, 0, 1, 0, 15, 3, 0};
        terrain = 0;
    } else terrain = demo_levels[scenario][5];
    unsigned selected_speed = selection.kind != START_SYNTHETIC ? original.record.speed : demo_levels[scenario][1];
    if (!selected_speed) {
        POP_LOG_ERROR("Start %d rejected: unsupported zero terrain mailbox cadence divisor", scenario);
        return 0;
    }
    if (!land_rules_load(directory, terrain, &loaded, error, sizeof error)) {
        POP_LOG_ERROR("Original LAND data: %s. Set POPULOUS_DATA_DIR to extracted land0..3.",error);
        return 0;
    }
    /* raw/font.dat is byte-identical to _font_data (asm:21949-22372);
     * optional, the end screen omits its text without it. */
    char font_path[4160];
    snprintf(font_path, sizeof font_path, "%s/font.dat", directory);
    FILE *font_file = fopen(font_path, "rb");
    game_font_loaded = font_file && fread(game_font, 1, sizeof game_font, font_file) == sizeof game_font &&
                       fgetc(font_file) == EOF;
    if (font_file) fclose(font_file);
    if (selection.kind == START_ORIGINAL) {
        uint8_t *live = prg_a4;
        prg_a4 = staged + 32768;
        int generated = original_generate(&original);
        prg_a4 = live;
        if (!generated) {
            POP_LOG_ERROR("Original level %d rejected: insufficient terrain or unsupported starting population", scenario);
            return 0;
        }
    } else if (selection.kind == START_TUTORIAL) {
        uint8_t *live = prg_a4;
        prg_a4 = staged + 32768;
        int generated = tutorial_generate();
        prg_a4 = live;
        if (!generated) { POP_LOG_ERROR("Tutorial rejected: unsupported starting population"); return 0; }
    }
    GraphicsAssets *loaded_assets = graphics_load(asset_directory,terrain,loaded.sprite_set,error,sizeof error);
    if (!loaded_assets) { POP_LOG_ERROR("Original graphics: %s",error); return 0; }
    graphics_free(game_assets);
    game_assets = loaded_assets;
    diagnostic_view = 0;
    current_start = selection;
    current_scenario = scenario;
    if (selection.kind != START_SYNTHETIC) core_custom_live = 1;
    /* _clear_map (asm:1023-1188) and _clear_send (asm:18234-18254) never write
     * 0x99fc, good_pop/bad_pop or _stats+0x14/+0x16/+0x22/+0x26/+0x2a; only
     * _move_peeps zeroes them (asm:3280-3320), after _devil_effect (asm:3268). */
    for (int owner = 0; owner < 2; ++owner) {
        static const struct { int offset, size; } kept[] = {
            {0x99fc,2},{0x99fe,4},{0x82ca,2},{0x82cc,2},{0x82d8,4},{0x82dc,4},{0x82e0,4}};
        for (size_t field = 0; field < sizeof kept / sizeof kept[0]; ++field) {
            int offset = kept[field].offset + owner * (kept[field].offset >= 0x99fc ? 16 : 46);
            memcpy(staged + 32768 + (int16_t)offset, A4_DISP(offset), (size_t)kept[field].size);
        }
    }
    /* Sound state is static data in the original, never reset by a start:
     * _music_now.._sound_in, _seqlen, _no_sounds.._Soundpri, the toggles. */
    static const struct { int offset, size; } sound_kept[] = {
        {0x3152,6},{0x3378,2},{0x3542,0x14},{0x8250,4}};
    for (size_t field = 0; field < sizeof sound_kept / sizeof sound_kept[0]; ++field)
        memcpy(staged + 32768 + (int16_t)sound_kept[field].offset, A4_DISP(sound_kept[field].offset),
               (size_t)sound_kept[field].size);
    /* Two players: the original A4 keeps the serial port and its settings
     * (_serial ring, _quick_io, _land_checksum, _use_modem, _login, _baud_rate). */
    if (selection.kind == START_CUSTOM && core_link) {
        static const struct { int offset, size; } serial_kept[] = {
            {0x989c,8+SERIAL_RING_SIZE},{0x3056,2},{0x3082,2},{0x3084,2},{0x3086,20},{0x83b8,2}};
        for (size_t field = 0; field < sizeof serial_kept / sizeof serial_kept[0]; ++field)
            memcpy(staged + 32768 + (int16_t)serial_kept[field].offset, A4_DISP(serial_kept[field].offset),
                   (size_t)serial_kept[field].size);
    }
    memcpy(prg_a4_area, staged, sizeof prg_a4_area);
    core_install_land(&loaded);
    for (int index = 0; index < 25; ++index) dir_offset_table(index) = core_offsets[index];
    for (int index = 0; index < 9; ++index) city_block_table(index) = core_city[index];
    for (int index = 0; index < 11; ++index) mana_value_table(index) = (uint32_t)core_mana_values[index];
    if (selection.kind == START_SYNTHETIC) {
    for (int owner = 0; owner < 2; ++owner) {
        core_mana_long(owner) = 399; /* _setup_display asm:1070 */
        core_tendency(owner) = 1; core_target(owner) = 0x820;
    }
    core_result = core_battles = core_settlements = core_tool = 0; core_set_pointer();
    const uint8_t *entry = demo_levels[scenario];
    conquest = entry[0];
    conq_01_speed = entry[1];
    conq_02_enemypow = entry[2];
    conq_03_yourpow = entry[3];
    conq_04_mode = entry[4];
    conq_05_terrain = entry[5];
    conq_08_seed = entry[8];
    in_conquest = scenario * 5;
    current_start_pop_you = entry[6];
    current_start_pop_enemy = entry[7];
    demo_mana = 50;
    demo_shield_timer = growth_tick = 0;
    prg_quit = 0;
    _setup_display();
    } else {
        current_start_pop_you = original.record.your_count;
        current_start_pop_enemy = original.record.enemy_count;
        demo_mana = 50;
        demo_shield_timer = growth_tick = 0;
        prg_quit = 0;
    }
    /* Conquest start asm:12762-12767 and speed asm:409-411, not static "abc".
     * Publish only after all fallible generation/asset work has succeeded. */
    core_clear_terrain_mailboxes();
    /* Every start: _setup_display clears _surender (asm:1149) and
     * _two_players derives _not_player from _player (asm:11779-11786). */
    surrender_side = -1; not_player = player ? 0 : 1;
    /* Single-player conquest start sets _serial_off (asm:12768); the end
     * score reads it (asm:14239). _score was cleared with the staged A4. */
    if (selection.kind != START_CUSTOM) serial_off = 1;
    core_turn_pending = 0;
    core_end_shown = 0; core_end_next = -1; core_disk_message = CORE_DISK_NONE;
    if (selection.kind != START_CUSTOM) {   /* the custom _stats stand (asm:430-457) */
        stats_build_mode(0) = 0; stats_build_mode(1) = 1;
        stats_build_latch(0) = 1; stats_build_latch(1) = 0;
        stats_build_period(0) = 1; stats_build_period(1) = selected_speed;
    }
    if (selection.kind == START_TUTORIAL) {
        /* The copied _stats words stand; _start_game sets _pause (asm:13988). */
        for (int owner = 0; owner < 2; ++owner)
            for (int field = 6; field <= 0x10; field += 2)
                A4_WORD(stats_base(owner) + field) = (int16_t)(tutorial_stats[owner][field] << 8 | tutorial_stats[owner][field+1]);
        pause = 1;
    }
    core_result = core_battles = core_settlements = core_tool = core_swamp_selected = 0; core_set_pointer();
    xoff = yoff = 6;
    if (selection.kind != START_SYNTHETIC) {
        /* Host camera accommodation: original inhabitants are not on demo plots. */
        int cell = peep_word(core_leader(0) - 1, 8);
        xoff = cell % 64 - 3;
        yoff = cell / 64 - 3;
        if (xoff < 0) xoff = 0;
        if (xoff > 56) xoff = 56;
        if (yoff < 0) yoff = 0;
        if (yoff > 56) yoff = 56;
    }
    SDL_SetWindowTitle(prg_screen->window,"Populous - original-rule intermediate port");
    return 1;
}

static void game_tick(void)
{
    /* Two players: _get_message's exchange every pass (05_serial.c). */
    if (core_serial_engaged()) { core_serial_tick(); return; }
    if (pause || prg_quit || core_result || core_req.kind) return;
    /* _animate asm:676-738: with _paint_map set neither _do_funny nor
     * _move_peeps runs (people stand still, _game_turn stays, no end-game
     * check); the frame only plots the peeps. Mailboxes are still read. */
    if (paint_map) {
        if (view_timer && !--view_timer) view_who = old_view_who;
        core_consume_terrain_mailboxes();
        return;
    }
    game_turn = (int16_t)((uint16_t)game_turn + 1u);
    /* asm:740-750: a temporary view (_set_temp_view) lapses after 10 turns.
     * The original also lapses it while a button is held; the port has no
     * held-button state, so only the countdown applies. */
    if (view_timer && !--view_timer) view_who = old_view_who;
    core_do_funny();   /* asm:680, before _move_peeps */
    _move_peeps();
    /* _move_peeps asm:4323-4347, after the peep loop: the pass's pre-move
     * good_pop or a surrender ends the game, _player first (___end_game(1)
     * lost, else ___end_game(0) won). There is no draw. */
    if (!core_good_pop(player) || player == surrender_side) core_result = 2;
    else if (!core_good_pop(not_player) || not_player == surrender_side) core_result = 1;
    /* Preserve the C population/result timing; never modify terrain mid-movement. */
    if (core_result) core_clear_terrain_mailboxes();
    else core_consume_terrain_mailboxes();
    /* asm:752-760: at game turn 4096 place type _start_seed&3 (3: none). */
    if (game_turn == 0x1000) core_place_funny(start_seed & 3, 0);
}

static void game_render(void)
{
    ensure_prg_screen();
    if (core_lord.shown && prg_screen) { core_draw_lord(); amiga_present(prg_screen); return; }
    ___draw_it(yoff, xoff);
    if (prg_screen) amiga_present(prg_screen);
}

static void _keyboard(void) { POP_LOGD("PRG", "_keyboard"); }
static void _mouse(void)    { POP_LOGD("PRG", "_mouse"); }

/* =========================================================================
 * TRANSLATED FROM THE ASSEMBLY CHUNK YOU PASTED (populous.prg, 1993 Hit Squad)
 * _main, _rs, _closeall, mouse/serial setup, open/close lib, _setup_display
 * (full conquest variable handling with the conq_ bytes, seed/terrain/powers/mode),
 * _animate (the heartbeat with icon setup, conquest show_world path, move_mana +
 * draw + sprite layers, peeps overview dots, do_funny/move_peeps, view_timer,
 * game_turn funny, show_shield, keyboard numpad scrolls + debug keys A(win)/F9(stats)/*(cheat),
 * sculpt/interogate/zoom/button handling).
 *
 * 1993 notes ("new section not in 1989", "1989 has two lines here", "Peter's interrupt",
 * conquest comments on seed/terrain/power bitfields/mode) are kept as comments.
 * A4 accesses use the DISP macros (exact d16 displacements from the listing).
 * Graphics are routed to the SDL layer (or printf + TODO for unimplemented pieces).
 * More bodies (the callees like _move_peeps, _draw_map etc.) will be filled when
 * you paste their assembly.
 * ========================================================================= */

/* --- translated small init / cleanup / interrupt / lib functions --- */

static void ensure_prg_screen(void)
{
    if (!prg_screen) {
        prg_screen = amiga_create_screen(320, 256, 4);  /* 16 colors for starters */
        if (prg_screen) {
            for (int index = 0; index < 16; ++index)
                amiga_set_color(prg_screen,index,graphics_palette[index]);
        }
    }
}

static void _main(void)
{
    ensure_prg_screen();
    if (!prg_screen) { prg_status = EXIT_FAILURE; return; }
    _openlibrary();

    /* AllocMem sequence for the big buffers (exact sizes + attributes from the PEA/JSR) */
    sprite_data    = amiga_alloc_mem(0x5960, 0);
    sprite_data_32 = amiga_alloc_mem(0x2080, 0);
    blk_data       = amiga_alloc_mem(0x8340, 0);
    back_scr       = amiga_alloc_mem(0x7D00, 2);
    screen_pointer = amiga_alloc_mem(0xfa00, 0x00010002);
    copper_list    = amiga_alloc_mem(0x384, 0x00010002);
    spare_cl       = amiga_alloc_mem(0x12c, 0x00010002);
    fb             = amiga_alloc_mem(0x104, 0x00010002);
    for (int index = 0; index < 8; ++index) {
        if (!host_buffers[index]) {
            POP_LOG_ERROR("PRG allocation failed");
            prg_status = EXIT_FAILURE;
            _closeall();
            return;
        }
    }

    last_tree = 0x32; last_rock = 0x2f;   /* _last_tree/_last_rock data, asm:24149-24152 */
    /* start_of_game from WB/cli (the $42 path or atoi of arg) */
    if (0 /* argc path */) {
        start_of_game = 0; /* ___atoi */
    } else {
        start_of_game = 0x42;
    }

    ___open_screen();
    ___read_sprites(sprites_in);
    ___sprite_to_amiga();
    ___start_game();

    /* 1989 had two lines (call + move); 1993 version */
    _setup_serial_interrupt();

    serial_period = 0x2580;
    ___set_serial();

    _animate();

    _closeall();
}

static void _rs(void)
{
    ___reset_screen(0);
}

static void _closeall(void)
{
    ___reset_screen(0);
    _free_inter();
    ___free_amiga_sprites();
    ___free_all_sounds();
    ___close_channels();

    for (int index = 0; index < 8; ++index) {
        amiga_free_mem(host_buffers[index]);
        host_buffers[index] = NULL;
    }
    graphics_free(game_assets);
    game_assets = NULL;
    amiga_destroy_screen(prg_screen);
    prg_screen = NULL;

    POP_LOGD("HW", "INTENA = 0x0010");
    _free_serial_interrupt();
    _closelibrary();
}

static void _create_mouse(void)
{
    mouse_limit = 0x018f;
    _setup_interrupt();
}

static void _setup_interrupt(void)
{
    LAB_52C40 = 0x02; LAB_52C41 = 0;
    /* "petersint" (Peter Molyneux) + mousex + ___mouse handler */
    big_mousex = 0; big_mousey = 0;
    ___setup_mouse();
    ___AddIntServer(5, (void*)dint);
}

static void _set_mouse(void)
{
    ___setup_mouse();
}

static void _free_inter(void)
{
    ___RemIntServer(5, (void*)dint);
    ___Delay(9);
}

static void _setup_serial_interrupt(void)
{
    LAB_52DAA = 0x02; LAB_52DAB = 0;
    ___SetIntVector(0xb, (void*)serial_read_vec); /* serial_read */

    LAB_52DC0 = 0x02; LAB_52DC1 = 0;
    ___SetIntVector(0, (void*)serial_write_vec); /* serial_write */
}

static void _free_serial_interrupt(void)
{
    POP_LOGD("HW", "INTENA = 0x0801");
}

static void _closelibrary(void)
{
    _set_pri();
    ___CloseLibrary((void*)(uintptr_t)GfxBase_prg);
    ___CloseLibrary((void*)(uintptr_t)IntuitionBase_prg);
}

static void _set_pri(void) {}
static void _debug(void)   {}   /* empty in both 1989 and 1993 */

static void _openlibrary(void)
{
    _set_pri();
    GfxBase_prg = (uint32_t)(uintptr_t)___OpenLibrary("graphics.library", 0x1f);
    if (!GfxBase_prg) ___exit(1);
    IntuitionBase_prg = (uint32_t)(uintptr_t)___OpenLibrary("intuition.library", 0x1f);
    if (!IntuitionBase_prg) ___exit(1);
    pal = 0; /* real: (GfxBase+$ce & 4) */
}

/* _setup_display (conquest path + normal path, with the detailed comments from source) */
static void _setup_display(void)
{
    if (in_conquest != (int16_t)0xffff) {
        /* Load map variables
         * seed = level mod 8 + bytes8+9
         * added last, suggesting the 5x expansion from 100 to 495 levels was late */
        seed = (in_conquest & 7) + conq_08_seed;
        start_seed = seed;

        /* Terrain type 0-3 */
        ground_in = conq_05_terrain;
        ___load_ground(0, ground_in);

        _clear_map();

        /* #0 and #1 can be 0 to 10 */
        LAB_516DE = conquest;
        LAB_516E2 = conq_01_speed;

        /* 02 and #3 can be one of 25 values from 0 to 16,135
         * bitfields for which powers can be used */
        LAB_516E0 = (conq_02_enemypow << 3) | 7;
        LAB_516B2 = (conq_03_yourpow << 3) | 7;

        /* #4 game mode - build on people/towns/up only/mo build, swamps, water */
        game_mode = conq_04_mode;
    } else {
        start_seed = seed;
        if (seed) {
            if (___newrand() & 1) {
                if (1 /* $A,A5 == -1 */) {
                    ground_in++;
                    if (ground_in > 3) ground_in = 0;
                    ___load_ground(0, ground_in);
                }
            }
        }
        if (0xffff != ground_in) {
            ___load_ground(0, ground_in);
        }
        _clear_map();
    }

    { /* Generate the synthetic map before the first frame. */
        _make_alt();
        _make_map();   /* real version takes 4 args - stub for now */
        _make_woods_rocks();
    }

    ___Setscreen(0xffff, (void*)(uintptr_t)back_scr, (void*)(uintptr_t)back_scr);
    _draw_map();   /* real version takes 4 args from the chunk */

    ___copy_screen((void*)(uintptr_t)w_screen, (void*)(uintptr_t)back_scr);
    ___copy_screen((void*)(uintptr_t)d_screen, (void*)(uintptr_t)back_scr);

    ___Setscreen(0xffff, (void*)-1, (void*)(uintptr_t)w_screen);

    _place_first_people();
    seed++;

    /* Quick visible game init status for testing different levels via CLI */
    POP_LOGD("INIT", "conquest=%d speed=%d enemy_pow=0x%02x your_pow=0x%02x mode=%d terrain=%d seed=%d",
           conquest, conq_01_speed, conq_02_enemypow, conq_03_yourpow, game_mode, ground_in, start_seed);
}


/* Visible minimal drawing so you actually SEE something (motivation!).
 * These touch the real chunky_buffer and call present.
 * Full accurate versions come when you paste more assembly.
 */
static void _clear_map(void)
{
    /* _alt is 65*65 words; cell buffers are each 64*64 bytes (asm 54274). */
    memset(map_vertices, 0, 65 * 65 * 2);
    memset(map_blk, 0, 4096);
    memset(map_alt, 0, 4096);
    memset(map_bk2, 0, 4096);
    memset(map_who, 0, 4096);
}

static void _make_alt(void)
{
    /* Valid synthetic plane for the bounded scenarios; original generator pending. */
    for (int row = 0; row <= 64; ++row)
        for (int column = 0; column <= 64; ++column) core_set_vertex(column,row,1);
}
static void _make_woods_rocks(void) {}

#if 0
static void _place_first_people(void)
{
    ensure_prg_screen();
    if (!prg_screen) return;
    /* seed a few bright demo peeps dots */
    for (int i = 0; i < 20; i++) {
        int px = 50 + (i * 7) % 200;
        int py = 50 + (i * 5) % 140;
        if (px < prg_screen->width && py < prg_screen->height)
            prg_screen->chunky_buffer[py * prg_screen->width + px] = prg_screen->palette[14]; /* bright */
    }
}
#endif

/* _move_mana asm:3164-3244, presentation only: the first _mana_values (A4
 * 0x8486) entry not below the player's mana picks the scale step, the
 * remainder (*8, divs) the eighth within it; sprite 69 is drawn at
 * x 160+16*(step-1)+2*eighth, y 8+8*(step-1)+eighth, past step 9 at
 * (0x137,0x57). Mana <= _mana_values[0] (unreachable: asm reads the long
 * before the table) is clamped to step 1 here. */
static void _move_mana(void)
{
    if (!game_assets || player < 0 || player > 1) return;
    int32_t mana = (int32_t)core_mana_long(player);
    int step = 0;
    while (step < 10 && mana > (int32_t)mana_value_table(step)) ++step;
    int x = 0x137, y = 0x57;
    if (step <= 9) {
        if (!step) step = 1;
        int32_t low = (int32_t)mana_value_table(step-1), high = (int32_t)mana_value_table(step);
        int eighth = high != low ? (int)(int16_t)((mana - low) * 8 / (high - low)) : 0;
        x = (step-1)*16 + eighth*2 + 0xa0; y = (step-1)*8 + eighth + 8;
    }
    graphics_blit(game_pixels,64000,320,200,graphics_sprites(game_assets),69,x,y);
}

/* _pointer (A4 0x84b6) indexes _a_sprite[], built by _sprite_to_amiga
 * asm:16783-16830 from these _sprite_data frames. Values as the original:
 * query 0, terrain player*3+1 (asm:615-620, 1054-1056, 2110-2116), magnet
 * player+2 (asm:2125-2131), swamp 5 (asm:2042), after the swamp target
 * mode-1 (asm:7480-7487). Port deviation: the port keeps its magnet tool
 * after a swamp target, so its magnet pointer is restored there. */
static const uint8_t core_pointer_frames[8] = {0x44,0x4e,0x50,0x51,0x4f,0x53,0x52,0x81};
