#ifndef HMBTN_PSX_SPU_DECODE_H
#define HMBTN_PSX_SPU_DECODE_H
#include "compat/types.h"
typedef struct {
    s32 previous, older;
} hm_adpcm_history;
/* One SPU block is exactly 16 bytes / 28 signed PCM samples. Returns zero for
 * an unsupported predictor; caller handles loop/end flags from block[1]. */
int hm_spu_decode_block(const u8 block[16], hm_adpcm_history *history, s16 output[28]);

enum { HM_ADSR_OFF, HM_ADSR_ATTACK, HM_ADSR_DECAY, HM_ADSR_SUSTAIN, HM_ADSR_RELEASE };
typedef struct {
    u16 adsr1, adsr2;
    u32 counter;
    s32 level;
    u8 phase;
} hm_adsr;
void hm_spu_adsr_key_on(hm_adsr *envelope, u16 adsr1, u16 adsr2);
void hm_spu_adsr_key_off(hm_adsr *envelope);
s32 hm_spu_adsr_tick(hm_adsr *envelope);
#endif
