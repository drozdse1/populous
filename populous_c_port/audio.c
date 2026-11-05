/* Paula-style 4-channel mixer for the original audio.device calls
 * (_PlaySound/_setsound asm:20307-20429, _kill_effect 20876-20894,
 * _check_effect 20898-20908). No device is required: without one the
 * mixer state still runs (tests) and the game is silent. */
#include "audio.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AudioChannel {
    AudioWrite queue[AUDIO_QUEUE];
    int count;
    uint32_t position;            /* 16.16 into queue[0] */
    uint16_t left;                /* cycles left, 0 = forever */
} AudioChannel;

static AudioChannel channels[AUDIO_CHANNELS];
static SDL_AudioDeviceID device;
static int device_rate, audio_inited;

static void lock(void)   { if (device) SDL_LockAudioDevice(device); }
static void unlock(void) { if (device) SDL_UnlockAudioDevice(device); }

uint32_t audio_step(uint16_t period, int rate)
{
    if (period < AUDIO_MIN_PERIOD) period = AUDIO_MIN_PERIOD;
    if (rate <= 0) return 0;
    return (uint32_t)(((uint64_t)AUDIO_PAL_CLOCK << 16) / ((uint64_t)period * (uint64_t)rate));
}

static void pop(AudioChannel *c)
{
    if (!c->count) return;
    memmove(c->queue, c->queue+1, sizeof c->queue[0] * (size_t)(--c->count));
    c->position = 0;
    c->left = c->count ? c->queue[0].cycles : 0;
}

void audio_write(int channel, const AudioWrite *write)
{
    if (channel < 0 || channel >= AUDIO_CHANNELS || !write) return;
    lock();
    AudioChannel *c = &channels[channel];
    if (c->count < AUDIO_QUEUE) {
        c->queue[c->count++] = *write;
        if (c->count == 1) { c->position = 0; c->left = write->cycles; }
    }
    unlock();
}

void audio_abort(int channel)
{
    if (channel < 0 || channel >= AUDIO_CHANNELS) return;
    lock();
    channels[channel].count = 0; channels[channel].position = 0;
    unlock();
}

int audio_busy(int channel)
{
    if (channel < 0 || channel >= AUDIO_CHANNELS) return 0;
    lock();
    int busy = channels[channel].count != 0;
    unlock();
    return busy;
}

int audio_current(int channel, AudioWrite *write)
{
    if (channel < 0 || channel >= AUDIO_CHANNELS) return 0;
    lock();
    int count = channels[channel].count;
    if (count && write) *write = channels[channel].queue[0];
    unlock();
    return count;
}

void audio_paula_start(int channel, const int8_t *data, uint32_t length, uint16_t period, uint16_t volume)
{
    if (channel < 0 || channel >= AUDIO_CHANNELS) return;
    AudioWrite write = {data, length, period, volume, 0};
    lock();
    AudioChannel *c = &channels[channel];
    c->queue[0] = write; c->count = 1; c->position = 0; c->left = 0;
    unlock();
}

/* The running pass ends normally; the latched pair then repeats. */
void audio_paula_latch(int channel, const int8_t *data, uint32_t length)
{
    if (channel < 0 || channel >= AUDIO_CHANNELS) return;
    lock();
    AudioChannel *c = &channels[channel];
    if (c->count) {
        AudioWrite next = c->queue[0];
        next.data = data; next.length = length; next.cycles = 0;
        c->queue[1] = next; c->count = 2; c->left = 1;
    }
    unlock();
}

void audio_paula_period(int channel, uint16_t period)
{
    if (channel < 0 || channel >= AUDIO_CHANNELS) return;
    lock();
    for (int index = 0; index < channels[channel].count; ++index) channels[channel].queue[index].period = period;
    unlock();
}

void audio_lock(void)   { lock(); }
void audio_unlock(void) { unlock(); }

void audio_reset(void)
{
    for (int channel = 0; channel < AUDIO_CHANNELS; ++channel) audio_abort(channel);
}

/* Zero-order hold like Paula; volume 0..64 per request. Left = 0+3,
 * right = 1+2; two full-scale channels just fit S16. */
static void mix(int16_t *out, int frames, int rate)
{
    for (int frame = 0; frame < frames; ++frame) {
        int32_t side[2] = {0, 0};
        for (int index = 0; index < AUDIO_CHANNELS; ++index) {
            AudioChannel *c = &channels[index];
            while (c->count && !c->queue[0].length) pop(c);
            if (!c->count || !c->queue[0].data) { if (c->count) pop(c); continue; }
            AudioWrite *w = &c->queue[0];
            int volume = w->volume > 64 ? 64 : w->volume;
            side[index == 1 || index == 2] += w->data[c->position >> 16] * volume * 2;
            c->position += audio_step(w->period, rate);
            while (c->count && (c->position >> 16) >= w->length) {
                c->position -= w->length << 16;
                if (c->left && !--c->left) pop(c);
            }
        }
        for (int s = 0; s < 2; ++s)
            out[frame*2+s] = (int16_t)(side[s] > 32767 ? 32767 : side[s] < -32768 ? -32768 : side[s]);
    }
}

void audio_mix(int16_t *out, int frames, int rate)
{
    if (!out || frames <= 0) return;
    lock(); mix(out, frames, rate); unlock();
}

static void callback(void *user, Uint8 *stream, int bytes)
{
    (void)user;
    mix((int16_t *)stream, bytes / 4, device_rate);
}

int audio_open(void)
{
    if (device) return 1;
    audio_reset();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Audio disabled: %s\n", SDL_GetError());
        return 0;
    }
    audio_inited = 1;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 2;
    want.samples = 1024; want.callback = callback;
    device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!device) {
        fprintf(stderr, "Audio disabled: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO); audio_inited = 0;
        return 0;
    }
    device_rate = have.freq;
    SDL_PauseAudioDevice(device, 0);
    return 1;
}

void audio_close(void)
{
    if (device) { SDL_CloseAudioDevice(device); device = 0; }
    if (audio_inited) { SDL_QuitSubSystem(SDL_INIT_AUDIO); audio_inited = 0; }
    audio_reset();
}

int audio_device_open(void) { return device != 0; }

static int fail(char *error, size_t size, const char *text)
{
    if (error && size) snprintf(error, size, "%s", text);
    return 0;
}

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be32(const uint8_t *p) { return (uint32_t)be16(p) << 16 | be16(p+2); }

void audio_bank_free(AudioBank *bank)
{
    if (!bank) return;
    free(bank->kit); free(bank->samples);
    memset(bank, 0, sizeof *bank);
}

/* Strict: a file must be consumed exactly as _load_sound reads it, and
 * every index the player can follow must stay inside its buffer. */
int audio_bank_parse(AudioBank *bank, const uint8_t *data, size_t size, char *error, size_t error_size)
{
    AudioBank b;
    memset(&b, 0, sizeof b);
    size_t at = 0;
#define NEED(n) do { if (size - at < (size_t)(n) || at > size) return fail(error, error_size, "truncated sound file"); } while (0)
    NEED(130);
    for (int k = 0; k < 64; ++k) b.measln[k] = be16(data + 2*k);
    b.seq_count = be16(data + 128); at = 130;
    if (b.seq_count > AUDIO_MAX_SEQ) return fail(error, error_size, "sequence longer than _sequn");
    NEED(4u*b.seq_count + 4);
    for (int k = 0; k < b.seq_count; ++k) {
        b.sequn[k][0] = be16(data + at); b.sequn[k][1] = be16(data + at + 2); at += 4;
        uint16_t m = b.sequn[k][0];
        if (m >= AUDIO_MEASURES/256 || !b.measln[m] || b.measln[m] > 64)
            return fail(error, error_size, "sequence measure outside the 0x1000 measure buffer");
    }
    b.measure_length = be32(data + at); at += 4;
    if (b.measure_length < AUDIO_MEASURES) return fail(error, error_size, "measure block shorter than the 0x1000 read");
    NEED(b.measure_length);
    memcpy(b.measures, data + at, AUDIO_MEASURES); at += b.measure_length;
    NEED(2); b.kit_count = be16(data + at); at += 2;
    NEED(8u*b.kit_count + 2);
    const uint8_t *kit = data + at; at += 8u*b.kit_count;
    b.no_sams = be16(data + at); at += 2;
    if (b.no_sams > AUDIO_MAX_SAMS) return fail(error, error_size, "more samples than _sams holds");
    NEED(8u*b.no_sams + 4);
    uint32_t offset = 0;
    for (int k = 0; k < b.no_sams; ++k, at += 8) {
        b.sams[k].length = be16(data + at); b.sams[k].loop = be16(data + at + 2);
        b.sams[k].offset = offset; offset += b.sams[k].length;
    }
    b.sample_length = be32(data + at); at += 4;
    if (offset > b.sample_length) return fail(error, error_size, "sample lengths exceed all_sample_length");
    NEED((size_t)b.sample_length + 2);
    if (at + b.sample_length + 2 != size) return fail(error, error_size, "unexpected sound file size");
    b.kit = calloc(b.kit_count ? b.kit_count : 1, sizeof *b.kit);
    b.samples = malloc(b.sample_length ? b.sample_length : 1);
    if (!b.kit || !b.samples) { audio_bank_free(&b); return fail(error, error_size, "out of memory"); }
    memcpy(b.samples, data + at, b.sample_length);
    for (int k = 0; k < b.kit_count; ++k) {
        const uint8_t *p = kit + 8*k;
        AudioKit *e = &b.kit[k];
        e->period = be16(p); e->length = be16(p+2);
        e->volume = (int8_t)p[4]; e->sample = (int8_t)p[5]; e->loop = be16(p+6);
        if (e->sample < 0 || e->sample >= b.no_sams || e->length > b.sams[e->sample].length) {
            audio_bank_free(&b); return fail(error, error_size, "drumkit entry outside its sample");
        }
        /* _PlaySound's second request: offset long = sams.loop:kit.loop. */
        uint32_t second = (uint32_t)b.sams[e->sample].loop << 16 | e->loop;
        if (e->loop && second > e->length) {
            audio_bank_free(&b); return fail(error, error_size, "repeat request outside its sample");
        }
    }
#undef NEED
    *bank = b;
    return 1;
}
