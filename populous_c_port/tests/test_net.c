/* Two-player link tests: the serial_link channel (memory pair, TCP), the
 * original serial protocol of modules/05_serial.c against a scripted other
 * side, and two processes playing over 127.0.0.1 with the same state. */
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#define main launcher_main
#define amiga_alloc_mem launcher_alloc_mem
#define amiga_free_mem launcher_free_mem
#include "../populous_init.c"
#undef amiga_alloc_mem
#undef amiga_free_mem
#undef main
#include "../populous_prg.c"

static uint64_t test_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}
static void tiny_sleep(void) { struct timespec pause_for = {0, 200000}; nanosleep(&pause_for, NULL); }

/* ---- serial_link ---------------------------------------------------- */

static void test_memory_pair(void)
{
    SerialLink *a, *b;
    assert(serial_link_mem_pair(&a, &b));
    assert(serial_link_state(a) == SERIAL_LINK_OPEN && serial_link_poll(b) == SERIAL_LINK_OPEN);
    assert(serial_link_write(a, "abc", 3) == 3 && serial_link_write(a, "de", 2) == 2);
    assert(serial_link_available(b) == 5 && !serial_link_available(a));
    char text[8] = {0};
    assert(serial_link_read(b, text, 2) == 2 && !memcmp(text, "ab", 2));
    assert(serial_link_read(b, text, 8) == 3 && !memcmp(text, "cde", 3));
    assert(!serial_link_read(b, text, 8));
    /* A large burst grows the queue and keeps the order. */
    static uint8_t burst[5000], back[5000];
    for (size_t index = 0; index < sizeof burst; ++index) burst[index] = (uint8_t)(index * 7);
    assert(serial_link_write(b, burst, sizeof burst) == sizeof burst);
    assert(serial_link_read(a, back, sizeof back) == sizeof back && !memcmp(burst, back, sizeof back));
    serial_link_close(a);
    assert(serial_link_state(b) == SERIAL_LINK_CLOSED && serial_link_write(b, "x", 1) == 0);
    serial_link_close(b);
}

static void test_endpoints(void)
{
    char host[64]; uint16_t port;
    assert(serial_link_parse_endpoint("192.168.1.5", host, sizeof host, &port) && !strcmp(host, "192.168.1.5") && port == 2611);
    assert(serial_link_parse_endpoint("10.0.0.2:4000", host, sizeof host, &port) && !strcmp(host, "10.0.0.2") && port == 4000);
    assert(serial_link_parse_endpoint("[::1]:99", host, sizeof host, &port) && !strcmp(host, "::1") && port == 99);
    assert(serial_link_parse_endpoint("fe80::1", host, sizeof host, &port) && !strcmp(host, "fe80::1") && port == 2611);
    assert(serial_link_parse_endpoint("pc.local", host, sizeof host, &port) && port == 2611);
    const char *bad[] = {"", ":2611", "host:", "host:0", "host:65536", "host:12x", "[::1", "[]:5"};
    for (size_t index = 0; index < sizeof bad / sizeof *bad; ++index)
        assert(!serial_link_parse_endpoint(bad[index], host, sizeof host, &port));
}

static void test_tcp_loopback(void)
{
    char error[128];
    SerialLink *host = serial_link_tcp_host(0, error, sizeof error);
    assert(host && serial_link_port(host));
    /* Written before a peer exists: lost, as on an unplugged cable. */
    assert(serial_link_write(host, "lost", 4) == 0);
    SerialLink *join = serial_link_tcp_join("127.0.0.1", serial_link_port(host), error, sizeof error);
    assert(join);
    uint64_t deadline = test_ms() + 3000;
    for (;;) {
        SerialLinkState hosting = serial_link_poll(host), joining = serial_link_poll(join);
        if (hosting == SERIAL_LINK_OPEN && joining == SERIAL_LINK_OPEN) break;
        assert(test_ms() < deadline); tiny_sleep();
    }
    assert(serial_link_write(join, "ping", 4) == 4);
    char text[8] = {0};
    while (serial_link_available(host) < 4) { serial_link_poll(host); assert(test_ms() < deadline); tiny_sleep(); }
    assert(serial_link_read(host, text, 8) == 4 && !memcmp(text, "ping", 4));
    serial_link_close(join);
    while (serial_link_poll(host) != SERIAL_LINK_CLOSED) { assert(test_ms() < deadline); tiny_sleep(); }
    serial_link_close(host);
    /* Joining before the host listens keeps retrying. */
    SerialLink *early = serial_link_tcp_join("127.0.0.1", 1, error, sizeof error);
    assert(early && serial_link_poll(early) == SERIAL_LINK_CONNECTING);
    serial_link_close(early);
    assert(!serial_link_tcp_join("no-such-host.invalid", 2611, error, sizeof error) && error[0]);
}

/* ---- The original protocol against a scripted other side --------------- */

static SerialLink *peer;   /* the other end of core_link */
static void boot_custom(void)
{
    core_custom_live = 0;
    core_land = (CustomLand){0, 1, 0};
    assert(game_reset_start((StartSelection){START_CUSTOM, 0}));
}
static void attach_pair(void)
{
    SerialLink *mine;
    assert(serial_link_mem_pair(&mine, &peer));
    core_serial_attach(mine);
}
static size_t peer_take(uint8_t *bytes, size_t size) { return serial_link_read(peer, bytes, size); }
static void peer_send(const void *bytes, size_t size) { assert(serial_link_write(peer, bytes, size) == size); }
static void click_button(const uint8_t *button)
{
    mousex = (int16_t)(core_rec_w(button,0) + 2); mousey = (int16_t)(core_rec_w(button,2) + 2);
    core_requester_click();
}

static void test_compress_and_ring(void)
{
    uint8_t box[3] = {14, 63, 63}, back[3];
    assert(core_compress(box) == 0xefff);
    box[0] = 3; box[1] = 5; box[2] = 9;
    assert(core_compress(box) == 0x3149);
    core_decompress(back, 0x3149);
    assert(back[0] == 3 && back[1] == 5 && back[2] == 9);
    box[0] = 0x1f; box[1] = 0x7f; box[2] = 0x40;   /* masked like ANDI */
    core_decompress(back, core_compress(box));
    assert(back[0] == 15 && back[1] == 63 && back[2] == 0);
    seed = 0x1234;
    uint8_t packet[5] = {1, 2, 3, 0, 0};
    core_full_packet(packet);
    assert(packet[3] == 0x34 && packet[4] == (uint8_t)(1 + 2 + 3 + 0x34));

    attach_pair();
    static uint8_t burst[130];
    for (int index = 0; index < 130; ++index) burst[index] = (uint8_t)index;
    peer_send(burst, sizeof burst);
    core_serial_pump();
    assert(core_serial_count() == SERIAL_RING_SIZE - 1 && serial_link_available(core_link) == 130 - 119);
    uint8_t got[130]; int count = 0;
    assert(core_read_serial(got, 130, &count, 0) == 0 && count >= 119);
    while (count < 130) { int result = core_read_serial(got, 130, &count, 0); assert(result >= 0); }
    assert(!memcmp(got, burst, 130));
    /* _check_serial compares the raw indices (asm:16984-17008). */
    serial_rd = 118; serial_wr = 2;
    assert(core_serial_count() == 4 && core_check_serial(116) && !core_check_serial(117));
    core_clear_serial();
    assert(!serial_rd && !serial_wr && !core_serial_count());
    core_serial_cancel = 1;
    count = 0;
    peer_send("x", 1);
    assert(core_read_serial(got, 1, &count, 1) == -1 && !count);   /* the key wins over data */
    core_serial_cancel = 0;
    assert(core_read_serial(got, 1, &count, 1) == 1 && got[0] == 'x');
    core_serial_detach();
    serial_link_close(peer);
    count = 0;
    assert(core_read_serial(got, 1, &count, 1) == -1);   /* no cable */
}

static void test_full_checking(void)
{
    attach_pair();
    uint8_t packet[5] = {1, 30, 31, 0x55, 0};
    packet[4] = (uint8_t)(1 + 30 + 31 + 0x55);
    uint8_t seen[16];
    /* _write_full: a 'b' asks again (answered 'g'), junk is dropped, '|' ends. */
    memset(&core_full, 0, sizeof core_full);
    assert(core_write_full(packet) == CORE_SERIAL_PENDING);
    assert(peer_take(seen, 16) == 5 && !memcmp(seen, packet, 5));
    peer_send("b", 1);
    assert(core_write_full(packet) == CORE_SERIAL_PENDING);
    assert(peer_take(seen, 16) == 6 && seen[0] == 'g' && !memcmp(seen + 1, packet, 5));
    peer_send("?", 1);
    assert(core_write_full(packet) == CORE_SERIAL_PENDING && !peer_take(seen, 16));
    peer_send("|", 1);
    assert(core_write_full(packet) == 0);
    /* _read_full: a bad sum sends 'b' until 'g', then the packet again. */
    uint8_t into[5], bad[5] = {1, 30, 31, 0x55, 0};
    memset(&core_full, 0, sizeof core_full);
    assert(core_read_full(into) == CORE_SERIAL_PENDING && !peer_take(seen, 16));
    peer_send(bad, 5);
    assert(core_read_full(into) == CORE_SERIAL_PENDING);
    assert(core_read_full(into) == CORE_SERIAL_PENDING);
    size_t asked = peer_take(seen, 16);
    assert(asked >= 2);
    for (size_t index = 0; index < asked; ++index) assert(seen[index] == 'b');
    peer_send("g", 1);
    assert(core_read_full(into) == CORE_SERIAL_PENDING);
    while (peer_take(seen, 16)) {}
    peer_send(packet, 5);
    assert(core_read_full(into) == 0 && !memcmp(into, packet, 5));
    assert(peer_take(seen, 16) == 1 && seen[0] == '|');
    /* Shift+Esc aborts (returns 1). */
    memset(&core_full, 0, sizeof core_full);
    core_serial_cancel = 1;
    assert(core_read_full(into) == 1);
    core_serial_cancel = 0;
    core_serial_detach();
    serial_link_close(peer);
}

/* One turn of _get_message as _player, the other side scripted. */
static int turn_until_done(void)
{
    for (int pass = 0; pass < 8; ++pass) {
        int result = core_turn_exchange();
        if (result != CORE_SERIAL_PENDING) return result;
    }
    return CORE_SERIAL_PENDING;
}
static void test_turn_exchange(void)
{
    boot_custom();
    attach_pair();
    uint8_t seen[32];
    /* Quick (NO SERIAL CHECKING), _player 0, a 16th turn: seed, then word. */
    quick_io = 1; player = 0; not_player = 1; game_turn = 32; seed = 0x2345;
    uint8_t *own = stats_bytes(0), *other = stats_bytes(1);
    own[0] = 1; own[1] = 70; own[2] = 12;   /* x is clipped to 63 first */
    memset(&core_turn, 0, sizeof core_turn);
    assert(turn_until_done() == CORE_SERIAL_PENDING);
    assert(peer_take(seen, 32) == 2 && core_get_be16(seen) == 0x2345);
    peer_send("\x23\x45", 2);
    assert(turn_until_done() == CORE_SERIAL_PENDING);
    assert(peer_take(seen, 32) == 2 && (uint16_t)core_get_be16(seen) == 0x1fcc && own[1] == 63);
    peer_send("\x24\x8a", 2);   /* lower at 18,10 */
    assert(turn_until_done() == 1 && !core_req.kind);
    assert(other[0] == 2 && other[1] == 18 && other[2] == 10);
    /* _player 1 reads first; a different _seed is the landscape error. */
    player = 1; not_player = 0; game_turn = 48; own = stats_bytes(1); other = stats_bytes(0);
    own[0] = 0; own[1] = 0; own[2] = 0;
    memset(&core_turn, 0, sizeof core_turn);
    assert(turn_until_done() == CORE_SERIAL_PENDING && !peer_take(seen, 32));
    peer_send("\x11\x11\x00\x00", 4);
    assert(turn_until_done() == 1);
    assert(peer_take(seen, 4) == 4 && core_get_be16(seen) == 0x2345 && !core_get_be16(seen + 2));
    assert(core_req.kind == CORE_REQ_SMSG && core_smsg_text == CORE_G_LANDSCAPE);
    /* Quick: ten 5-byte groups led by a command-15 word (asm:11208-11224). */
    for (int group = 0; group < 10; ++group) assert(peer_take(seen, 5) == 5 && (seen[0] & 0xf0) == 0xf0);
    assert(!peer_take(seen, 32));
    pause = 0;
    core_smsg_layout(0); click_button(core_end_ok);
    assert(!core_req.kind && serial_off == 1 && pause == 1 && stats_build_mode(0) == 1 && !core_link);
    serial_link_close(peer);

    /* Full checking, _player 0: write_full then read_full; seed bytes match. */
    attach_pair();
    quick_io = 0; serial_off = 0; pause = 0; player = 0; not_player = 1; seed = 0x0042;
    own = stats_bytes(0); other = stats_bytes(1);
    own[0] = 3; own[1] = 8; own[2] = 9;
    memset(&core_turn, 0, sizeof core_turn); memset(&core_full, 0, sizeof core_full);
    assert(turn_until_done() == CORE_SERIAL_PENDING);
    assert(peer_take(seen, 32) == 5 && seen[3] == 0x42 && seen[4] == (uint8_t)(3 + 8 + 9 + 0x42));
    peer_send("|", 1);
    uint8_t reply[5] = {14, 1, 7, 0x42, 0};
    reply[4] = (uint8_t)(14 + 1 + 7 + 0x42);
    peer_send(reply, 5);
    assert(turn_until_done() == 1 && !core_req.kind);
    assert(other[0] == 14 && other[2] == 7 && peer_take(seen, 32) == 1 && seen[0] == '|');
    /* The other side hangs up: "gone off line" instead of the abort text. */
    memset(&core_turn, 0, sizeof core_turn); memset(&core_full, 0, sizeof core_full);
    serial_link_close(peer);
    assert(turn_until_done() == 1 && core_req.kind == CORE_REQ_SMSG && core_smsg_text == CORE_G_GONE);
    core_smsg_layout(0); click_button(core_end_ok);
    assert(serial_off == 1 && pause == 1 && stats_build_mode(1) == 1 && !core_link);
    pause = 0;
}

/* _animate asm:752-760 before _get_message (asm:1010-1012): a paused pass at
 * game turn 4096 places the funny, drawing once, before the 16th-turn
 * quick exchange sends _seed. */
static void test_serial_funny_order(void)
{
    boot_custom();
    attach_pair();
    quick_io = 1; serial_off = 0; player = 0; not_player = 1;
    for (int slot = 0xd1; slot < 0xd3; ++slot) set_peep_population(slot, 0);
    start_seed = 0; pause = 1; game_turn = 0x1000; view_timer = 0;
    uint8_t *own = stats_bytes(0);
    own[0] = own[1] = own[2] = 0;
    uint16_t before = seed;
    uint16_t drawn = (uint16_t)((uint16_t)(before * 0x24a1u) + 0x24dfu) & 0x7fff;
    memset(&core_turn, 0, sizeof core_turn);
    core_turn_pending = 0; core_consume_next = -1;
    uint8_t seen[8];
    game_tick();
    assert(peep_population(0xd1) == 1 && peep_position(0xd1) == (drawn % 125) / 2 + 0xfc0);
    assert(seed == drawn && game_turn == 0x1000);
    for (int pass = 0; pass < 8 && core_turn_pending; ++pass) game_tick();
    assert(peer_take(seen, 8) == 2 && core_get_be16(seen) == drawn);
    serial_link_close(peer);
    core_serial_attach(NULL);
    core_turn_pending = 0;
    serial_off = 1; pause = 0;
}

/* _try_serial against a scripted other side, and its result codes. */
static void test_try_serial(void)
{
    boot_custom();
    attach_pair();
    uint8_t seen[64];
    start_seed = 77; game_mode = 5;
    core_cli_side = 0;
    core_two_open(0, CORE_TWO_FROM_CLI);
    assert(core_req.kind == CORE_REQ_TWO && core_two.side == 0 && core_two.this_land);
    /* CONNECT ME (row 7). */
    mousex = (int16_t)(core_rec_w(core_serial_entry(7),0) + 0x22); mousey = (int16_t)(core_rec_w(core_serial_entry(7),2) + 0x30);
    core_requester_click();
    assert(core_req.kind == CORE_REQ_TRY);
    core_serial_frame(); core_serial_frame();
    assert(peer_take(seen, 64) == 2 && seen[0] == 'r' && seen[1] == 'r' && core_try.count == 2);
    peer_send("r", 1);
    core_serial_frame();
    assert(peer_take(seen, 64) == 2 && seen[0] == 'r' && seen[1] == 'z');   /* 'r' answered by 'z' */
    uint8_t answer[22] = {0};
    int16_t words[11] = {1, 4321, 9, 3, 0x1ff, 6, 1, 2, 0, 0, 0};   /* EVIL, this landscape */
    for (int index = 0; index < 11; ++index) core_put_be16(answer + index*2, words[index]);
    peer_send("rrz", 3);
    peer_send(answer, 22);
    core_serial_frame();
    assert(core_req.kind == CORE_REQ_NONE && !serial_off && player == 0 && not_player == 1);
    /* GOOD takes EVIL's start, mode, terrain and its computer settings. */
    assert(start_seed == 4321 && game_mode == 9 && ground_in == 2 && current_start.kind == START_CUSTOM);
    assert(stats_rating(1) == 3 && stats_power_mask(1) == 0x1ff && stats_build_period(1) == 6);
    assert(!stats_build_mode(0) && !stats_build_mode(1) && !game_turn && !pause);
    while (peer_take(seen, 64)) {}
    /* Same side -> "HE'S TRYING TO BE GOOD"; the requester stays. */
    serial_off = 1;
    core_two_open(0, CORE_TWO_FROM_CLI);
    core_try_open();
    peer_send("r", 1);
    core_serial_frame();
    words[0] = 0;
    for (int index = 0; index < 11; ++index) core_put_be16(answer + index*2, words[index]);
    peer_send("z", 1); peer_send(answer, 22);
    core_serial_frame();
    assert(core_req.kind == CORE_REQ_TWO && core_two.message == 2 && serial_off == 1);
    /* Different checking -> 5; CANCEL in the try requester -> 99. */
    core_try_open();
    peer_send("z", 1);
    words[0] = 1; words[9] = 1;
    for (int index = 0; index < 11; ++index) core_put_be16(answer + index*2, words[index]);
    peer_send(answer, 22);
    core_serial_frame();
    assert(core_req.kind == CORE_REQ_TWO && core_two.message == 5);
    core_try_open();
    core_try_layout(0); click_button(core_end_cancel);
    core_serial_frame();
    assert(core_req.kind == CORE_REQ_TWO && core_two.message == 99);
    core_two_layout(0); click_button(core_end_cancel);   /* CANCEL leaves _two_players */
    assert(!core_req.kind && serial_off == 1);
    serial_link_close(peer);
    core_serial_detach();
}

/* ---- Two processes over 127.0.0.1 ---------------------------------------- */

typedef struct { uint64_t hash; int16_t turn; int message_ok, notice_ok; } SideReport;

static uint64_t state_hash(void)
{
    uint64_t hash = 1469598103934665603ull;
#define MIX(pointer, size) do { const uint8_t *bytes_ = (const uint8_t *)(pointer); \
        for (size_t index_ = 0; index_ < (size_t)(size); ++index_) { hash ^= bytes_[index_]; hash *= 1099511628211ull; } } while (0)
    MIX(A4_DISP(0xae86), 0x1081 * 2);   /* vertex heights */
    MIX(map_who, 4096); MIX(map_blk, 4096); MIX(map_alt, 4096); MIX(map_bk2, 4096);
    MIX(peeps, 208 * 22);
    int16_t scalars[] = {no_peeps, seed, game_turn, war_flag, game_mode, paint_map};
    MIX(scalars, sizeof scalars);
    for (int owner = 0; owner < 2; ++owner) {
        uint32_t mana = core_mana_long(owner);
        MIX(&mana, sizeof mana);
        MIX(stats_bytes(owner) + 5, 41);   /* the records past the mailbox and packet bytes */
    }
#undef MIX
    return hash;
}

#define NET_TURNS 120
static void play_side(int host, SerialLink *link, int quick, SideReport *report)
{
    alarm(15);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    boot_custom();
    core_serial_attach(link);
    core_cli_side = host ? 0 : 1;
    core_two_open(0, CORE_TWO_FROM_CLI);
    if (quick) { use_modem = 1; quick_io = 1; }
    core_try_open();
    uint64_t deadline = test_ms() + 8000;
    while (core_req.kind) { core_serial_frame(); assert(test_ms() < deadline); tiny_sleep(); }
    assert(!serial_off && player == (host ? 0 : 1) && !game_turn);
    memset(report, 0, sizeof *report);
    int posted_raise = 0, posted_message = 0, posted_options = 0;
    while (game_turn < NET_TURNS || core_turn_pending || core_consume_next >= 0 || core_req.kind) {
        assert(test_ms() < deadline);
        core_serial_frame();
        /* Scripted input, through the mailbox like the input handlers. */
        if (!core_req.kind && core_serial_can_post()) {
            if (host && !posted_raise && game_turn >= 10) posted_raise = core_serial_post(1, 30, 30);
            if (!host && !posted_raise && game_turn >= 12) posted_raise = core_serial_post(2, 20, 20);
            if (host && !posted_message && game_turn >= 20) posted_message = core_serial_post(14, 0, 2);
            if (!host && !posted_options && game_turn >= 40) posted_options = core_serial_post(14, 1, 7);
        }
        if (core_req.kind == CORE_REQ_MSG_OUT) {
            for (const char *key = "hello lan"; *key; ++key) core_msg_key(*key);
            core_msg_key(13);
        } else if (core_req.kind == CORE_REQ_MSG_IN && core_msg.done) {
            report->message_ok = !strcmp(core_message, "HELLO LAN");
            core_msg_key(13);
        } else if (core_req.kind == CORE_REQ_OPTIONS) {
            /* Toggle CAN USE SWAMP for the own side, then OK. */
            stats_power_mask(player) ^= 0x10;
            click_button(core_end_ok);
        } else if (core_req.kind == CORE_REQ_NOTICE) {
            report->notice_ok = core_notice_queue[0] == core_g_text[CORE_G_HIS];
            core_notice_layout(0); click_button(core_end_ok);
        }
        int before = game_turn;
        game_tick();
        if (game_turn == before) tiny_sleep();
        if (game_turn >= NET_TURNS && !core_turn_pending && core_consume_next < 0 && !core_req.kind) break;
    }
    assert(game_turn == NET_TURNS && !core_result);
    report->hash = state_hash();
    report->turn = game_turn;
}

static void test_two_processes(int quick)
{
    char error[128];
    SerialLink *host = serial_link_tcp_host(0, error, sizeof error);
    assert(host);
    uint16_t port = serial_link_port(host);
    int channel[2];
    assert(!pipe(channel));
    fflush(stdout);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        close(channel[0]);
        SerialLink *join = serial_link_tcp_join("127.0.0.1", port, error, sizeof error);
        assert(join);
        SideReport report;
        play_side(0, join, quick, &report);
        assert(write(channel[1], &report, sizeof report) == (ssize_t)sizeof report);
        close(channel[1]);
        _exit(0);   /* hang up without saying so */
    }
    close(channel[1]);
    SideReport mine, theirs;
    play_side(1, host, quick, &mine);
    assert(read(channel[0], &theirs, sizeof theirs) == (ssize_t)sizeof theirs);
    close(channel[0]);
    int status = 0;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    printf("net %s: %d turns, hash %016llx / %016llx\n", quick ? "quick" : "full", mine.turn,
           (unsigned long long)mine.hash, (unsigned long long)theirs.hash);
    assert(mine.turn == theirs.turn && mine.hash == theirs.hash);
    assert(theirs.message_ok && mine.notice_ok);
    /* The other process is gone: the next exchange says so and drops to one player. */
    uint64_t deadline = test_ms() + 4000;
    while (core_req.kind != CORE_REQ_SMSG) { assert(test_ms() < deadline); core_serial_frame(); game_tick(); tiny_sleep(); }
    assert(core_smsg_text == CORE_G_GONE);
    core_smsg_layout(0); click_button(core_end_ok);
    assert(serial_off == 1 && pause == 1 && stats_build_mode(not_player) == 1 && !core_link);
    int16_t turn = game_turn;
    game_tick();
    assert(game_turn == turn);   /* single play, paused */
    alarm(0);
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    if (argc == 2 && !strcmp(argv[1], "channel")) {
        test_memory_pair(); test_endpoints(); test_tcp_loopback();
    } else if (argc == 2 && !strcmp(argv[1], "protocol")) {
        assert(SDL_Init(SDL_INIT_VIDEO) == 0);
        test_compress_and_ring(); test_full_checking(); test_turn_exchange(); test_serial_funny_order(); test_try_serial();
        SDL_Quit();
    } else if (argc == 2 && !strcmp(argv[1], "tcp-full")) test_two_processes(0);
    else if (argc == 2 && !strcmp(argv[1], "tcp-quick")) test_two_processes(1);
    else { fprintf(stderr, "usage: test_net channel|protocol|tcp-full|tcp-quick\n"); return 2; }
    printf("net %s: PASS\n", argv[1]);
    return 0;
}
