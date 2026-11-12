#include "ps1_reverb.h"
#include "ps1_spu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static void configure_arithmetic(Ps1Reverb *r) {
    uint16_t v[32] = {0};
    v[RV_VLIN] = v[RV_VRIN] = v[RV_VIIR] = v[RV_VWALL] = 16384;
    v[RV_VCOMB1] = v[RV_VAPF1] = 16384;
    v[RV_VCOMB2] = (uint16_t)-8192;
    v[RV_VCOMB3] = 4096; v[RV_VCOMB4] = (uint16_t)-4096;
    v[RV_VAPF2] = 8192;
    v[RV_MLSAME] = 1; v[RV_DLSAME] = 2;
    v[RV_MLDIFF] = 3; v[RV_DRDIFF] = 4;
    v[RV_MLCOMB1] = 1; v[RV_MLCOMB2] = 3;
    v[RV_MLCOMB3] = 5; v[RV_MLCOMB4] = 6;
    v[RV_MLAPF1] = 8; v[RV_MLAPF2] = 10;
    v[RV_DAPF1] = v[RV_DAPF2] = 1;
    memset(r, 0, sizeof(*r));
    CHECK(ps1_reverb_set_registers(r, v, 256, 1));
    unsigned b = r->base;
    r->ram[b + 3] = 1000; r->ram[b + 8] = 2000;
    r->ram[b + 11] = -2000; r->ram[b + 16] = 3000;
    r->ram[b + 20] = 3200; r->ram[b + 24] = -1600;
    r->ram[b + 28] = 800; r->ram[b + 36] = -400;
}

static void test_register_arithmetic(void) {
    Ps1Reverb r;
    configure_arithmetic(&r);
    CHECK(ps1_reverb_step(&r, 0, 10000, 1) == -2);
    CHECK(r.ram[r.base + 4] == 3500);  /* same-side reflection */
    CHECK(r.ram[r.base + 12] == 2250); /* cross reflection */
    CHECK(r.ram[r.base + 32] == 1387); /* four comb taps -> APF1 */
    CHECK(r.ram[r.base + 40] == 1593); /* APF1 -> APF2 */
    CHECK(r.cursor == r.base);       /* advance only after right phase */
    configure_arithmetic(&r);
    r.registers[RV_VIIR] = 0x8000;
    ps1_reverb_step(&r, 0, 10000, 1);
    CHECK(r.ram[r.base + 4] == 4000); /* documented IIR -1 edge case */
    configure_arithmetic(&r);
    r.registers[RV_VLIN] = r.registers[RV_VIIR] = r.registers[RV_VWALL] = 32767;
    r.ram[r.base + 3] = -32768; r.ram[r.base + 8] = 32767;
    ps1_reverb_step(&r, 0, 32767, 1);
    CHECK(r.ram[r.base + 4] == 32767);
    configure_arithmetic(&r);
    r.registers[RV_VLIN] = r.registers[RV_VIIR] = r.registers[RV_VWALL] = 32767;
    r.ram[r.base + 3] = 32767; r.ram[r.base + 8] = -32768;
    ps1_reverb_step(&r, 0, -32768, 1);
    CHECK(r.ram[r.base + 4] == -32768);
    configure_arithmetic(&r);
    int16_t before[128]; memcpy(before, r.ram + r.base, sizeof(before));
    ps1_reverb_step(&r, 0, 10000, 0);
    CHECK(memcmp(before, r.ram + r.base, sizeof(before)) == 0);
}

static void test_wrap_and_presets(void) {
    static const unsigned bytes[10] = {
        0x10,0x26c0,0x1f40,0x4840,0x6fe0,0xade0,0xf6c0,0x18040,0x18040,0x3c00
    };
    static const unsigned first[10] = {0,125,51,177,227,421,829,1,1,23};
    Ps1Reverb r = {0};
    for (int mode = 0; mode < 10; mode++) {
        CHECK(ps1_reverb_set_preset(&r, mode, 1));
        CHECK(r.base == PS1_REVERB_WORDS - bytes[mode] / 2);
        CHECK(r.registers[RV_DAPF1] == first[mode]);
        if (r.base) r.ram[r.base - 1] = 12345;
        r.cursor = PS1_REVERB_WORDS - 1;
        for (int i = 0; i < 1000; i++) {
            int16_t output[2];
            ps1_reverb_clock(&r, 80000, -80000, 1, output);
            CHECK(r.cursor >= r.base && r.cursor < PS1_REVERB_WORDS);
        }
        if (r.base) CHECK(r.ram[r.base - 1] == 12345);
    }
    CHECK(!ps1_reverb_set_preset(&r, 10, 1));
    CHECK(ps1_reverb_set_preset(&r, 7, 1));
    ps1_reverb_set_echo(&r, 7, 63, 64);
    CHECK(r.registers[RV_MLSAME] == 4062 && r.registers[RV_MRSAME] == 2030);
    CHECK(r.registers[RV_VWALL] == 16642);
}

static void test_half_rate_fir_and_disable(void) {
    Ps1Reverb r = {0};
    uint16_t v[32] = {0};
    CHECK(ps1_reverb_set_registers(&r, v, 256, 1));
    for (unsigned i = r.base; i < PS1_REVERB_WORDS; i++) r.ram[i] = 8192;
    /* With APF gains zero and RAM writes disabled, the final APF read is
     * constant. These independent 39-tap step values also verify the one-clock
     * offset between left/right half-rate computations and 2x upsampling. */
    static const int16_t expected[39] = {
        -1,0,0,0,-5,0,13,0,-39,0,
        94,0,-214,0,452,0,-1028,0,4095,8192,
        9218,8192,7738,8192,8404,8192,8096,8192,8229,8192,
        8178,8192,8195,8192,8190,8192,8191,8192,8191,
    };
    for (int i = 0; i < 39; i++) {
        int16_t output[2];
        ps1_reverb_clock(&r, 0, 0, 0, output);
        CHECK(output[0] == expected[i]);
        CHECK(output[1] == (i ? expected[i - 1] : 0));
    }
    CHECK(r.cursor == r.base + 19);
    for (unsigned i = r.base; i < PS1_REVERB_WORDS; i++) CHECK(r.ram[i] == 8192);
}

static void test_full_rate_hifi_is_stereo_and_optional(void) {
    Ps1Reverb authentic = {0}, hifi = {0};
    uint16_t regs[32] = {0};
    CHECK(ps1_reverb_set_registers(&authentic, regs, 256, 1));
    CHECK(ps1_reverb_set_registers(&hifi, regs, 256, 1));
    for (unsigned i = authentic.base; i < PS1_REVERB_WORDS; ++i) {
        authentic.ram[i] = hifi.ram[i] = 8192;
    }
    ps1_reverb_set_hifi(&hifi, 1);
    int16_t hw[2], hd[2];
    ps1_reverb_clock(&authentic, 0, 0, 0, hw);
    ps1_reverb_clock(&hifi, 0, 0, 0, hd);
    CHECK(hd[0] == 8192 && hd[1] == 8192);
    CHECK(hw[0] != hd[0] && hw[1] != hd[1]);
    CHECK(hifi.cursor == hifi.base + 1);
    CHECK(authentic.cursor == authentic.base);
    CHECK(hifi.phase == 0 && hifi.history_position == 0);
    for (unsigned i = authentic.base; i < PS1_REVERB_WORDS; ++i)
        CHECK(hifi.ram[i] == 8192); /* write_enable=0 never modifies RAM */
    ps1_reverb_set_hifi(&hifi, 0);
    CHECK(!ps1_reverb_get_hifi(&hifi));
    ps1_reverb_clock(&hifi, 0, 0, 0, hd);
    CHECK(hifi.phase == 1 && hifi.history_position == 1);
}

static void test_cd_send_and_depth(void) {
    Ps1Spu spu;
    ps1_spu_init(&spu);
    ps1_spu_set_reverb_params(&spu, 5, 127, 127, 0, 0, 0, 0);
    ps1_spu_set_reverb_enabled(&spu, 1);
    int16_t source[64] = {32767, 32767}, pcm[512];
    spu.cd_input = source; spu.cd_input_frames = 32;
    spu.cd_reverb = 1; spu.cd_mix = 0; /* wet only, no direct CD mix */
    ps1_spu_mix(&spu, pcm, 32, 0);
    CHECK(spu.cd_input_position == 32);
    spu.cd_input = NULL;
    int tail = 0;
    for (int i = 0; i < 100; i++) {
        ps1_spu_mix(&spu, pcm, 256, 0);
        for (int sample = 0; sample < 512; sample++) if (pcm[sample]) tail = 1;
    }
    CHECK(tail);
    ps1_spu_set_reverb_params(&spu, 5, 0, 0, 0, 0, 0, 0);
    ps1_spu_mix(&spu, pcm, 256, 0);
    for (int i = 0; i < 512; i++) CHECK(pcm[i] == 0);
    CHECK(ps1_spu_set_reverb_voice(&spu, 1u << 23, 1) == (1u << 23));
    CHECK(ps1_spu_set_reverb_voice(&spu, 1u << 23, 0) == 0);
}

static void test_mix_block_boundaries(void) {
    Ps1Spu whole, split;
    ps1_spu_init(&whole);
    ps1_spu_set_reverb_params(&whole, 5, 127, 127, 0, 0, 0, 0);
    ps1_spu_set_reverb_enabled(&whole, 1);
    int16_t source[1024], full_pcm[1024], split_pcm[1024];
    for (int i = 0; i < 1024; i++) source[i] = (int16_t)((i * 97) % 8192 - 4096);
    whole.cd_input = source;
    whole.cd_input_frames = 512;
    whole.cd_mix = whole.cd_reverb = 1;
    split = whole;
    ps1_spu_mix(&whole, full_pcm, 512, 0);
    ps1_spu_mix(&split, split_pcm, 137, 0);
    ps1_spu_mix(&split, split_pcm + 137 * 2, 375, 0);
    CHECK(memcmp(full_pcm, split_pcm, sizeof(full_pcm)) == 0);
    CHECK(memcmp(&whole.reverb, &split.reverb, sizeof(whole.reverb)) == 0);
    CHECK(whole.cd_input_position == 512 && split.cd_input_position == 512);
}

static void test_reverb_null_and_bounds_safety(void) {
    int16_t out[2] = {0};
    /* NULL safety checks */
    ps1_reverb_set_echo(NULL, 7, 64, 64);
    ps1_reverb_clock(NULL, 0, 0, 0, out);
    ps1_reverb_clock(NULL, 0, 0, 0, NULL);
    CHECK(ps1_reverb_step(NULL, 0, 1000, 1) == 0);

    /* Zero/empty reverb struct safety (length <= 0 division protection) */
    Ps1Reverb r;
    memset(&r, 0, sizeof(r));
    r.base = PS1_REVERB_WORDS; /* length = 0 */
    ps1_reverb_clock(&r, 1000, 1000, 1, out);

    /* Out of bounds channel index protection (c > 1) */
    CHECK(ps1_reverb_step(&r, 2, 1000, 1) == 0);
    CHECK(ps1_reverb_step(&r, 999, 1000, 1) == 0);
}

int main(void) {
    test_reverb_null_and_bounds_safety();
    test_register_arithmetic();
    test_wrap_and_presets();
    test_half_rate_fir_and_disable();
    test_full_rate_hifi_is_stereo_and_optional();
    test_cd_send_and_depth();
    test_mix_block_boundaries();
    return 0;
}
