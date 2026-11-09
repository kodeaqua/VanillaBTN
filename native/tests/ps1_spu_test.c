/* ps1_spu_test.c — synthetic and fixture tests for PS1 SPU emulation */

#include "ps1_spu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef assert
#define assert(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

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
    /* Each signed nibble is expanded to 12 fractional bits, then shifted down. */
    assert(out[0] == 256);
    assert(out[1] == 512);
    assert(out[2] == -256);
    assert(out[3] == -512);
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
    memcpy(header + 18, &num_progs, 2);
    memcpy(header + 20, &num_tones, 2);
    memcpy(header + 22, &num_vags, 2);
    header[32] = 1;

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
    header[offtbl_off] = 2; /* Explicit 16-byte dummy prefix. */
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

    /* Program numbers are sparse; tone blocks are packed by active program. */
    header[32] = 0;
    header[32 + 5 * 16] = 1;
    id = ps1_spu_load_vab(&spu, header, sizeof(header), body, sizeof(body));
    assert(id >= 0);
    ps1_spu_key_on_vab(&spu, 1, id, 5, 0, 60, 0, 127, 127);
    assert(spu.voices[1].active);
    ps1_spu_free_vab(&spu, id);
}

static void test_final_adpcm_block_is_not_discarded(void) {
    Ps1Spu spu;
    uint8_t block[16];
    int16_t pcm[29 * 2];
    ps1_spu_init(&spu);
    memset(block, 0x11, sizeof(block));
    block[0] = 4;
    block[1] = PS1_ADPCM_FLAG_LOOP_END;
    ps1_spu_key_on_raw(&spu, 0, block, sizeof(block), 0x10000, 32767, 32767);
    ps1_spu_mix(&spu, pcm, 29, 0);
    assert(pcm[0] > 0 && pcm[27 * 2] > 0);
    assert(pcm[28 * 2] == 0);
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

static void test_spu_adsr_envelope(void) {
    Ps1Spu spu;
    uint8_t block[16] = {4, 7};
    int16_t pcm[64];
    ps1_spu_init(&spu);
    ps1_spu_key_on_raw(&spu, 0, block, sizeof(block), 0x10000, 32767, 32767);
    Ps1Voice *v = &spu.voices[0];
    /* Independent clock values from PS1 shift/step envelope rules. */
    ps1_spu_voice_set_adsr(&spu, 0, 11 << 10, 0);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 7);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 14);
    ps1_spu_voice_set_adsr(&spu, 0, 12 << 10, 0);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 0);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 7);
    ps1_spu_voice_set_adsr(&spu, 0, 127 << 8, 0);
    ps1_spu_mix(&spu, pcm, 32, 0);
    assert(v->env_level == 0 && v->env_phase == PS1_ADSR_ATTACK);
    /* Zero registers mean fastest attack, not an envelope bypass. */
    ps1_spu_voice_set_adsr(&spu, 0, 0, 0);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 14336 && v->adsr_sustain_level == 0x800);
    ps1_spu_mix(&spu, pcm, 2, 0);
    assert(v->env_level == 32767 && v->env_phase == PS1_ADSR_DECAY);
    /* Linear release shift zero takes two clocks from full volume. */
    ps1_spu_key_off(&spu, 0);
    assert(v->active && v->env_phase == PS1_ADSR_RELEASE);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 16383 && v->active);
    ps1_spu_key_off(&spu, 0);
    assert(v->active && v->env_level == 16383);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 0 && !v->active && v->env_phase == PS1_ADSR_OFF);

    ps1_spu_key_on_raw(&spu, 0, block, sizeof(block), 0x10000, 32767, 32767);
    ps1_spu_voice_set_adsr(&spu, 0, 0, (1 << 15) | (1 << 14) | (11 << 8));
    v->env_phase = PS1_ADSR_SUSTAIN;
    v->env_level = 16384;
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 16380); /* exponential decrease: -8 * 0.5 */
    ps1_spu_voice_set_adsr(&spu, 0, 0, 31);
    v->env_level = 12345;
    ps1_spu_key_off(&spu, 0);
    ps1_spu_mix(&spu, pcm, 32, 0);
    assert(v->env_level == 12345 && v->active); /* all-ones release holds */
    /* Decay uses exponential -8 steps and changes phase at its target. */
    ps1_spu_voice_set_adsr(&spu, 0, (11 << 4) | 7, 0);
    v->env_phase = PS1_ADSR_DECAY;
    v->env_level = 16388;
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 16383 && v->env_phase == PS1_ADSR_SUSTAIN);
    assert(v->adsr_sustain_level == 16384);

    /* Above 0x6000, exponential attack shift 11 ticks once every 4 clocks. */
    ps1_spu_voice_set_adsr(&spu, 0, (1 << 15) | (11 << 10), 0);
    v->env_level = 0x6001;
    ps1_spu_mix(&spu, pcm, 3, 0);
    assert(v->env_level == 0x6001);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 0x6008);
    /* Shift 10 splits the slowdown between step and counter. */
    ps1_spu_voice_set_adsr(&spu, 0, (1 << 15) | (10 << 10), 0);
    v->env_level = 0x6001;
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 0x6001);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 0x6008);
    /* A slow non-all-ones rate still steps after 32768 clocks. */
    ps1_spu_voice_set_adsr(&spu, 0, 126 << 8, 0);
    for (int i = 0; i < 1023; i++) ps1_spu_mix(&spu, pcm, 32, 0);
    ps1_spu_mix(&spu, pcm, 31, 0);
    assert(v->env_level == 0);
    ps1_spu_mix(&spu, pcm, 1, 0);
    assert(v->env_level == 5);

}

static void test_spu_reverb_without_voice_sends(void) {
    Ps1Spu dry, configured;
    ps1_spu_init(&dry);
    ps1_spu_init(&configured);
    ps1_spu_set_reverb_enabled(&configured, 1);
    ps1_spu_set_reverb_params(&configured, 5, 64, 64, 0x4000, 0x4000, 60, 64);
    assert(configured.reverb_enabled && configured.reverb_type == 5);
    assert(configured.reverb_vol_left == 0x4000 && configured.reverb_delay == 64);
    uint8_t block[16] = {4, PS1_ADPCM_FLAG_LOOP_END};
    memset(block + 2, 0x77, 14);
    ps1_spu_key_on_raw(&dry, 0, block, sizeof(block), 0x10000, 32767, 32767);
    ps1_spu_key_on_raw(&configured, 0, block, sizeof(block), 0x10000, 32767, 32767);
    int16_t dry_pcm[1024], configured_pcm[1024];
    /* No voice has a modeled EON send. Global enable/configuration must not
     * invent a wet signal; compare past both old fabricated echo taps. */
    for (int chunk = 0; chunk < 12; chunk++) {
        ps1_spu_mix(&dry, dry_pcm, 512, 0);
        ps1_spu_mix(&configured, configured_pcm, 512, 0);
        if (chunk == 0) assert(dry_pcm[0] > 0);
        assert(memcmp(dry_pcm, configured_pcm, sizeof(dry_pcm)) == 0);
    }
}

int main(void) {
    test_final_adpcm_block_is_not_discarded();
    test_adpcm_decode_basic();
    test_adpcm_filter_prediction();
    test_spu_synthetic_vab_and_mix();
    test_spu_raw_playback_and_volume();
    test_spu_adsr_envelope();
    test_spu_reverb_without_voice_sends();
    printf("ps1_spu tests passed.\n");
    return 0;
}
