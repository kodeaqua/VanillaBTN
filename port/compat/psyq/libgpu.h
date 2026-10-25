#ifndef HMBTN_COMPAT_LIBGPU_H
#define HMBTN_COMPAT_LIBGPU_H

#include "compat/types.h"

/* Packet fields follow public GP0 command formats and src/ field use.
 * The first word is a PS1 DMA tag, never a truncated host pointer.
 * No OT traversal or rasterizer is provided by these declarations. */
typedef struct { s16 x, y, w, h; } RECT;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, x1, y1, x2, y2;
} POLY_F3;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, x1, y1, x2, y2, x3, y3;
} POLY_F4;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad2;
} POLY_FT3;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad2;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad3;
} POLY_FT4;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
} POLY_G3;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} POLY_G4;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad3;
} POLY_GT3;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad3;
    u8 r3, g3, b3, pad4;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad5;
} POLY_GT4;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} SPRT;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, w, h;
} TILE;
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0, x1, y1;
} LINE_F2;

typedef struct { u32 tag, code[1]; } DR_TPAGE;
typedef struct { u32 tag, code[2]; } DR_MODE;
typedef struct { u32 tag, code[2]; } DR_AREA;
/* clut.c reserves 0x44 bytes for this inline transfer packet. */
typedef struct { u32 tag, code[3], p[13]; } DR_LOAD;
typedef struct { u32 tag, code[15]; } DR_ENV;

/* Host-side environments have no pointers and retain the game's field layout. */
typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;
typedef struct {
    RECT disp, screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

#define HM_GPU_SIZE(type, bytes) _Static_assert(sizeof(type) == (bytes), #type " packet size")
HM_GPU_SIZE(RECT, 8);
HM_GPU_SIZE(POLY_F3, 20);
HM_GPU_SIZE(POLY_F4, 24);
HM_GPU_SIZE(POLY_FT3, 32);
HM_GPU_SIZE(POLY_FT4, 40);
HM_GPU_SIZE(POLY_G3, 28);
HM_GPU_SIZE(POLY_G4, 36);
HM_GPU_SIZE(POLY_GT3, 40);
HM_GPU_SIZE(POLY_GT4, 52);
HM_GPU_SIZE(SPRT, 20);
HM_GPU_SIZE(TILE, 16);
HM_GPU_SIZE(LINE_F2, 16);
HM_GPU_SIZE(DR_TPAGE, 8);
HM_GPU_SIZE(DR_MODE, 12);
HM_GPU_SIZE(DR_AREA, 12);
HM_GPU_SIZE(DR_LOAD, 68);
HM_GPU_SIZE(DRAWENV, 92);
HM_GPU_SIZE(DISPENV, 20);
#undef HM_GPU_SIZE

#define HM_GPU_OFFSET(type, field, bytes)                                                            \
    _Static_assert(__builtin_offsetof(type, field) == (bytes), #type "." #field " offset")
HM_GPU_OFFSET(POLY_F3, code, 7);
HM_GPU_OFFSET(POLY_F3, x2, 16);
HM_GPU_OFFSET(POLY_F4, x3, 20);
HM_GPU_OFFSET(POLY_FT3, clut, 14);
HM_GPU_OFFSET(POLY_FT3, tpage, 22);
HM_GPU_OFFSET(POLY_FT4, x3, 32);
HM_GPU_OFFSET(POLY_G3, r1, 12);
HM_GPU_OFFSET(POLY_G4, x3, 32);
HM_GPU_OFFSET(POLY_GT3, tpage, 26);
HM_GPU_OFFSET(POLY_GT4, x3, 44);
HM_GPU_OFFSET(SPRT, w, 16);
HM_GPU_OFFSET(TILE, w, 12);
HM_GPU_OFFSET(LINE_F2, x1, 12);
HM_GPU_OFFSET(DR_TPAGE, code, 4);
HM_GPU_OFFSET(DR_MODE, code, 4);
HM_GPU_OFFSET(DR_AREA, code, 4);
HM_GPU_OFFSET(DR_LOAD, p, 16);
HM_GPU_OFFSET(DRAWENV, ofs, 8);
HM_GPU_OFFSET(DRAWENV, tw, 12);
HM_GPU_OFFSET(DRAWENV, tpage, 20);
HM_GPU_OFFSET(DRAWENV, dr_env, 28);
HM_GPU_OFFSET(DISPENV, screen, 8);
HM_GPU_OFFSET(DISPENV, isinter, 16);
#undef HM_GPU_OFFSET

s32 DrawSync(s32 mode);
s32 ResetGraph(s32 mode);
s32 SetGraphDebug(s32 mode);
DRAWENV *SetDefDrawEnv(DRAWENV *environment, s32 x, s32 y, s32 width, s32 height);
DISPENV *SetDefDispEnv(DISPENV *environment, s32 x, s32 y, s32 width, s32 height);
DRAWENV *PutDrawEnv(DRAWENV *environment);
DISPENV *PutDispEnv(DISPENV *environment);
u32 *ClearOTagR(u32 *table, s32 entries);
void DrawOTag(u32 *table);

#endif
