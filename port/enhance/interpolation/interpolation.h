#ifndef HMBTN_INTERPOLATION_H
#define HMBTN_INTERPOLATION_H
#include "gfx/frame_events.h"
#include <libgpu.h>
typedef struct {
    const u16 *pixels;
    u32 stride;
    RECT area;
} hm_interpolated_view;
int hm_interpolation_enable(int enabled);
int hm_interpolation_render(u64 game_ns, const hm_render_frame *live, hm_interpolated_view *view);
/* Replay to a bound output backend without promising a CPU scanout image. */
int hm_interpolation_replay(u64 game_ns, const hm_render_frame *live);
#endif
