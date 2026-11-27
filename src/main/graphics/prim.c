/* Primitive builders: the wrappers that take a primitive from the prim buffer and fill in a tile, line or rectangle. */
#include "types.h"
#include "main.h"
#include "prim.h"

/* The GP0 command codes of the primitives built here; bit 1 makes one semi-transparent. */
#define PRIM_CODE_DOT 0x68
#define PRIM_CODE_LINE 0x40
#define PRIM_CODE_GOURAUD_RECT 0x38
#define PRIM_CODE_RECT 0x28
#define PRIM_CODE_TILE 0x60
#define PRIM_SEMI_TRANS 2

/* Add a primitive to `ot`. Each takes its packet from the work buffer (gs_work_alloc)
 * and links it in with its prim_make_* builder. */
void prim_add_pixel(void *ot, s16 x, s16 y, s32 color, s16 semi_trans) {
    prim_make_pixel(gs_work_alloc(sizeof(TILE_1)), ot, x, y, color, semi_trans);
}

void prim_add_line(void *ot, s16 x0, s16 y0, s16 x1, s16 y1, s32 color, s16 semi_trans) {
    prim_make_line(gs_work_alloc(sizeof(LINE_F2)), ot, x0, y0, x1, y1, color, semi_trans);
}

void prim_add_gouraud_rect(void *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1, s32 c2, s32 c3,
                          s16 semi_trans) {
    prim_make_gouraud_rect(gs_work_alloc(sizeof(POLY_G4)), ot, x, y, w, h, c0, c1, c2, c3,
                           semi_trans);
}

void prim_add_rect(void *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans) {
    prim_make_rect(gs_work_alloc(sizeof(POLY_F4)), ot, x, y, w, h, color, semi_trans);
}

void prim_add_tile(void *ot, s16 x, s16 y, s16 w, s16 h, s32 packed, s16 flag) {
    prim_make_tile(gs_work_alloc(sizeof(TILE)), ot, x, y, w, h, packed, flag);
}

void prim_add_rect_mode(void *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans, u8 flags) {
    gs_work_release(prim_make_rect_mode(gs_work_alloc(0), ot, x, y, w, h, color, semi_trans, flags));
}

/* Build a one-pixel tile at (x, y) in colour 0xRRGGBB, optionally
 * semi-transparent, link it into `ot` and return the next free packet. */
TILE_1 *prim_make_pixel(TILE_1 *tile, void *ot, s16 x, s16 y, s32 color, s16 semi_trans) {
    setlen(tile, 2);
    tile->code = PRIM_CODE_DOT;
    tile->code = semi_trans ? tile->code | PRIM_SEMI_TRANS : tile->code & ~PRIM_SEMI_TRANS;
    tile->x0 = x;
    tile->y0 = y;
    tile->r0 = color >> 16;
    tile->g0 = color >> 8;
    tile->b0 = color;
    addPrim(ot, tile);
    return tile + 1;
}

/* Build a flat line from (x0, y0) to (x1, y1) in colour 0xRRGGBB, optionally
 * semi-transparent, link it into `ot` and return the next free packet. */
LINE_F2 *prim_make_line(LINE_F2 *line, void *ot, s16 x0, s16 y0, s16 x1, s16 y1, s32 color,
                       s16 semi_trans) {
    setlen(line, 3);
    line->code = PRIM_CODE_LINE;
    line->code = semi_trans ? line->code | PRIM_SEMI_TRANS : line->code & ~PRIM_SEMI_TRANS;
    line->x0 = x0;
    line->y0 = y0;
    line->x1 = x1;
    line->y1 = y1;
    line->r0 = color >> 16;
    line->g0 = color >> 8;
    line->b0 = color;
    addPrim(ot, line);
    return line + 1;
}

/* Build a Gouraud-shaded w x h rectangle at (x, y) with corner colours
 * 0xRRGGBB c0..c3, optionally semi-transparent, link it into `ot` and
 * return the next free packet. */
POLY_G4 *prim_make_gouraud_rect(POLY_G4 *poly, void *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1,
                                s32 c2, s32 c3, s16 semi_trans) {
    setlen(poly, 8);
    poly->code = PRIM_CODE_GOURAUD_RECT;
    poly->code = semi_trans ? poly->code | PRIM_SEMI_TRANS : poly->code & ~PRIM_SEMI_TRANS;
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + w;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->r0 = c0 >> 16;
    poly->g0 = c0 >> 8;
    poly->b0 = c0;
    poly->r1 = c1 >> 16;
    poly->g1 = c1 >> 8;
    poly->b1 = c1;
    poly->r2 = c2 >> 16;
    poly->g2 = c2 >> 8;
    poly->b2 = c2;
    poly->r3 = c3 >> 16;
    poly->g3 = c3 >> 8;
    poly->b3 = c3;
    addPrim(ot, poly);
    return poly + 1;
}

/* Build a flat w x h rectangle at (x, y) in colour 0xRRGGBB, optionally
 * semi-transparent, link it into `ot` and return the next free packet. */
void *prim_make_rect(POLY_F4 *poly, void *ot, s16 x, s16 y, s16 w, s16 h, s32 color, s16 semi_trans) {
    setlen(poly, 5);
    poly->code = PRIM_CODE_RECT;
    poly->r0 = color >> 16;
    poly->g0 = color >> 8;
    poly->b0 = color;
    poly->code = semi_trans ? poly->code | PRIM_SEMI_TRANS : poly->code & ~PRIM_SEMI_TRANS;
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + w;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    addPrim(ot, poly);
    return poly + 1;
}

/* Build a w x h tile at (x, y) in colour 0xRRGGBB, optionally semi-transparent, link it
 * into `src` and return the next free packet. */
TILE *prim_make_tile(TILE *dst, void *src, s16 x, s16 y, s16 w, s16 h, s32 packed, s16 flag) {
    setlen(dst, 3);
    dst->code = PRIM_CODE_TILE;
    dst->r0 = packed >> 16;
    dst->g0 = packed >> 8;
    dst->b0 = packed;
    dst->code = flag ? dst->code | PRIM_SEMI_TRANS : dst->code & ~PRIM_SEMI_TRANS;
    dst->h = h;
    dst->w = w;
    dst->x0 = x;
    dst->y0 = y;
    addPrim(src, dst);
    return dst + 1;
}

/* Build a flat rectangle as prim_make_rect does, then a draw-mode packet that sets its
 * semi-transparency mode (bits 5 and 6) from flags. Returns the next free packet. */
void *prim_make_rect_mode(void *first, void *source, s16 x, s16 y, s16 w, s16 h, s32 color,
                          s16 semi_trans, u8 flags) {
    DR_TPAGE *mode;
    mode = prim_make_rect(first, source, x, y, w, h, color, semi_trans);
    setlen(mode, 1);
    mode->code[0] = ((flags & 3) << 5) | 0xE1000200;
    addPrim(source, mode);
    mode++;
    return mode;
}
