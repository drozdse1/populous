#define _POSIX_C_SOURCE 200809L
#include "../land_data.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned char fixture[33715];
static void word(size_t offset, unsigned value)
{
    fixture[offset] = value >> 8;
    fixture[offset + 1] = value;
}

static void synthetic(void)
{
    LandRules rules, saved;
    char error[128];
    memset(fixture, 0, sizeof fixture);
    word(0, 7);
    word(2, 0x1234);
    word(24, 0x2345);
    word(46, 17);
    word(48, 3);
    word(68, 0x3456);
    word(90, 0x4567);
    fixture[96] = 15;
    word(112, 2);
    assert(land_rules_parse(fixture, 33714, &rules, error, sizeof error));
    assert(error[0] == '\0');
    assert(rules.walk_death == 7 && rules.population_add[0] == 0x1234);
    assert(rules.mana_add[0] == 0x2345 && rules.weapons_add[0] == 17);
    assert(rules.battle_add1[0] == 0x3456 && rules.battle_add2[0] == 0x4567);
    assert(rules.map_colour[0] == 15 && rules.map_colour[16] == 15);
    assert(rules.sprite_set == 2);
    assert(rules.weapons_order[0] == 0 && rules.weapons_order[9] == 3 && rules.weapons_order[10] == 17);
    saved = rules;
    for (size_t size = 0; size < 33714; ++size)
        assert(!land_rules_parse(fixture, size, &rules, error, sizeof error));
    assert(!land_rules_parse(fixture, 33715, &rules, error, sizeof error));
    assert(!land_rules_parse(NULL, 33714, &rules, error, sizeof error));
    assert(!land_rules_parse(fixture, 33714, NULL, NULL, 0));
    const size_t offsets[] = {0, 2, 24, 46, 68, 90, 112};
    for (size_t index = 0; index < sizeof offsets / sizeof offsets[0]; ++index) {
        size_t offset = offsets[index];
        unsigned char original = fixture[offset];
        fixture[offset] = 0x80;
        assert(!land_rules_parse(fixture, 33714, &rules, error, sizeof error));
        assert(error[0] && memcmp(&saved, &rules, sizeof rules) == 0);
        fixture[offset] = original;
    }
    word(0, 0);
    assert(!land_rules_parse(fixture, 33714, &rules, error, sizeof error));
    word(0, 7);
    fixture[96] = 16;
    assert(!land_rules_parse(fixture, 33714, &rules, error, sizeof error));
    fixture[96] = 15;
    word(112, 10);
    assert(!land_rules_parse(fixture, 33714, &rules, error, sizeof error));
    word(112, 2);

    char directory[] = "/tmp/populous-land-test-XXXXXX";
    assert(mkdtemp(directory));
    char path[256];
    snprintf(path, sizeof path, "%s/land0", directory);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(fixture, 1, 33714, file) == 33714);
    assert(fclose(file) == 0);
    assert(land_rules_load(directory, 0, &rules, error, sizeof error));
    assert(rules.population_add[0] == 0x1234);
    assert(!land_rules_load(directory, 1, &rules, error, sizeof error));
    assert(!land_rules_load(directory, 4, &rules, error, sizeof error));
    assert(!land_rules_load(NULL, 0, &rules, error, sizeof error));
    file = fopen(path, "ab");
    assert(file && fputc(0, file) == 0 && fclose(file) == 0);
    assert(!land_rules_load(directory, 0, &rules, error, sizeof error));
    assert(unlink(path) == 0);
    assert(symlink("/dev/zero", path) == 0);
    assert(!land_rules_load(directory, 0, &rules, error, sizeof error));
    assert(unlink(path) == 0 && rmdir(directory) == 0);
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        const unsigned death[] = {1, 8, 8, 4};
        const unsigned growth[] = {5, 10, 9, 10};
        for (unsigned land = 0; land < 4; ++land) {
            LandRules rules;
            char error[128];
            if (!land_rules_load(argv[1], land, &rules, error, sizeof error)) {
                fprintf(stderr, "land%u: %s\n", land, error);
                return 1;
            }
            assert(rules.walk_death == death[land]);
            assert(rules.population_add[10] == growth[land]);
            assert(rules.battle_add1[0] == 50 && rules.battle_add1[10] == 2000);
            assert(rules.battle_add2[0] == 100 && rules.battle_add2[2] == 3000);
            assert(rules.sprite_set == 0);
        }
        puts("local original LAND integration passed (four files)");
    } else {
        assert(argc == 1);
        synthetic();
        puts("synthetic LAND parser and file loader passed");
    }
    return 0;
}
