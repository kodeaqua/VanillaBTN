#include "psx/cd_runtime.h"
#include <libcd.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int failed;
static int hm_read(void *context, u64 offset, void *destination, size_t size) {
    (void)context;
    if (failed || offset % 2352 || size != 2352)
        return 0;
    u8 *raw = destination;
    memset(raw, 0, size);
    memset(raw + 1, 255, 10);
    raw[15] = 2;
    memset(raw + 24, (u8)(offset / 2352), 2048);
    return 1;
}

int main(void) {
    hm_disc disc = {.stream = {NULL, 100 * 2352, hm_read}, .sectors = 100, .mounted = 1};
    hm_cd_attach(&disc);
    CHECK(CdInit() == 1);
    CdlLOC position = {0};
    CdIntToPos(5, &position);
    CHECK(CdControl(CdlSetloc, (const u8 *)&position, NULL) == 1);
    CHECK(CdLastCom() == CdlSetloc);
    u8 result[8] = {0};
    CHECK(CdSync(1, result) == 2 && result[0] == 2);
    u32 buffer[6 * 2048 / 4];
    memset(buffer, 0, sizeof(buffer));
    CHECK(CdRead(6, buffer, 0x80) == 1);
    CHECK(CdReadSync(1, NULL) == 6);
    CHECK(CdReadSync(1, NULL) == 6);
    hm_cd_tick();
    CHECK(CdReadSync(1, NULL) == 4);
    hm_cd_tick();
    CHECK(CdReadSync(1, NULL) == 1);
    hm_cd_tick();
    CHECK(CdReadSync(1, NULL) == 0);
    const u8 *bytes = (const u8 *)buffer;
    for (u32 i = 0; i < 6 * 2048; ++i)
        CHECK(bytes[i] == 5 + i / 2048);
    CHECK(CdRead(6, buffer, 0) == 1);
    for (s32 remaining = 5; remaining >= 3; --remaining) {
        hm_cd_tick();
        CHECK(CdReadSync(1, NULL) == remaining);
    }
    hm_cd_tick();
    CHECK(CdReadSync(1, NULL) == 1);
    CHECK(CdReadSync(0, result) == 0 && result[0] == 2);
    CHECK(CdRead(6, buffer, 0x80) == 1);
    CHECK(CdReadBreak() == 1 && CdReadSync(1, NULL) == 0);
    CHECK(CdRead(6, buffer, 0x80) == 1);
    CHECK(CdControl(9, NULL, NULL) == 1 && CdReadSync(1, NULL) == 0);
    CHECK(CdRead(6, buffer, 0x80) == 1);
    failed = 1;
    CHECK(CdReadSync(0, result) == -1 && result[0] == 3);
    CHECK(CdSync(0, NULL) == 5);
    failed = 0;
    CHECK(CdRead(6, buffer, 0x80) == 1 && CdReadSync(0, NULL) == 0);
    CHECK(CdFlush() == 1);
    hm_cd_detach();
    return 0;
}
