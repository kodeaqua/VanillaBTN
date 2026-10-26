#ifndef HMBTN_COMPAT_LIBETC_H
#define HMBTN_COMPAT_LIBETC_H

#include "compat/types.h"

s32 VSync(s32 mode);
s32 SetVideoMode(s32 mode);
/* afile.c obtains callback setup through libetc.h as well. */
s32 ResetCallback(void);
void (*VSyncCallback(void (*callback)(void)))(void);

#endif
