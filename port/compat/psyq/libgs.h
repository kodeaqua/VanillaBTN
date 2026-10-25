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

void GsInitGraph(s32 width, s32 height, s32 mode, s32 interlace, s32 dither);
void GsDefDispBuff(s32 x0, s32 y0, s32 x1, s32 y1);
void GsSetOffset(s32 x, s32 y);
void GsSetDrawBuffOffset(void);
void GsInit3D(void);
void *GsGetWorkBase(void);
void GsSetWorkBase(void *base);

#endif
