#include "sdk_cd.h"
#include "sdk_sound.h"
#include "sdk_sync.h"
#include "afile.h"
#include "cd_audio.h"
#include "event_flags.h"
#include "farm_state.h"
#include "main.h"
#include "sound.h"
#include "sound_request.h"
#include <libcd.h>
#include <libetc.h>
#include <libsnd.h>
#include <libspu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

#define ENTRY9_OFFSET 4096u
#define ENTRY9_BYTES (4096u + 45u * 4096u + 2048u)
#define AFILE_TOTAL_BYTES (ENTRY9_OFFSET + ENTRY9_BYTES)

extern s32 sound_offsets[151];

static u8 demo_and_region_storage[sizeof(InputDemo) + 64];
u8 *region_base = demo_and_region_storage + sizeof(InputDemo);
FrameState frame_records[2];
FrameState *current_frame = frame_records;
s32 frame_index;
static u_long ot[2];
u_long *active_ot = ot;
VabStream vab_stream_copy;
u8 *main_get_region_base(void) { return region_base; }
u8 main_get_speed_mode(void) { return MAIN_SPEED_FAST_2; }
void main_set_clear_screen(s32 value) { (void)value; }
void main_set_put_env(s32 value) { (void)value; }
s32 farm_clamp_status(s8 stamina, s8 fatigue, s16 happiness, s32 unused3) {
    (void)stamina; (void)fatigue; (void)happiness; (void)unused3;
    return 1;
}
s32 item_use_coin(s8 amount, s8 reset, s32 unused, s32 *out) {
    (void)amount; (void)reset; (void)unused; (void)out;
    return 1;
}
s32 farm_adjust_stamina_fatigue(s8 maxStaminaDelta, s8 fatigueDelta) {
    (void)maxStaminaDelta; (void)fatigueDelta;
    return 1;
}
static uint64_t clock_time;
static uint64_t now(void *context) { (void)context; return clock_time; }
static void wait_until(uint64_t deadline, void *context) { (void)context; clock_time = deadline; }
static u_long buffer[2048];
static u8 sound_read_scratch[256 * 1024];

static const char *const xa_paths[4] = {
    "STR/01.XA", "STR/02.XA", "STR/03.XA", "STR/04.XA"
};

static void build_test_vab_entry9(uint8_t *entry9) {
    memset(entry9, 0, ENTRY9_BYTES);
    /* 1-program, 1-tone, 1-VAG header at offset 0 (4096-byte header region). */
    memcpy(entry9, "pBAV", 4);
    uint32_t version = 7;
    uint32_t fsize = 4096u + 32u;
    uint16_t num_progs = 1;
    uint16_t num_tones = 1;
    uint16_t num_vags = 1;
    memcpy(entry9 + 4, &version, 4);
    memcpy(entry9 + 12, &fsize, 4);
    memcpy(entry9 + 18, &num_progs, 2);
    memcpy(entry9 + 20, &num_tones, 2);
    memcpy(entry9 + 22, &num_vags, 2);
    entry9[24] = 127; /* master vol */
    entry9[25] = 64;  /* master pan */

    /* Program 0 attribute at offset 32: 1 tone, volume 127, pan 64. */
    entry9[32] = 1;
    entry9[33] = 127;
    entry9[36] = 64;

    /* Tone 0 attribute for program 0 at offset 0x820 (2080). */
    uint8_t *atr = entry9 + 0x820;
    atr[2] = 127; /* tone vol */
    atr[3] = 64;  /* tone pan */
    atr[4] = 60;  /* center note */
    atr[6] = 0;   /* min note */
    atr[7] = 127; /* max note */
    /* Use actual ADSR: fast attack, highest decay target, frozen sustain,
     * and the fastest linear release (two clocks from near-full volume). */
    atr[16] = 0xff;
    atr[18] = 0xc0;
    atr[19] = 0x1f;
    uint16_t vag_idx = 1;
    memcpy(atr + 22, &vag_idx, 2);

    /* VAG offset table at 32 + 2048 + 512 = 2592: VAG 1 is 32 bytes (4 * 8). */
    uint16_t offtbl0 = 0;
    uint16_t offtbl1 = 4;
    memcpy(entry9 + 2592, &offtbl0, 2);
    memcpy(entry9 + 2594, &offtbl1, 2);

    /* VAB body at offset 4096: two 16-byte looping ADPCM blocks with non-zero nibbles. */
    uint8_t *body = entry9 + 4096;
    body[0] = 0x00;
    body[1] = 0x04; /* loop start */
    memset(body + 2, 0x33, 14);
    body[16] = 0x00;
    body[17] = 0x03; /* loop end + repeat */
    memset(body + 18, 0x33, 14);

    /* Valid pQES sequence at sound_offsets[2] (24576) for sound_queue_seq_pair(1, 0). */
    static const uint8_t test_seq[] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1,
        0x00, 0x60,             /* ppqn = 96 */
        0x07, 0xA1, 0x20,       /* tempo = 500000 us/qn */
        0x04, 0x04,             /* time signature */
        0x00, 0x90, 60, 100,    /* delta 0: Note On ch 0, note 60, vel 100 */
        0x60, 0x80, 60, 0,      /* delta 96: Note Off ch 0, note 60 */
        0x00, 0xFF, 0x2F        /* delta 0: End of Track */
    };
    memcpy(entry9 + 24576, test_seq, sizeof(test_seq));
}

static void fixtures(void) {
    FILE *file = fopen("A_FILE.HDT", "wb");
    CHECK(file != NULL);
    for (unsigned i = 0; i < 33; ++i) {
        uint32_t offset = i == 0 ? 0u : i == 1 ? 2048u : i <= 9 ? ENTRY9_OFFSET : AFILE_TOTAL_BYTES;
        unsigned char bytes[4] = {
            (unsigned char)(offset & 0xFFu),
            (unsigned char)((offset >> 8) & 0xFFu),
            (unsigned char)((offset >> 16) & 0xFFu),
            (unsigned char)((offset >> 24) & 0xFFu),
        };
        CHECK(fwrite(bytes, 1, 4, file) == 4);
    }
    CHECK(fclose(file) == 0);
    file = fopen("A_FILE.BIN", "wb");
    CHECK(file != NULL);
    for (unsigned i = 0; i < ENTRY9_OFFSET; ++i) CHECK(fputc((int)(i % 251), file) != EOF);
    uint8_t *entry9 = (uint8_t *)malloc(ENTRY9_BYTES);
    CHECK(entry9 != NULL);
    build_test_vab_entry9(entry9);
    CHECK(fwrite(entry9, 1, ENTRY9_BYTES, file) == ENTRY9_BYTES);
    free(entry9);
    CHECK(fclose(file) == 0);
    for (unsigned i = 0; i < 4; ++i) {
        file = fopen(xa_paths[i], "wb");
        CHECK(file != NULL);
        for (unsigned sector = 0; sector < 16; sector++) {
            uint8_t xa[2336] = {0};
            xa[0] = sector == 0 ? 2 : 1;
            unsigned second_audio = i == 2 ? 11 : 10; /* file 3 has an underrun gap */
            xa[1] = sector == 2 || sector == second_audio ? 0 : 1;
            xa[2] = 0x64;
            xa[3] = 1; /* stereo, 37.8 kHz, 4-bit */
            memcpy(xa + 4, xa, 4);
            for (unsigned group = 0; group < 18; group++) {
                uint8_t *sg = xa + 8 + group * 128;
                memset(sg, 4, 16);
                memset(sg + 16, sector < 2 ? 0x77 : 0xe1, 112);
            }
            CHECK(fwrite(xa, 1, sizeof(xa), file) == sizeof(xa));
        }
        CHECK(fclose(file) == 0);
    }
}

static void test_xa_sdk_stream(void) {
    SpuCommonAttr common = {0};
    common.mask = SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDMIX;
    common.mvol.left = common.mvol.right = 32767;
    common.cd.mix = 1;
    SpuSetCommonAttr(&common);
    SsSetSerialAttr(0, SS_MIX, SS_ON);
    SsSetSerialVol(0, 127, 64);
    uint8_t mode = 0xc8, filter[2] = {1, 0};
    CHECK(CdControl(CdlSetmode, &mode, NULL));
    CHECK(CdControl(CdlSetfilter, filter, NULL));
    CdlFILE xa_file;
    CHECK(CdSearchFile(&xa_file, "\\STR\\01.XA;1"));
    CHECK(xa_file.size == 16 * 2048);
    CdlFILE next_xa;
    CHECK(CdSearchFile(&next_xa, "\\STR\\02.XA;1"));
    CHECK(CdPosToInt(&next_xa.pos) - CdPosToInt(&xa_file.pos) == 16);
    CdlLOC start = xa_file.pos;
    CHECK(CdControl(CdlSetloc, (uint8_t *)&start, NULL));
    CHECK(CdControl(CdlReadS, NULL, NULL));
    int16_t pcm[2940 * 2];
    sdk_sound_mix(pcm, 735);
    /* At double speed each sector arrives after 294 output frames. The
     * selected channel is in sectors 2 and 10 (one 37.8 kHz stereo block
     * every 8 sectors). It cannot sound during the first 735 frames. */
    for (int i = 0; i < 1470; i++) CHECK(pcm[i] == 0);
    const int raw_left[10] = {-1,-1,-3,-2,-3,-2,-4,-3,-2,-10};
    const int raw_right[10] = {0,-1,-1,-3,-3,-4,-2,-4,-6,5};
    uint8_t location[8];
    CHECK(CdControl(CdlGetlocP, NULL, location));
    CHECK(CdPosToInt((CdlLOC *)(location + 5)) == CdPosToInt(&start) + 2);
    CHECK(CdControl(CdlPause, NULL, NULL));
    sdk_sound_mix(pcm, 735);
    for (int i = 0; i < 1470; i++) CHECK(pcm[i] == 0);
    CHECK(sdk_cd_xa_status().sector == (unsigned long)CdPosToInt(&start) + 2);
    CHECK(CdControl(CdlReadS, NULL, NULL)); /* Resume buffered audio. */
    sdk_sound_mix(pcm, 735);
    for (int i = 0; i < 146 * 2; i++) CHECK(pcm[i] == 0);
    for (int i = 0; i < 10; i++) {
        int l = ((raw_left[i] * (127 * 258)) >> 15) * 32767 >> 15;
        int r = ((raw_right[i] * (64 * 258)) >> 15) * 32767 >> 15;
        CHECK(pcm[(146 + i) * 2] == l && pcm[(146 + i) * 2 + 1] == r);
    }
    CHECK(sdk_cd_xa_status().sector == (unsigned long)CdPosToInt(&start) + 5);
    CHECK(CdControl(CdlPause, NULL, NULL));
    CdlLOC offset;
    CHECK(CdIntToPos(CdPosToInt(&start) + 3, &offset));
    CHECK(CdControl(CdlSetloc, (uint8_t *)&offset, NULL));
    CHECK(sdk_cd_xa_status().sector == (unsigned long)CdPosToInt(&start) + 5);
    CHECK(CdControl(CdlReadS, NULL, NULL));
    sdk_sound_mix(pcm, 2351);
    for (int i = 0; i < 2351 * 2; i++) CHECK(pcm[i] == 0);
    sdk_sound_mix(pcm, 10);
    CHECK(pcm[0] == -1 && pcm[1] == 0);

    CHECK(CdControl(CdlReadS, (uint8_t *)&start, NULL));
    SsSetSerialAttr(0, SS_MIX, SS_OFF);
    sdk_sound_mix(pcm, 892);
    for (int i = 0; i < 1784; i++) CHECK(pcm[i] == 0);
    SsSetSerialAttr(0, SS_MIX, SS_ON);
    SsSetSerialVol(0, 0, 0);
    sdk_sound_mix(pcm, 10);
    for (int i = 0; i < 20; i++) CHECK(pcm[i] == 0);
    CHECK(CdControl(CdlStandby, NULL, NULL));
    CHECK(!sdk_cd_xa_status().streaming);
    SsSetSerialVol(0, 127, 64);
    CHECK(CdControl(CdlReadS, (uint8_t *)&start, NULL));
    SsSetMono(); /* libsnd mono mode affects voices, not the CD input. */
    sdk_sound_mix(pcm, 892);
    for (int i = 0; i < 10; i++) {
        int l = ((raw_left[i] * (127 * 258)) >> 15) * 32767 >> 15;
        int r = ((raw_right[i] * (64 * 258)) >> 15) * 32767 >> 15;
        CHECK(pcm[(881 + i) * 2] == l && pcm[(881 + i) * 2 + 1] == r);
    }
    SsSetStereo();
    /* Normal data reads cancel the XA drive stream. */
    CdlFILE archive;
    CHECK(CdSearchFile(&archive, "\\A_FILE.HDT;1"));
    CHECK(CdControl(CdlSetloc, (uint8_t *)&archive.pos, NULL));
    CHECK(CdRead(1, buffer, 0x80));
    CHECK(CdReadSync(0, NULL) == 0 && !sdk_cd_xa_status().streaming);
    sdk_sound_mix(pcm, 10);
    for (int i = 0; i < 20; i++) CHECK(pcm[i] == 0);
    /* A late interleaved sector creates actual silence instead of allowing
     * the decoder to read ahead. FIR history survives this underflow. */
    CHECK(CdSearchFile(&xa_file, "\\STR\\03.XA;1"));
    CHECK(CdControl(CdlSetmode, &mode, NULL));
    CHECK(CdControl(CdlReadS, (uint8_t *)&xa_file.pos, NULL));
    sdk_sound_mix(pcm, 2940);
    sdk_sound_mix(pcm, 293);
    sdk_sound_mix(pcm, 294);
    for (int i = 0; i < 588; i++) CHECK(pcm[i] == 0);
    sdk_sound_mix(pcm, 1);
    int l = ((221 * (127 * 258)) >> 15) * 32767 >> 15;
    int r = ((-476 * (64 * 258)) >> 15) * 32767 >> 15;
    CHECK(pcm[0] == l && pcm[1] == r);
    unsigned long position_before = sdk_cd_xa_status().sector;
    for (int i = 0; i < 50; i++) CHECK(CdControl(CdlGetlocP, NULL, location));
    CHECK(sdk_cd_xa_status().sector == position_before);
    CHECK(CdControl(CdlSetloc, (uint8_t *)&start, NULL));
    CHECK(sdk_cd_xa_status().sector == position_before); /* target only */
    sdk_sound_mix(pcm, 1);
    CHECK(pcm[0] != 0 && pcm[1] != 0); /* Setloc alone does not pause audio. */
    CHECK(CdControl(CdlReadS, NULL, NULL));
    sdk_sound_mix(pcm, 1);
    CHECK(pcm[0] == 0 && pcm[1] == 0);
    CHECK(CdControl(CdlStandby, NULL, NULL));
    sdk_sound_reset();
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
    test_xa_sdk_stream();
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

    /* Exercise original sound.c and sound_request.c initialization, fader, VAB loader, and voices. */
    sound_fader_init();
    sound_init();
    sound_init_stereo_xa();
    sound_update();
    SdkSoundStatus st = sdk_sound_status();
    CHECK(st.initialized && st.running && st.stereo && st.cd_mix);
    CHECK(st.tick_mode == 1 && st.reserved_voices == 0x16);
    CHECK(sdk_sound_spu()->master_vol_left == 0x3FFF && sdk_sound_spu()->cd_vol_left == 0);
    CHECK(sound_fader_ready() == 1 && sound_fader_mode() == SOUND_FADER_IDLE);

    sound_set_mono();
    CHECK(!sdk_sound_status().stereo);
    sound_set_stereo();
    CHECK(sdk_sound_status().stereo);

    /* Master volume fade out and fade in over 4 ticks. */
    sound_fade_out(4);
    CHECK(sound_fader_mode() == SOUND_FADER_FADE_OUT);
    for (unsigned i = 0; i < 4; ++i) sound_load_tick();
    CHECK(sound_fader_mode() == SOUND_FADER_IDLE && sdk_sound_status().master_vol_left == 0);
    sound_fade_in(4);
    CHECK(sound_fader_mode() == SOUND_FADER_FADE_IN);
    for (unsigned i = 0; i < 4; ++i) sound_load_tick();
    CHECK(sound_fader_mode() == SOUND_FADER_FADE_IN && sdk_sound_status().master_vol_left == 127);
    sound_load_tick(); /* > SOUND_FADER_FULL clamps and returns to SOUND_FADER_IDLE. */
    CHECK(sound_fader_mode() == SOUND_FADER_IDLE && sdk_sound_status().master_vol_left == 127);

    /* Stream VAB 0 from A_FILE entry 9 through sound_request(SOUND_REQUEST_RESET) + sound_load_tick(). */
    sound_set_seq_arg(0, sound_read_scratch);
    sound_request(SOUND_REQUEST_RESET, 0);
    CHECK(sound_fader_ready() == 0 && sound_current_entry() == 0);
    for (unsigned step = 0; step < 16 && !sound_fader_ready(); ++step) {
        sound_load_tick();
    }
    CHECK(sound_fader_ready() == 1 && sound_current_entry() == -1);
    CHECK(sdk_sound_status().open_vab_count == 1 && sdk_sound_spu()->vabs[0].loaded == 1);

    /* Invalid PPQN must fail without consuming an SDK sequence handle. */
    uint8_t invalid_seq[20] = {'p','Q','E','S',0,0,0,1,0,0,0x0F,0x42,0x40,4,2,
                              0,0xFF,0x2F,0,0};
    sdk_sound_sequence_source(invalid_seq, sizeof(invalid_seq));
    CHECK(SsSeqOpen((u_long *)invalid_seq, 0) == -1);
    CHECK(sdk_sound_status().open_seq_count == 0);
    /* A short buffer cannot be scanned past its declared extent, and a valid
     * sequence needs a fresh view for each SDK call. */
    uint8_t short_seq[15] = {'p','Q','E','S',0,0,0,1,0,100,0x0F,0x42,0x40,4,2};
    sdk_sound_sequence_source(short_seq, sizeof(short_seq));
    CHECK(SsSeqOpen((u_long *)short_seq, 0) == -1);
    uint8_t sequence[] = {'p','Q','E','S',0,0,0,1,255,255,0x07,0xA1,0x20,4,2,
                          0,0x90,60,127, 0xFF,0x2F,0xFF,0x2F};
    CHECK(SsSeqOpen((u_long *)sequence, 0) == -1);
    sdk_sound_sequence_source(sequence, sizeof(sequence));
    short handle = SsSeqOpen((u_long *)sequence, 0);
    CHECK(handle >= 0);
    SsSeqPlay(handle, SSPLAY_PLAY, SSPLAY_INFINITY);
    int16_t fast_pcm[4];
    sdk_sound_mix(fast_pcm, 2);
    CHECK(sdk_sound_spu()->voices[0].active);
    SsSeqClose(handle);
    CHECK(sdk_sound_status().open_seq_count == 0);
    /* Closing requests key-off; let the real envelope release before reusing
     * voice zero in the following independent SDK effect fixture. */
    sdk_sound_mix(fast_pcm, 2);
    CHECK(!sdk_sound_spu()->voices[0].active);

    /* Trigger SOUND_MENU_CONFIRM (VAB 0, prog 0, tone 0, note 60) and verify SPU voice + PCM mix. */
    VSync(0); /* Advance VSync counter so voice_stamps[0] != now. */
    VSync(0);
    s32 voice = sound_play(SOUND_MENU_CONFIRM);
    CHECK(voice == 0 && sdk_sound_spu()->voices[0].active == 1);
    int16_t pcm[64];
    memset(pcm, 0, sizeof(pcm));
    sdk_sound_mix(pcm, 32);
    bool non_zero_pcm = false;
    for (unsigned i = 0; i < 64; ++i) {
        if (pcm[i] != 0) non_zero_pcm = true;
    }
    CHECK(non_zero_pcm);
    sound_voice_set_volume((s16)voice, 30, 90);
    CHECK(sdk_sound_spu()->voices[0].vol_left < sdk_sound_spu()->voices[0].vol_right);
    sound_release_voice(voice);
    CHECK(sdk_sound_spu()->voices[0].active == 1);
    CHECK(sdk_sound_spu()->voices[0].env_phase == PS1_ADSR_RELEASE);
    sdk_sound_mix(pcm, 2);
    CHECK(sdk_sound_spu()->voices[0].active == 0);

    /* Load and play the pQES sequence at offset 24576 via sound_queue_seq_pair(4, 0) + sound_update. */
    sound_offsets[8] = 24576;
    sound_offsets[10] = 28672;
    CHECK(sound_queue_seq_pair(4, 0) == 0);
    for (unsigned step = 0; step < 8 && sound_current_entry() != -1; ++step) {
        sound_load_step();
    }
    CHECK(sound_current_entry() == -1);
    sound_update();
    CHECK(sdk_sound_status().open_seq_count == 1);
    CHECK(sound_seq_start_reverb(0, 1, 0, 100) == 0);
    st = sdk_sound_status();
    CHECK(st.playing_seq_count == 1 && st.reverb_enabled && st.reverb_type == 5);
    CHECK(st.reverb_depth_left == 40 && st.reverb_depth_right == 40);
    memset(pcm, 0, sizeof(pcm));
    sdk_sound_mix(pcm, 32);
    non_zero_pcm = false;
    for (unsigned i = 0; i < 64; ++i) {
        if (pcm[i] != 0) non_zero_pcm = true;
    }
    CHECK(non_zero_pcm);
    CHECK(sound_seq_volume_if_idle(0, 0, 0, 80, 0) == 0);
    CHECK(sound_seq_stop_first(0) == 0);
    CHECK(sdk_sound_status().playing_seq_count == 0);
    CHECK(sound_release_from(0, 0) == 0);
    CHECK(sdk_sound_status().open_vab_count == 0 && sdk_sound_status().open_seq_count == 0);

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
    CHECK(!CdRead(96, buffer, 0x80)); /* Never read past the selected file (95 sectors). */
    sdk_cd_unmount();
    CHECK(CdReadSync(1, NULL) == -1);
    FILE *invalid = fopen("A_FILE.HDT", "ab");
    CHECK(invalid != NULL);
    for (unsigned i = 0; i < 4096; ++i) CHECK(fputc(0, invalid) != EOF);
    CHECK(fclose(invalid) == 0);
    CHECK(sdk_cd_mount("."));
    CHECK(afile_init() == -1); /* Reject before a sector read can overflow scratch. */
    sdk_cd_unmount();
    sdk_sound_reset();
    for (unsigned i = 0; i < 4; ++i) CHECK(remove(xa_paths[i]) == 0);
    CHECK(remove("A_FILE.HDT") == 0 && remove("A_FILE.BIN") == 0);
    CHECK(sdk_sync_bind(NULL, NULL, NULL));
    return 0;
}
