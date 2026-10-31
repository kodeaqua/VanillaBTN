#include "gfx/frame_replay.h"
#include "gfx/replay_sink.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    hm_render_frame meta;
    hm_gpu_environment env;
    hm_vram_context before;
    u32 *words, used, word_capacity;
    int complete;
    u32 *group_offset, *group_world, groups, group_capacity;
    u64 serial;
} hm_captured_frame;
struct hm_frame_replay {
    hm_captured_frame frames[2];
    hm_gpu_environment env;
    hm_vram_context work;
    int current, capturing, invalidated, references;
    const char *failure;
    u64 serial;
};
static hm_frame_replay *hm_shared_replay;
static void hm_capture_fail(hm_frame_replay *s, const char *reason) {
    hm_captured_frame *f = &s->frames[s->current];
    s->failure = reason;
    s->frames[0].complete = s->frames[1].complete = 0;
    f->complete = 0;
    s->capturing = 0;
}
static int hm_capture_reserve_words(hm_frame_replay *s, hm_captured_frame *f, u32 count) {
    if (count > UINT32_MAX - f->used) {
        hm_capture_fail(s, "frame replay word count overflow");
        return 0;
    }
    u32 required = f->used + count;
    if (required <= f->word_capacity)
        return 1;
    u32 capacity = f->word_capacity ? f->word_capacity : 1024;
    while (capacity < required) {
        if (capacity > UINT32_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    if ((size_t)capacity > SIZE_MAX / sizeof(*f->words)) {
        hm_capture_fail(s, "frame replay word allocation overflow");
        return 0;
    }
    u32 *words = realloc(f->words, (size_t)capacity * sizeof(*words));
    if (!words) {
        hm_capture_fail(s, "frame replay word allocation");
        return 0;
    }
    f->words = words;
    f->word_capacity = capacity;
    return 1;
}
static int hm_capture_reserve_groups(hm_frame_replay *s, hm_captured_frame *f) {
    if (f->groups < f->group_capacity)
        return 1;
    u32 capacity = f->group_capacity ? f->group_capacity * 2 : 16;
    if (capacity < f->group_capacity || (size_t)capacity > SIZE_MAX / sizeof(u32)) {
        hm_capture_fail(s, "frame replay group allocation overflow");
        return 0;
    }
    u32 *offsets = realloc(f->group_offset, (size_t)capacity * sizeof(*offsets));
    if (!offsets) {
        hm_capture_fail(s, "frame replay group allocation");
        return 0;
    }
    f->group_offset = offsets;
    u32 *worlds = realloc(f->group_world, (size_t)capacity * sizeof(*worlds));
    if (!worlds) {
        hm_capture_fail(s, "frame replay group allocation");
        return 0;
    }
    f->group_world = worlds;
    f->group_capacity = capacity;
    return 1;
}
static void hm_capture_append(hm_frame_replay *s, const u32 *words, u32 count) {
    if (!s->capturing || !count)
        return;
    if (!words) {
        hm_capture_fail(s, "null frame replay packet");
        return;
    }
    hm_captured_frame *f = &s->frames[s->current];
    if (!hm_capture_reserve_words(s, f, count))
        return;
    memcpy(f->words + f->used, words, (size_t)count * sizeof(*words));
    f->used += count;
}
static void hm_capture_transfer(void *context, s32 x, s32 y, u32 width, u32 height) {
    hm_frame_replay *s = context;
    for (int i = 0; i < 2; ++i) {
        if (s->capturing && i == s->current)
            continue;
        const RECT *r = &s->frames[i].env.draw.clip;
        if (r->w > 0 && r->h > 0 && x < r->x + r->w && y < r->y + r->h && (s64)x + width > r->x &&
            (s64)y + height > r->y)
            s->frames[i].complete = 0;
    }
}
static void hm_capture_invalidate(void *context) {
    hm_frame_replay *s = context;
    s->capturing = 0;
    s->invalidated = 1;
    s->frames[0].complete = s->frames[1].complete = 0;
}
static void hm_capture_begin(void *context, const hm_render_frame *meta) {
    hm_frame_replay *s = context;
    s->failure = NULL;
    const hm_gpu_environment *env = hm_gpu_environment_state();
    int retained = -1;
    if (meta->retain && !meta->bypass) {
        for (int i = 0; i < 2; ++i) {
            int slot = s->current ^ i;
            hm_captured_frame *prior = &s->frames[slot];
            if (prior->complete && !prior->meta.bypass && meta->tick >= prior->meta.tick &&
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
        s->invalidated = 0;
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
    s->invalidated = 0;
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
    if (!hm_capture_reserve_groups(s, f))
        return;
    f->group_offset[f->groups] = f->used;
    f->group_world[f->groups++] = !!world;
}
static void hm_capture_packet(void *context, const u32 *words, u32 count) {
    hm_frame_replay *s = context;
    hm_capture_append(s, words, count);
}
static void hm_capture_direct_command(void *context, const u32 *words, u32 count, int payload) {
    hm_frame_replay *s = context;
    if (!payload && s->capturing && count >= 3) {
        u32 opcode = words[0] >> 24;
        if (opcode == 2 || opcode == 0x80 || opcode == 0xa0) {
            u32 x = 0, y = 0, width = 0, height = 0;
            if (opcode == 0x80) {
                if (count >= 4) {
                    x = words[2] & 1023u;
                    y = (words[2] >> 16) & 511u;
                    width = (((words[3] & 0xffffu) - 1u) & 1023u) + 1u;
                    height = (((words[3] >> 16) - 1u) & 511u) + 1u;
                }
            } else if (opcode == 0xa0) {
                x = words[1] & 1023u;
                y = (words[1] >> 16) & 511u;
                width = (((words[2] & 0xffffu) - 1u) & 1023u) + 1u;
                height = (((words[2] >> 16) - 1u) & 511u) + 1u;
            } else {
                x = words[1] & 0x3f0u;
                y = (words[1] >> 16) & 511u;
                width = ((words[2] & 1023u) + 15u) & ~15u;
                height = (words[2] >> 16) & 511u;
            }
            if (width && height) {
                u32 first_w = width < 1024u - x ? width : 1024u - x;
                u32 first_h = height < 512u - y ? height : 512u - y;
                hm_capture_transfer(s, (s32)x, (s32)y, first_w, first_h);
                hm_capture_transfer(s, 0, (s32)y, width - first_w, first_h);
                hm_capture_transfer(s, (s32)x, 0, first_w, height - first_h);
                hm_capture_transfer(s, 0, 0, width - first_w, height - first_h);
            }
        }
    }
    hm_capture_append(s, words, count);
}
static void hm_capture_release_frame(hm_captured_frame *f) {
    free(f->words);
    free(f->group_offset);
    free(f->group_world);
}
static void hm_capture_end(void *context) {
    hm_frame_replay *s = context;
    if (s->capturing)
        s->frames[s->current].complete =
            !s->invalidated &&
            (s->frames[s->current].used != 0 || s->frames[s->current].env.draw.isbg);
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
                                                  hm_capture_invalidate, hm_capture_transfer,
                                                  hm_capture_direct_command})) {
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
    for (int i = 0; i < 2; ++i)
        hm_capture_release_frame(&s->frames[i]);
    free(s);
    hm_shared_replay = NULL;
}
static hm_captured_frame *hm_capture_select(hm_frame_replay *s, const hm_render_frame *live) {
    if (!s)
        return NULL;
    const RECT *display = &hm_gpu_environment_state()->display.disp;
    for (int i = 0; i < 2; ++i) {
        hm_captured_frame *f = &s->frames[s->current ^ i];
        const RECT *draw = &f->env.draw.clip;
        /* The simulation may enter a new area/cutscene before the next buffer
         * flip. Present the complete capture of the buffer actually scanned out;
         * the new simulation key is not the identity of those displayed pixels.
         * Interpolation keeps its separate, strict scene-key checks. */
        if (f->complete && (!live->bypass || f->meta.bypass) && draw->x == display->x &&
            draw->y == display->y && draw->w == display->w && draw->h == display->h &&
            live->tick >= f->meta.tick)
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
    /* Every private replay runs on a copy. GPU scene construction may ask the
     * reference executor to materialize prior VRAM writes for feedback reads. */
    s->work = f->before;
    hm_vram_context *target_vram = &s->work;
    s->env = f->env;
    hm_vram_context *vram = hm_vram_bind(target_vram);
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
const char *hm_frame_replay_failure(const hm_frame_replay *s) {
    return s ? s->failure : NULL;
}
