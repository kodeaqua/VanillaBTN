#include "psx/spu_decode.h"
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
    u8 block[16] = {0};
    s16 output[28];
    hm_adpcm_history history = {0};
    block[2] = 0x87;
    CHECK(hm_spu_decode_block(block, &history, output));
    CHECK(output[0] == 28672 && output[1] == -32768 && output[2] == 0);
    block[0] = 12;
    CHECK(hm_spu_decode_block(block, &history, output));
    CHECK(output[0] == 7 && output[1] == -8);
    block[0] = 15;
    CHECK(hm_spu_decode_block(block, &history, output));
    CHECK(output[0] == 56 && output[1] == -64);
    memset(block, 0, sizeof(block));
    block[0] = 0x10;
    history = (hm_adpcm_history){4096, 0};
    CHECK(hm_spu_decode_block(block, &history, output));
    CHECK(output[0] == 3840 && output[1] == 3600 && output[2] == 3375 && output[3] == 3164);
    history = (hm_adpcm_history){-4096, 0};
    CHECK(hm_spu_decode_block(block, &history, output));
    CHECK(output[0] == -3840 && output[3] == -3164);
    const s16 predictions[3] = {2203, 1961, 2375};
    for (u32 filter = 2; filter <= 4; ++filter) {
        block[0] = (u8)(filter << 4);
        history = (hm_adpcm_history){1000, -500};
        CHECK(hm_spu_decode_block(block, &history, output) && output[0] == predictions[filter - 2]);
    }
    history = (hm_adpcm_history){32767, -32768};
    CHECK(hm_spu_decode_block(block, &history, output) && output[0] == 32767);
    block[0] = 0x50;
    CHECK(!hm_spu_decode_block(block, &history, output));

    hm_adsr envelope;
    hm_spu_adsr_key_on(&envelope, 0, 0);
    CHECK(hm_spu_adsr_tick(&envelope) == 14336);
    CHECK(hm_spu_adsr_tick(&envelope) == 28672);
    CHECK(hm_spu_adsr_tick(&envelope) == 32767 && envelope.phase == HM_ADSR_DECAY);
    CHECK(hm_spu_adsr_tick(&envelope) == 16383);
    for (u32 i = 0; i < 10 && envelope.phase == HM_ADSR_DECAY; ++i)
        hm_spu_adsr_tick(&envelope);
    CHECK(envelope.phase == HM_ADSR_SUSTAIN);
    hm_spu_adsr_key_off(&envelope);
    CHECK(hm_spu_adsr_tick(&envelope) == 0 && envelope.phase == HM_ADSR_OFF);
    hm_spu_adsr_key_on(&envelope, 0x7F00, 31);
    for (u32 i = 0; i < 50000; ++i)
        CHECK(hm_spu_adsr_tick(&envelope) == 0);
    envelope.level = 12345;
    hm_spu_adsr_key_off(&envelope);
    for (u32 i = 0; i < 50000; ++i)
        CHECK(hm_spu_adsr_tick(&envelope) == 12345);
    return 0;
}
