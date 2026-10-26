#include "psx/disc.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/* Entirely synthetic raw sectors and ISO directory records, no game data. */
static u8 track[24][2352];
static int fail_reads;

static int hm_read(void *context, u64 offset, void *destination, size_t size) {
    (void)context;
    if (fail_reads || offset > sizeof(track) || size > sizeof(track) - offset)
        return 0;
    memcpy(destination, (const u8 *)track + offset, size);
    return 1;
}

static void hm_both32(u8 *out, u32 value) {
    for (u32 i = 0; i < 4; ++i) {
        out[i] = (u8)(value >> (i * 8));
        out[7 - i] = out[i];
    }
}

static void hm_record(u8 *out, const char *name, u32 lba, u32 size, u8 flags) {
    size_t length = strlen(name);
    out[0] = (u8)(33 + length + !(length & 1));
    hm_both32(out + 2, lba);
    hm_both32(out + 10, size);
    out[25] = flags;
    out[32] = (u8)length;
    memcpy(out + 33, name, length);
}

int main(void) {
    for (u32 i = 0; i < 24; ++i) {
        memset(track[i] + 1, 255, 10);
        track[i][15] = 2;
    }
    u8 output[4096];
    track[0][24] = 0xAB;
    CHECK(hm_disc_decode_sector(track[0], output) == HM_DISC_OK && output[0] == 0xAB);
    track[0][18] = track[0][22] = 0x20;
    CHECK(hm_disc_decode_sector(track[0], output) == HM_DISC_UNSUPPORTED);
    track[0][22] = 0;
    CHECK(hm_disc_decode_sector(track[0], output) == HM_DISC_FORMAT);
    track[0][15] = 1;
    CHECK(hm_disc_decode_sector(track[0], output) == HM_DISC_OK);
    track[0][1] = 0;
    CHECK(hm_disc_decode_sector(track[0], output) == HM_DISC_FORMAT);

    hm_disc disc = {.stream = {NULL, sizeof(track), hm_read},
                    .root = {17, 4096, 2},
                    .sectors = 24,
                    .mounted = 1};
    /* Padding in root's first block must advance to the next block. */
    hm_record(track[18] + 24, "DATA", 19, 2048, 2);
    hm_record(track[19] + 24, "TEST.BIN;1", 20, 2051, 0);
    memset(track[20] + 24, 0xA7, 2048);
    memset(track[21] + 24, 0xB8, 2048);
    hm_disc_entry entry;
    CHECK(hm_disc_find(&disc, "\\data\\test.bin;1", &entry) == HM_DISC_OK);
    CHECK(entry.sector == 20 && entry.size == 2051);
    CHECK(hm_disc_read(&disc, &entry, 2046, output, 5) == HM_DISC_OK);
    CHECK(output[0] == 0xA7 && output[1] == 0xA7 && output[2] == 0xB8 && output[4] == 0xB8);
    CHECK(hm_disc_read(&disc, &entry, 2051, output, 0) == HM_DISC_OK);
    CHECK(hm_disc_read(&disc, &entry, 2051, output, 1) == HM_DISC_RANGE);
    CHECK(hm_disc_read(&disc, &entry, UINT32_MAX, output, 1) == HM_DISC_RANGE);
    CHECK(hm_disc_find(&disc, "DATA/MISSING.BIN;1", &entry) == HM_DISC_NOT_FOUND);
    CHECK(hm_disc_find(&disc, "DATA/../TEST.BIN;1", &entry) == HM_DISC_NOT_FOUND);
    CHECK(hm_disc_find(&disc, "/", &entry) == HM_DISC_OK && entry.sector == 17);
    CHECK(hm_disc_sector(&disc, 24, output) == HM_DISC_RANGE);
    track[19][24] = 33;
    CHECK(hm_disc_find(&disc, "DATA/TEST.BIN;1", &entry) == HM_DISC_FORMAT);
    hm_record(track[19] + 24, "TEST.BIN;1", 20, 2051, 0x80);
    CHECK(hm_disc_find(&disc, "DATA/TEST.BIN;1", &entry) == HM_DISC_UNSUPPORTED);
    hm_record(track[19] + 24, "TEST.BIN;1", 24, 2051, 0);
    CHECK(hm_disc_find(&disc, "DATA/TEST.BIN;1", &entry) == HM_DISC_RANGE);
    hm_record(track[19] + 24, "TEST.BIN;1", 20, 2051, 0);
    track[19][24 + 6] ^= 1;
    CHECK(hm_disc_find(&disc, "DATA/TEST.BIN;1", &entry) == HM_DISC_FORMAT);
    fail_reads = 1;
    CHECK(hm_disc_sector(&disc, 20, output) == HM_DISC_IO);
    CHECK(hm_disc_mount(&disc, (hm_disc_stream){NULL, sizeof(track), hm_read}) == HM_DISC_IO);
    CHECK(!disc.mounted);
    fail_reads = 0;
    CHECK(hm_disc_mount(&disc, (hm_disc_stream){NULL, sizeof(track), hm_read}) ==
          HM_DISC_WRONG_IMAGE);
    CHECK(!disc.mounted);
    CHECK(hm_disc_mount(&disc, (hm_disc_stream){NULL, 2351, hm_read}) == HM_DISC_WRONG_IMAGE);
    return 0;
}
