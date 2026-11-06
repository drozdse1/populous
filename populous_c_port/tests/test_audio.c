/*
 * test_audio.c - Audio subsystem tests: sample loading, mixer and title music.
 *
 * Run via `make test`. Each test_* function is self-contained and
 * aborts through assert() on the first mismatch.
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "../audio.h"
#include "../title_music.h"

#ifndef POPULOUS_DEFAULT_ASSET_DIR
#define POPULOUS_DEFAULT_ASSET_DIR "../binaries/populous-extracted"
#endif

static int16_t out[2 * 8192];
static int8_t samples[64];

static AudioWrite write_of(const int8_t *data, uint32_t length, uint16_t period, uint16_t volume,
                           uint16_t cycles)
{
    AudioWrite w = {data, length, period, volume, cycles};
    return w;
}
/* Frames until channel stops, at most limit. */
static int frames_until_idle(int channel, int rate, int limit)
{
    int frames = 0;
    while (audio_busy(channel) && frames < limit) {
        audio_mix(out, 1, rate);
        ++frames;
    }
    return frames;
}

/* Step. */
static void test_step(void)
{
    /* Paula: PAL clock / period bytes per second. */
    assert(audio_step(357, 44100) == (uint32_t)((3546895ull << 16) / (357ull * 44100)));
    assert(audio_step(357, 44100) == 14764);
    assert(audio_step(100, 44100) == audio_step(AUDIO_MIN_PERIOD, 44100));
    assert(audio_step(357, 0) == 0);
}

/* Volume and panning. */
static void test_volume_and_panning(void)
{
    audio_reset();
    memset(samples, 64, sizeof samples);
    for (int channel = 0; channel < 4; ++channel) {
        for (int volume = 0; volume <= 80; volume += 16) {
            audio_reset();
            AudioWrite w = write_of(samples, 64, 400, (uint16_t)volume, 0);
            audio_write(channel, &w);
            audio_mix(out, 1, 44100);
            int expect = 64 * (volume > 64 ? 64 : volume) * 2;
            int right = channel == 1 || channel == 2;
            assert(out[right] == expect && out[!right] == 0);
        }
    }
    /* Left 0+3, right 1+2 sum; full-scale pairs saturate at S16. */
    audio_reset();
    memset(samples, 127, sizeof samples);
    for (int channel = 0; channel < 4; ++channel) {
        AudioWrite w = write_of(samples, 64, 400, 64, 0);
        audio_write(channel, &w);
    }
    audio_mix(out, 1, 44100);
    assert(out[0] == 32512 && out[1] == 32512);
    memset(samples, -128, sizeof samples);
    audio_mix(out, 1, 44100);
    assert(out[0] == -32768 && out[1] == -32768);
    audio_reset();
    audio_mix(out, 4, 44100);
    for (int k = 0; k < 8; ++k)
        assert(out[k] == 0);
}

/* One shot and loops. */
static void test_one_shot_and_loops(void)
{
    const int rate = 44100;
    uint32_t step = audio_step(400, rate);
    int once = (int)((40ull << 16) / step) + 1; /* frames for 40 bytes */
    memset(samples, 10, sizeof samples);
    audio_reset();
    AudioWrite w = write_of(samples, 40, 400, 64, 1);
    audio_write(0, &w);
    assert(audio_busy(0));
    int frames = frames_until_idle(0, rate, 100000);
    assert(frames >= once - 1 && frames <= once + 1);
    /* Three cycles take three times as long. */
    w.cycles = 3;
    audio_write(0, &w);
    frames = frames_until_idle(0, rate, 100000);
    assert(frames >= 3 * once - 2 && frames <= 3 * once + 2);
    /* Cycles 0 loops until aborted. */
    w.cycles = 0;
    audio_write(0, &w);
    assert(frames_until_idle(0, rate, 20 * once) == 20 * once && audio_busy(0));
    audio_abort(0);
    assert(!audio_busy(0));
    audio_mix(out, 1, rate);
    assert(out[0] == 0);
    /* _PlaySound: one-shot request, then the queued repeat request. */
    int8_t first[8], second[8];
    memset(first, 20, sizeof first);
    memset(second, -20, sizeof second);
    AudioWrite a = write_of(first, 8, 400, 64, 1), b = write_of(second, 8, 400, 64, 0);
    audio_write(3, &a);
    audio_write(3, &b);
    audio_mix(out, 1, rate);
    assert(out[0] == 20 * 64 * 2);
    for (int k = 0; k < 4 * once; ++k)
        audio_mix(out, 1, rate);
    assert(out[0] == -20 * 64 * 2 && audio_busy(3));
    audio_abort(3);
    /* A zero-length request completes at once; no data plays silent. */
    AudioWrite empty = write_of(samples, 0, 400, 64, 1);
    audio_write(1, &empty);
    audio_mix(out, 1, rate);
    assert(!audio_busy(1) && out[1] == 0);
    audio_write(-1, &w);
    audio_write(4, &w);
    audio_abort(9);
    assert(!audio_busy(-1) && !audio_busy(4));
}

/* Minimal _load_sound file: one measure, one sequence entry, two kits. */
static size_t build_bank(uint8_t *file, uint16_t seq_measure, uint32_t measure_length,
                         int kit_sample)
{
    size_t at = 0;
#define W16(v)                                                                                     \
    do {                                                                                           \
        file[at] = (uint8_t)((v) >> 8);                                                            \
        file[at + 1] = (uint8_t)(v);                                                               \
        at += 2;                                                                                   \
    } while (0)
#define W32(v)                                                                                     \
    do {                                                                                           \
        W16((uint32_t)(v) >> 16);                                                                  \
        W16((v) & 0xffff);                                                                         \
    } while (0)
    for (int k = 0; k < 64; ++k)
        W16(k == 0 ? 48 : 64);
    W16(1);
    W16(seq_measure);
    W16(1);
    W32(measure_length);
    memset(file + at, ' ', measure_length);
    file[at] = 'B';
    at += measure_length;
    W16(2);
    W16(400);
    W16(6);
    file[at++] = 64;
    file[at++] = 0;
    W16(0);
    W16(300);
    W16(4);
    file[at++] = 20;
    file[at++] = (uint8_t)kit_sample;
    W16(2);
    W16(2);
    W16(6);
    W16(6);
    W32(0xdeadbeef);
    W16(4);
    W16(0);
    W32(0);
    W32(10);
    for (int k = 0; k < 10; ++k)
        file[at++] = (uint8_t)(k - 5);
    W16(0x1b39);
#undef W16
#undef W32
    return at;
}

/* Bank. */
static void test_bank(void)
{
    static uint8_t file[0x3000];
    AudioBank bank;
    char error[128];
    size_t size = build_bank(file, 0, 0x1000, 1);
    assert(audio_bank_parse(&bank, file, size, error, sizeof error));
    assert(bank.seq_count == 1 && bank.sequn[0][0] == 0 && bank.sequn[0][1] == 1 &&
           bank.measln[0] == 48);
    assert(bank.measures[0] == 'B' && bank.kit_count == 2 && bank.no_sams == 2);
    assert(bank.kit[1].period == 300 && bank.kit[1].length == 4 && bank.kit[1].volume == 20);
    assert(bank.kit[1].sample == 1 && bank.kit[1].loop == 2);
    /* Pointers rebuilt from lengths, not taken from the file. */
    assert(bank.sams[0].offset == 0 && bank.sams[1].offset == 6 && bank.sample_length == 10);
    assert(bank.samples[0] == -5 && bank.samples[9] == 4);
    audio_bank_free(&bank);
    assert(!bank.kit && !bank.samples);
    /* Longer measure block: 0x1000 read, the rest skipped (asm:20760-20767). */
    size = build_bank(file, 0, 0x1800, 1);
    assert(audio_bank_parse(&bank, file, size, error, sizeof error));
    audio_bank_free(&bank);
    /* Rejections: every byte accounted for, every index inside its buffer. */
    size = build_bank(file, 0, 0x1000, 1);
    assert(!audio_bank_parse(&bank, file, size - 1, error, sizeof error));
    assert(!audio_bank_parse(&bank, file, size + 1, error, sizeof error) && strstr(error, "size"));
    assert(!audio_bank_parse(&bank, file, 10, error, sizeof error));
    size = build_bank(file, 16, 0x1000, 1);
    assert(!audio_bank_parse(&bank, file, size, error, sizeof error) && strstr(error, "measure"));
    size = build_bank(file, 0, 0x800, 1);
    assert(!audio_bank_parse(&bank, file, size, error, sizeof error));
    size = build_bank(file, 0, 0x1000, 2);
    assert(!audio_bank_parse(&bank, file, size, error, sizeof error) && strstr(error, "sample"));
    /* A repeat request past its sample: kit 1 is 4 bytes, sams[1].loop is 0. */
    size = build_bank(file, 0, 0x1000, 1);
    file[size - 2 - 10 - 4 - 16 - 2 - 8 + 6] = 0;
    file[size - 2 - 10 - 4 - 16 - 2 - 8 + 7] = 5;
    assert(!audio_bank_parse(&bank, file, size, error, sizeof error) && strstr(error, "repeat"));
}

static uint8_t *read_raw(const char *name, size_t *size)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/raw/%s", POPULOUS_DEFAULT_ASSET_DIR, name);
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    static uint8_t data[2][0x40000];
    static int which;
    uint8_t *buffer = data[which++ & 1];
    *size = fread(buffer, 1, sizeof data[0], file);
    fclose(file);
    return buffer;
}

/* The extracted original banks, when present (raw/ is tracked). */
static void test_original_banks(void)
{
    size_t size;
    uint8_t *data = read_raw("gmusic1", &size);
    if (!data) {
        puts("original gmusic1 absent: skipped");
        return;
    }
    AudioBank bank;
    char error[128];
    assert(size == 90534 && audio_bank_parse(&bank, data, size, error, sizeof error));
    assert(bank.kit_count == 104 && bank.no_sams == 12 && bank.sample_length == 73046);
    assert(bank.seq_count == 8 && bank.measure_length == 0x4000 && bank.measln[10] == 48);
    /* Effect ids used by _effect and the heartbeat (asm:19637-19671). */
    static const uint16_t kit[][5] = {
        {0x42, 654, 4682, 64, 3},   {0x43, 701, 1500, 51, 4},  {0x44, 715, 1500, 64, 4},
        {0x45, 714, 4192, 64, 5},   {0x46, 480, 12976, 31, 6}, {0x47, 357, 1334, 30, 7},
        {0x48, 325, 1334, 41, 7},   {0x49, 3367, 2528, 64, 8}, {0x4a, 759, 12470, 64, 9},
        {0x4b, 1704, 5928, 54, 10}, {0x4c, 357, 6890, 51, 11}, {0x4d, 952, 8456, 20, 1}};
    for (size_t k = 0; k < sizeof kit / sizeof kit[0]; ++k) {
        const AudioKit *e = &bank.kit[kit[k][0]];
        assert(e->period == kit[k][1] && e->length == kit[k][2] && e->volume == kit[k][3] &&
               e->sample == kit[k][4]);
        assert(e->loop == (kit[k][0] == 0x4c ? 5 : 0));
    }
    audio_bank_free(&bank);
    data = read_raw("gwords", &size);
    if (!data) {
        puts("original gwords absent: skipped");
        return;
    }
    assert(size == 79076 && audio_bank_parse(&bank, data, size, error, sizeof error));
    assert(bank.kit_count == 104 && bank.no_sams == 9 && bank.seq_count == 0 &&
           bank.sample_length == 61644);
    audio_bank_free(&bank);
}

/* Paula DMA as the title replayer drives it: the started pair plays
 * once, then the latched pair repeats; PER changes reach both. */
static void test_paula(void)
{
    audio_reset();
    for (int i = 0; i < 8; ++i)
        samples[i] = (int8_t)(10 + i);
    int rate = (int)(AUDIO_PAL_CLOCK / 128); /* one source byte per frame */
    audio_paula_start(1, samples, 4, 128, 64);
    audio_paula_latch(1, samples + 6, 2);
    audio_paula_period(1, 128);
    AudioWrite w;
    assert(audio_current(1, &w) == 2 && w.data == samples && w.period == 128);
    int16_t expect[] = {10, 11, 12, 13, 16, 17, 16, 17, 16};
    for (size_t i = 0; i < sizeof expect / sizeof expect[0]; ++i) {
        audio_mix(out, 1, rate);
        assert(out[0] == 0 && out[1] == expect[i] * 64 * 2);
    }
    assert(audio_current(1, &w) == 1 && w.data == samples + 6 && w.cycles == 0);
    audio_paula_latch(1, samples, 1); /* re-latch while repeating */
    audio_mix(out, 2, rate);
    assert(audio_current(1, &w) == 1 && w.data == samples);
    audio_paula_latch(2, samples, 1); /* DMA off: nothing to latch */
    assert(!audio_busy(2));
    audio_reset();
}

static size_t build_popsam(uint8_t *file, uint16_t rate)
{
    size_t at = 0;
    for (int index = 0; index < TITLE_SAMPLES; ++index) {
        uint32_t length = 64;
        file[at] = 0;
        file[at + 1] = 0;
        file[at + 2] = 0;
        file[at + 3] = (uint8_t)length;
        file[at + 4] = (uint8_t)(rate >> 8);
        file[at + 5] = (uint8_t)rate;
        at += 6;
        for (uint32_t k = 0; k < length; ++k)
            file[at++] = (uint8_t)(index * 16 + k);
    }
    return at;
}

/* popsam: 8 x (BE32 length, BE16 rate, PCM), SUB_2F9C asm:2650-2662. */
static void test_title_bank(void)
{
    static uint8_t file[TITLE_POPSAM_SIZE + 8];
    TitleBank bank;
    char error[128];
    size_t size = build_popsam(file, 8000);
    assert(title_bank_parse(&bank, file, size, error, sizeof error));
    assert(bank.samples[0].offset == 6 && bank.samples[1].offset == 76 &&
           bank.samples[7].length == 64);
    assert(bank.samples[3].rate == 8000 && bank.pcm[bank.samples[1].offset] == 16);
    title_bank_free(&bank);
    assert(!title_bank_parse(&bank, file, size - 1, error, sizeof error) &&
           !strcmp(error, "truncated popsam"));
    assert(!title_bank_parse(&bank, file, size + 1, error, sizeof error) &&
           !strcmp(error, "unexpected popsam size"));
    assert(!title_bank_parse(&bank, file, TITLE_POPSAM_SIZE + 1, error, sizeof error));
    size = build_popsam(file, 54); /* 0x369e99/54 overflows DIVU */
    assert(!title_bank_parse(&bank, file, size, error, sizeof error));
    size = build_popsam(file, 0);
    assert(!title_bank_parse(&bank, file, size, error, sizeof error));
    assert(!title_music_start(NULL, 0) && !title_music_playing());
}

/* The extracted popsam with the transcribed score (asm:2899-3089). */
static void test_title_music(void)
{
    size_t size;
    uint8_t *data = read_raw("popsam", &size);
    if (!data) {
        puts("original popsam absent: skipped");
        return;
    }
    TitleBank bank;
    char error[128];
    assert(size == TITLE_POPSAM_SIZE && title_bank_parse(&bank, data, size, error, sizeof error));
    static const uint32_t lengths[] = {21004, 6032, 8244, 35662, 15340, 8008, 1754, 18626};
    static const uint16_t rates[] = {10000, 10000, 8000, 8000, 4000, 8000, 5300, 8000};
    for (int i = 0; i < TITLE_SAMPLES; ++i)
        assert(bank.samples[i].length == lengths[i] && bank.samples[i].rate == rates[i]);
    audio_reset();
    assert(title_music_start(&bank, 0) && title_music_playing() && title_music_tempo() == 9);
    /* LAB_3532 note steps; period = step * (0x369e99 / rate) >> 10 (asm:2806-2814). */
    static const uint16_t steps[] = {
        0x1000, 0x0f18, 0x0e40, 0x0d74, 0x0cb4, 0x0bfc, 0x0b50, 0x0aac, 0x0a14, 0x0984,
        0x08fc, 0x0878, 0x0800, 0x078c, 0x0720, 0x06ba, 0x065a, 0x05fe, 0x05a8, 0x0556,
        0x050a, 0x04c2, 0x047e, 0x043c, 0x0400, 0x03c6, 0x0390, 0x035d, 0x032d, 0x02ff,
        0x02d4, 0x02ab, 0x0285, 0x0261, 0x023f, 0x021e, 0x0200, 0x01e3, 0x01c8, 0x01ae,
        0x0196, 0x017f, 0x016a, 0x0155, 0x0142, 0x0130, 0x011f, 0x010f, 0x0100};
    int notes = 0, rests = 0, sounding = 0;
    long energy = 0;
    TitleVoice before[TITLE_VOICES];
    for (int v = 0; v < TITLE_VOICES; ++v)
        assert(title_music_voice(v, &before[v]) && before[v].counter == 1);
    for (int frame = 0; frame < 3000; ++frame) {
        title_music_vbi();
        for (int v = 0; v < TITLE_VOICES; ++v) {
            TitleVoice now;
            assert(title_music_voice(v, &now));
            /* An event reloads the counter with duration * tempo. */
            int reloaded = now.counter > before[v].counter;
            if (reloaded) assert(now.counter % 9 == 0 && now.event);
            if (reloaded && now.event == 2) ++rests;
            if (reloaded && now.event == 1 && now.instrument >= 0 &&
                now.instrument < TITLE_SAMPLES) {
                uint32_t base = 0x369e99u / rates[now.instrument];
                assert(now.period == (uint16_t)((steps[now.note] * base) >> 10));
                ++notes;
            }
            before[v] = now;
            if (audio_busy(v)) ++sounding;
        }
        audio_mix(out, 882, 44100);
        for (int k = 0; k < 2 * 882; ++k)
            energy += abs(out[k]);
    }
    assert(notes > 100 && notes + rests < 3000 * TITLE_VOICES && sounding > 3000 && energy > 0);
    title_music_stop();
    assert(!title_music_playing());
    for (int v = 0; v < TITLE_VOICES; ++v)
        assert(!audio_busy(v));
    title_bank_free(&bank);
}

/* No device must never stop the game: the mixer keeps its state. */
static void test_device(void)
{
    setenv("SDL_AUDIODRIVER", "populous-invalid-driver", 1);
    assert(!audio_open() && !audio_device_open());
    memset(samples, 1, sizeof samples);
    AudioWrite w = write_of(samples, 64, 400, 64, 0);
    audio_write(0, &w);
    assert(audio_busy(0));
    audio_close();
    assert(!audio_busy(0));
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    int opened = audio_open();
    assert(opened == audio_device_open());
    audio_write(2, &w);
    assert(audio_busy(2));
    audio_close();
    audio_close();
    assert(!audio_device_open() && !audio_busy(2));
    printf("dummy audio device: %s\n", opened ? "opened" : "unavailable (silent)");
}

/* ========================================================================
 * Test runner (focused --flags first, then the full suite)
 * ======================================================================== */
int main(void)
{
    test_step();
    test_volume_and_panning();
    test_one_shot_and_loops();
    test_bank();
    test_original_banks();
    test_paula();
    test_title_bank();
    test_title_music();
    test_device();
    puts("audio tests: PASS");
    return 0;
}
