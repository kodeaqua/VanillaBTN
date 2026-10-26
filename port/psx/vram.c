#include "psx/vram.h"
#include "psx/error.h"
#include <libgpu.h>
#include <string.h>

static hm_vram_context hm_default_vram;
static hm_vram_context *hm_context = &hm_default_vram;
const hm_vram_context *hm_vram_state(void) { return hm_context; }
#define hm_vram (hm_context->pixels)
#define hm_dirty (hm_context->dirty)
#define hm_set_mask (hm_context->set_mask)
#define hm_check_mask (hm_context->check_mask)

hm_vram_context *hm_vram_bind(hm_vram_context *context) {
    hm_vram_context *previous = hm_context;
    hm_context = context ? context : &hm_default_vram;
    return previous;
}
void hm_vram_snapshot(hm_vram_context *destination) {
    if (!destination)
        hm_runtime_error("hm_vram_snapshot", "null destination");
    memcpy(destination, hm_context, sizeof(*destination));
}

void hm_vram_reset(void) {
    memset(hm_vram, 0, sizeof(hm_vram));
    memset(hm_dirty, 255, sizeof(hm_dirty));
    hm_set_mask = hm_check_mask = 0;
}

u16 hm_vram_pixel(u32 x, u32 y) { return hm_vram[y & 511][x & 1023]; }

void hm_vram_mask(int set_mask, int check_mask) {
    hm_set_mask = set_mask ? 0x8000 : 0;
    hm_check_mask = check_mask ? 0x8000 : 0;
}

u64 hm_vram_dirty_row(u32 tile_y) {
    if (tile_y >= 32)
        hm_runtime_error("hm_vram_dirty_row", "invalid tile row");
    return hm_dirty[tile_y];
}

void hm_vram_acknowledge(u32 tile_y, u64 tiles) {
    if (tile_y >= 32)
        hm_runtime_error("hm_vram_acknowledge", "invalid tile row");
    hm_dirty[tile_y] &= ~tiles;
}

static void hm_vram_put(u32 x, u32 y, u16 value, int masked) {
    x &= 1023;
    y &= 511;
    if (masked) {
        if (hm_vram[y][x] & hm_check_mask)
            return;
        value |= hm_set_mask;
    }
    if (hm_vram[y][x] != value) {
        hm_vram[y][x] = value;
        hm_dirty[y / 16] |= UINT64_C(1) << (x / 16);
    }
}

void hm_vram_draw_pixel(u32 x, u32 y, u16 value) { hm_vram_put(x, y, value, 1); }

static void hm_vram_area(const RECT *area, const void *pixels, const char *api) {
    if (!area || !pixels)
        hm_runtime_error(api, "null image argument");
}

s32 LoadImage(const RECT *area, const u32 *pixels) {
    hm_vram_area(area, pixels, "LoadImage");
    u32 width = (((u32)area->w - 1) & 1023) + 1;
    u32 height = (((u32)area->h - 1) & 511) + 1;
    const u8 *in = (const u8 *)pixels;
    for (u32 y = 0; y < height; ++y)
        for (u32 x = 0; x < width; ++x) {
            u16 value = (u16)((u16)in[0] | ((u16)in[1] << 8));
            hm_vram_put((u32)area->x + x, (u32)area->y + y, value, 1);
            in += 2;
        }
    return 0;
}

s32 StoreImage(const RECT *area, u32 *pixels) {
    hm_vram_area(area, pixels, "StoreImage");
    u32 width = (((u32)area->w - 1) & 1023) + 1;
    u32 height = (((u32)area->h - 1) & 511) + 1;
    u8 *out = (u8 *)pixels;
    for (u32 y = 0; y < height; ++y)
        for (u32 x = 0; x < width; ++x) {
            u16 value = hm_vram_pixel((u32)area->x + x, (u32)area->y + y);
            out[0] = (u8)value;
            out[1] = (u8)(value >> 8);
            out += 2;
        }
    /* Transfer padding is not a pixel and is never consumed by game callers.
     * Leave it untouched rather than accessing past a tightly packed buffer. */
    return 0;
}

s32 ClearImage(const RECT *area, u8 r, u8 g, u8 b) {
    if (!area)
        hm_runtime_error("ClearImage", "null image argument");
    u32 x0 = (u32)area->x & 0x3F0;
    u32 y0 = (u32)area->y & 511;
    u32 width = (((u32)area->w & 1023) + 15) & ~15u;
    u32 height = (u32)area->h & 511;
    u16 color = (u16)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
    for (u32 y = 0; y < height; ++y)
        for (u32 x = 0; x < width; ++x)
            hm_vram_put(x0 + x, y0 + y, color, 0);
    return 0;
}

s32 DrawSync(s32 mode) {
    if (mode != 0 && mode != 1)
        hm_runtime_error("DrawSync", "unsupported synchronization mode");
    /* API transfers and ordering-table submission execute on the game thread. */
    return 0;
}

s32 MoveImage(const RECT *area, s32 x, s32 y) {
    if (!area)
        hm_runtime_error("MoveImage", "null source rectangle");
    u32 sx = (u32)area->x & 1023, sy = (u32)area->y & 511;
    u32 dx = (u32)x & 1023, dy = (u32)y & 511;
    u32 width = (((u32)area->w - 1) & 1023) + 1;
    u32 height = (((u32)area->h - 1) & 511) + 1;
    int overlap_x = ((dx - sx) & 1023) < width || ((sx - dx) & 1023) < width;
    int overlap_y = ((dy - sy) & 511) < height || ((sy - dy) & 511) < height;
    if (overlap_x && overlap_y && (sx != dx || sy != dy))
        hm_runtime_error("MoveImage",
                         "overlapping transfers require hardware copy-order validation");
    for (u32 row = 0; row < height; ++row)
        for (u32 column = 0; column < width; ++column)
            hm_vram_put(dx + column, dy + row, hm_vram_pixel(sx + column, sy + row), 1);
    return 0;
}

s32 MoveImage2(const RECT *area, s32 x, s32 y) { return MoveImage(area, x, y); }
