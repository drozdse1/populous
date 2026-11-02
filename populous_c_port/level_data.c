#define _POSIX_C_SOURCE 200809L
#include "level_data.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
    return 0;
}

int level_data_parse(const uint8_t *data, size_t size, LevelTable *out,
                     char *error, size_t error_size)
{
    if (!data || !out || size != LEVEL_FILE_SIZE)
        return fail(error, error_size, "level.dat requires exactly 990 bytes and an output object");
    LevelTable parsed = {0};
    for (size_t index = 0; index < LEVEL_RECORD_COUNT; ++index) {
        const uint8_t *record = data + index * 10;
        if (record[5] > 3)
            return fail(error, error_size, "level.dat terrain must be 0..3");
        parsed.records[index] = (LevelRecord){
            record[0], record[1], record[2], record[3], record[4], record[5],
            record[6], record[7], (uint16_t)((uint16_t)record[8] << 8 | record[9])
        };
    }
    *out = parsed;
    if (error && error_size) error[0] = '\0';
    return 1;
}

int level_data_select(const LevelTable *table, unsigned level, LevelStart *out)
{
    if (!table || !out || level >= LEVEL_COUNT) return 0;
    LevelStart selected = {0};
    selected.record = table->records[level / 5];
    selected.internal_level = (uint16_t)(5 * level);
    selected.initial_seed = (uint16_t)(selected.record.seed_offset + (selected.internal_level & 7));
    *out = selected;
    return 1;
}

int level_data_load(const char *directory, LevelTable *out,
                    char *error, size_t error_size)
{
    if (!directory || !directory[0] || !out)
        return fail(error, error_size, "provide a level.dat directory and output object");
    int directory_fd = open(directory, O_RDONLY | O_DIRECTORY);
    if (directory_fd < 0) return fail(error, error_size, "cannot open level.dat directory");
    int descriptor = openat(directory_fd, "level.dat", O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    close(directory_fd);
    if (descriptor < 0)
        return fail(error, error_size, "cannot open regular level.dat (symlinks are not allowed)");
    struct stat status;
    if (fstat(descriptor, &status) || !S_ISREG(status.st_mode) || status.st_size != LEVEL_FILE_SIZE) {
        close(descriptor);
        return fail(error, error_size, "level.dat must be a regular file of exactly 990 bytes");
    }
    uint8_t data[LEVEL_FILE_SIZE + 1];
    size_t used = 0;
    while (used < sizeof data) {
        ssize_t count = read(descriptor, data + used, sizeof data - used);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) {
            close(descriptor);
            return fail(error, error_size, "error reading level.dat");
        }
        if (!count) break;
        used += (size_t)count;
    }
    close(descriptor);
    return level_data_parse(data, used, out, error, error_size);
}
