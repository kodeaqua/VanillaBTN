#ifndef HMBTN_PSX_REVERB_H
#define HMBTN_PSX_REVERB_H
#include "compat/types.h"
void hm_reverb_reset(void);
void hm_reverb_sample(s32 left, s32 right, s32 output[2]);
#endif
