/* ps1_audio_test.c — synthetic tests for PS1 audio mixing and SDL3 bridge */

#include "ps1_audio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_audio_init_shutdown(void) {
    Ps1Audio audio;
    bool ok = ps1_audio_init(&audio);
    (void)ok;
    assert(ok);

    /* In test environment, has_output may be true or false depending on audio hardware */
    ps1_audio_shutdown(&audio);
    assert(audio.stream == NULL);
    assert(!audio.has_output);
}

static void test_audio_mix_combined(void) {
    Ps1Audio audio;
    ps1_audio_init(&audio);

    /* Set up SPU test tone */
    uint8_t spu_block[16];
    memset(spu_block, 0, sizeof(spu_block));
    spu_block[0] = 0x04;
    spu_block[1] = PS1_ADPCM_FLAG_LOOP_START | PS1_ADPCM_FLAG_LOOP_END | PS1_ADPCM_FLAG_LOOP_REPEAT;
    spu_block[2] = 0x22;

    ps1_spu_key_on_raw(&audio.spu, 0, spu_block, sizeof(spu_block), 0x10000, 0x4000, 0x4000);

    /* Set up XA test stream */
    uint8_t xa_stream[PS1_XA_SECTOR_SIZE];
    memset(xa_stream, 0, sizeof(xa_stream));
    xa_stream[0] = 1; xa_stream[1] = 0; xa_stream[2] = 0x64; xa_stream[3] = 0x01;
    for (int b = 0; b < 16; b++) xa_stream[8 + b] = 0x04;
    xa_stream[8 + 16] = 0x33;

    ps1_xa_set_file(&audio.xa, 0, xa_stream, sizeof(xa_stream));
    ps1_xa_play_track(&audio.xa, 0);

    /* Mix 256 frames */
    int16_t pcm[512];
    memset(pcm, 0, sizeof(pcm));
    ps1_audio_mix(&audio, pcm, 256);

    int has_audio = 0;
    for (int i = 0; i < 512; i++) {
        if (pcm[i] != 0) {
            has_audio = 1;
            break;
        }
    }
    assert(has_audio);

    /* Advance 1 tick */
    ps1_audio_tick(&audio, 1);

    ps1_audio_shutdown(&audio);
}

int main(void) {
    test_audio_init_shutdown();
    test_audio_mix_combined();
    printf("ps1_audio tests passed.\n");
    return 0;
}
