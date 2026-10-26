#ifndef HMBTN_SCALING_H
#define HMBTN_SCALING_H
#include "gfx/frame_events.h"
#include "gfx/replay_sink.h"
typedef struct {
    const u16 *pixels;
    u32 width, height;
} hm_scaled_view;
int hm_scaling_enable(int enabled);
/* Configure only on output changes; steady-state rendering allocates nothing.
 * The limit is 4096 per axis / 16M pixels. Unsupported sizes return failure. */
int hm_scaling_prepare(u32 width, u32 height);
hm_replay_sink hm_scaling_begin(void);
int hm_scaling_replay(const hm_render_frame *live);
int hm_scaling_end(hm_replay_sink previous, hm_scaled_view *view);
#endif
