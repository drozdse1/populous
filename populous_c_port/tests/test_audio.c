#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "../audio.h"

#ifndef POPULOUS_DEFAULT_ASSET_DIR
#define POPULOUS_DEFAULT_ASSET_DIR "../binaries/populous-extracted"
#endif

static int16_t out[2*8192];
static int8_t samples[64];

static AudioWrite write_of(const int8_t *data, uint32_t length, uint16_t period, uint16_t volume, uint16_t cycles)
{
    AudioWrite w = {data, length, period, volume, cycles};
    return w;
}
/* Frames until channel stops, at most limit. */
static int frames_until_idle(int channel, int rate, int limit)
{
    int frames = 0;
    while (audio_busy(channel) && frames < limit) { audio_mix(out, 1, rate); ++frames; }
    return frames;
}

static void test_step(void)
{
    /* Paula: PAL clock / period bytes per second. */
    assert(audio_step(357, 44100) == (uint32_t)((3546895ull << 16) / (357ull*44100)));
    assert(audio_step(357, 44100) == 14764);
    assert(audio_step(100, 44100) == audio_step(AUDIO_MIN_PERIOD, 44100));
    assert(audio_step(357, 0) == 0);
}

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
    for (int k = 0; k < 8; ++k) assert(out[k] == 0);
}

static void test_one_shot_and_loops(void)
{
    const int rate = 44100;
    uint32_t step = audio_step(400, rate);
    int once = (int)((40ull << 16) / step) + 1;   /* frames for 40 bytes */
    memset(samples, 10, sizeof samples);
    audio_reset();
    AudioWrite w = write_of(samples, 40, 400, 64, 1);
    audio_write(0, &w);
    assert(audio_busy(0));
    int frames = frames_until_idle(0, rate, 100000);
    assert(frames >= once-1 && frames <= once+1);
    /* Three cycles take three times as long. */
    w.cycles = 3; audio_write(0, &w);
    frames = frames_until_idle(0, rate, 100000);
    assert(frames >= 3*once-2 && frames <= 3*once+2);
    /* Cycles 0 loops until aborted. */
    w.cycles = 0; audio_write(0, &w);
    assert(frames_until_idle(0, rate, 20*once) == 20*once && audio_busy(0));
    audio_abort(0);
    assert(!audio_busy(0));
    audio_mix(out, 1, rate);
    assert(out[0] == 0);
    /* _PlaySound: one-shot request, then the queued repeat request. */
    int8_t first[8], second[8];
    memset(first, 20, sizeof first); memset(second, -20, sizeof second);
    AudioWrite a = write_of(first, 8, 400, 64, 1), b = write_of(second, 8, 400, 64, 0);
    audio_write(3, &a); audio_write(3, &b);
    audio_mix(out, 1, rate);
    assert(out[0] == 20*64*2);
    for (int k = 0; k < 4*once; ++k) audio_mix(out, 1, rate);
    assert(out[0] == -20*64*2 && audio_busy(3));
    audio_abort(3);
    /* A zero-length request completes at once; no data plays silent. */
    AudioWrite empty = write_of(samples, 0, 400, 64, 1);
    audio_write(1, &empty);
    audio_mix(out, 1, rate);
    assert(!audio_busy(1) && out[1] == 0);
    audio_write(-1, &w); audio_write(4, &w); audio_abort(9);
    assert(!audio_busy(-1) && !audio_busy(4));
}

/* Minimal _load_sound file: one measure, one sequence entry, two kits. */
static size_t build_bank(uint8_t *file, uint16_t seq_measure, uint32_t measure_length, int kit_sample)
{
    size_t at = 0;
#define W16(v) do { file[at] = (uint8_t)((v) >> 8); file[at+1] = (uint8_t)(v); at += 2; } while (0)
#define W32(v) do { W16((uint32_t)(v) >> 16); W16((v) & 0xffff); } while (0)
    for (int k = 0; k < 64; ++k) W16(k == 0 ? 48 : 64);
    W16(1); W16(seq_measure); W16(1);
    W32(measure_length);
    memset(file + at, ' ', measure_length); file[at] = 'B'; at += measure_length;
    W16(2);
    W16(400); W16(6); file[at++] = 64; file[at++] = 0; W16(0);
    W16(300); W16(4); file[at++] = 20; file[at++] = (uint8_t)kit_sample; W16(2);
    W16(2);
    W16(6); W16(6); W32(0xdeadbeef);
    W16(4); W16(0); W32(0);
    W32(10);
    for (int k = 0; k < 10; ++k) file[at++] = (uint8_t)(k - 5);
    W16(0x1b39);
#undef W16
#undef W32
    return at;
}

static void test_bank(void)
{
    static uint8_t file[0x3000];
    AudioBank bank;
    char error[128];
    size_t size = build_bank(file, 0, 0x1000, 1);
    assert(audio_bank_parse(&bank, file, size, error, sizeof error));
    assert(bank.seq_count == 1 && bank.sequn[0][0] == 0 && bank.sequn[0][1] == 1 && bank.measln[0] == 48);
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
    assert(!audio_bank_parse(&bank, file, size-1, error, sizeof error));
    assert(!audio_bank_parse(&bank, file, size+1, error, sizeof error) && strstr(error, "size"));
    assert(!audio_bank_parse(&bank, file, 10, error, sizeof error));
    size = build_bank(file, 16, 0x1000, 1);
    assert(!audio_bank_parse(&bank, file, size, error, sizeof error) && strstr(error, "measure"));
    size = build_bank(file, 0, 0x800, 1);
    assert(!audio_bank_parse(&bank, file, size, error, sizeof error));
    size = build_bank(file, 0, 0x1000, 2);
    assert(!audio_bank_parse(&bank, file, size, error, sizeof error) && strstr(error, "sample"));
    /* A repeat request past its sample: kit 1 is 4 bytes, sams[1].loop is 0. */
    size = build_bank(file, 0, 0x1000, 1);
    file[size-2-10-4-16-2-8+6] = 0; file[size-2-10-4-16-2-8+7] = 5;
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
    if (!data) { puts("original gmusic1 absent: skipped"); return; }
    AudioBank bank;
    char error[128];
    assert(size == 90534 && audio_bank_parse(&bank, data, size, error, sizeof error));
    assert(bank.kit_count == 104 && bank.no_sams == 12 && bank.sample_length == 73046);
    assert(bank.seq_count == 8 && bank.measure_length == 0x4000 && bank.measln[10] == 48);
    /* Effect ids used by _effect and the heartbeat (asm:19637-19671). */
    static const uint16_t kit[][5] = {
        {0x42,654,4682,64,3},{0x43,701,1500,51,4},{0x44,715,1500,64,4},{0x45,714,4192,64,5},
        {0x46,480,12976,31,6},{0x47,357,1334,30,7},{0x48,325,1334,41,7},{0x49,3367,2528,64,8},
        {0x4a,759,12470,64,9},{0x4b,1704,5928,54,10},{0x4c,357,6890,51,11},{0x4d,952,8456,20,1}};
    for (size_t k = 0; k < sizeof kit/sizeof kit[0]; ++k) {
        const AudioKit *e = &bank.kit[kit[k][0]];
        assert(e->period == kit[k][1] && e->length == kit[k][2] && e->volume == kit[k][3] && e->sample == kit[k][4]);
        assert(e->loop == (kit[k][0] == 0x4c ? 5 : 0));
    }
    audio_bank_free(&bank);
    data = read_raw("gwords", &size);
    if (!data) { puts("original gwords absent: skipped"); return; }
    assert(size == 79076 && audio_bank_parse(&bank, data, size, error, sizeof error));
    assert(bank.kit_count == 104 && bank.no_sams == 9 && bank.seq_count == 0 && bank.sample_length == 61644);
    audio_bank_free(&bank);
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
    audio_close(); audio_close();
    assert(!audio_device_open() && !audio_busy(2));
    printf("dummy audio device: %s\n", opened ? "opened" : "unavailable (silent)");
}

int main(void)
{
    test_step();
    test_volume_and_panning();
    test_one_shot_and_loops();
    test_bank();
    test_original_banks();
    test_device();
    puts("audio tests: PASS");
    return 0;
}
