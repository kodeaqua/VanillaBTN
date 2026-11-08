#include "sdk_cd.h"
#include "sdk_sync.h"
#include "afile.h"
#include <libcd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

/* The archive module requests an XA stop. This fixture records that boundary
 * call; it does not claim to test the original audio subsystem. */
static unsigned audio_stops;
void cd_audio_stop(void) { ++audio_stops; }
static uint64_t clock_time;
static uint64_t now(void *context) { (void)context; return clock_time; }
static void wait_until(uint64_t deadline, void *context) { (void)context; clock_time = deadline; }
static u_long buffer[2048];

static void fixtures(void) {
    FILE *file = fopen("A_FILE.HDT", "wb");
    CHECK(file != NULL);
    for (unsigned i = 0; i < 33; ++i) {
        unsigned offset = i == 0 ? 0 : i == 1 ? 2048 : 4096;
        unsigned char bytes[4] = {(unsigned char)offset, (unsigned char)(offset >> 8), 0, 0};
        CHECK(fwrite(bytes, 1, 4, file) == 4);
    }
    CHECK(fclose(file) == 0);
    file = fopen("A_FILE.BIN", "wb");
    CHECK(file != NULL);
    for (unsigned i = 0; i < 4096; ++i) CHECK(fputc((int)(i % 251), file) != EOF);
    CHECK(fclose(file) == 0);
}

int main(void) {
    _Static_assert(sizeof(CdlLOC) == 4 && sizeof(CdlFILE) == 24, "CD SDK wire sizes");
    fixtures();
    CHECK(sdk_sync_bind(now, wait_until, NULL));
    CHECK(sdk_cd_mount("."));
    CdlFILE file;
    CHECK(!CdSearchFile(&file, "\\..\\A_FILE.BIN;1"));
    CHECK(!CdSearchFile(&file, "\\missing.bin;1"));
    CdlLOC location;
    CHECK(CdIntToPos(12345, &location) == &location);
    CHECK(CdPosToInt(&location) == 12345);
    location.second = 0x6A;
    CHECK(CdPosToInt(&location) == -1);
    CHECK(afile_init() == 0);
    memset(buffer, 0xAA, sizeof(buffer));
    CHECK(afile_read_range(0, 1, (u8 *)buffer, (u8 *)buffer) == 0);
    for (unsigned i = 0; i < 2048; ++i) CHECK(((u8 *)buffer)[i] == i % 251);
    memset(buffer, 0xAA, sizeof(buffer));
    CHECK(afile_read_async(0, 0, 2048, (u8 *)buffer, 0) == 0);
    CHECK(((u8 *)buffer)[0] == 0xAA); /* Destination changes on completion. */
    CHECK(afile_read_async(0, 0, 2048, (u8 *)buffer, 0) == 1);
    for (unsigned i = 0; i < 4096; ++i) CHECK(((u8 *)buffer)[i] == i % 251);
    memset(buffer, 0xAA, sizeof(buffer));
    CHECK(afile_read_bytes(0, 2040, 24, (u8 *)buffer, 1, 0) == NULL);
    CHECK(afile_read_bytes(0, 2040, 24, (u8 *)buffer, 1, 0) == (u8 *)buffer);
    for (unsigned i = 0; i < 24; ++i) CHECK(((u8 *)buffer)[i] == (2040 + i) % 251);
    CHECK(audio_stops == 1);
    CHECK(CdSearchFile(&file, "\\A_FILE.HDT;1") == &file);
    CHECK(CdControl(CdlSetloc, (u_char *)&file.pos, NULL));
    CHECK(CdRead(1, buffer, 0x80));
    CHECK(!CdRead(1, buffer, 0x80));
    CdReadBreak();
    CHECK(CdReadSync(1, NULL) == 0);
    CHECK(CdRead(1, buffer, 0x80));
    CHECK(CdReadSync(0, NULL) == 0);
    for (unsigned i = 132; i < 2048; ++i) CHECK(((u8 *)buffer)[i] == 0);
    CHECK(CdSearchFile(&file, "\\A_FILE.BIN;1") == &file);
    CHECK(CdControl(CdlSetloc, (u_char *)&file.pos, NULL));
    CHECK(!CdRead(3, buffer, 0x80)); /* Never read past the selected file. */
    sdk_cd_unmount();
    CHECK(CdReadSync(1, NULL) == -1);
    FILE *invalid = fopen("A_FILE.HDT", "ab");
    CHECK(invalid != NULL);
    for (unsigned i = 0; i < 4096; ++i) CHECK(fputc(0, invalid) != EOF);
    CHECK(fclose(invalid) == 0);
    CHECK(sdk_cd_mount("."));
    CHECK(afile_init() == -1); /* Reject before a sector read can overflow scratch. */
    sdk_cd_unmount();
    CHECK(remove("A_FILE.HDT") == 0 && remove("A_FILE.BIN") == 0);
    CHECK(sdk_sync_bind(NULL, NULL, NULL));
    return 0;
}
