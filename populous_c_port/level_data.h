#ifndef POPULOUS_LEVEL_DATA_H
#define POPULOUS_LEVEL_DATA_H
#include <stddef.h>
#include <stdint.h>

enum { LEVEL_RECORD_COUNT = 99, LEVEL_FILE_SIZE = 990, LEVEL_COUNT = 495 };
typedef struct {
    uint8_t rating, speed, enemy_powers, your_powers, mode_flags, terrain;
    uint8_t your_count, enemy_count;
    uint16_t seed_offset;
} LevelRecord;
typedef struct { LevelRecord records[LEVEL_RECORD_COUNT]; } LevelTable;
typedef struct {
    LevelRecord record;
    uint16_t internal_level, initial_seed;
} LevelStart;

int level_data_parse(const uint8_t *data, size_t size, LevelTable *out,
                     char *error, size_t error_size);
int level_data_load(const char *directory, LevelTable *out,
                    char *error, size_t error_size);
int level_data_select(const LevelTable *table, unsigned level, LevelStart *out);
#endif
