/* ps1_spu_test.c — synthetic and fixture tests for PS1 SPU emulation */

#include "ps1_spu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_adpcm_decode_basic(void) {
    /* 16-byte block: shift=4, filter=0, no loop flags */
    uint8_t block[16];
    memset(block, 0, sizeof(block));
    block[0] = 0x04; /* shift 4, filter 0 */
    block[1] = 0x00; /* flags */

    /* Put known nibble values: 1, 2, -1 (0xF), -2 (0xE) */
    block[2] = 0x21; /* lo=1, hi=2 */
    block[3] = 0xEF; /* lo=-1, hi=-2 */

    int16_t out[28];
    int16_t h1 = 0, h2 = 0;
    uint8_t flags = ps1_adpcm_decode_block(block, out, &h1, &h2);
    (void)flags;

    assert(flags == 0x00);
    /* sample 0: 1 << 4 = 16 */
    assert(out[0] == 16);
    /* sample 1: 2 << 4 = 32 */
    assert(out[1] == 32);
    /* sample 2: -1 << 4 = -16 */
    assert(out[2] == -16);
    /* sample 3: -2 << 4 = -32 */
    assert(out[3] == -32);
    /* hist1 should equal out[27] */
    assert(h1 == out[27]);
}

static void test_adpcm_filter_prediction(void) {
    /* Test filter 1: f0 = 60/32 */
    uint8_t block[16];
    memset(block, 0, sizeof(block));
    block[0] = 0x10; /* shift 0, filter 1 */
    block[1] = 0x00;

    int16_t out[28];
    int16_t h1 = 64, h2 = 0;
    ps1_adpcm_decode_block(block, out, &h1, &h2);

    /* For sample 0 with nibble 0: (60 * 64 + 0 + 32) / 64 = (3840 + 32) / 64 = 60 */
    assert(out[0] == 60);
}

static void test_spu_synthetic_vab_and_mix(void) {
    Ps1Spu spu;
    ps1_spu_init(&spu);

    /* Construct a minimal valid synthetic VAB header */
    uint8_t header[4096];
    memset(header, 0, sizeof(header));

    /* Magic "pBAV" */
    header[0] = 0x70;
    header[1] = 0x42;
    header[2] = 0x41;
    header[3] = 0x56;

    uint16_t num_progs = 1;
    uint16_t num_tones = 1;
    uint16_t num_vags = 1;
    memcpy(header + 20, &num_progs, 2);
    memcpy(header + 22, &num_tones, 2);
    memcpy(header + 24, &num_vags, 2);

    /* ProgAtr: offset 32 (128 * 16 bytes = 2048) -> ends at 2080 (0x820) */
    /* VagAtr for prog 0, tone 0: starts at 0x820 (32 bytes) */
    uint32_t vag_atr_off = 32 + 2048;
    header[vag_atr_off + 2] = 127; /* vol */
    header[vag_atr_off + 3] = 64;  /* pan */
    header[vag_atr_off + 4] = 60;  /* center note: 60 (middle C) */
    uint16_t vag_idx = 1;
    memcpy(header + vag_atr_off + 22, &vag_idx, 2);

    /* VAG offset table: after 16 VagAtr (512 bytes) = 2080 + 512 = 2592 (0xA20) */
    uint32_t offtbl_off = 32 + 2048 + 16 * 32;
    uint16_t vag_len_div8 = 4; /* 4 * 8 = 32 bytes (2 blocks) */
    memcpy(header + offtbl_off + 2, &vag_len_div8, 2); /* vag_offset[1] */

    /* Body: 16 bytes dummy + 2 blocks (32 bytes) */
    uint8_t body[64];
    memset(body, 0, sizeof(body));
    /* Block 1 (at offset 16): loop start */
    body[16] = 0x04; /* shift 4, filter 0 */
    body[17] = PS1_ADPCM_FLAG_LOOP_START;
    body[18] = 0x11;
    /* Block 2 (at offset 32): loop end without repeat */
    body[32] = 0x04;
    body[33] = PS1_ADPCM_FLAG_LOOP_END;
    body[34] = 0x22;

    Ps1VabId id = ps1_spu_load_vab(&spu, header, sizeof(header), body, sizeof(body));
    (void)id;
    assert(id >= 0);

    /* Key on voice 0 with middle C (note 60) */
    ps1_spu_key_on_vab(&spu, 0, id, 0, 0, 60, 0, 127, 127);
    assert(ps1_spu_voice_is_active(&spu, 0));

    /* Mix 64 frames */
    int16_t pcm[128];
    memset(pcm, 0, sizeof(pcm));
    ps1_spu_mix(&spu, pcm, 64, 0);

    /* Verify non-zero output was mixed */
    int has_audio = 0;
    for (int i = 0; i < 128; i++) {
        if (pcm[i] != 0) {
            has_audio = 1;
            break;
        }
    }
    assert(has_audio);

    /* Key off and verify silence */
    ps1_spu_key_off(&spu, 0);
    assert(!ps1_spu_voice_is_active(&spu, 0));

    ps1_spu_free_vab(&spu, id);
    assert(!spu.vabs[id].loaded);
}

static void test_spu_raw_playback_and_volume(void) {
    Ps1Spu spu;
    ps1_spu_init(&spu);

    /* Single repeating block */
    uint8_t block[16];
    memset(block, 0, sizeof(block));
    block[0] = 0x04;
    block[1] = PS1_ADPCM_FLAG_LOOP_START | PS1_ADPCM_FLAG_LOOP_END | PS1_ADPCM_FLAG_LOOP_REPEAT;
    block[2] = 0x33;

    ps1_spu_key_on_raw(&spu, 3, block, sizeof(block), 0x10000, 0x4000, 0x4000);
    assert(ps1_spu_voice_is_active(&spu, 3));

    int16_t pcm1[64];
    ps1_spu_mix(&spu, pcm1, 32, 0);

    /* Mute master volume and mix again */
    ps1_spu_set_master_vol(&spu, 0, 0);
    int16_t pcm2[64];
    ps1_spu_mix(&spu, pcm2, 32, 0);

    for (int i = 0; i < 64; i++) {
        assert(pcm2[i] == 0);
    }

    ps1_spu_key_off_all(&spu);
    assert(!ps1_spu_voice_is_active(&spu, 3));
}

int main(void) {
    test_adpcm_decode_basic();
    test_adpcm_filter_prediction();
    test_spu_synthetic_vab_and_mix();
    test_spu_raw_playback_and_volume();
    printf("ps1_spu tests passed.\n");
    return 0;
}
