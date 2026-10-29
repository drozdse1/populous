#ifndef POPULOUS_LAND_DATA_H
#define POPULOUS_LAND_DATA_H

#include <stddef.h>
#include <stdint.h>

#define LAND_RULE_COUNT 11
#define LAND_HEADER_SIZE 114
#define LAND_FILE_SIZE (LAND_HEADER_SIZE + 0x8340)

/* Host-endian values decoded from _load_ground (asm:16468-16621).
 * No pointers, graphics ownership, mutable global state or A4 writes. */
typedef struct {
    uint16_t walk_death;
    uint16_t population_add[LAND_RULE_COUNT];
    uint16_t mana_add[LAND_RULE_COUNT];
    uint16_t weapons_add[LAND_RULE_COUNT];
    uint16_t battle_add1[LAND_RULE_COUNT];
    uint16_t battle_add2[3];
    uint8_t map_colour[32]; /* Original duplicates the 16 input bytes. */
    uint16_t sprite_set;
    uint16_t weapons_order[LAND_RULE_COUNT]; /* Original ascending copy. */
} LandRules;

/* Return 1 on success, 0 on failure; leave *out unchanged on failure.
 * Optional error buffer receives a bounded diagnostic (empty on success).
 * LAND has no magic number: exact size and supported numeric ranges are checked,
 * not authenticity. Graphics bytes must be present but are not decoded. */
int land_rules_parse(const uint8_t *data, size_t size, LandRules *out,
                     char *error, size_t error_size);

/* Read a regular, non-symlink land0..3 in an explicit external directory.
 * No fallback/default tables; no dependency on current working directory. */
int land_rules_load(const char *directory, unsigned land_index, LandRules *out,
                    char *error, size_t error_size);

#endif
