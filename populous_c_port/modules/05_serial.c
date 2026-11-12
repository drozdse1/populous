/*
 * modules/05_serial.c
 * -------------------
 * Unity-included from populous_prg.c (do not compile alone).
 *
 * Two-player play over the serial port (src/populous_prg.asm). The cable is
 * the serial_link byte channel (TCP on a LAN, an in-memory pair in tests);
 * everything above the bytes follows the original:
 *   _r_ser_buff/_read_serial/_write_serial/_check_serial/_clear_serial
 *     asm:16984-17008, 20051-20186 (the 0x78-byte receive ring in the A4)
 *   _get_message lockstep exchange asm:17707-17928, _write_full/_read_full
 *     asm:18258-18372, _compress/_decompress asm:21105-21145
 *   _serial_message asm:11185-11261, _do_message asm:11098-11139,
 *   _get_a_message asm:11143-11181, _two_players asm:11265-11899,
 *   _try_serial asm:11903-12102
 *   _do_action y=2 (message) asm:18404-18575, y=6 (pause) asm:18591-18606,
 *     y=7 (options) asm:18607-18728, y=8 (game options) asm:18729-18942
 *
 * The original blocks in _read_serial (only Shift+Esc or a requester's
 * CANCEL aborts). The port keeps the game loop running instead: each
 * blocking routine is a resumable step (CORE_CORO_*) polled every tick or
 * frame, and the waiting requesters are core_req kinds, so the simulation
 * is stopped exactly where the original would sit in its loop.
 */

#define CORE_SERIAL_PENDING (-2)
/* Resumable step helpers: (s)->pc is the source line to resume at. */
#define CORE_CORO_BEGIN(s) switch ((s)->pc) { case 0:
#define CORE_CORO_WAIT(s, condition) do { (s)->pc = __LINE__; __attribute__((fallthrough)); \
    case __LINE__: if (!(condition)) return CORE_SERIAL_PENDING; } while (0)
#define CORE_CORO_YIELD(s) do { (s)->pc = __LINE__; return CORE_SERIAL_PENDING; \
    case __LINE__:; } while (0)
#define CORE_CORO_RETURN(s, value) do { (s)->pc = 0; return (value); } while (0)
#define CORE_CORO_END(s) default: break; } (s)->pc = 0

/* _g_text asm:24465-24486 (strings asm:21140-21250). */
static const char *const core_g_text[17] = {
    "SEND A MESSAGE", "MESSAGE FROM GOOD", "MESSAGE FROM EVIL",
    "ON TWO PLAYER SERIAL GAME\nSWITCHING BACK TO ONE\n PLAYER GAME IN PAUSE",
    "OTHER PLAYER HAS SAVED HIS\nGAME DO YOU WANT TO SAVE\nYOUR GAME NOW AS WELL",
    "OTHER PLAYER HAS CHANGED\nGAME OPTIONS", "OTHER PLAYER HAS SELECTED\nPAINT MODE",
    "OTHER PLAYER HAS CHANGED\nSIDES", "OTHER PLAYER HAS CHANGED\nHIS OPTIONS",
    "OTHER PLAYER HAS CHANGED\nYOUR OPTIONS", "OTHER PLAYER HAS SELECTED\nCOMPUTER ASSISTANCE",
    "OTHER PLAYER HAS TURNED OFF\nCOMPUTER ASSISTANCE", "OTHER PLAYER HAS GONE OFF\nLINE, NOW IN ONE PLAYER",
    "SERIAL IO ABORTED", "SERIAL CHECKSUM ERROR", "INCOMPATABLE LANDSCAPE ERROR",
    "INSERT THE ORIGINAL\nPOPULOUS DISK IN DF0:",
};
enum { CORE_G_SAVED = 4, CORE_G_OPTIONS = 5, CORE_G_PAINT = 6, CORE_G_SIDES = 7, CORE_G_HIS = 8,
       CORE_G_ASSIST_ON = 10, CORE_G_ASSIST_OFF = 11, CORE_G_GONE = 12, CORE_G_ABORT = 13,
       CORE_G_LANDSCAPE = 15 };   /* LAB_51BA8..LAB_51BD4 */

/* _serial_text asm:24930-25140: word x, word y, word width, text. */
static const CoreMenuEntry core_serial_template[20] = {
    {16,0x10,0x00,"STARTUP SERIAL"},         {16,0x1f,0xd0,"SEND LOGIN"},
    {16,0x29,0xd0,"BAUD RATE"},              {16,0x33,0x50,"\x80 DATALINK"},
    {16,0x3d,0x60,"\x80 FULL SERIAL CHECKING"}, {16,0x47,0x58,"\x80 I AM GOOD"},
    {16,0x51,0xa8,"\x81 USE THIS LANDSCAPE"}, {16,0x5b,0x60,"\x80 CONNECT ME"},
    {16,0x33,0x38,"\x81 MODEM"},             {16,0x3d,0x58,"\x81 NO SERIAL CHECKING"},
    {16,0x47,0x58,"\x81 I AM EVIL"},         {16,0x51,0xb0,"\x81 USE SAVED LANDSCAPE"},
    {16,0x65,0x0b,"HE'S TRYING TO BE GOOD"}, {16,0x65,0x0b,"HE'S TRYING TO BE EVIL"},
    {16,0x65,0x0b,"HE'S NOT USING A SAVED GAME"}, {16,0x65,0x0b,"HE'S USING A SAVED GAME"},
    {16,0x65,0x0b,"SERIAL ERROR!"},          {16,0x65,0x0b,"HE IS USING NO CHECKING"},
    {16,0x65,0x0b,"HE IS USING FULL CHECKING"}, {16,0x65,0x0b,"INCOMPATABLE SAVED GAMES"},
};

/* ---- The cable --------------------------------------------------------- */

static int core_serial_cancel;   /* Shift+Esc (_read_serial key $1b) or a requester's CANCEL */
static int core_link_reported = -1;
static uint32_t core_serial_wait_since;   /* host ms a wait began (port "waiting" overlay) */

static void core_put_be16(uint8_t *bytes, int value) { bytes[0] = (uint8_t)(value >> 8); bytes[1] = (uint8_t)value; }
static int16_t core_get_be16(const uint8_t *bytes) { return (int16_t)(bytes[0] << 8 | bytes[1]); }

static void core_serial_attach(SerialLink *link)
{
    if (core_link) serial_link_close(core_link);
    core_link = link; core_link_reported = -1;
    serial_rd = serial_wr = 0;
}
static void core_serial_detach(void)
{
    if (core_link) serial_link_close(core_link);
    core_link = NULL;
}
static int core_link_dead(void) { return !core_link || serial_link_state(core_link) == SERIAL_LINK_CLOSED; }
static int core_serial_count(void) { return (serial_wr - serial_rd + SERIAL_RING_SIZE) % SERIAL_RING_SIZE; }

/* _r_ser_buff asm:20058-20071, once per received byte. The original ring
 * silently overruns after 0x78 bytes; the port leaves the rest queued. */
static void core_serial_pump(void)
{
    if (!core_link) return;
    SerialLinkState state = serial_link_poll(core_link);
    if ((int)state != core_link_reported) {
        core_link_reported = (int)state;
        POP_LOG_INFO("LAN link %s: %s", serial_link_describe(core_link),
                     state == SERIAL_LINK_OPEN ? "connected" : state == SERIAL_LINK_CLOSED ? "closed" : "waiting for the other player");
    }
    for (;;) {
        int next = (serial_wr + 1) % SERIAL_RING_SIZE;
        uint8_t byte;
        if (next == serial_rd || !serial_link_read(core_link, &byte, 1)) break;
        serial_ring[serial_wr] = byte;
        serial_wr = (int16_t)next;
    }
}
/* _clear_serial asm:20051-20057: both indices to 0, dropping the ring. */
static void core_clear_serial(void) { serial_rd = serial_wr = 0; }
/* _check_serial asm:16984-17008: |read - write| >= n, no wrap correction. */
static int core_check_serial(int count)
{
    int distance = serial_rd - serial_wr;
    return (distance < 0 ? -distance : distance) >= count;
}
/* _write_serial asm:20141-20186: SERDAT per byte. Without a peer the
 * bytes are lost, as on an unplugged cable. */
static void core_write_serial(const void *data, int size)
{
    if (core_link && size > 0) serial_link_write(core_link, data, (size_t)size);
}
/* One pass of _read_serial asm:20080-20137: the cancel test comes first
 * (with cancel set), then the bytes that have arrived. 1 all read, 0 still
 * waiting, -1 aborted (cancel, or the line is gone and the ring empty). */
static int core_read_serial(uint8_t *buffer, int size, int *got, int cancel)
{
    core_serial_pump();
    if (cancel && core_serial_cancel) return -1;
    while (*got < size && core_serial_count()) {
        buffer[(*got)++] = serial_ring[serial_rd];
        serial_rd = (int16_t)((serial_rd + 1) % SERIAL_RING_SIZE);
    }
    if (*got == size) return 1;
    return core_link_dead() ? -1 : 0;
}

/* The 22 handshake bytes: the ring first, then only as many from the line,
 * so the _clear_serial after them cannot drop the other side's first turn
 * (it may restart and send faster than this side reads; port detail). */
static int core_read_exact(uint8_t *buffer, int size, int *got)
{
    if (core_serial_cancel) return -1;
    while (*got < size && core_serial_count()) {
        buffer[(*got)++] = serial_ring[serial_rd];
        serial_rd = (int16_t)((serial_rd + 1) % SERIAL_RING_SIZE);
    }
    if (core_link && *got < size) {
        serial_link_poll(core_link);
        *got += (int)serial_link_read(core_link, buffer + *got, (size_t)(size - *got));
    }
    if (*got == size) return 1;
    return core_link_dead() && !serial_link_available(core_link) ? -1 : 0;
}

/* _compress asm:21105-21123: cmd 4 bits, x and y 6 bits each. */
static uint16_t core_compress(const uint8_t *box)
{
    return (uint16_t)((box[0] & 15) << 12 | (box[1] & 63) << 6 | (box[2] & 63));
}
/* _decompress asm:21126-21145. */
static void core_decompress(uint8_t *box, uint16_t word)
{
    box[2] = (uint8_t)(word & 63);
    box[1] = (uint8_t)(word >> 6 & 63);
    box[0] = (uint8_t)(word >> 12 & 15);
}
/* The 5-byte full-checking packet: mailbox, low byte of _seed, sum. */
static void core_full_packet(uint8_t *box)
{
    box[3] = (uint8_t)seed;   /* LAB_52DD1, the low byte of _seed */
    box[4] = 0;
    for (int index = 0; index <= 3; ++index) box[4] = (uint8_t)(box[4] + box[index]);
}

/* ---- Requester helpers --------------------------------------------------- */

enum { CORE_AFTER_NONE, CORE_AFTER_CONSUME, CORE_AFTER_ABORT };
static const char *core_notice_queue[8];
static int core_notice_count, core_notice_cancel;
static struct { int pending_save, tail; int16_t new_map; } core_sync;
static void core_serial_continue(void);
static void core_serial_message(int text);
static void core_two_open(int resync, int origin);

static void core_notice(int text)   /* _do_message(text,0) asm:11098-11139, queued */
{
    if (core_notice_count < (int)(sizeof core_notice_queue / sizeof *core_notice_queue))
        core_notice_queue[core_notice_count++] = core_g_text[text];
}
static void core_notice_layout(int draw)
{
    core_buttons_install();
    if (core_notice_cancel) core_requester(0x20,0x3c,0x100,0x50,core_end_ok,core_end_cancel,draw);
    else core_requester(0x28,0x3c,0x100,0x30,core_end_ok,NULL,draw);
}
static void core_notice_show(void)
{
    core_req.kind = CORE_REQ_NOTICE; left_button = 2;
    core_notice_layout(0); tempo = 0;
}
static void core_notice_click(void)
{
    core_notice_layout(0);
    int result = core_check_cancel(core_end_ok, core_notice_cancel ? core_end_cancel : NULL);
    if (!result) return;
    left_button = 2; core_req.kind = CORE_REQ_NONE;
    if (core_notice_cancel) {
        /* LAB_4C04A asm:18906-18916: OK saves the game as well. */
        core_notice_cancel = 0;
        if (result == 1) { core_file_open(1); core_file_origin = CORE_FILE_FROM_SYNC; return; }
    } else {
        memmove(core_notice_queue, core_notice_queue + 1, sizeof core_notice_queue - sizeof *core_notice_queue);
        --core_notice_count;
    }
    core_serial_continue();
}
static void core_draw_notice(void)
{
    core_notice_layout(1);
    if (core_notice_cancel) amiga_text(game_pixels, 0x30, 0x4c, core_g_text[CORE_G_SAVED]);
    else amiga_text(game_pixels, 0x38, 0x48, core_notice_queue[0]);
}

/* ---- _serial_message asm:11185-11261 ---------------------------------- */

static int core_smsg_text;
static void core_smsg_layout(int draw) { core_buttons_install(); core_requester(0x28,0x3c,0x100,0x50,core_end_ok,NULL,draw); }
static void core_serial_message(int text)
{
    /* Port: a dropped connection says so instead of the call site's text. */
    if (core_link && core_link_dead()) text = CORE_G_GONE;
    POP_LOGW("LAN", "%s", core_g_text[text]);
    core_smsg_text = text;
    if (quick_io) {
        /* asm:11208-11224: ten words with command 15 for the other side.
         * The original sends 5 bytes from a 2-byte local; the 3 stack
         * bytes after it are zero here. */
        uint8_t *box = (uint8_t *)core_message, packet[5] = {0};
        box[0] = 15;
        core_put_be16(packet, core_compress(box));
        for (int repeat = 0; repeat < 10; ++repeat) core_write_serial(packet, 5);
    }
    core_req.kind = CORE_REQ_SMSG; left_button = 2;
    core_smsg_layout(0); tempo = 0;
}
static void core_smsg_click(void)
{
    core_smsg_layout(0);
    if (!core_check_cancel(core_end_ok, NULL)) return;
    core_clear_serial();
    left_button = 2;
    serial_off = 1;
    pause = !pause;   /* asm:11241-11247 toggles: an already paused game resumes */
    toggle = 0;
    core_clear_terrain_mailboxes();
    stats_build_mode(not_player) = 1;   /* the computer takes the other side */
    core_req.kind = CORE_REQ_NONE;
    /* Port: hang up, so the other side learns the line is gone. */
    core_serial_detach();
    core_serial_continue();
}
static void core_draw_smsg(void)
{
    core_smsg_layout(1);
    amiga_text(game_pixels, 0x38, 0x4c, core_g_text[core_smsg_text]);
    amiga_text(game_pixels, 0x38, 0x56, core_g_text[3]);
}

/* ---- _write_full / _read_full asm:18258-18372 --------------------------- */

typedef struct { int pc, got, r; uint8_t byte; } CoreSerialStep;
static CoreSerialStep core_full;

/* 0 sent and acknowledged ('|'), 1 aborted. 'b' asks for a resend
 * (answered by 'g'); anything else is dropped with _clear_serial. */
static int core_write_full(const uint8_t *packet)
{
    CoreSerialStep *s = &core_full;
    CORE_CORO_BEGIN(s);
resend:
    core_write_serial(packet, 5);
acknowledge:
    s->got = 0;
    CORE_CORO_WAIT(s, (s->r = core_read_serial(&s->byte, 1, &s->got, 1)) != 0);
    if (s->r < 0) CORE_CORO_RETURN(s, 1);
    if (s->byte == '|') CORE_CORO_RETURN(s, 0);
    core_clear_serial();
    if (s->byte != 'b') goto acknowledge;
    core_write_serial("g", 1);
    goto resend;
    CORE_CORO_END(s);
    return 2;
}
/* 0 packet read and acknowledged, 1 aborted. A bad sum clears the ring
 * and sends 'b' until the writer's 'g', then reads the packet again. */
static int core_read_full(uint8_t *packet)
{
    CoreSerialStep *s = &core_full;
    CORE_CORO_BEGIN(s);
again:
    s->got = 0;
    CORE_CORO_WAIT(s, (s->r = core_read_serial(packet, 5, &s->got, 1)) != 0);
    if (s->r < 0) CORE_CORO_RETURN(s, 1);
    if (packet[4] == (uint8_t)(packet[0] + packet[1] + packet[2] + packet[3])) {
        core_write_serial("|", 1);
        CORE_CORO_RETURN(s, 0);
    }
    POP_LOGW("LAN", "serial checksum error, asking for the packet again");
    core_clear_serial();
    s->byte = 0;
bad:
    core_write_serial("b", 1);
    if (core_serial_cancel || (core_link_dead() && !core_serial_count())) CORE_CORO_RETURN(s, 1);
    core_serial_pump();
    if (core_check_serial(1)) { s->got = 0; core_read_serial(&s->byte, 1, &s->got, 0); }
    if (s->byte == 'g') goto again;
    CORE_CORO_YIELD(s);
    goto bad;
    CORE_CORO_END(s);
    return 2;
}

/* ---- _get_message, the serial part asm:17710-17928 ---------------------- */

static struct { int pc, got, r; uint8_t own_seed[2], seed_in[2], word_out[2], word_in[2]; } core_turn;

/* 1 done (the mailboxes may be consumed), CORE_SERIAL_PENDING waiting. */
static int core_turn_exchange(void)
{
    __typeof__(core_turn) *s = &core_turn;
    uint8_t *own = stats_bytes(player), *other = stats_bytes(not_player);
    CORE_CORO_BEGIN(s);
    if (own[1] > 0x3f) own[1] = 0x3f;
    if (own[2] > 0x3f) own[2] = 0x3f;
    if (quick_io) {
        /* NO SERIAL CHECKING: the _compress word; every 16th turn _seed. */
        core_put_be16(s->word_out, core_compress(own));
        core_put_be16(s->own_seed, seed);
        core_put_be16(s->seed_in, seed);
        if (!player) {
            if (!(game_turn & 15)) {
                core_write_serial(s->own_seed, 2);
                s->got = 0;
                CORE_CORO_WAIT(s, (s->r = core_read_serial(s->seed_in, 2, &s->got, 1)) != 0);
                if (s->r < 0) { core_serial_message(CORE_G_ABORT); CORE_CORO_RETURN(s, 1); }
            }
            core_write_serial(s->word_out, 2);
            s->got = 0;
            CORE_CORO_WAIT(s, (s->r = core_read_serial(s->word_in, 2, &s->got, 1)) != 0);
            if (s->r < 0) { core_serial_message(CORE_G_ABORT); CORE_CORO_RETURN(s, 1); }
        } else {
            if (!(game_turn & 15)) {
                s->got = 0;
                CORE_CORO_WAIT(s, (s->r = core_read_serial(s->seed_in, 2, &s->got, 1)) != 0);
                if (s->r < 0) { core_serial_message(CORE_G_ABORT); CORE_CORO_RETURN(s, 1); }
                core_write_serial(s->own_seed, 2);
            }
            s->got = 0;
            CORE_CORO_WAIT(s, (s->r = core_read_serial(s->word_in, 2, &s->got, 1)) != 0);
            if (s->r < 0) { core_serial_message(CORE_G_ABORT); CORE_CORO_RETURN(s, 1); }
            core_write_serial(s->word_out, 2);
        }
        core_decompress(other, (uint16_t)core_get_be16(s->word_in));
        if (core_get_be16(s->seed_in) != seed) core_serial_message(CORE_G_LANDSCAPE);
    } else {
        /* FULL SERIAL CHECKING: 5 bytes, acknowledged; _player 0 sends first. */
        core_full_packet(own);
        if (!player) {
            CORE_CORO_WAIT(s, (s->r = core_write_full(own)) != CORE_SERIAL_PENDING);
            if (s->r) { core_serial_message(s->r + 12); CORE_CORO_RETURN(s, 1); }
            CORE_CORO_WAIT(s, (s->r = core_read_full(other)) != CORE_SERIAL_PENDING);
            if (s->r) { core_serial_message(s->r + 12); CORE_CORO_RETURN(s, 1); }
        } else {
            CORE_CORO_WAIT(s, (s->r = core_read_full(other)) != CORE_SERIAL_PENDING);
            if (s->r) { core_serial_message(s->r + 12); CORE_CORO_RETURN(s, 1); }
            CORE_CORO_WAIT(s, (s->r = core_write_full(own)) != CORE_SERIAL_PENDING);
            if (s->r) { core_serial_message(s->r + 12); CORE_CORO_RETURN(s, 1); }
        }
        if (own[3] != other[3]) core_serial_message(CORE_G_LANDSCAPE);
    }
    CORE_CORO_END(s);
    return 1;
}

/* ---- The two-player turn (game_tick while _serial_off is 0) ------------- */

static int core_serial_can_post(void) { return !core_turn_pending && core_consume_next < 0; }
static int core_serial_engaged(void) { return core_serial_live() || core_turn_pending || core_consume_next >= 0; }

/* _animate asm:676-760 with the _get_message at its end (asm:1011): pause
 * and _paint_map skip _move_peeps, but the exchange runs every pass. */
static void core_serial_tick(void)
{
    if (prg_quit || core_req.kind) return;
    if (core_consume_next >= 0) {
        /* A blocking _do_action has finished: the rest of the loop. */
        if (core_consume_abort) { core_clear_terrain_mailboxes(); core_consume_next = -1; core_consume_abort = 0; return; }
        if (!core_consume_from(core_consume_next) && core_turn_moved && game_turn == 0x1000) core_place_funny(start_seed & 3, 0);
        return;
    }
    if (!core_turn_pending) {
        if (core_result) return;
        core_turn_moved = 0;
        if (!pause && !paint_map) {
            game_turn = (int16_t)((uint16_t)game_turn + 1u);
            if (view_timer && !--view_timer) view_who = old_view_who;
            core_do_funny();
            _move_peeps();
            core_turn_moved = 1;
            if (!core_good_pop(player) || player == surrender_side) core_result = 2;
            else if (!core_good_pop(not_player) || not_player == surrender_side) core_result = 1;
            /* Both sides end on the same turn; neither sends its last mailbox. */
            if (core_result) { core_clear_terrain_mailboxes(); return; }
        } else if (view_timer && !--view_timer) view_who = old_view_who;
        core_turn_pending = 1; core_turn.pc = 0;
        core_serial_wait_since = SDL_GetTicks();
        core_serial_cancel = 0;
    }
    if (core_turn_exchange() == CORE_SERIAL_PENDING) return;
    core_turn_pending = 0;
    if (core_req.kind) { core_consume_next = 0; return; }   /* _serial_message first */
    if (!core_consume_from(0) && core_turn_moved && game_turn == 0x1000) core_place_funny(start_seed & 3, 0);
}

/* ---- The other side's packets for y=7/y=8 (no requester in the original) */

static struct { int pc, got, r, size, action; uint8_t packet[12]; } core_swait;
static void core_swait_open(int action, int size)
{
    core_swait.pc = core_swait.got = 0; core_swait.size = size; core_swait.action = action;
    core_req.kind = CORE_REQ_SWAIT;
    core_serial_wait_since = SDL_GetTicks(); core_serial_cancel = 0;
}
/* y=7 for the other side, asm:18669-18726. */
static void core_options_received(const uint8_t *packet)
{
    int side = packet[2] & 1;
    int16_t powers = core_get_be16(packet);
    if (stats_power_mask(side) != powers) core_notice(CORE_G_HIS + (side == player));
    stats_power_mask(side) = powers;
    stats_rating(side) = packet[3];
    stats_build_period(side) = packet[4];
}
static void core_y8_tail(void);
/* y=8 for the other side, asm:18756-18916. Bytes are zero-extended, so a
 * _surender of -1 arrives as 255 (which matches neither side). */
static void core_game_options_received(const uint8_t *packet)
{
    int16_t modes = core_get_be16(packet);
    if (game_mode != modes) core_notice(CORE_G_OPTIONS);
    game_mode = modes;
    if (!paint_map && packet[3]) core_notice(CORE_G_PAINT);
    if (packet[2] != serial_off) core_notice(CORE_G_GONE);
    else {
        if (packet[5] != stats_build_mode(0)) core_notice(packet[5] == 1 ? CORE_G_ASSIST_ON : CORE_G_ASSIST_OFF);
        if (packet[6] != stats_build_mode(1)) core_notice(packet[6] == 1 ? CORE_G_ASSIST_ON : CORE_G_ASSIST_OFF);
    }
    serial_off = packet[2];
    paint_map = packet[3];
    int sides = packet[4] != not_player;
    if (sides) core_notice(CORE_G_SIDES);
    not_player = packet[4];
    player = not_player ? 0 : 1;
    /* asm:18847-18852: only _mode 2 (plain terrain) refreshes _pointer. */
    if (sides && core_tool == 0 && !core_swamp_selected) core_set_pointer();
    stats_build_mode(0) = packet[5];
    stats_build_mode(1) = packet[6];
    options_new_map = packet[7];
    surrender_side = packet[8];
    serial_off = packet[10];
    bring_up_two = packet[11];
    core_sync.pending_save = packet[9];
    core_sync.tail = 1;
}
static void core_swait_step(void)
{
    __typeof__(core_swait) *s = &core_swait;
    s->r = core_read_serial(s->packet, s->size, &s->got, 1);
    if (!s->r) return;
    core_req.kind = CORE_REQ_NONE;
    if (s->r < 0) {
        /* asm:18677-18683, 18764-18769: _serial_message, _do_action returns 1. */
        core_consume_abort = 1;
        core_serial_message(CORE_G_ABORT);
        return;
    }
    if (s->action == 7) core_options_received(s->packet);
    else core_game_options_received(s->packet);
    core_serial_continue();
}

/* After a serial requester: queued notices, the save offer, the y=8 tail;
 * then the tick resumes the _get_message loop (core_consume_next). */
static void core_serial_continue(void)
{
    if (core_req.kind) return;
    if (core_notice_count) { core_notice_show(); return; }
    if (core_sync.pending_save) { core_sync.pending_save = 0; core_notice_cancel = 1; core_notice_show(); return; }
    if (core_sync.tail) { core_sync.tail = 0; core_y8_tail(); }
}
/* asm:18917-18940 on both sides: _new_map restarts (1 keeps _seed),
 * _bring_up_two reconnects with _two_players(1); both return 1. */
static void core_y8_tail(void)
{
    int16_t new_map = options_new_map;
    if (new_map) {
        core_consume_abort = 1;
        if (!(in_conquest == -1 ? core_custom_new_map(new_map) : game_reset_start(current_start)))
            POP_LOG_ERROR("Game options: map restart failed; keeping the current game.");
        return;
    }
    if (bring_up_two) { core_consume_abort = 1; core_two_open(1, CORE_TWO_FROM_SYNC); }
}

/* ---- _do_action y=2/6/7/8 with the link up ------------------------------ */

static int core_y7_side = -1, core_y8_local;
static void core_msg_out_open(void);
static void core_msg_in_open(void);
/* 0 go on, 1 stop the loop (clear_send), 2 suspended until the requester
 * or the other side's bytes are done (core_consume_next). */
static int core_serial_do_action(int owner, int x, int y)
{
    const uint8_t *other = stats_bytes(owner ? 0 : 1);
    if (y == 2) {
        /* asm:18404-18428: both sending in the same turn send nothing. */
        if (other[2] == 2 && other[0] == 14) return 0;
        if (owner == player) core_msg_out_open(); else core_msg_in_open();
        return 2;
    }
    if (y == 6) { pause = !pause; toggle = 0; return 0; }   /* asm:18591-18606 */
    if (y == 7) {
        if (other[2] == 7 && other[0] == 14) return 0;
        if (owner == player) {
            core_y7_side = x ? player : not_player;
            core_options_open(core_y7_side);
        } else core_swait_open(7, 5);
        return 2;
    }
    if (y == 8) {
        if (owner == player) {
            options_new_map = 0;
            core_game_options_open();
            core_y8_local = 1;
        } else core_swait_open(8, 12);
        return 2;
    }
    return 0;
}
/* _options returned (asm:18646-18666): tell the other side. */
static void core_serial_options_closed(void)
{
    if (core_y7_side < 0) return;
    int side = core_y7_side;
    core_y7_side = -1;
    if (serial_off) return;
    uint8_t *packet = (uint8_t *)core_message;
    core_put_be16(packet, stats_power_mask(side));
    packet[2] = (uint8_t)side;
    packet[3] = (uint8_t)stats_rating(side);
    packet[4] = (uint8_t)stats_build_period(side);
    core_write_serial(packet, 5);
}
/* _game_options returned (asm:18734-18753): if the link was up when it
 * opened, send the 12-byte state, then the shared tail. 1 = handled. */
static int core_serial_game_options_closed(void)
{
    if (!core_y8_local) return 0;
    core_y8_local = 0;
    if (!core_req.saved_serial) {
        uint8_t *packet = (uint8_t *)core_message;
        core_put_be16(packet, game_mode);
        packet[2] = (uint8_t)serial_off; packet[3] = (uint8_t)paint_map; packet[4] = (uint8_t)player;
        packet[5] = (uint8_t)stats_build_mode(0); packet[6] = (uint8_t)stats_build_mode(1);
        packet[7] = (uint8_t)options_new_map; packet[8] = (uint8_t)surrender_side;
        packet[9] = (uint8_t)saved_flag; packet[10] = (uint8_t)serial_off; packet[11] = (uint8_t)bring_up_two;
        core_write_serial(packet, 12);
    }
    core_y8_tail();
    return 1;
}

/* ---- The message requester: _get_a_message / y=2 receive ---------------- */

#define CORE_MESSAGE_BOXES 0x1c       /* _get_name(..., 0x1c) asm:11172 */
#define CORE_MESSAGE_KEEP  0x28       /* _message up to _bring_up_two (A4 0x3080) */
static struct { int length, blink, received, done; } core_msg;
static void core_msg_layout(int draw)
{
    core_buttons_install();
    core_requester(0x28,0x3c,0x100,0x50,core_end_ok,core_req.kind == CORE_REQ_MSG_OUT ? core_end_cancel : NULL,draw);
}
static void core_msg_out_open(void)
{
    core_message[0] = 0;
    core_msg.length = core_msg.blink = 0;
    core_req.kind = CORE_REQ_MSG_OUT; left_button = 2;
    core_msg_layout(0); tempo = 0;
}
/* asm:18436-18469: the text byte by byte, then 'e' (which no upper-case
 * entry can contain). */
static void core_msg_send(void)
{
    int length = (int)strlen(core_message);
    core_write_serial(core_message, length);
    core_write_serial("e", 1);
    core_req.kind = CORE_REQ_NONE; left_button = 2;
}
/* _get_name keys as core_file_key: 0x20..0x7a upper-cased, delete, return. */
static void core_msg_key(int key)
{
    if (core_req.kind == CORE_REQ_MSG_IN) {
        if (key == 13 && core_msg.done) { core_message[0] = 0; core_req.kind = CORE_REQ_NONE; left_button = 2; }
        return;
    }
    if (key >= 0x20 && key <= 0x7a) {
        if (core_msg.length >= CORE_MESSAGE_BOXES - 1) return;
        core_message[core_msg.length++] = (char)(key >= 'a' ? key - 0x20 : key);
        core_message[core_msg.length] = 0;
    } else if ((key == 8 || key == 0x7f) && core_msg.length) core_message[--core_msg.length] = 0;
    else if (key == 13) core_msg_send();
}
static void core_msg_in_open(void)
{
    core_message[0] = 0;
    core_msg.received = core_msg.done = 0;
    core_req.kind = CORE_REQ_MSG_IN; left_button = 2;
    core_msg_layout(0); tempo = 0;
    core_serial_wait_since = SDL_GetTicks(); core_serial_cancel = 0;
}
/* asm:18504-18548: one byte at a time until 'e' or 100 bytes. The port
 * keeps the first 39 so a long line cannot reach _bring_up_two. */
static void core_msg_in_step(void)
{
    while (!core_msg.done) {
        uint8_t byte; int got = 0;
        int result = core_read_serial(&byte, 1, &got, 1);
        if (!result) return;
        if (result < 0) {
            core_req.kind = CORE_REQ_NONE; core_consume_abort = 1;
            core_serial_message(CORE_G_ABORT);
            return;
        }
        if (byte == 'e') { core_msg.done = 1; break; }
        if (core_msg.received < CORE_MESSAGE_KEEP - 1) {
            core_message[core_msg.received] = (char)byte;
            core_message[core_msg.received + 1] = 0;
        }
        if (++core_msg.received == 0x64) core_msg.done = 1;
    }
    /* enter_4BC2A asm:18550-18573: nothing received closes at once. */
    if (!core_msg.received) { core_req.kind = CORE_REQ_NONE; left_button = 2; }
}
static void core_msg_click(void)
{
    core_msg_layout(0);
    int result = core_check_cancel(core_end_ok, core_req.kind == CORE_REQ_MSG_OUT ? core_end_cancel : NULL);
    if (!result) return;
    if (core_req.kind == CORE_REQ_MSG_IN) {
        if (!core_msg.done) return;
        core_message[0] = 0; core_req.kind = CORE_REQ_NONE; left_button = 2;
        return;
    }
    if (result == 2) { core_message[0] = 0; core_msg.length = 0; }   /* _get_name CANCEL empties */
    core_msg_send();
}
static void core_draw_msg(void)
{
    core_msg_layout(1);
    const char *title = core_g_text[core_req.kind == CORE_REQ_MSG_OUT ? 0 : not_player + 1];
    amiga_text(game_pixels, 0xa8 - (int)strlen(title) * 8 / 2, 0x4c, title);
    if (core_req.kind == CORE_REQ_MSG_IN) { amiga_text(game_pixels, 0x30, 0x64, core_message); return; }
    char row[CORE_MESSAGE_BOXES + 1];
    for (int k = 0; k < CORE_MESSAGE_BOXES; ++k) row[k] = k < core_msg.length ? core_message[k] : (char)0x82;
    row[CORE_MESSAGE_BOXES] = 0;
    if (!(++core_msg.blink / 10 & 1)) row[core_msg.length] = (char)0x83;
    amiga_text(game_pixels, 0x30, 0x64, row);
}

/* ---- _two_players asm:11265-11899 and _try_serial asm:11903-12102 ------- */

static struct { int resync, origin, side, this_land, message; } core_two;
static int core_cli_side = -1;
static uint8_t core_two_continue[46];
static uint8_t *core_two_button(void) { return core_two.resync ? core_two_continue : core_end_cancel; }
static void core_two_layout(int draw)
{
    core_buttons_install();
    memset(core_two_continue, 0, sizeof core_two_continue);
    strcpy(core_rec_text(core_two_continue), "CONTINUE");   /* LAB_46C80 */
    core_requester(0x20,0x2e,0x100,0x80,core_two_button(),NULL,draw);
}
static void core_two_open(int resync, int origin)
{
    core_two.resync = resync; core_two.origin = origin; core_two.message = 0;
    core_two.this_land = 1;   /* -44(A5) = 1, USE THIS LANDSCAPE */
    core_two.side = core_cli_side >= 0 ? core_cli_side : player;
    core_cli_side = -1;
    if (!baud_rate) baud_rate = 0x2580;   /* _baud_rate data value */
    core_menu_install(core_serial_entry(0), core_serial_template, 20);
    if (resync) core_clear_serial();
    core_req.kind = CORE_REQ_TWO; left_button = 2;
    core_two_layout(0); tempo = 0;
}
static void core_two_loaded(void) { core_two.this_land = 0; core_two.message = 0; }   /* -44 = 0: saved game */
static void core_two_back(void)   /* back from _save_load / _try_serial */
{
    core_req.kind = CORE_REQ_TWO; left_button = 2;
    core_two_layout(0);
}
static void core_two_close(int connected)
{
    core_req.kind = CORE_REQ_NONE; left_button = 2;
    if (core_two.origin == CORE_TWO_FROM_OPTIONS) {
        /* _game_options: failure redraws it (-4 = 1), success returns. */
        if (!connected) { core_req.kind = CORE_REQ_GAME; core_requester_layout(); return; }
        options_new_map = 0;
        core_game_options_close();
    }
}
/* LAB_46B3E asm:11779-11833. */
static void core_two_connected(void)
{
    player = (int16_t)core_two.side; not_player = player ? 0 : 1;
    if (!core_two.resync || core_two.this_land) { seed = start_seed; game_turn = 0; }
    serial_off = 0;
    pause = 0; toggle = 0;
    stats_build_mode(1) = 0; stats_build_mode(0) = 0;
    core_set_pointer();
    POP_LOG_INFO("LAN: connected, playing %s", player ? "EVIL" : "GOOD");
    int restart = core_two.this_land && !core_two.resync;
    core_two_close(1);
    /* _setup_display(0,-2): the agreed landscape from _start_seed. */
    if (restart && !core_custom_start(seed, -2)) POP_LOG_ERROR("LAN: the agreed landscape could not be built.");
}

static struct { int pc, got, r, count; uint8_t byte, out[22], in[22]; } core_try;
static void core_try_layout(int draw) { core_buttons_install(); core_requester(0x30,0x46,0xe4,0x40,core_end_cancel,NULL,draw); }
static void core_try_open(void)
{
    __typeof__(core_try) *s = &core_try;
    memset(s, 0, sizeof *s);
    int side = core_two.side;
    int16_t words[11] = {(int16_t)side, start_seed, game_mode, stats_rating(side), stats_power_mask(side),
                         stats_build_period(side), (int16_t)core_two.this_land, ground_in, paint_map, quick_io, land_checksum};
    for (int index = 0; index < 11; ++index) core_put_be16(s->out + index*2, words[index]);
    core_serial_cancel = 0;
    core_req.kind = CORE_REQ_TRY; left_button = 2;
    core_try_layout(0); tempo = 0;
}
/* asm:12038-12085: 2 same side, 3 saved/this landscape differ, 5 checking
 * differs, 6 saved games differ; else EVIL's start, mode, terrain, paint and
 * checking win, and each side takes the other's computer settings. */
static int core_try_evaluate(void)
{
    const uint8_t *in = core_try.in, *out = core_try.out;
    int16_t side = core_get_be16(in), land = core_get_be16(in + 12);
    if (side == core_get_be16(out)) return 2;
    if (land != core_get_be16(out + 12)) return 3;
    if (core_get_be16(in + 18) != core_get_be16(out + 18)) return 5;
    if (core_get_be16(in + 20) != core_get_be16(out + 20) && !core_two.this_land) return 6;
    if (side == 1) {
        start_seed = core_get_be16(in + 2); game_mode = core_get_be16(in + 4);
        stats_rating(1) = core_get_be16(in + 6); stats_power_mask(1) = core_get_be16(in + 8);
        stats_build_period(1) = core_get_be16(in + 10);
        ground_in = core_get_be16(in + 14); paint_map = core_get_be16(in + 16); quick_io = core_get_be16(in + 18);
        return 1;
    }
    if (side) return 99;
    stats_rating(0) = core_get_be16(in + 6); stats_power_mask(0) = core_get_be16(in + 8);
    stats_build_period(0) = core_get_be16(in + 10);
    return 1;
}
/* The TRY NUMBER loop, one pass per frame (the original spins): 'r' until
 * the other side answers; 'r' -> 'z' and wait for its 'z'; then 22 bytes
 * each way. 99 on CANCEL. */
static int core_try_step(void)
{
    __typeof__(core_try) *s = &core_try;
    CORE_CORO_BEGIN(s);
attempt:
    if (++s->count > 30000) s->count = 1;
    if (core_serial_cancel) goto cancelled;
    /* Poll first: an 'r' sent before the connection is up would be lost,
     * and the other side must see one 'r' before this side's 'z'. */
    core_serial_pump();
    core_write_serial("r", 1);
    if (core_check_serial(1)) {
        s->got = 0;
        CORE_CORO_WAIT(s, (s->r = core_read_serial(&s->byte, 1, &s->got, 1)) != 0);
        if (s->r < 0) goto cancelled;
        if (s->byte == 'r') {
            core_write_serial("z", 1);
            do {
                s->got = 0;
                CORE_CORO_WAIT(s, (s->r = core_read_serial(&s->byte, 1, &s->got, 1)) != 0);
                if (s->r < 0) goto cancelled;
            } while (s->byte != 'z');
            goto exchange;
        }
        if (s->byte == 'z') goto exchange;
    }
    CORE_CORO_YIELD(s);
    goto attempt;
exchange:
    core_write_serial(s->out, 22);
    s->got = 0;
    CORE_CORO_WAIT(s, (s->r = core_read_exact(s->in, 22, &s->got)) != 0);
    if (s->r < 0) goto cancelled;
    core_clear_serial();
    CORE_CORO_RETURN(s, core_try_evaluate());
cancelled:
    core_clear_serial();
    left_button = 2;
    CORE_CORO_RETURN(s, 99);
    CORE_CORO_END(s);
    return 99;
}
static void core_try_click(void)
{
    core_try_layout(0);
    if (core_check_cancel(core_end_cancel, NULL)) core_serial_cancel = 1;
}
/* The click loop LAB_46894..LAB_46C76: rows 1..7 by their own entry. */
static void core_two_click(void)
{
    core_two_layout(0);
    int row = 1;
    for (; row < 8; ++row) {
        const uint8_t *entry = core_serial_entry(row);
        int x = core_rec_w(entry,0) + 0x20, y = core_rec_w(entry,2) + 0x2e;
        if (mousex >= x && mousex <= x + core_rec_w(entry,4) && mousey >= y && mousey <= y + 8) break;
    }
    switch (row) {
    case 1: if (use_modem) POP_LOG_INFO("LAN: SEND LOGIN has no modem on a network link"); break;
    case 2: POP_LOG_INFO("LAN: BAUD RATE does not apply to a network link"); break;
    case 3: use_modem = !use_modem; core_two.message = 0; break;
    case 4: if (use_modem) quick_io = !quick_io; break;
    case 5: core_two.side = !core_two.side; break;
    case 6:
        if (core_two.this_land) { core_file_open(0); core_file_origin = CORE_FILE_FROM_TWO; return; }
        core_two.this_land = 1; core_two.message = 0;
        break;
    case 7: core_try_open(); return;
    default: break;
    }
    left_button = 2;
    if (core_check_cancel(core_two_button(), NULL) == 1) {
        if (core_two.resync) { core_try_open(); return; }   /* CONTINUE connects */
        core_two_close(0);
    }
}
static void core_draw_two(void)
{
    core_two_layout(1);
    for (int row = 0; row < 8; ++row) {
        if (!use_modem && (row == 1 || row == 4)) continue;
        int index = row == 3 ? use_modem*5 + 3 : row == 4 ? quick_io*5 + 4 : row == 5 ? core_two.side*5 + 5 :
                    row == 6 ? !core_two.this_land*5 + 6 : row;
        const uint8_t *entry = core_serial_entry(index);
        amiga_text(game_pixels, core_rec_w(entry,0) + 0x20, core_rec_w(entry,2) + 0x2e, core_rec_text((uint8_t *)entry));
    }
    char value[24];
    int column = core_rec_w(core_serial_entry(1),0) + 0x78;
    snprintf(value, sizeof value, "%d", (int)(uint16_t)baud_rate);
    amiga_text(game_pixels, column, core_rec_w(core_serial_entry(2),2) + 0x2e, value);
    if (use_modem) {
        snprintf(value, sizeof value, "%.17s", core_login);
        amiga_text(game_pixels, column, core_rec_w(core_serial_entry(1),2) + 0x2e, value);
    }
    int text = core_two.message == 2 ? core_two.side + 12 : core_two.message == 3 ? core_two.this_land + 14 :
               core_two.message == 4 ? 16 : core_two.message == 5 ? quick_io + 17 : core_two.message == 6 ? 19 : -1;
    if (text >= 0) {
        const uint8_t *entry = core_serial_entry(text);
        amiga_text(game_pixels, core_rec_w(entry,0) + 0x20, core_rec_w(entry,2) + 0x2e, core_rec_text((uint8_t *)entry));
        if (text == 19) { snprintf(value, sizeof value, "%d", (int)(uint16_t)land_checksum); amiga_text(game_pixels, 0, 0, value); }
    }
}
static void core_draw_try(void)
{
    core_try_layout(1);
    char number[12];
    snprintf(number, sizeof number, "%d", core_try.count);
    amiga_text(game_pixels, 0x40, 0x58, "TRY NUMBER ");
    amiga_text(game_pixels, 0x98, 0x58, number);
}

/* ---- Frame hooks ------------------------------------------------------- */

/* Every frame: move received bytes into the ring and advance whatever
 * waits on the other side outside the tick. */
static void core_serial_frame(void)
{
    if (!core_link) return;
    if (core_req.kind != CORE_REQ_TRY) core_serial_pump();   /* the try step reads for itself */
    if (core_req.kind == CORE_REQ_TRY) {
        int result = core_try_step();
        if (result == CORE_SERIAL_PENDING) return;
        if (result == 1) { core_two_connected(); return; }
        core_two.message = result;   /* -40(A5): the redraw shows why */
        core_two_back();
    } else if (core_req.kind == CORE_REQ_MSG_IN) core_msg_in_step();
    else if (core_req.kind == CORE_REQ_SWAIT) core_swait_step();
}
/* Shift+Esc aborts a wait as in _read_serial (key $1b with _shift). */
static int core_serial_blocked(void)
{
    return core_turn_pending || core_req.kind == CORE_REQ_SWAIT || (core_req.kind == CORE_REQ_MSG_IN && !core_msg.done);
}
/* Port: after a second without the other side, say what the game waits for. */
static uint8_t core_wait_button[46];
static void core_serial_draw_wait(void)
{
    if (!core_serial_blocked() || core_req.kind == CORE_REQ_MSG_IN || core_req.kind == CORE_REQ_SMSG) return;
    if (core_req.kind && core_req.kind != CORE_REQ_SWAIT) return;
    if (SDL_GetTicks() - core_serial_wait_since < 1000) return;
    memset(core_wait_button, 0, sizeof core_wait_button);
    strcpy(core_rec_text(core_wait_button), "SHIFT ESC ABORTS");
    core_requester(0x30,0x46,0xe0,0x40,core_wait_button,NULL,1);
    amiga_text(game_pixels, 0x40, 0x58, "WAITING FOR THE OTHER\nPLAYER");
}
static void core_serial_draw(void)
{
    if (core_req.kind == CORE_REQ_TWO) core_draw_two();
    else if (core_req.kind == CORE_REQ_TRY) { core_draw_two(); core_draw_try(); }
    else if (core_req.kind == CORE_REQ_SMSG) core_draw_smsg();
    else if (core_req.kind == CORE_REQ_MSG_OUT || core_req.kind == CORE_REQ_MSG_IN) core_draw_msg();
    else if (core_req.kind == CORE_REQ_NOTICE) core_draw_notice();
    core_serial_draw_wait();
}
static void core_serial_click(void)
{
    if (core_req.kind == CORE_REQ_TWO) core_two_click();
    else if (core_req.kind == CORE_REQ_TRY) core_try_click();
    else if (core_req.kind == CORE_REQ_SMSG) core_smsg_click();
    else if (core_req.kind == CORE_REQ_MSG_OUT || core_req.kind == CORE_REQ_MSG_IN) core_msg_click();
    else if (core_req.kind == CORE_REQ_NOTICE) core_notice_click();
}

/* ---- Input while the link is up ---------------------------------------- */

/* Post into the own mailbox like the original input paths; while this
 * turn's exchange is outstanding the original reads no input at all. */
static int core_serial_post(int command, int x, int y)
{
    if (!core_serial_can_post() || core_result || player < 0 || player > 1) return 0;
    uint8_t *box = stats_bytes(player);
    box[0] = (uint8_t)command; box[1] = (uint8_t)x; box[2] = (uint8_t)y;
    return 1;
}
/* Port shortcuts that act directly in single play go through the mailbox
 * with the link up (the toolbar codes of _zoom_map asm:1700-2451).
 * Returns 1 when the key was taken. */
static int core_serial_shortcut(SDL_Keycode key)
{
    if (key == SDLK_SPACE) core_serial_post(14, 0, 6);
    else if (key >= SDLK_0 && key <= SDLK_3) core_serial_post(14, (int)(key - SDLK_0), 1);
    else if (key == SDLK_q) core_serial_post(3, xoff, yoff);
    else if (key == SDLK_v) core_serial_post(6, xoff, yoff);
    else if (key == SDLK_f) core_serial_post(14, 0, 4);
    else if (key == SDLK_k) core_serial_post(14, 0, 5);
    else if (key == SDLK_a) core_serial_post(14, 0, 3);
    else if (key == SDLK_r) POP_LOG_INFO("LAN: restart through GAME OPTIONS so both sides restart");
    else return 0;
    return 1;
}
/* A map click: _sculpt raise/lower (1/2), swamp (4), papal magnet (5),
 * the same post as single play into the link-guarded mailbox. */
static void core_serial_world_click(int column, int row, int left)
{
    core_sculpt_post(column, row, left, core_serial_post);
}

/* ---- Command line: --host [port] / --join <ip>[:port] ------------------- */

static int core_serial_parse(int argc, char **argv, int *matched)
{
    *matched = 0;
    if (argc < 2 || !argv || !argv[1]) return 1;
    char error[256] = "";
    SerialLink *link = NULL;
    if (!strcmp(argv[1], "--host") && argc <= 3) {
        *matched = 1;
        unsigned long port = SERIAL_LINK_DEFAULT_PORT;
        if (argc == 3) {
            char *end; errno = 0;
            port = strtoul(argv[2], &end, 10);
            if (errno || *end || !argv[2][0] || !port || port > 65535) return 0;
        }
        link = serial_link_tcp_host((uint16_t)port, error, sizeof error);
        core_cli_side = 0;
    } else if (!strcmp(argv[1], "--join") && argc == 3) {
        *matched = 1;
        char host[256]; uint16_t port;
        if (!serial_link_parse_endpoint(argv[2], host, sizeof host, &port)) return 0;
        link = serial_link_tcp_join(host, port, error, sizeof error);
        core_cli_side = 1;
    } else return 1;
    if (!link) { POP_LOG_ERROR("LAN: %s", error); return 0; }
    core_serial_attach(link);
    POP_LOG_INFO("LAN: %s; choose GOOD/EVIL and CONNECT ME on both machines", serial_link_describe(link));
    return 1;
}
