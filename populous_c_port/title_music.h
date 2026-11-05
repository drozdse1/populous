#ifndef TITLE_MUSIC_H
#define TITLE_MUSIC_H

#include <stddef.h>
#include <stdint.h>

/* populous_main's title music (TITLE MUSIC BY ROB HUBBARD, asm:2405):
 * SUB_2C36 asm:2418-2471 reads "popsam" (0x1c01e bytes into 0x1c020 CHIP),
 * SUB_2F9C asm:2645-2698 starts a song, LAB_3060 asm:2706-2896 runs per
 * vertical blank (LAB_2E9E asm:2577-2589), SUB_3046 asm:2699-2705 stops.
 * popsam holds only the 8 samples: per sample BE32 length, BE16 rate in Hz,
 * then signed 8-bit PCM (asm:2650-2662). Score, instruments and pulse waves
 * live in the launcher's code (LAB_3532..LAB_3F90, asm:2899-3089). */
#define TITLE_SAMPLES     8
#define TITLE_POPSAM_SIZE 0x1c01eu
#define TITLE_VOICES      4

typedef struct TitleSample { uint32_t offset, length; uint16_t rate; } TitleSample;
typedef struct TitleBank {
    TitleSample samples[TITLE_SAMPLES];
    int8_t *pcm;                  /* whole file; sample data at offset */
    size_t size;
} TitleBank;

int  title_bank_parse(TitleBank *bank, const uint8_t *data, size_t size, char *error, size_t error_size);
void title_bank_free(TitleBank *bank);

/* The bank must outlive playback. Song 0 is the only one (asm:2639). */
int  title_music_start(const TitleBank *bank, int song);
void title_music_vbi(void);       /* one 50 Hz LAB_3060 pass */
void title_music_stop(void);      /* SUB_3046: DMACON $000f */
int  title_music_playing(void);

typedef struct TitleVoice {
    uint16_t counter, period, flags;
    int note, instrument;         /* instrument -1 until the first $80 */
    int event;                    /* last pattern event: 1 note, 2 rest */
    uint32_t pattern;             /* A3-relative pattern position */
} TitleVoice;
int  title_music_voice(int voice, TitleVoice *out);
int  title_music_tempo(void);

#endif
