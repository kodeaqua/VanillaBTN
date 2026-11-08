#include "sdk_cd.h"
#include "sdk_sound.h"
#include "sdk_sync.h"
#include "afile.h"
#include "cd_audio.h"
#include "event_flags.h"
#include "farm_state.h"
#include "main.h"
#include <libcd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

FarmState game_state;
u8 main_get_speed_mode(void) { return MAIN_SPEED_FAST_2; }
static uint64_t clock_time;
static uint64_t now(void *context) { (void)context; return clock_time; }
static void wait_until(uint64_t deadline, void *context) { (void)context; clock_time = deadline; }
static u_long buffer[2048];

static const char *const xa_paths[4] = {
    "STR/01.XA", "STR/02.XA", "STR/03.XA", "STR/04.XA"
};

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
    for (unsigned i = 0; i < 4; ++i) {
        file = fopen(xa_paths[i], "wb");
        CHECK(file != NULL);
        for (unsigned j = 0; j < 2336; ++j) CHECK(fputc((int)((i + j) & 0xFF), file) != EOF);
        CHECK(fclose(file) == 0);
    }
}

int main(void) {
    _Static_assert(sizeof(CdlLOC) == 4 && sizeof(CdlFILE) == 24, "CD SDK wire sizes");
    fixtures();
    sdk_sound_reset();
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
    xa_search_files();
    memset(buffer, 0xAA, sizeof(buffer));
    CHECK(afile_read_range(0, 1, (u8 *)buffer, (u8 *)buffer) == 0);
    for (unsigned i = 0; i < 2048; ++i) CHECK(((u8 *)buffer)[i] == i % 251);
    memset(buffer, 0xAA, sizeof(buffer));
    CHECK(afile_read_async(0, 0, 2048, (u8 *)buffer, 0) == 0);
    CHECK(((u8 *)buffer)[0] == 0xAA); /* Destination changes on completion. */
    CHECK(afile_read_async(0, 0, 2048, (u8 *)buffer, 0) == 1);
    for (unsigned i = 0; i < 4096; ++i) CHECK(((u8 *)buffer)[i] == i % 251);

    /* Exercise original cd_audio.c and event_flags.c guards and fade loop. */
    memset(&game_state, 0, sizeof(game_state));
    game_state.system.menuState = 1;
    cd_audio_play_track(CD_TRACK_COW_PRAISE);
    CHECK(cd_audio_get_state() == CD_AUDIO_IDLE);
    game_state.system.menuState = MENU_STATE_IDLE;
    event_flag_set(EVENT_FLAG_REQUEST_BUSY, 1);
    CHECK(event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1);
    cd_audio_play_track(CD_TRACK_COW_PRAISE);
    CHECK(cd_audio_get_state() == CD_AUDIO_IDLE);
    event_flag_set(EVENT_FLAG_REQUEST_BUSY, 0);
    CHECK(event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0);

    /* Track 7 ({0x003B, file 1, channel 0}): volume 59 fades by 2 per frame. */
    cd_audio_play_track(7);
    CHECK(cd_audio_get_state() == CD_AUDIO_REQUESTED);
    CHECK(!sdk_cd_xa_status().streaming && sdk_cd_xa_status().mode == 0xC8);
    CHECK(sdk_cd_xa_status().file == 1 && sdk_cd_xa_status().channel == 0);
    for (unsigned i = 0; i < 3; ++i) {
        cd_audio_fade_tick();
        CHECK(cd_audio_get_state() == CD_AUDIO_REQUESTED && !sdk_cd_xa_status().streaming);
    }
    cd_audio_fade_tick(); /* CD_AUDIO_START issues CdlReadS. */
    CHECK(sdk_cd_xa_status().streaming && cd_audio_get_state() == CD_AUDIO_REQUESTED);
    cd_audio_fade_tick(); /* First CD_AUDIO_FADE tick: vol 59 -> 57, serial vol 45. */
    CHECK(cd_audio_get_state() == CD_AUDIO_FADING);
    CHECK(sdk_sound_serial_vol(0).left == 45 && sdk_sound_serial_vol(0).right == 45);
    CHECK(CdLastCom() == CdlGetlocP);

    /* Archive streamed reads stop active CD audio through original cd_audio_stop. */
    memset(buffer, 0xAA, sizeof(buffer));
    CHECK(afile_read_bytes(0, 2040, 24, (u8 *)buffer, 1, 0) == NULL);
    CHECK(cd_audio_get_state() == CD_AUDIO_IDLE && !sdk_cd_xa_status().streaming);
    CHECK(sdk_sound_serial_vol(0).left == 0 && sdk_sound_serial_vol(0).right == 0);
    CHECK(afile_read_bytes(0, 2040, 24, (u8 *)buffer, 1, 0) == (u8 *)buffer);
    for (unsigned i = 0; i < 24; ++i) CHECK(((u8 *)buffer)[i] == (2040 + i) % 251);

    /* Full fade-out of track 7 (volume 59 -> 0 at step 2) down to CdlStandby. */
    cd_audio_play_track(7);
    for (unsigned i = 0; i < 4; ++i) cd_audio_fade_tick();
    for (unsigned i = 0; i < 14; ++i) cd_audio_fade_tick(); /* 59 -> 31 (< 33). */
    CHECK(cd_audio_get_state() == CD_AUDIO_FADING);
    CHECK(sdk_sound_serial_vol(0).left == 31 * 4 && sdk_sound_serial_vol(0).right == 31 * 4);
    for (unsigned i = 0; i < 16; ++i) cd_audio_fade_tick(); /* 31 -> 0. */
    CHECK(sdk_sound_serial_vol(0).left == 0 && sdk_sound_serial_vol(0).right == 0);
    cd_audio_fade_tick(); /* Volume <= 0 triggers cd_audio_stop and CdlStandby. */
    CHECK(cd_audio_get_state() == CD_AUDIO_IDLE && !sdk_cd_xa_status().streaming);

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
    for (unsigned i = 0; i < 4; ++i) CHECK(remove(xa_paths[i]) == 0);
    CHECK(remove("A_FILE.HDT") == 0 && remove("A_FILE.BIN") == 0);
    CHECK(sdk_sync_bind(NULL, NULL, NULL));
    return 0;
}
