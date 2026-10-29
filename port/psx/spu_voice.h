#ifndef HMBTN_PSX_SPU_VOICE_H
#define HMBTN_PSX_SPU_VOICE_H
#include "psx/spu_decode.h"
typedef struct {
    const u8 *data;
    u32 bytes, block, loop, fraction, pitch, decoded_at;
    hm_adpcm_history history;
    hm_adsr envelope;
    s16 decoded[28], recent[4], left, right;
    u8 flags;
    u8 reverb;
} hm_spu_voice;
void hm_spu_voice_start(hm_spu_voice *voice, const void *data, u32 bytes, u32 pitch, u16 adsr1,
                        u16 adsr2, s16 left, s16 right);
void hm_spu_voice_mix(hm_spu_voice voices[24], s16 *stereo, u32 frames);
void hm_spu_voice_mix_reverb(hm_spu_voice voices[24], s16 *stereo, u32 frames);
s16 hm_spu_gaussian(const s16 recent[4], u32 fraction);
void hm_spu_set_hq(int enabled);
int hm_spu_get_hq(void);
#endif
