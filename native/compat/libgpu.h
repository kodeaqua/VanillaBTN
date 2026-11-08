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
typedef struct DR_AREA { u_long tag, code[2]; } DR_AREA;
typedef struct DR_TPAGE { u_long tag, code[1]; } DR_TPAGE;
typedef struct DR_LOAD { u_long tag, code[3], p[13]; } DR_LOAD;
typedef struct DR_ENV { u_long tag, code[15]; } DR_ENV;

typedef struct POLY_F3 {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0, x1, y1, x2, y2;
} POLY_F3;

typedef struct POLY_FT3 {
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
    u_short pad1;
} POLY_FT3;

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

typedef struct POLY_F4 {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0, x1, y1, x2, y2, x3, y3;
} POLY_F4;

typedef struct POLY_G3 {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, pad1;
    short x1, y1;
    u_char r2, g2, b2, pad2;
    short x2, y2;
} POLY_G3;

typedef struct POLY_GT3 {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    u_char r1, g1, b1, p1;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    u_char r2, g2, b2, p2;
    short x2, y2;
    u_char u2, v2;
    u_short pad2;
} POLY_GT3;

typedef struct POLY_G4 {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, pad1;
    short x1, y1;
    u_char r2, g2, b2, pad2;
    short x2, y2;
    u_char r3, g3, b3, pad3;
    short x3, y3;
} POLY_G4;

typedef struct POLY_GT4 {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    u_char r1, g1, b1, p1;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    u_char r2, g2, b2, p2;
    short x2, y2;
    u_char u2, v2;
    u_short pad2;
    u_char r3, g3, b3, p3;
    short x3, y3;
    u_char u3, v3;
    u_short pad3;
} POLY_GT4;

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
 * pointers. Arenas may be registered explicitly; module-owned OTs and private
 * static packets are mapped by ClearOTagR/addPrim from trusted host pointers. */
void sdk_add_prim(void *ot, void *packet);
void ClearOTagR(u_long *ot, long count);
void DrawOTag(u_long *ot);

#define setlen(p, n) (*(u_long *)(p) = (*(u_long *)(p) & 0xFFFFFFul) | ((u_long)(n) << 24))
#define addPrim(ot, p) sdk_add_prim((ot), (p))
#define AddPrim(ot, p) sdk_add_prim((ot), (p))
#define getClut(x, y) ((u_short)(((unsigned)(y) << 6) | (((unsigned)(x) >> 4) & 63)))
#define getTPage(tp, abr, x, y) ((u_short)(((tp) << 7) | ((abr) << 5) | (((y) & 256) >> 4) | (((x) & 1023) >> 6) | (((y) & 512) << 2)))
#define setSprt(p) (setlen((p), 4), (p)->code = 0x64)
#define setPolyF4(p) (setlen((p), 5), (p)->code = 0x28)
#define SetPolyF4(p) setPolyF4(p)
#define setPolyFT4(p) (setlen((p), 9), (p)->code = 0x2C)
#define setSemiTrans(p, enabled) ((p)->code = ((p)->code & ~2) | ((enabled) ? 2 : 0))
#define SetSemiTrans(p, enabled) setSemiTrans(p, enabled)
#define setShadeTex(p, enabled) ((p)->code = ((p)->code & ~1) | ((enabled) ? 1 : 0))
#define setRGB0(p, r, g, b) ((p)->r0 = (u_char)(r), (p)->g0 = (u_char)(g), (p)->b0 = (u_char)(b))
#define setXY0(p, x, y) ((p)->x0 = (short)(x), (p)->y0 = (short)(y))
#define setWH(p, width, height) ((p)->w = (short)(width), (p)->h = (short)(height))
#define setXYWH(p, x, y, w, h) \
    ((p)->x0 = (short)(x), (p)->y0 = (short)(y), \
     (p)->x1 = (short)((x) + (w)), (p)->y1 = (short)(y), \
     (p)->x2 = (short)(x), (p)->y2 = (short)((y) + (h)), \
     (p)->x3 = (short)((x) + (w)), (p)->y3 = (short)((y) + (h)))
#define setUV0(p, u, v) ((p)->u0 = (u_char)(u), (p)->v0 = (u_char)(v))
#define setUVWH(p, u, v, w, h) \
    ((p)->u0 = (u_char)(u), (p)->v0 = (u_char)(v), \
     (p)->u1 = (u_char)((u) + (w)), (p)->v1 = (u_char)(v), \
     (p)->u2 = (u_char)(u), (p)->v2 = (u_char)((v) + (h)), \
     (p)->u3 = (u_char)((u) + (w)), (p)->v3 = (u_char)((v) + (h)))
#define GetTPage(tp, abr, x, y) getTPage(tp, abr, x, y)
#define GetClut(x, y) getClut(x, y)
#define setDrawTPage(p, dfe, dtd, page) (setlen((p), 1), (p)->code[0] = 0xE1000000ul | ((dfe) ? 1024 : 0) | ((dtd) ? 512 : 0) | (page))

int LoadImage(RECT *rect, u_long *pixels);
int StoreImage(RECT *rect, u_long *pixels);
int DrawSync(int mode);
void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
void SetDrawArea(DR_AREA *p, RECT *r);
void SetDrawLoad(DR_LOAD *p, RECT *r);
DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
DISPENV *GetDispEnv(DISPENV *env);
int ClearImage(RECT *rect, u_char r, u_char g, u_char b);
int MoveImage(RECT *rect, int x, int y);
#define MoveImage2(rect, x, y) MoveImage((rect), (x), (y))
void SetDispMask(int mask);

#ifdef NDEBUG
#define FntPrint(...) ((void)0)
#define FntFlush(...) ((void)0)
#define FntLoad(...) ((void)0)
#define FntOpen(...) (0)
#define SetDumpFnt(...) ((void)0)
#define KanjiFntOpen(...) (0)
#define KanjiFntClose() ((void)0)
#define KanjiFntPrint(...) (0)
#define KanjiFntFlush(...) ((void)0)
#else
void FntPrint(const char *format, ...);
void FntFlush(int id);
void FntLoad(int tx, int ty);
int FntOpen(int x, int y, int w, int h, int isbg, int n);
void SetDumpFnt(int id);
int KanjiFntOpen(int x, int y, int w, int h, int dx, int dy, int cx, int cy, int isbg, int n);
void KanjiFntClose(void);
int KanjiFntPrint(const char *format, ...);
void KanjiFntFlush(int id);
#endif

int ResetGraph(int mode);
int SetGraphDebug(int level);

#endif
