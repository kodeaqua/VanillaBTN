#ifndef HMBTN_PSX_VRAM_H
#define HMBTN_PSX_VRAM_H

#include "compat/types.h"

enum { HM_VRAM_WIDTH = 1024, HM_VRAM_HEIGHT = 512 };
typedef struct {
    u16 pixels[HM_VRAM_HEIGHT][HM_VRAM_WIDTH];
    u64 dirty[HM_VRAM_HEIGHT / 16];
    u16 set_mask, check_mask;
} hm_vram_context;
/* Bind a private render context on the game thread, then restore the returned
 * context. NULL selects the normal native VRAM. */
hm_vram_context *hm_vram_bind(hm_vram_context *context);
void hm_vram_snapshot(hm_vram_context *destination);
const hm_vram_context *hm_vram_state(void);
int hm_vram_is_native(void);
void hm_vram_reset(void);
u16 hm_vram_pixel(u32 x, u32 y);
void hm_vram_draw_pixel(u32 x, u32 y, u16 value);
void hm_vram_mask(int set_mask, int check_mask);
/* 16x16-word dirty tiles; the renderer acknowledges uploads explicitly. */
u64 hm_vram_dirty_row(u32 tile_y);
void hm_vram_acknowledge(u32 tile_y, u64 tiles);

#endif
