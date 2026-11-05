#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

/* Paula emulation behind the original audio.device usage (_open_channels
 * asm:20569-20646: four IOAudio channels, CMD_WRITE with ADIOF_PERVOL).
 * Channels 0 and 3 are left, 1 and 2 right, as on the Amiga. */
#define AUDIO_CHANNELS   4
#define AUDIO_PAL_CLOCK  3546895u
#define AUDIO_MIN_PERIOD 124u     /* Paula DMA limit */
#define AUDIO_QUEUE      4

/* One CMD_WRITE: ioa_Data/Length/Period/Volume/Cycles (_setsound
 * asm:20390-20429). Cycles 0 repeats until aborted. */
typedef struct AudioWrite {
    const int8_t *data;
    uint32_t length;
    uint16_t period, volume, cycles;
} AudioWrite;

int  audio_open(void);            /* 1: device running; 0: silent mixer only */
void audio_close(void);
int  audio_device_open(void);
void audio_write(int channel, const AudioWrite *write);    /* BeginIO */
void audio_abort(int channel);                             /* AbortIO */
int  audio_busy(int channel);                              /* !CheckIO */
/* Request now playing and the queue depth (0 if idle); for tests. */
int  audio_current(int channel, AudioWrite *write);
void audio_reset(void);
/* 16.16 source bytes per output frame for a Paula period. */
uint32_t audio_step(uint16_t period, int rate);
/* Interleaved stereo S16; usable without a device (tests, callback). */
void audio_mix(int16_t *out, int frames, int rate);

/* _load_sound asm:20712-20872 file layout, big-endian:
 * measln[64] w, seqlen w, sequn[seqlen] (measure w, repeats w),
 * measure_length l, measures (0x1000 read, rest skipped), no_sounds w,
 * drumkit[no_sounds] 8 bytes, sample count w, sams[n] 8 bytes,
 * all_sample_length l, signed 8-bit PCM, one trailing word. */
#define AUDIO_MAX_SEQ     128     /* _sequn A4 0x3178..0x3377 */
#define AUDIO_MAX_SAMS    40      /* _sams A4 0x33fa..0x3539 */
#define AUDIO_MEASURES    0x1000  /* 16 measures x 4 tracks x 64 beats */

typedef struct AudioKit {         /* drumkit entry */
    uint16_t period, length;      /* +0, +2 */
    int8_t volume, sample;        /* +4 volume = priority, +5 sams index */
    uint16_t loop;                /* +6 second request word, 0 = one-shot */
} AudioKit;

typedef struct AudioSam {
    uint16_t length, loop;        /* +0 bytes, +2 word pushed by _PlaySound */
    uint32_t offset;              /* +4 pointer, rebuilt from lengths */
} AudioSam;

typedef struct AudioBank {
    uint16_t measln[64], seq_count, sequn[AUDIO_MAX_SEQ][2];
    uint32_t measure_length;
    uint8_t measures[AUDIO_MEASURES];
    uint16_t kit_count, no_sams;
    AudioKit *kit;
    AudioSam sams[AUDIO_MAX_SAMS];
    uint32_t sample_length;
    int8_t *samples;
} AudioBank;

int  audio_bank_parse(AudioBank *bank, const uint8_t *data, size_t size, char *error, size_t error_size);
void audio_bank_free(AudioBank *bank);

#endif
