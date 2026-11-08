#ifndef VANILLABTN_COMPAT_LIBGTE_H
#define VANILLABTN_COMPAT_LIBGTE_H

/* Only declarations needed by the currently compiled original modules belong
 * here. Unsupported GTE operations must fail compilation rather than no-op. */
#include "sdk_types.h"

typedef struct VECTOR { long vx, vy, vz, pad; } VECTOR;
typedef struct SVECTOR { short vx, vy, vz, pad; } SVECTOR;
typedef struct DVECTOR { short vx, vy; } DVECTOR;
typedef struct CVECTOR { u_char r, g, b, cd; } CVECTOR;
typedef struct MATRIX { short m[3][3]; long t[3]; } MATRIX;

int csqrt(int value);
long ratan2(long y, long x);

#endif
