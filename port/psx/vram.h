#ifndef HMBTN_PSX_VRAM_H
#define HMBTN_PSX_VRAM_H

#include "compat/types.h"

enum { HM_VRAM_WIDTH = 1024, HM_VRAM_HEIGHT = 512 };
void hm_vram_reset(void);
u16 hm_vram_pixel(u32 x, u32 y);
void hm_vram_mask(int set_mask, int check_mask);
/* 16x16-word dirty tiles; the renderer acknowledges uploads explicitly. */
u64 hm_vram_dirty_row(u32 tile_y);
void hm_vram_acknowledge(u32 tile_y, u64 tiles);

#endif
