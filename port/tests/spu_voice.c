#include "psx/spu_voice.h"
#include <stdio.h>
#include <string.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    s16 impulse[4] = {32767, 0, 0, 0};
    CHECK(hm_spu_gaussian(impulse, 0) == -1);
    impulse[0] = 0;
    impulse[2] = 32767;
    CHECK(hm_spu_gaussian(impulse, 0) == 22962);
    u8 sample[32];
    memset(sample, 0x77, sizeof(sample));
    sample[0] = sample[16] = 0x04;
    sample[1] = 4;
    sample[17] = 3; /* Two-block repeat, no one-shot completion. */
    hm_spu_voice whole[24] = {0}, split[24] = {0};
    hm_spu_voice_start(whole, sample, sizeof(sample), 4096, 0x000F, 0x1FC0, 32767, 0);
    memcpy(split, whole, sizeof(whole));
    s16 a[2048], b[2048];
    hm_spu_voice_mix(whole, a, 1024);
    hm_spu_voice_mix(split, b, 17);
    hm_spu_voice_mix(split, b + 34, 1007);
    CHECK(!memcmp(a, b, sizeof(a)));
    CHECK(whole[0].envelope.phase != HM_ADSR_OFF && whole[0].block < sizeof(sample));
    int audible = 0;
    for (u32 i = 0; i < 1024; ++i) {
        audible |= a[i * 2] != 0;
        CHECK(a[i * 2 + 1] == 0);
    }
    CHECK(audible);
    hm_spu_adsr_key_off(&whole[0].envelope);
    hm_spu_voice_mix(whole, a, 1024);
    CHECK(whole[0].envelope.phase == HM_ADSR_OFF);
    sample[1] = 1;
    memset(whole, 0, sizeof(whole));
    hm_spu_voice_start(whole, sample, 16, 4096, 0, 0x1FC0, 32767, 32767);
    hm_spu_voice_mix(whole, a, 64);
    CHECK(whole[0].envelope.phase == HM_ADSR_OFF);
    CHECK(!a[126] && !a[127]);
    return 0;
}
