#ifndef PRIM_H
#define PRIM_H

/* Building the GPU primitives into the prim buffer. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* PSY-Q's POLY_FT4, with each x/y pair and u/v/clut group as one word. */
typedef struct PolyFT4 {
    u8 addr[3];         /* ordering-table link */
    u8 len;
    u8 r0, g0, b0;
    u8 code;
    s32 xy0;
    u32 uvClut0;
    s32 xy1;
    u32 uvTpage1;
    s32 xy2;
    u32 uv2;
    s32 xy3;
    u32 uv3;
} PolyFT4;

/* The primitive cursor: the buffer holds packets of every kind, appended by
 * size, and each drawer walks it as the packet it writes. */
typedef union PrimCursor {
    u8 *bytes;
    u_long *tag;
    POLY_FT4 *ft4;
    SPRT *sprt;
    DR_MODE *mode;
    PolyFT4 *ft4Words;  /* the GTE stores whole words */
} PrimCursor;

/* Link the primitive `p`, a `type`, into the ordering table `ot`: its tag takes
 * the next link in the low 24 bits and the packet's length in words after the
 * tag in the top byte, and the table entry takes the primitive. */
#define LINK_PRIM(ot, p, type)                                                  \
    (*(u32 *)(p) = ((u32)(sizeof(type) / 4 - 1) << 24) | (*(ot) & 0xFFFFFF),   \
     *(ot) = (u32)(p) & 0xFFFFFF)

void prim_add_pixel(void *ot, s16 x, s16 y, s32 color, s16 semi_trans);
void prim_add_line(void *ot, s16 x0, s16 y0, s16 x1, s16 y1, s32 color, s16 semi_trans);
void prim_add_gouraud_rect(void *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1, s32 c2, s32 c3,
                          s16 semi_trans);
void prim_add_rect(void *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans);
void prim_add_tile(void *ot, s16 x, s16 y, s16 w, s16 h, s32 packed, s16 flag);
void prim_add_rect_mode(void *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans, u8 flags);
TILE_1 *prim_make_pixel(TILE_1 *tile, void *ot, s16 x, s16 y, s32 color, s16 semi_trans);
LINE_F2 *prim_make_line(LINE_F2 *line, void *ot, s16 x0, s16 y0, s16 x1, s16 y1, s32 color,
                       s16 semi_trans);
POLY_G4 *prim_make_gouraud_rect(POLY_G4 *poly, void *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1,
                                s32 c2, s32 c3, s16 semi_trans);
void *prim_make_rect(POLY_F4 *poly, void *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans);
TILE *prim_make_tile(TILE *dst, void *src, s16 x, s16 y, s16 w, s16 h, s32 packed, s16 flag);
void *prim_make_rect_mode(void *first, void *source, s16 x, s16 y, s16 w, s16 h, s32 color,
                          s16 semi_trans, u8 flags);

#endif /* PRIM_H */
