#include "gfx/replay_sink.h"
#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#include "psx/gpu_environment.h"
#endif
static hm_replay_sink current;
hm_replay_sink hm_replay_bind(hm_replay_sink sink) {
    hm_replay_sink previous = current;
    current = sink;
    return previous;
}
void hm_replay_group(int world) {
#ifdef HMBTN_ENH_WIDESCREEN
    hm_widescreen_replay_group(world);
#else
    (void)world;
#endif
}
void hm_replay_begin(const RECT *area) {
#ifdef HMBTN_ENH_WIDESCREEN
    hm_widescreen_replay_begin(area);
    RECT expanded = hm_widescreen_area(*area);
    area = &expanded;
#endif
    if (current.begin)
        current.begin(current.context, area);
}
void hm_replay_command(const u32 *words, u32 available, int after) {
    if (!current.command)
        return;
#ifdef HMBTN_ENH_WIDESCREEN
    RECT saved = hm_gpu_environment_state()->draw.clip;
    u32 overlay[5];
    int full = hm_widescreen_overlay(words, available, overlay);
    if (full) {
        words = overlay;
        available = (u32)full;
    }
    hm_gpu_environment_set_clip(full ? hm_widescreen_area(saved) : hm_widescreen_clip(saved));
#endif
    if (current.command)
        current.command(current.context, words, available, after);
#ifdef HMBTN_ENH_WIDESCREEN
    hm_gpu_environment_set_clip(saved);
#endif
}
u32 hm_replay_consume(const u32 *words, u32 available) {
    if (!current.consume)
        return 0;
#ifdef HMBTN_ENH_WIDESCREEN
    RECT saved = hm_gpu_environment_state()->draw.clip;
    u32 overlay[5];
    int full = hm_widescreen_overlay(words, available, overlay);
    if (full) {
        words = overlay;
        available = (u32)full;
    }
    hm_gpu_environment_set_clip(full ? hm_widescreen_area(saved) : hm_widescreen_clip(saved));
#endif
    u32 used = current.consume ? current.consume(current.context, words, available) : 0;
#ifdef HMBTN_ENH_WIDESCREEN
    hm_gpu_environment_set_clip(saved);
#endif
    return used;
}
void hm_replay_motion(const u32 *previous, const u32 *next, u32 count, u32 alpha) {
    if (current.motion)
        current.motion(current.context, previous, next, count, alpha);
}
