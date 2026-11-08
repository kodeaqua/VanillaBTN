#ifndef VANILLABTN_COMPAT_LIBGS_H
#define VANILLABTN_COMPAT_LIBGS_H

#include "sdk_types.h"
#include <libgte.h>

/* Pointers belong to the host; this runtime descriptor is never read from a
 * serialized PS1 structure. Packet tags themselves remain 32-bit words. */
typedef struct GsOT {
    u_long length;
    u_long *org;
    u_short offset, point;
    u_long *tag;
} GsOT;

extern short HWD0, VWD0;
extern MATRIX GsIDMATRIX;

/* Original scene descriptors are runtime data. Declaring the remaining GS
 * lighting boundary does not provide a pretend implementation: linking a
 * caller still requires its platform implementation. */
typedef struct GsF_LIGHT {
    int vx, vy, vz;
    unsigned char r, g, b;
} GsF_LIGHT;

int GsSetFlatLight(int id, GsF_LIGHT *light);
void GsSetAmbient(long r, long g, long b);
void GsSetLightMode(int mode);
void GsSetProjection(long distance);

#endif
