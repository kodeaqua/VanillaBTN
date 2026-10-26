#include "compat/family_names.h"
#include "family.h"
#include "farm_state.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
extern u32 family_tables[5];
static u8 track[300][2352];
static int failed;
static int read_memory(void *context, u64 offset, void *out, size_t size) {
    (void)context;
    if (failed || offset > sizeof track || size > sizeof track - offset)
        return 0;
    memcpy(out, (u8 *)track + offset, size);
    return 1;
}
static void put32(u8 *p, u32 value) {
    for (u32 i = 0; i < 4; ++i)
        p[i] = (u8)(value >> (i * 8));
}
static void both32(u8 *p, u32 value) {
    put32(p, value);
    for (u32 i = 0; i < 4; ++i)
        p[7 - i] = p[i];
}
static u32 record(u8 *p, const char *name, u32 sector, u32 bytes) {
    u32 n = (u32)strlen(name), length = 33 + n + !(n & 1);
    p[0] = (u8)length;
    both32(p + 2, sector);
    both32(p + 10, bytes);
    p[32] = (u8)n;
    memcpy(p + 33, name, n);
    return length;
}
static u8 *archive_byte(u32 offset) { return &track[30 + offset / 2048][24 + offset % 2048]; }
static int read_file(void *context, u64 offset, void *out, size_t size) {
    return !fseek(context, (long)offset, SEEK_SET) && fread(out, 1, size, context) == size;
}
int main(int argc, char **argv) {
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        CHECK(file);
        CHECK(!fseek(file, 0, SEEK_END));
        long bytes = ftell(file);
        CHECK(bytes > 0);
        hm_disc disc;
        CHECK(hm_disc_mount(&disc, (hm_disc_stream){file, (u64)bytes, read_file}) == HM_DISC_OK);
        CHECK(hm_family_names_load(&disc) == HM_DISC_OK);
        for (int i = 0; i < 5; ++i) {
            game_state.family.wifeIndex = i;
            CHECK(family_table_lookup() == hm_family_name(i));
            int n = 0;
            while (n < 9 && hm_family_name(i)[n] != 0xffff)
                ++n;
            CHECK(n > 0 && n < 9);
        }
        fclose(file);
        puts("Five original family names loaded from validated disc");
        return 0;
    }
    for (u32 i = 0; i < 300; ++i) {
        memset(track[i] + 1, 255, 10);
        track[i][15] = 2;
    }
    u32 n = record(track[18] + 24, "A_FILE.HDT;1", 20, 132);
    record(track[18] + 24 + n, "A_FILE.BIN;1", 30, 0x71000);
    put32(track[20] + 24 + 44, 0);
    put32(track[20] + 24 + 48, 0x71000);
    for (u32 i = 0; i < 5; ++i) {
        u32 offset = family_tables[i] - 0x800b7e20u;
        *archive_byte(offset) = (u8)(100 + i);
        *archive_byte(offset + 1) = 0;
        *archive_byte(offset + 2) = 255;
        *archive_byte(offset + 3) = 255;
    }
    hm_disc disc = {.stream = {NULL, sizeof track, read_memory},
                    .root = {18, 2048, 2},
                    .sectors = 300,
                    .mounted = 1};
    CHECK(hm_family_names_load(&disc) == HM_DISC_OK);
    for (int i = 0; i < 5; ++i) {
        game_state.family.wifeIndex = i;
        CHECK(family_table_lookup() == hm_family_name(i));
        CHECK(hm_family_name(i)[0] == 100 + i && hm_family_name(i)[1] == 0xffff);
        for (int j = 2; j < 9; ++j)
            CHECK(hm_family_name(i)[j] == 0xffff);
    }
    put32(track[20] + 24 + 48, 0xffffffff);
    CHECK(hm_family_names_load(&disc) == HM_DISC_RANGE);
    put32(track[20] + 24 + 48, 0x71000);
    for (u32 i = 0; i < 18; ++i)
        *archive_byte(family_tables[0] - 0x800b7e20u + i) = 0;
    CHECK(hm_family_names_load(&disc) == HM_DISC_FORMAT);
    failed = 1;
    CHECK(hm_family_names_load(&disc) == HM_DISC_IO);
    CHECK(hm_family_names_load(NULL) == HM_DISC_FORMAT);
    return 0;
}
