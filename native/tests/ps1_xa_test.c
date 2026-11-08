/* ps1_xa_test.c — synthetic and fixture tests for PS1 CD-XA playback */

#include "ps1_xa.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void) {
    Ps1XaPlayer player;
    int16_t pcm[2];
    ps1_xa_init(&player);
    player.state = PS1_XA_PLAYING;
    player.curr_sample = 32767;
    player.next_sample = 32767;
    player.resample_step = 0;
    player.serial_volume = 128;
    ps1_xa_mix(&player, pcm, 1, 32767, 32767, 0);
    assert(pcm[0] == 32767 && pcm[1] == 32767);
    test_xa_synthetic_decode();
    test_xa_player_filtering_and_fading();
    printf("ps1_xa tests passed.\n");
    return 0;
}
