#include "enhance/scaling/scaling.h"
#include "enhance/scaling/raster.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <stdlib.h>
#include <string.h>

enum { WORDS = 65536 };
typedef struct {
    hm_render_frame meta;
    hm_gpu_environment env;
    hm_vram_context before;
    u32 words[WORDS], used;
    int complete;
    u64 serial;
} captured_frame;
typedef struct {
    captured_frame frames[2];
    hm_gpu_environment env;
    hm_vram_context work;
    hm_scaled_surface surface;
    int current, capturing;
    u64 serial, cached_serial, cached_revision;
} scaling;
static scaling *state;
static void hm_scaling_capture_invalidate(void *context) {
    scaling *s = context;
    s->capturing = 0;
    s->frames[0].complete = s->frames[1].complete = 0;
}
static void hm_scaling_capture_begin(void *context, const hm_render_frame *meta) {
    scaling *s = context;
    s->current ^= 1;
    captured_frame *f = &s->frames[s->current];
    f->meta = *meta;
    f->env = *hm_gpu_environment_state();
    f->used = 0;
    f->complete = 0;
    f->serial = ++s->serial;
    hm_vram_snapshot(&f->before);
    s->capturing = 1;
}
static void hm_scaling_capture_packet(void *context, const u32 *words, u32 count) {
    scaling *s = context;
    if (!s->capturing)
        return;
    captured_frame *f = &s->frames[s->current];
    if (count > WORDS - f->used) {
        s->capturing = 0;
        return;
    }
    memcpy(f->words + f->used, words, count * sizeof(*words));
    f->used += count;
}
static void hm_scaling_capture_end(void *context) {
    scaling *s = context;
    if (s->capturing)
        s->frames[s->current].complete = s->frames[s->current].used != 0;
    s->capturing = 0;
}
int hm_scaling_enable(int enabled) {
    if (!enabled) {
        hm_render_unsubscribe(state);
        if (state)
            free(state->surface.pixels);
        free(state);
        state = NULL;
        return 1;
    }
    if (state)
        return 1;
    state = calloc(1, sizeof(*state));
    if (!state)
        return 0;
    if (!hm_render_subscribe((hm_render_observer){state, hm_scaling_capture_begin, NULL,
                                                  hm_scaling_capture_packet, hm_scaling_capture_end,
                                                  hm_scaling_capture_invalidate})) {
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
    const hm_gpu_environment *current = hm_gpu_environment_state();
    captured_frame *f = NULL;
    for (int i = 0; i < 2; ++i) {
        captured_frame *candidate = &s->frames[s->current ^ i];
        if (candidate->complete && candidate->meta.key == live->key &&
            candidate->env.draw.clip.x == current->display.disp.x &&
            candidate->env.draw.clip.y == current->display.disp.y &&
            candidate->env.draw.clip.w == current->display.disp.w &&
            candidate->env.draw.clip.h == current->display.disp.h &&
            live->tick >= candidate->meta.tick && live->tick - candidate->meta.tick <= 4) {
            f = candidate;
            break;
        }
    }
    if (!f)
        return 0;
    if (s->cached_serial == f->serial && s->cached_revision == s->surface.revision) {
        s->surface.begun = 1;
        return 1;
    }
    s->work = f->before;
    s->env = f->env;
    hm_vram_context *saved_vram = hm_vram_bind(&s->work);
    hm_gpu_environment *saved_env = hm_gpu_environment_bind(&s->env);
    hm_replay_begin(&f->env.draw.clip);
    hm_gpu_execute_words(f->words, f->used);
    hm_gpu_environment_bind(saved_env);
    hm_vram_bind(saved_vram);
    if (s->surface.begun) {
        s->cached_serial = f->serial;
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
