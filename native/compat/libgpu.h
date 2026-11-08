#ifndef VANILLABTN_COMPAT_LIBGPU_H
#define VANILLABTN_COMPAT_LIBGPU_H

#include "sdk_types.h"

typedef struct RECT {
    short x, y, w, h;
} RECT;

typedef struct SPRT {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short w, h;
} SPRT;

typedef struct TILE {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0, w, h;
} TILE;

typedef struct DR_MODE { u_long tag, code[2]; } DR_MODE;
typedef struct DR_TPAGE { u_long tag, code[1]; } DR_TPAGE;
typedef struct DR_ENV { u_long tag, code[15]; } DR_ENV;

typedef struct POLY_FT4 {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    short x2, y2;
    u_char u2, v2;
    u_short pad2;
    short x3, y3;
    u_char u3, v3;
    u_short pad3;
} POLY_FT4;

typedef struct DRAWENV {
    RECT clip;
    short ofs[2];
    RECT tw;
    u_short tpage;
    u_char dtd, dfe, isbg, r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;

typedef struct DISPENV {
    RECT disp, screen;
    u_char isinter, isrgb24, pad0, pad1;
} DISPENV;

/* Tags keep PS1 packet lengths but hold mapped IDs instead of truncated x64
 * pointers. All referenced memory must be registered with sdk_gpu_map. */
void sdk_add_prim(void *ot, void *packet);
void ClearOTagR(u_long *ot, long count);
void DrawOTag(u_long *ot);

#define setlen(p, n) (*(u_long *)(p) = (*(u_long *)(p) & 0xFFFFFFul) | ((u_long)(n) << 24))
#define addPrim(ot, p) sdk_add_prim((ot), (p))
#define getClut(x, y) ((u_short)(((unsigned)(y) << 6) | (((unsigned)(x) >> 4) & 63)))
#define getTPage(tp, abr, x, y) ((u_short)(((tp) << 7) | ((abr) << 5) | (((y) & 256) >> 4) | (((x) & 1023) >> 6) | (((y) & 512) << 2)))
#define setSprt(p) (setlen((p), 4), (p)->code = 0x64)
#define setPolyFT4(p) (setlen((p), 9), (p)->code = 0x2C)
#define setSemiTrans(p, enabled) ((p)->code = ((p)->code & ~2) | ((enabled) ? 2 : 0))
#define setShadeTex(p, enabled) ((p)->code = ((p)->code & ~1) | ((enabled) ? 1 : 0))
#define setXY0(p, x, y) ((p)->x0 = (short)(x), (p)->y0 = (short)(y))
#define setWH(p, width, height) ((p)->w = (short)(width), (p)->h = (short)(height))
#define setUV0(p, u, v) ((p)->u0 = (u_char)(u), (p)->v0 = (u_char)(v))
#define setDrawTPage(p, dfe, dtd, page) (setlen((p), 1), (p)->code[0] = 0xE1000000ul | ((dfe) ? 1024 : 0) | ((dtd) ? 512 : 0) | (page))

int LoadImage(RECT *rect, u_long *pixels);
int StoreImage(RECT *rect, u_long *pixels);
int DrawSync(int mode);

#endif
