#ifndef HMBTN_PSX_PAD_H
#define HMBTN_PSX_PAD_H

#include "compat/types.h"

/* The platform samples these held-button bits in the game's native PadButton
 * order. The runtime publishes a digital pad reply on the game thread. */
void hm_pad_sample(s32 port, int connected, u16 held);

#endif
