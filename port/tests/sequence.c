#include "psx/sequence.h"
#include <libsnd.h>
#include <stdio.h>
#include <string.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    const u8 score[] = {
        0, 0xC2, 7,    0, 0x92, 60,   100,  0x81, 0,    64, 80, /* running-status note, delta 128 */
        0, 0xFF, 0x51, 3, 7,    0xA1, 0x20, 0,    0x82, 60, 0,  0, 0xFF, 0x2F, 0};
    hm_seq_cursor cursor = {score, sizeof(score), 0, 0};
    hm_seq_event event;
    CHECK(hm_seq_next(&cursor, &event) == 1 && event.status == 0xC2 && event.bytes == 1 &&
          event.payload[0] == 7);
    CHECK(hm_seq_next(&cursor, &event) == 1 && event.status == 0x92 && event.payload[0] == 60);
    CHECK(hm_seq_next(&cursor, &event) == 1 && event.status == 0x92 && event.delta == 128 &&
          event.payload[0] == 64);
    CHECK(hm_seq_next(&cursor, &event) == 1 && event.meta == 0x51 && event.bytes == 3);
    CHECK(hm_seq_next(&cursor, &event) == 1 && event.status == 0x82);
    CHECK(hm_seq_next(&cursor, &event) == 1 && event.meta == 0x2F && !event.bytes);
    CHECK(hm_seq_next(&cursor, &event) == 0);
    /* Every truncation must terminate inside its supplied bounds. */
    for (u32 size = 0; size < sizeof(score); ++size) {
        cursor = (hm_seq_cursor){score, size, 0, 0};
        int result;
        do {
            result = hm_seq_next(&cursor, &event);
        } while (result == 1);
        CHECK(cursor.at <= size);
    }
    const u8 malformed[][6] = {
        {0, 60, 100, 0, 0, 0},          /* missing running status */
        {0xFF, 0xFF, 0xFF, 0xFF, 0, 0}, /* oversized variable integer */
        {0, 0x90, 0xFF, 0, 0, 0},       /* channel payload is seven-bit */
        {0, 0xFF, 0x51, 127, 0, 0}      /* truncated metadata */
    };
    for (u32 i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
        cursor = (hm_seq_cursor){malformed[i], sizeof(malformed[i]), 0, 0};
        CHECK(hm_seq_next(&cursor, &event) == -1);
    }
    u8 bank_header[0xA20] = {0};
    memcpy(bank_header, "pBAV", 4);
    bank_header[4] = 7;
    bank_header[12] = 0x20;
    bank_header[13] = 0x0A;
    SsInit();
    u32 table[16];
    SsSetTableSize(table, 2, 1);
    CHECK(SsVabOpenHead(bank_header, -1) == 0);
    u8 asset[15 + sizeof(score)] = {'p', 'Q', 'E', 'S', 0, 0, 0, 1, 1, 0x80, 7, 0xA1, 0x20, 4, 2};
    memcpy(asset + 15, score, sizeof(score));
    hm_sound_sequence_register(asset, sizeof(asset));
    CHECK(SsSeqOpen((const u32 *)asset, 0) == 0);
    CHECK(SsSeqOpen((const u32 *)asset, 0) == 1);
    CHECK(SsSeqOpen((const u32 *)asset, 0) == -1);
    SsSeqSetVol(0, 30, 50);
    SsSeqStop(0);
    SsSeqClose(0);
    SsSeqClose(1);
    hm_sound_sequence_register(asset, sizeof(asset));
    CHECK(SsSeqOpen((const u32 *)asset, 0) == 0);
    SsInit();
    return 0;
}
