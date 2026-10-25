#ifndef HMBTN_COMPAT_LIBGTE_H
#define HMBTN_COMPAT_LIBGTE_H

#include "compat/types.h"

/* Source-facing declarations inferred from game call sites, not SDK headers. */
typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

_Static_assert(sizeof(VECTOR) == 16, "Four PS1 words per VECTOR");
_Static_assert(__builtin_offsetof(VECTOR, vx) == 0, "VECTOR x offset");
_Static_assert(__builtin_offsetof(VECTOR, vy) == 4, "VECTOR y offset");
_Static_assert(__builtin_offsetof(VECTOR, vz) == 8, "VECTOR z offset");
_Static_assert(__builtin_offsetof(VECTOR, pad) == 12, "VECTOR padding offset");

s32 rsin(s32 angle);
s32 rcos(s32 angle);
s32 SquareRoot0(s32 value);
s32 VectorNormal(VECTOR *input, VECTOR *output);

#endif
