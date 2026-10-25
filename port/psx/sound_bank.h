#ifndef HMBTN_PSX_SOUND_BANK_H
#define HMBTN_PSX_SOUND_BANK_H
#include "compat/types.h"
typedef struct {
    u8 *header, *body;
    u32 header_bytes, body_bytes, uploaded;
    u16 programs, samples;
} hm_sound_bank;
const hm_sound_bank *hm_sound_bank_get(s32 bank);
void hm_sound_banks_reset(void);
#endif
