#ifndef HMBTN_COMPAT_LIBGS_H
#define HMBTN_COMPAT_LIBGS_H

#include <libgte.h>

typedef u32 GsOT_TAG;

/* Host-only descriptors: retain native pointers, not the PS1 descriptor size. */
typedef struct {
    u32 length;
    GsOT_TAG *org;
    u32 offset, point;
    GsOT_TAG *tag;
} GsOT;

typedef struct {
    s32 vx, vy, vz;
    u8 r, g, b, pad;
} GsF_LIGHT;

#endif
