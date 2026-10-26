#ifndef HMBTN_PAUSE_CANVAS_H
#define HMBTN_PAUSE_CANVAS_H
#include "compat/types.h"
/* Frozen RGBA canvas is width x 240, width >= 320. UI remains 320 x 240. */
void hm_pause_canvas_extract(u8 *ui, const u8 *frozen, u32 width);
void hm_pause_canvas_compose(u8 *output, const u8 *frozen, u32 width, const u8 *ui);
#endif
