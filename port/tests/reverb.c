#include "psx/reverb.h"
#include "psx/spu_voice.h"
#include <libsnd.h>
#include <stdio.h>
#include <string.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static s16 whole[20000], split[20000];
int main(void) {
    SsInit();
    const s32 types[] = {1, 4, 5};
    for (u32 preset = 0; preset < 3; ++preset) {
        CHECK(SsUtSetReverbType(types[preset]) == types[preset]);
        SsUtReverbOn();
        SsUtSetReverbDepth(127, 127);
        int audible = 0;
        for (u32 i = 0; i < 65536; ++i) {
            s32 wet[2];
            hm_reverb_sample(i == 0 ? 32767 : 0, 0, wet);
            audible |= wet[0] != 0 || wet[1] != 0;
        }
        CHECK(audible);
    }
    CHECK(SsUtSetReverbType(0) == 0);
    s32 wet[2];
    hm_reverb_sample(32767, 32767, wet);
    CHECK(!wet[0] && !wet[1]);
    u8 sample[16];
    memset(sample, 0x77, sizeof(sample));
    sample[0] = 4;
    sample[1] = 7;
    hm_spu_voice voices[24] = {0};
    hm_spu_voice_start(voices, sample, sizeof(sample), 4096, 15, 0x1FC0, 32767, 32767);
    voices[0].reverb = 1;
    hm_spu_voice reference[24];
    memcpy(reference, voices, sizeof(voices));
    SsUtSetReverbType(5);
    SsUtSetReverbDepth(64, 64);
    hm_spu_voice_mix_reverb(voices, whole, 10000);
    SsUtSetReverbType(5);
    SsUtSetReverbDepth(64, 64);
    hm_spu_voice_mix_reverb(reference, split, 333);
    hm_spu_voice_mix_reverb(reference, split + 666, 9667);
    CHECK(!memcmp(whole, split, sizeof(whole)));
    return 0;
}
