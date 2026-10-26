#ifndef HMBTN_COMPAT_FIXED_MATH_H
#define HMBTN_COMPAT_FIXED_MATH_H

#include "compat/types.h"

/* Avoid collisions with the host C99 complex math names. */
s32 hm_psx_csin(s32 angle);
s32 hm_psx_ccos(s32 angle);
s32 hm_psx_csqrt(s32 value);
#define csin hm_psx_csin
#define ccos hm_psx_ccos
#define csqrt hm_psx_csqrt

#endif
