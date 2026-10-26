#include <libcd.h>

#include <stdio.h>

#define CHECK(condition)                                                                            \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    const struct {
        s32 lba;
        u8 minute, second, frame;
    } vectors[] = {
        {-150, 0x00, 0x00, 0x00}, {-1, 0x00, 0x01, 0x74},
        {0, 0x00, 0x02, 0x00}, {74, 0x00, 0x02, 0x74}, {75, 0x00, 0x03, 0x00},
        {4349, 0x00, 0x59, 0x74}, {4350, 0x01, 0x00, 0x00},
        {12345, 0x02, 0x46, 0x45}, {449849, 0x99, 0x59, 0x74},
    };
    CdlLOC position = {0, 0, 0, 0xA5};
    for (u32 i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        CHECK(CdIntToPos(vectors[i].lba, &position) == &position);
        CHECK(position.minute == vectors[i].minute);
        CHECK(position.second == vectors[i].second);
        CHECK(position.sector == vectors[i].frame);
        CHECK(position.track == 0xA5);
        CHECK(CdPosToInt(&position) == vectors[i].lba);
    }
    /* Exhaust the valid two-digit-minute BCD range, not malformed positions. */
    for (s32 lba = -150; lba <= 449849; ++lba) {
        CdIntToPos(lba, &position);
        CHECK(CdPosToInt(&position) == lba);
        CHECK(position.track == 0xA5);
    }
    /* Match cd_audio.c's GetlocP subview. The final byte ends at the ASan redzone. */
    const u8 response[8] = {0, 0, 0, 0, 0, 0x12, 0x34, 0x56};
    CHECK(CdPosToInt((const CdlLOC *)&response[5]) == (12 * 60 + 34) * 75 + 56 - 150);
    return 0;
}
