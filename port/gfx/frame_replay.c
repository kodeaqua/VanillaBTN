#include "gfx/frame_replay.h"
#include "gfx/replay_sink.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <stdlib.h>
#include <string.h>
enum { HM_REPLAY_WORDS = 65536 };
typedef struct {
    hm_render_frame meta;
    hm_gpu_environment env;
    hm_vram_context before;
    u32 words[HM_REPLAY_WORDS], used;
    int complete;
    u32 group_offset[64], group_world[64], groups;
    u64 serial;
} hm_captured_frame;
struct hm_frame_replay {
    hm_captured_frame frames[2];
    hm_gpu_environment env;
    hm_vram_context work;
    int current, capturing, references;
    u64 serial;
};
static hm_frame_replay *hm_shared_replay;
static void hm_capture_invalidate(void *context) {
    hm_frame_replay *s = context;
    s->capturing = 0;
    s->frames[0].complete = s->frames[1].complete = 0;
}
static void hm_capture_begin(void *context, const hm_render_frame *meta) {
    hm_frame_replay *s = context;
    const hm_gpu_environment *env = hm_gpu_environment_state();
    int retained = -1;
    if (meta->retain && !meta->bypass) {
        for (int i = 0; i < 2; ++i) {
            int slot = s->current ^ i;
            hm_captured_frame *prior = &s->frames[slot];
            if (prior->complete && !prior->meta.bypass && meta->tick >= prior->meta.tick &&
                meta->tick - prior->meta.tick <= 4 &&
                !memcmp(&prior->env.draw.clip, &env->draw.clip, sizeof(RECT))) {
                retained = slot;
                break;
            }
        }
    }
    /* Reuse the captured commands and original VRAM snapshot, including the
     * geometry outside PS1 VRAM. Replaying only the new fade loses those sides.
     * Native execution still sees precisely the original commands. */
    if (retained >= 0) {
        s->current = retained;
        hm_captured_frame *f = &s->frames[retained];
        f->meta = *meta;
        f->complete = 0;
        f->serial = ++s->serial;
        s->capturing = 1;
        return;
    }
    s->current ^= 1;
    hm_captured_frame *f = &s->frames[s->current];
    f->meta = *meta;
    f->env = *hm_gpu_environment_state();
    f->used = f->groups = 0;
    f->complete = 0;
    f->serial = ++s->serial;
    hm_vram_snapshot(&f->before);
    s->capturing = 1;
}
static void hm_capture_group(void *context, int world) {
    hm_frame_replay *s = context;
    hm_captured_frame *f = &s->frames[s->current];
    if (!s->capturing)
        return;
    if (f->groups && f->group_world[f->groups - 1] == (u32) !!world)
        return;
    if (f->groups == 64) {
        s->capturing = 0;
        return;
    }
    f->group_offset[f->groups] = f->used;
    f->group_world[f->groups++] = !!world;
}
static void hm_capture_packet(void *context, const u32 *words, u32 count) {
    hm_frame_replay *s = context;
    if (!s->capturing)
        return;
    hm_captured_frame *f = &s->frames[s->current];
    if (count > HM_REPLAY_WORDS - f->used) {
        s->capturing = 0;
        return;
    }
    memcpy(f->words + f->used, words, count * sizeof(*words));
    f->used += count;
}
static void hm_capture_end(void *context) {
    hm_frame_replay *s = context;
    if (s->capturing)
        s->frames[s->current].complete =
            s->frames[s->current].used != 0 || s->frames[s->current].env.draw.isbg;
    s->capturing = 0;
}
hm_frame_replay *hm_frame_replay_acquire(void) {
    if (hm_shared_replay) {
        ++hm_shared_replay->references;
        return hm_shared_replay;
    }
    hm_frame_replay *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    if (!hm_render_subscribe((hm_render_observer){s, hm_capture_begin, hm_capture_group,
                                                  hm_capture_packet, hm_capture_end,
                                                  hm_capture_invalidate})) {
        free(s);
        return NULL;
    }
    s->references = 1;
    hm_shared_replay = s;
    return s;
}
void hm_frame_replay_release(hm_frame_replay *s) {
    if (!s || s != hm_shared_replay)
        return;
    if (--s->references)
        return;
    hm_render_unsubscribe(s);
    free(s);
    hm_shared_replay = NULL;
}
static hm_captured_frame *hm_capture_select(hm_frame_replay *s, const hm_render_frame *live) {
    if (!s || live->bypass)
        return NULL;
    const RECT *display = &hm_gpu_environment_state()->display.disp;
    for (int i = 0; i < 2; ++i) {
        hm_captured_frame *f = &s->frames[s->current ^ i];
        const RECT *draw = &f->env.draw.clip;
        /* The simulation may enter a new area/cutscene before the next buffer
         * flip. Present the complete capture of the buffer actually scanned out;
         * the new simulation key is not the identity of those displayed pixels.
         * Interpolation keeps its separate, strict scene-key checks. */
        if (f->complete && !f->meta.bypass && draw->x == display->x && draw->y == display->y &&
            draw->w == display->w && draw->h == display->h && live->tick >= f->meta.tick &&
            live->tick - f->meta.tick <= 4)
            return f;
    }
    return NULL;
}
u64 hm_frame_replay_serial(hm_frame_replay *s, const hm_render_frame *live) {
    hm_captured_frame *f = hm_capture_select(s, live);
    return f ? f->serial : 0;
}
int hm_frame_replay_draw(hm_frame_replay *s, const hm_render_frame *live) {
    hm_captured_frame *f = hm_capture_select(s, live);
    if (!f)
        return 0;
    s->work = f->before;
    s->env = f->env;
    hm_vram_context *vram = hm_vram_bind(&s->work);
    hm_gpu_environment *env = hm_gpu_environment_bind(&s->env);
    hm_replay_begin(&f->env.draw.clip);
    u32 at = 0;
    for (u32 i = 0; i < f->groups; ++i) {
        u32 end = f->group_offset[i];
        if (end > at)
            hm_gpu_execute_words(f->words + at, end - at);
        hm_replay_group((int)f->group_world[i]);
        at = end;
    }
    if (at < f->used)
        hm_gpu_execute_words(f->words + at, f->used - at);
    hm_replay_group(0);
    hm_gpu_environment_bind(env);
    hm_vram_bind(vram);
    return 1;
}
