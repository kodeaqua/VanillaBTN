#include "enhance/scaling/scaling.h"
#include "enhance/scaling/raster.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <stdlib.h>
#include <string.h>

#include "gfx/frame_replay.h"
typedef struct {
    hm_frame_replay *replay;
    hm_scaled_surface surface;
    u64 cached_serial, cached_revision;
} scaling;
static scaling *state;
int hm_scaling_enable(int enabled) {
    if (!enabled) {
        if (state) {
            hm_frame_replay_release(state->replay);
            free(state->surface.pixels);
            free(state);
            state = NULL;
        }
        return 1;
    }
    if (state)
        return 1;
    state = calloc(1, sizeof(*state));
    if (!state)
        return 0;
    state->replay = hm_frame_replay_acquire();
    if (!state->replay) {
        free(state);
        state = NULL;
        return 0;
    }
    return 1;
}
int hm_scaling_prepare(u32 width, u32 height) {
    if (!state || !width || !height || width > 4096 || height > 4096)
        return 0;
    hm_scaled_surface *s = &state->surface;
    if (s->width == width && s->height == height)
        return 1;
    u16 *pixels = malloc((size_t)width * height * sizeof(*pixels));
    if (!pixels)
        return 0;
    free(s->pixels);
    *s = (hm_scaled_surface){.pixels = pixels, .width = width, .height = height};
    return 1;
}
hm_replay_sink hm_scaling_begin(void) {
    if (!state)
        return hm_replay_bind((hm_replay_sink){0});
    state->surface.begun = 0;
    return hm_replay_bind(hm_scaled_sink(&state->surface));
}
int hm_scaling_replay(const hm_render_frame *live) {
    scaling *s = state;
    if (!s || live->bypass)
        return 0;
    u64 serial = hm_frame_replay_serial(s->replay, live);
    if (!serial)
        return 0;
    if (s->cached_serial == serial && s->cached_revision == s->surface.revision) {
        s->surface.begun = 1;
        return 1;
    }
    if (!hm_frame_replay_draw(s->replay, live))
        return 0;
    if (s->surface.begun) {
        s->cached_serial = serial;
        s->cached_revision = s->surface.revision;
    }
    return s->surface.begun;
}
int hm_scaling_end(hm_replay_sink previous, hm_scaled_view *view) {
    hm_replay_bind(previous);
    if (!state || !state->surface.begun)
        return 0;
    *view = (hm_scaled_view){state->surface.pixels, state->surface.width, state->surface.height};
    return 1;
}
