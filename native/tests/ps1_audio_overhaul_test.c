/* ps1_audio_overhaul_test.c — test suite for PS1 audio subsystem enhancements:
 * - 64-voice polyphony expansion (eliminating voice stealing)
 * - 64-sample micro-fade pop suppression
 * - Seamless 0ms gapless CD-XA looping
 * - VAG loop boundary crossfade smoothing
 * - 32-bit floating-point audio mixing with soft limiter
 */

#include "ps1_spu.h"
#include "ps1_xa.h"
#include "sdk_sound.h"
#include "sdk_cd.h"
#include <libsnd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <crtdbg.h>
#endif

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

/* Helper: synthetic 16-byte ADPCM block */
static void make_test_adpcm_block(uint8_t *block, uint8_t filter, uint8_t shift,
                                  uint8_t flags, int8_t nibble_val) {
    memset(block, 0, 16);
    block[0] = (uint8_t)((filter << 4) | (shift & 0x0F));
    block[1] = flags;
    uint8_t packed = (uint8_t)((nibble_val & 0x0F) | ((nibble_val & 0x0F) << 4));
    for (int i = 2; i < 16; i++) {
        block[i] = packed;
    }
}

static void setup_test_vab(void) {
    static uint8_t header[4096];
    memset(header, 0, sizeof(header));
    header[0] = 0x70; header[1] = 0x42; header[2] = 0x41; header[3] = 0x56; /* pBAV */
    uint16_t num_progs = 1, num_tones = 1, num_vags = 1;
    memcpy(header + 18, &num_progs, 2);
    memcpy(header + 20, &num_tones, 2);
    memcpy(header + 22, &num_vags, 2);
    header[32] = 1; /* Prog 0 has 1 tone */
    uint32_t vag_atr_off = 32 + 2048;
    header[vag_atr_off + 2] = 127; /* vol */
    header[vag_atr_off + 4] = 60;  /* center note */
    uint16_t vag_idx = 1;
    memcpy(header + vag_atr_off + 22, &vag_idx, 2);
    uint32_t offtbl_off = 32 + 2048 + 16 * 32;
    uint16_t vag_len_div8 = 4;
    header[offtbl_off] = 2;
    memcpy(header + offtbl_off + 2, &vag_len_div8, 2);

    short slot = SsVabOpenHead(header, 0);
    TEST_ASSERT(slot == 0);
}

/* 1. Polyphony Expansion Test */
static void test_polyphony_expansion(void) {
    Ps1Spu spu;
    ps1_spu_init(&spu);
    TEST_ASSERT(spu.num_voices == 24);
    TEST_ASSERT(ps1_spu_get_polyphony_expansion(&spu) == 0);

    ps1_spu_set_polyphony_expansion(&spu, 1);
    TEST_ASSERT(spu.num_voices == 64);
    TEST_ASSERT(ps1_spu_get_polyphony_expansion(&spu) == 1);

    ps1_spu_set_polyphony_expansion(&spu, 0);
    TEST_ASSERT(spu.num_voices == 24);
    TEST_ASSERT(ps1_spu_get_polyphony_expansion(&spu) == 0);

    /* Test via sdk_sound */
    sdk_sound_reset();
    TEST_ASSERT(!sdk_sound_get_polyphony_expansion());
    setup_test_vab();

    /* In authentic mode: fill 24 voices */
    for (int v = 0; v < 24; v++) {
        short allocated = SsUtKeyOnV(v, 0, 0, 0, 60, 0, 64, 64);
        TEST_ASSERT(allocated == v);
    }
    /* Voice 24 must be rejected when polyphony is not expanded */
    TEST_ASSERT(SsUtKeyOnV(24, 0, 0, 0, 60, 0, 64, 64) == -1);
    /* SsUtKeyOn must fail because all 24 are occupied */
    TEST_ASSERT(SsUtKeyOn(0, 0, 0, 60, 0, 64, 64) == -1);

    /* Now enable polyphony expansion */
    sdk_sound_set_polyphony_expansion(true);
    TEST_ASSERT(sdk_sound_get_polyphony_expansion());

    /* SsUtKeyOn should now succeed using channels 24..63! */
    short sfx_voice = SsUtKeyOn(0, 0, 0, 60, 0, 64, 64);
    TEST_ASSERT(sfx_voice >= 24 && sfx_voice < 64);

    /* Can explicitly key on voice 63 */
    TEST_ASSERT(SsUtKeyOnV(63, 0, 0, 0, 60, 0, 64, 64) == 63);

    /* Mix audio to ensure all voices process safely */
    int16_t out[128 * 2];
    sdk_sound_mix(out, 128);

    /* Clean up */
    sdk_sound_set_polyphony_expansion(false);
    sdk_sound_reset();
    printf("test_polyphony_expansion passed.\n");
}

/* 2. Micro-fade Pop Suppression Test */
static void test_pop_suppression(void) {
    Ps1Spu spu;
    ps1_spu_init(&spu);
    TEST_ASSERT(ps1_spu_get_pop_suppression(&spu) == 0);

    /* Synthetic looping block with high DC offset */
    uint8_t block[32];
    make_test_adpcm_block(block, 0, 4, PS1_ADPCM_FLAG_LOOP_START, 7);
    make_test_adpcm_block(block + 16, 0, 4, PS1_ADPCM_FLAG_LOOP_END | PS1_ADPCM_FLAG_LOOP_REPEAT, 7);

    /* Case A: Authentic mode (pop suppression OFF) */
    ps1_spu_set_pop_suppression(&spu, 0);
    ps1_spu_key_on_raw(&spu, 0, block, 32, 0x10000, 0x7FFF, 0x7FFF);
    int16_t out[128 * 2];
    ps1_spu_mix(&spu, out, 10, 0);
    int16_t active_sample = out[9 * 2];
    TEST_ASSERT(abs(active_sample) > 100);

    /* Abrupt key-off all */
    ps1_spu_key_off_all(&spu);
    /* In authentic mode, voice is immediately deactivated */
    TEST_ASSERT(!ps1_spu_voice_is_active(&spu, 0));
    ps1_spu_mix(&spu, out, 10, 0);
    /* Output dropped immediately to 0 (hard DC click) */
    TEST_ASSERT(out[0] == 0 && out[1] == 0);

    /* Case B: Modern mode (pop suppression ON) */
    ps1_spu_set_pop_suppression(&spu, 1);
    TEST_ASSERT(ps1_spu_get_pop_suppression(&spu) == 1);
    ps1_spu_key_on_raw(&spu, 0, block, 32, 0x10000, 0x7FFF, 0x7FFF);
    ps1_spu_mix(&spu, out, 10, 0);
    active_sample = out[9 * 2];
    TEST_ASSERT(abs(active_sample) > 100);

    /* Key-off all with pop suppression */
    ps1_spu_key_off_all(&spu);
    /* Voice is STILL active, smoothly fading down */
    TEST_ASSERT(ps1_spu_voice_is_active(&spu, 0));
    TEST_ASSERT(spu.voices[0].fade_samples_remaining == 64);

    /* Mix next sample: must NOT be 0, but smoothly attenuated */
    ps1_spu_mix(&spu, out, 1, 0);
    TEST_ASSERT(out[0] != 0);
    TEST_ASSERT(abs(out[0]) <= abs(active_sample));

    /* After 64 samples, voice completes fadeout and turns off */
    ps1_spu_mix(&spu, out, 70, 0);
    TEST_ASSERT(!ps1_spu_voice_is_active(&spu, 0));
    TEST_ASSERT(out[69 * 2] == 0);

    printf("test_pop_suppression passed.\n");
}

/* Synthetic CD-XA Reader for testing */
typedef struct {
    uint8_t sectors[3][PS1_XA_SECTOR_SIZE];
    uint32_t total_sectors;
} TestXaStream;

static int test_xa_reader(void *context, uint32_t index, uint8_t *sector) {
    TestXaStream *stream = (TestXaStream *)context;
    if (index >= stream->total_sectors) return 0; /* EOF */
    memcpy(sector, stream->sectors[index], PS1_XA_SECTOR_SIZE);
    return 1;
}

/* 3. Seamless CD-XA Looping Test */
static void test_seamless_cd_loop(void) {
    TestXaStream stream;
    memset(&stream, 0, sizeof(stream));
    stream.total_sectors = 2;

    /* Build 2 valid XA Form 2 audio sectors */
    for (int s = 0; s < 2; s++) {
        uint8_t *sec = stream.sectors[s];
        sec[0] = 1;     /* File number 1 */
        sec[1] = 0;     /* Channel 0 */
        sec[2] = 0x64;  /* Submode: audio, Form 2 */
        sec[3] = 0x01;  /* Coding: stereo, 37.8kHz */
        /* Sound groups: 18 groups */
        for (int g = 0; g < PS1_XA_SOUND_GROUPS; g++) {
            uint8_t *grp = sec + 16 + g * 128;
            grp[0] = 0x00; /* filter 0, shift 0 */
            grp[1] = 0x00;
            grp[2] = 0x00;
            grp[3] = 0x00;
            for (int b = 4; b < 128; b++) {
                grp[b] = 0x33; /* Constant non-zero samples */
            }
        }
    }

    /* Case A: Authentic mode (seamless loop OFF) */
    Ps1XaPlayer player;
    ps1_xa_init(&player);
    ps1_xa_set_seamless_loop(&player, 0);
    TEST_ASSERT(ps1_xa_get_seamless_loop(&player) == 0);

    ps1_xa_play_source(&player, test_xa_reader, &stream, 0, 2, 1, 0);
    TEST_ASSERT(ps1_xa_get_state(&player) == PS1_XA_PLAYING);

    int16_t out[1024 * 2];
    /* Mix enough samples to exhaust 2 sectors (each sector is ~4032 samples at 44.1kHz) */
    for (int i = 0; i < 20; i++) {
        ps1_xa_mix(&player, out, 1024, 0x7FFF, 0x7FFF, 0);
    }
    /* At EOF, playback must stop and enter IDLE state */
    TEST_ASSERT(ps1_xa_get_state(&player) == PS1_XA_IDLE);

    /* Case B: Modern mode (seamless loop ON) */
    ps1_xa_init(&player);
    ps1_xa_play_source(&player, test_xa_reader, &stream, 0, 2, 1, 0);
    ps1_xa_set_seamless_loop(&player, 1);
    TEST_ASSERT(ps1_xa_get_seamless_loop(&player) == 1);
    TEST_ASSERT(ps1_xa_get_state(&player) == PS1_XA_PLAYING);

    /* Mix same duration (20 * 1024 frames) */
    for (int i = 0; i < 20; i++) {
        ps1_xa_mix(&player, out, 1024, 0x7FFF, 0x7FFF, 0);
    }
    /* With seamless loop enabled, state remains PLAYING without stopping! */
    TEST_ASSERT(ps1_xa_get_state(&player) == PS1_XA_PLAYING);

    /* Also verify sdk_cd wrapper */
    sdk_cd_set_seamless_loop(true);
    TEST_ASSERT(sdk_cd_get_seamless_loop());
    sdk_cd_set_seamless_loop(false);
    TEST_ASSERT(!sdk_cd_get_seamless_loop());

    printf("test_seamless_cd_loop passed.\n");
}

/* 4. VAG Loop Crossfade Test */
static void test_vag_loop_crossfade(void) {
    Ps1Spu spu;
    ps1_spu_init(&spu);
    TEST_ASSERT(ps1_spu_get_vag_crossfade(&spu) == 0);

    /* Synthetic 1-block looped ADPCM with abrupt step between end and start */
    uint8_t block[16];
    make_test_adpcm_block(block, 0, 4,
        PS1_ADPCM_FLAG_LOOP_START | PS1_ADPCM_FLAG_LOOP_END | PS1_ADPCM_FLAG_LOOP_REPEAT, 5);

    /* Enable VAG loop crossfade */
    ps1_spu_set_vag_crossfade(&spu, 1);
    TEST_ASSERT(ps1_spu_get_vag_crossfade(&spu) == 1);

    ps1_spu_key_on_raw(&spu, 0, block, 16, 0x10000, 0x7FFF, 0x7FFF);
    TEST_ASSERT(spu.voices[0].vag_loop_crossfade == 1);

    int16_t out[128 * 2];
    /* Advance through several loop repetitions */
    for (int i = 0; i < 10; i++) {
        ps1_spu_mix(&spu, out, 128, 0);
    }
    /* Voice remains active and successfully looped without errors */
    TEST_ASSERT(ps1_spu_voice_is_active(&spu, 0));

    printf("test_vag_loop_crossfade passed.\n");
}

/* 5. 32-bit Float Mixing with Soft Limiter Test */
static void test_float_mixing(void) {
    Ps1Spu spu;
    ps1_spu_init(&spu);
    TEST_ASSERT(ps1_spu_get_float_mixing(&spu) == 0);

    uint8_t block[16];
    make_test_adpcm_block(block, 0, 0,
        PS1_ADPCM_FLAG_LOOP_START | PS1_ADPCM_FLAG_LOOP_END | PS1_ADPCM_FLAG_LOOP_REPEAT, 7);

    /* Key on 10 voices with maximum volume to exceed 32767 */
    for (int v = 0; v < 10; v++) {
        ps1_spu_key_on_raw(&spu, v, block, 16, 0x10000, 0x7FFF, 0x7FFF);
    }

    int16_t out_int[32 * 2];
    int16_t out_flt[32 * 2];

    /* Authentic mode: hard integer clamping */
    ps1_spu_set_float_mixing(&spu, 0);
    ps1_spu_mix(&spu, out_int, 32, 0);

    /* Float mode: smooth soft limiter */
    ps1_spu_set_float_mixing(&spu, 1);
    TEST_ASSERT(ps1_spu_get_float_mixing(&spu) == 1);
    ps1_spu_mix(&spu, out_flt, 32, 0);

    /* Float limiter must keep output within range [-32768, 32767] */
    for (int i = 0; i < 32 * 2; i++) {
        TEST_ASSERT(out_flt[i] <= 32767 && out_flt[i] >= -32768);
    }

    /* For quiet signal below knee (28000), float limiter must match linear output */
    ps1_spu_init(&spu);
    ps1_spu_key_on_raw(&spu, 0, block, 16, 0x10000, 0x1000, 0x1000); /* Low volume */

    ps1_spu_set_float_mixing(&spu, 0);
    ps1_spu_mix(&spu, out_int, 32, 0);

    ps1_spu_init(&spu);
    ps1_spu_key_on_raw(&spu, 0, block, 16, 0x10000, 0x1000, 0x1000);
    ps1_spu_set_float_mixing(&spu, 1);
    ps1_spu_mix(&spu, out_flt, 32, 0);

    for (int i = 0; i < 32 * 2; i++) {
        /* Difference between 16-bit integer and 32-bit float mixing must be <= 1 LSB */
        TEST_ASSERT(abs((int)out_flt[i] - (int)out_int[i]) <= 1);
    }

    printf("test_float_mixing passed.\n");
}

int main(void) {
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    test_polyphony_expansion();
    test_pop_suppression();
    test_seamless_cd_loop();
    test_vag_loop_crossfade();
    test_float_mixing();

    printf("All PS1 audio overhaul tests PASSED successfully!\n");
    return 0;
}