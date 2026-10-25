#ifndef HMBTN_COMPAT_LIBAPI_H
#define HMBTN_COMPAT_LIBAPI_H

#include "compat/types.h"

s32 ResetCallback(void);
void SetMem(s32 megabytes);
void SystemError(s32 code, s32 detail);

#endif
