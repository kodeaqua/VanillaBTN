#ifndef HMBTN_PSX_SOUND_BANK_H
#define HMBTN_PSX_SOUND_BANK_H
#include "compat/types.h"
typedef struct hm_sound_samples hm_sound_samples;
typedef struct {
    u8 *header, *body;
    u32 header_bytes, body_bytes, uploaded;
    u16 programs, samples;
    hm_sound_samples *storage;
} hm_sound_bank;
const hm_sound_bank *hm_sound_bank_get(s32 bank);
void hm_sound_banks_reset(void);
const u8 *hm_sound_bank_tone(s32 bank, u32 program, u32 tone);
const u8 *hm_sound_bank_sample(s32 bank, u32 sample, u32 *bytes);
hm_sound_samples *hm_sound_samples_retain(s32 bank);
void hm_sound_samples_release(hm_sound_samples *samples);
#endif
