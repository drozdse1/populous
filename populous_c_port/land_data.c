#define _POSIX_C_SOURCE 200809L
#include "land_data.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define POPULOUS_WIN_POSIX_MACROS
#include "win_posix.h"
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int fail(char *error, size_t size, const char *message)
{
    if (error && size)
        snprintf(error, size, "%s", message);
    return 0;
}

static uint16_t be16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] << 8 | data[1]);
}

int land_rules_parse(const uint8_t *data, size_t size, LandRules *out,
                     char *error, size_t error_size)
{
    if (!data || !out || size != LAND_FILE_SIZE)
        return fail(error, error_size, "LAND requires exactly 33714 bytes and an output object");
    LandRules parsed = {0};
    /* _load_ground sequential Read calls: asm:16483-16529. */
    parsed.walk_death = be16(data);
    if (!parsed.walk_death || parsed.walk_death > INT16_MAX)
        return fail(error, error_size, "LAND walk_death must be a positive signed word");
    uint16_t *tables[] = {parsed.population_add, parsed.mana_add,
                         parsed.weapons_add, parsed.battle_add1, parsed.battle_add2};
    const size_t offsets[] = {2, 24, 46, 68, 90};
    for (size_t table = 0; table < 5; ++table) {
        size_t count = table == 4 ? 3 : LAND_RULE_COUNT;
        for (size_t index = 0; index < count; ++index) {
            uint16_t value = be16(data + offsets[table] + index * 2);
            /* These rule increments/thresholds are used as signed words. */
            if (value > INT16_MAX)
                return fail(error, error_size, "LAND rule value exceeds supported nonnegative signed-word range");
            tables[table][index] = value;
        }
    }
    for (size_t index = 0; index < 16; ++index) {
        if (data[96 + index] > 15)
            return fail(error, error_size, "LAND map colour exceeds the four-plane palette");
        parsed.map_colour[index] = parsed.map_colour[index + 16] = data[96 + index];
    }
    parsed.sprite_set = be16(data + 112);
    /* _read_sprites inserts a single decimal digit (asm:16410-16413). */
    if (parsed.sprite_set > 9)
        return fail(error, error_size, "LAND sprite set must be a single decimal digit");
    memcpy(parsed.weapons_order, parsed.weapons_add, sizeof parsed.weapons_order);
    /* _load_ground's compare/swap loops (asm:16552-16606) sort ascending. */
    for (size_t index = 1; index < LAND_RULE_COUNT; ++index) {
        uint16_t value = parsed.weapons_order[index];
        size_t position = index;
        while (position && parsed.weapons_order[position - 1] > value) {
            parsed.weapons_order[position] = parsed.weapons_order[position - 1];
            --position;
        }
        parsed.weapons_order[position] = value;
    }
    *out = parsed;
    if (error && error_size)
        error[0] = '\0';
    return 1;
}

int land_file_read(const char *directory, unsigned land_index, uint8_t *out,
                   char *error, size_t error_size)
{
    if (!directory || !directory[0] || land_index > 8 || !out)
        return fail(error, error_size, "provide an external data directory, land index 0..8 and output buffer");
    int directory_fd = open(directory, O_RDONLY | O_DIRECTORY);
    if (directory_fd < 0)
        return fail(error, error_size, "cannot open LAND data directory");
    char name[] = "land0";
    name[4] += land_index;
    int descriptor = openat(directory_fd, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    close(directory_fd);
    if (descriptor < 0)
        return fail(error, error_size, "cannot open regular land file (symlinks are not allowed)");
    struct stat status;
    if (fstat(descriptor, &status) || !S_ISREG(status.st_mode) || status.st_size != LAND_FILE_SIZE) {
        close(descriptor);
        return fail(error, error_size, "LAND must be a regular file of exactly 33714 bytes");
    }
    uint8_t data[LAND_FILE_SIZE + 1];
    size_t used = 0;
    while (used < sizeof data) {
        ssize_t count = read(descriptor, data + used, sizeof data - used);
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0) {
            close(descriptor);
            return fail(error, error_size, "error reading LAND file");
        }
        if (!count)
            break;
        used += (size_t)count;
    }
    close(descriptor);
    if (used != LAND_FILE_SIZE)
        return fail(error, error_size, "LAND must be a regular file of exactly 33714 bytes");
    memcpy(out, data, LAND_FILE_SIZE);
    if (error && error_size)
        error[0] = '\0';
    return 1;
}

int land_rules_load(const char *directory, unsigned land_index, LandRules *out,
                    char *error, size_t error_size)
{
    uint8_t data[LAND_FILE_SIZE];
    if (!out)
        return fail(error, error_size, "provide an external data directory, land index 0..8 and output object");
    if (!land_file_read(directory, land_index, data, error, error_size))
        return 0;
    return land_rules_parse(data, sizeof data, out, error, error_size);
}
