#ifndef HMBTN_COMPAT_LIBETC_H
#define HMBTN_COMPAT_LIBETC_H

#include "compat/types.h"

s32 VSync(s32 mode);
void (*VSyncCallback(void (*callback)(void)))(void);

#endif
