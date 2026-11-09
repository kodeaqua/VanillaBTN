/* ps1_xa_test.c — synthetic and fixture tests for PS1 CD-XA playback */

#include "ps1_xa.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep regression failures headless on Windows. */
#undef assert
#define assert(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static void test_xa_synthetic_decode(void) {
    uint8_t sector[PS1_XA_SECTOR_SIZE];
    memset(sector, 0, sizeof(sector));

    /* Subheader: file 1, chan 0, submode 0x64 (audio Form 2), coding 0x01 */
    sector[0] = 1;
    sector[1] = 0;
    sector[2] = 0x64;
    sector[3] = 0x01;
    sector[4] = 1;
    sector[5] = 0;
    sector[6] = 0x64;
    sector[7] = 0x01;

    /* Sound group 0: shift 4, filter 0 for all blocks */
    uint8_t *sg0 = sector + 8;
    for (int b = 0; b < 16; b++) {
        sg0[b] = 0x04; /* shift 4, filter 0 */
    }
    /* Put non-zero data words: w0 = 0x21 (block 0=1, block 1=2) */
    sg0[16] = 0x21;

    int16_t samples[PS1_XA_MAX_SECTOR_SAMPLES];
    int16_t h1 = 0, h2 = 0;
    int count = ps1_xa_decode_sector(sector, samples, &h1, &h2);
    (void)count;

    assert(count == PS1_XA_MAX_SECTOR_SAMPLES);
    /* Block 0, sample 0: (1 << 12) >> 4 = 256 */
    assert(samples[0] == 256);
    /* Block 1, sample 0: (2 << 12) >> 4 = 512 (starts at sample 28) */
    assert(samples[28] == 512);
}

static void test_xa_player_filtering_and_fading(void) {
    Ps1XaPlayer player;
    ps1_xa_init(&player);

    /* Construct 2 sectors in file 0:
     * Sector 0: channel 0
     * Sector 1: channel 1
     */
    uint8_t stream[PS1_XA_SECTOR_SIZE * 2];
    memset(stream, 0, sizeof(stream));

    /* Sector 0: channel 0 */
    stream[0] = 1; stream[1] = 0; stream[2] = 0x64; stream[3] = 0x01;
    for (int b = 0; b < 16; b++) stream[8 + b] = 0x04;
    stream[8 + 16] = 0x11;

    /* Sector 1: channel 1 */
    uint8_t *sec1 = stream + PS1_XA_SECTOR_SIZE;
    sec1[0] = 1; sec1[1] = 1; sec1[2] = 0x64; sec1[3] = 0x01;
    for (int b = 0; b < 16; b++) sec1[8 + b] = 0x04;
    sec1[8 + 16] = 0x22;

    ps1_xa_set_file(&player, 0, stream, sizeof(stream));

    /* Track 1 in s_tracks has file 0, channel 1 */
    const Ps1XaTrackEntry *entry = ps1_xa_get_track_entry(1);
    (void)entry;
    assert(entry != NULL);
    assert(entry->file == 0 && entry->channel == 1);

    ps1_xa_play_track(&player, 1);
    assert(ps1_xa_get_state(&player) == PS1_XA_PLAYING);

    /* Mix 128 frames */
    int16_t pcm[256];
    memset(pcm, 0, sizeof(pcm));
    ps1_xa_mix(&player, pcm, 128, 0x1FFF, 0x1FFF, 0);

    /* Verify audio was produced */
    int has_audio = 0;
    for (int i = 0; i < 256; i++) {
        if (pcm[i] != 0) {
            has_audio = 1;
            break;
        }
    }
    assert(has_audio);

    /* Fade-out test */
    player.fade_volume = 2; /* set small volume to test stop trigger */
    ps1_xa_fade_tick(&player, 1); /* vol -> 1 */
    assert(ps1_xa_get_state(&player) == PS1_XA_FADING);

    ps1_xa_fade_tick(&player, 1); /* vol -> 0 -> stop */
    assert(ps1_xa_get_state(&player) == PS1_XA_IDLE);

    ps1_xa_stop(&player);
    assert(ps1_xa_get_state(&player) == PS1_XA_IDLE);
}

static void test_xa_stereo_decode_and_histories(void) {
    uint8_t sector[PS1_XA_SECTOR_SIZE];
    memset(sector, 0, sizeof(sector));

    /* Subheader: file 1, chan 0, submode 0x64 (audio Form 2), coding 0x01 (stereo, 37.8kHz) */
    sector[0] = 1;
    sector[1] = 0;
    sector[2] = 0x64;
    sector[3] = 0x01;
    sector[4] = 1;
    sector[5] = 0;
    sector[6] = 0x64;
    sector[7] = 0x01;

    /* Configure all sound groups with filter 1 for Left and filter 2 for Right */
    for (int g = 0; g < PS1_XA_SOUND_GROUPS; g++) {
        uint8_t *sg = sector + 8 + g * 128;
        for (int unit = 0; unit < 8; unit++) {
            sg[4 + unit] = (unit & 1) ? 0x20 : 0x10;
        }
        memcpy(sg, sg + 4, 4);
        memcpy(sg + 12, sg + 8, 4);
        /* Non-zero nibbles: w0 = 0x21 (Left=1, Right=2) */
        sg[16] = 0x21;
    }

    int16_t samples[PS1_XA_MAX_SECTOR_SAMPLES];
    int16_t h1_l = 64, h2_l = 0;
    int16_t h1_r = 128, h2_r = 0;

    int count = ps1_xa_decode_sector_stereo(sector, samples, &h1_l, &h2_l, &h1_r, &h2_r);
    assert(count == PS1_XA_MAX_SECTOR_SAMPLES);

    /* Left sample 0: sn = (1 << 12) >> 0 = 4096; pred = (60 * 64 + 32) / 64 = 60 -> 4156 */
    assert(samples[0] == 4156);
    /* Right sample 0: sn = (2 << 12) >> 0 = 8192; pred = (115 * 128 + 32) / 64 = 230 -> 8422 */
    assert(samples[1] == 8422);

    /* Histories should have updated separately without cross-channel bleed */
    assert(h1_l != 0 && h1_r != 0);
    assert(h1_l != h1_r);
}

static void test_xa_coding_info_rates_and_stereo(void) {
    Ps1XaPlayer player;
    ps1_xa_init(&player);

    uint8_t stream[PS1_XA_SECTOR_SIZE * 2];
    memset(stream, 0, sizeof(stream));

    /* Sector 0: channel 0, coding info 0x00 (mono, 37.8 kHz) */
    stream[0] = 1;
    stream[1] = 0;
    stream[2] = 0x64;
    stream[3] = 0x00;
    for (int b = 0; b < 16; b++) stream[8 + b] = 0x04;
    stream[8 + 16] = 0x11;

    /* Sector 1: channel 1, coding info 0x05 (stereo, 18.9 kHz) */
    uint8_t *sec1 = stream + PS1_XA_SECTOR_SIZE;
    sec1[0] = 1;
    sec1[1] = 1;
    sec1[2] = 0x64;
    sec1[3] = 0x05;
    for (int b = 0; b < 16; b++) sec1[8 + b] = 0x04;
    sec1[8 + 16] = 0x22;

    ps1_xa_set_file(&player, 0, stream, sizeof(stream));

    /* Play track 0 (file 0, channel 0) -> 37.8 kHz mono */
    ps1_xa_play_track(&player, 0);
    assert(player.is_stereo == 0);
    assert(player.sample_rate == 37800);

    /* Play track 1 (file 0, channel 1) -> 18.9 kHz stereo */
    ps1_xa_play_track(&player, 1);
    assert(player.is_stereo == 1);
    assert(player.sample_rate == 18900);

    /* Mix stereo and verify distinct channels */
    int16_t pcm[256];
    memset(pcm, 0, sizeof(pcm));
    ps1_xa_mix(&player, pcm, 128, 0x7FFF, 0x7FFF, 0);
    int non_zero = 0;
    for (int i = 0; i < 256; i++) {
        if (pcm[i] != 0) non_zero = 1;
    }
    assert(non_zero);
}

static void test_xa_hardware_units(void) {
    uint8_t sector[PS1_XA_SECTOR_SIZE] = {0};
    int16_t samples[PS1_XA_MAX_SECTOR_SAMPLES];
    sector[2] = 0x64;
    sector[3] = 1;
    uint8_t *group = sector + 8;
    /* Canonical parameters differ deliberately from their repair copies. */
    for (int unit = 0; unit < 8; unit++) group[4 + unit] = 4;
    for (int i = 0; i < 28; i++) {
        group[16 + i * 4] = 0x21;
        group[17 + i * 4] = 0x43;
        group[18 + i * 4] = 0x65;
        group[19 + i * 4] = 0x87;
    }
    assert(ps1_xa_decode_sector_stereo(sector, samples, NULL, NULL, NULL, NULL) == 4032);
    for (int pair = 0; pair < 4; pair++) {
        for (int i = 0; i < 28; i++) {
            assert(samples[(pair * 28 + i) * 2] == (pair * 2 + 1) * 256);
            assert(samples[(pair * 28 + i) * 2 + 1] == (pair == 3 ? -2048 : (pair * 2 + 2) * 256));
        }
    }
    memset(group, 15, 16);
    assert(ps1_xa_decode_sector_stereo(sector, samples, NULL, NULL, NULL, NULL) == 4032);
    assert(samples[0] == 8 && samples[1] == 16);

    memset(sector + 8, 0, sizeof(sector) - 8);
    for (int g = 0; g < PS1_XA_SOUND_GROUPS; g++) memset(sector + 8 + g * 128, 0x1c, 16);
    int16_t left = -1, right = 1, previous_l = 0, previous_r = 0;
    assert(ps1_xa_decode_sector_stereo(sector, samples, &left, &previous_l, &right, &previous_r) == 4032);
    assert(samples[0] == -1 && samples[1] == 1 && left == -1 && right == 1);

    memset(sector + 8, 0, sizeof(sector) - 8);
    sector[3] = 0x11;
    memset(group + 4, 8, 4);
    for (int i = 0; i < 28; i++) {
        group[16 + i * 4] = 1;
        group[17 + i * 4] = 255;
        group[18 + i * 4] = 2;
        group[19 + i * 4] = 254;
    }
    assert(ps1_xa_decode_sector_stereo(sector, samples, NULL, NULL, NULL, NULL) == 2016);
    assert(samples[0] == 1 && samples[1] == -1 && samples[56] == 2 && samples[57] == -2);
    sector[3] = 0x21;
    left = -7;
    samples[0] = 1234;
    assert(ps1_xa_decode_sector_stereo(sector, samples, &left, NULL, NULL, NULL) == 0);
    assert(left == -7 && samples[0] == 1234);
}

static int read_one_sector(void *context, uint32_t index, uint8_t *sector) {
    assert(index == 0);
    memcpy(sector, context, PS1_XA_SECTOR_SIZE);
    return 1;
}

static void test_xa_exact_stream_duration(void) {
    for (int width = 0; width < 2; width++) {
        for (int slow = 0; slow < 2; slow++) {
            for (int stereo = 0; stereo < 2; stereo++) {
                uint8_t sector[PS1_XA_SECTOR_SIZE] = {1, 0, 0x64};
                sector[3] = (uint8_t)((width << 4) | (slow << 2) | stereo);
                for (int g = 0; g < PS1_XA_SOUND_GROUPS; g++) {
                    uint8_t *group = sector + 8 + g * 128;
                    memset(group, width ? 8 : 12, 16);
                    memset(group + 16, width ? 1 : 0x11, 112);
                }
                Ps1XaPlayer player;
                ps1_xa_play_source(&player, read_one_sector, sector, 0, 1, 1, 0);
                int source_frames = 4032 / (width ? 2 : 1) / (stereo ? 2 : 1);
                int expected_frames = source_frames * 7 / (slow ? 3 : 6);
                int16_t pcm[2];
                /* Every source sample is exactly one. The last decoded frame
                 * must survive lookahead EOF; duration follows the exact 7:6
                 * or 7:3 clock ratio, not a truncated 16.16 increment. */
                for (int i = 0; i < expected_frames; i++) {
                    ps1_xa_mix(&player, pcm, 1, 32767, 32767, 0);
                    assert(pcm[0] == 1 && pcm[1] == 1);
                }
                ps1_xa_mix(&player, pcm, 1, 32767, 32767, 0);
                assert(pcm[0] == 0 && pcm[1] == 0);
                assert(player.state == PS1_XA_IDLE);
            }
        }
    }
}

int main(void) {
    test_xa_exact_stream_duration();
    test_xa_hardware_units();
    Ps1XaPlayer player;
    int16_t pcm[2];
    ps1_xa_init(&player);
    player.state = PS1_XA_PLAYING;
    player.curr_sample = 32767;
    player.next_sample = 32767;
    player.curr_sample_r = player.next_sample_r = 32767;
    player.sample_rate = 0;
    player.serial_volume = 128;
    ps1_xa_mix(&player, pcm, 1, 32767, 32767, 0);
    assert(pcm[0] == 32767 && pcm[1] == 32767);
    test_xa_synthetic_decode();
    test_xa_player_filtering_and_fading();
    test_xa_stereo_decode_and_histories();
    test_xa_coding_info_rates_and_stereo();
    printf("ps1_xa tests passed.\n");
    return 0;
}
