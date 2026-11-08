#ifndef VANILLABTN_COMPAT_LIBGPU_H
#define VANILLABTN_COMPAT_LIBGPU_H

#include "sdk_types.h"

typedef struct RECT {
    short x, y, w, h;
} RECT;

/* Opaque until primitive submission is connected; never allocate or serialize
 * these incomplete types as if their native layout were the PS1 layout. */
typedef struct SPRT SPRT;
typedef struct DR_MODE DR_MODE;
typedef struct POLY_FT4 POLY_FT4;

int LoadImage(RECT *rect, u_long *pixels);
int StoreImage(RECT *rect, u_long *pixels);
int DrawSync(int mode);

#endif
