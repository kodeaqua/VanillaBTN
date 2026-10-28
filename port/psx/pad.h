#ifndef HMBTN_PSX_PAD_H
#define HMBTN_PSX_PAD_H

#include "compat/types.h"

enum {
    HM_PAD_L2       = 0x0001,
    HM_PAD_R2       = 0x0002,
    HM_PAD_L1       = 0x0004,
    HM_PAD_R1       = 0x0008,
    HM_PAD_TRIANGLE = 0x0010,
    HM_PAD_CIRCLE   = 0x0020,
    HM_PAD_CROSS    = 0x0040,
    HM_PAD_SQUARE   = 0x0080,
    HM_PAD_SELECT   = 0x0100,
    HM_PAD_START    = 0x0800,
    HM_PAD_UP       = 0x1000,
    HM_PAD_RIGHT    = 0x2000,
    HM_PAD_DOWN     = 0x4000,
    HM_PAD_LEFT     = 0x8000
};

/* The platform samples these held-button bits in the game's native PadButton
 * order. The runtime publishes a digital pad reply on the game thread. */
void hm_pad_sample(s32 port, int connected, u16 held);

#endif
