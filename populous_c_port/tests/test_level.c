#define _POSIX_C_SOURCE 200809L
#include "../level_data.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint8_t bytes[LEVEL_FILE_SIZE + 1];
static void write_fixture(const char *path, size_t size)
{
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, size, file) == size);
    assert(fclose(file) == 0);
}
int main(void)
{
    LevelTable table, saved;
    char error[128];
    for (int index = 0; index < LEVEL_RECORD_COUNT; ++index) {
        uint8_t record[] = {10, 9, 135, 63, 31, 3, 12, 14, 0xab, (uint8_t)index};
        memcpy(bytes + index * 10, record, 10);
    }
    assert(level_data_parse(bytes, LEVEL_FILE_SIZE, &table, error, sizeof error));
    assert(!error[0]);
    for (int index = 0; index < LEVEL_RECORD_COUNT; ++index) {
        const LevelRecord *record = &table.records[index];
        assert(record->rating == 10 && record->speed == 9);
        assert(record->enemy_powers == 135 && record->your_powers == 63);
        assert(record->mode_flags == 31 && record->terrain == 3);
        assert(record->your_count == 12 && record->enemy_count == 14);
        assert(record->seed_offset == 0xab00 + index);
    }
    saved = table;
    for (size_t size = 0; size <= LEVEL_FILE_SIZE + 1; ++size) {
        if (size == LEVEL_FILE_SIZE) continue;
        assert(!level_data_parse(bytes, size, &table, error, sizeof error));
        assert(error[0] && !memcmp(&saved, &table, sizeof table));
    }
    assert(!level_data_parse(NULL, LEVEL_FILE_SIZE, &table, NULL, 0));
    assert(!level_data_parse(bytes, LEVEL_FILE_SIZE, NULL, NULL, 0));
    bytes[985] = 4;
    assert(!level_data_parse(bytes, LEVEL_FILE_SIZE, &table, error, sizeof error));
    assert(!memcmp(&saved, &table, sizeof table));
    bytes[985] = 3;
    const unsigned levels[] = {0, 4, 5, 494};
    const unsigned records[] = {0, 0, 1, 98};
    const unsigned internal[] = {0, 20, 25, 2470};
    const unsigned seeds[] = {0xab00, 0xab04, 0xab02, 0xab68};
    for (size_t index = 0; index < 4; ++index) {
        LevelStart start;
        assert(level_data_select(&table, levels[index], &start));
        assert(start.internal_level == internal[index]);
        assert(start.record.seed_offset == 0xab00 + records[index]);
        assert(start.initial_seed == seeds[index]);
    }
    LevelStart start, before;
    memset(&start, 0xa5, sizeof start); before = start;
    assert(!level_data_select(&table, 495, &start));
    assert(!memcmp(&start, &before, sizeof start));
    table.records[0].seed_offset = 65535;
    assert(level_data_select(&table, 4, &start) && start.initial_seed == 3);
    assert(!level_data_select(NULL, 0, &start));
    assert(!level_data_select(&table, 0, NULL));

    char directory[] = "/tmp/populous-level-test-XXXXXX", path[256];
    assert(mkdtemp(directory));
    snprintf(path, sizeof path, "%s/level.dat", directory);
    table = saved;
    assert(!level_data_load(directory, &table, error, sizeof error));
    assert(!level_data_load(NULL, &table, error, sizeof error));
    assert(!level_data_load(directory, NULL, error, sizeof error));
    write_fixture(path, LEVEL_FILE_SIZE);
    memset(&table, 0xa5, sizeof table);
    assert(level_data_load(directory, &table, error, sizeof error));
    assert(!memcmp(&saved, &table, sizeof table));
    write_fixture(path, LEVEL_FILE_SIZE - 1);
    assert(!level_data_load(directory, &table, error, sizeof error));
    write_fixture(path, LEVEL_FILE_SIZE + 1);
    assert(!level_data_load(directory, &table, error, sizeof error));
    assert(unlink(path) == 0);
    assert(symlink("/dev/zero", path) == 0);
    assert(!level_data_load(directory, &table, error, sizeof error));
    assert(unlink(path) == 0 && mkfifo(path, 0600) == 0);
    assert(!level_data_load(directory, &table, error, sizeof error));
    assert(unlink(path) == 0 && mkdir(path, 0700) == 0);
    assert(!level_data_load(directory, &table, error, sizeof error));
    assert(!memcmp(&saved, &table, sizeof table));
    assert(rmdir(path) == 0 && rmdir(directory) == 0);
    puts("level decoder/selection/secure file loading: PASS");
    return 0;
}
