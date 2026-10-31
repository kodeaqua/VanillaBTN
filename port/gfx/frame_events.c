#include "gfx/frame_events.h"
#include "gfx/packet_identity.h"
enum { OBSERVERS = 4 };
static hm_render_observer observers[OBSERVERS];
static int hm_group_base, hm_group_effective;
static int hm_capturing;
static u32 hm_transfer_command_depth;
int hm_render_subscribe(hm_render_observer observer) {
    if (!observer.context)
        return 0;
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].context == observer.context) {
            observers[i] = observer;
            return 1;
        }
    for (int i = 0; i < OBSERVERS; ++i)
        if (!observers[i].context) {
            observers[i] = observer;
            return 1;
        }
    return 0;
}
void hm_render_unsubscribe(void *context) {
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].context == context)
            observers[i] = (hm_render_observer){0};
}
void hm_render_begin(const hm_render_frame *frame) {
    hm_capturing = 1;
    hm_group_base = hm_group_effective = 0;
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].begin)
            observers[i].begin(observers[i].context, frame);
}
int hm_render_capture_active(void) { return hm_capturing; }
void hm_render_group(int world) {
    hm_group_base = hm_group_effective = !!world;
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].group)
            observers[i].group(observers[i].context, world);
}
void hm_render_packet(const u32 *words, u32 count) {
    int desired = hm_group_base || hm_packet_background_get(words);
    if (desired != hm_group_effective) {
        for (int i = 0; i < OBSERVERS; ++i)
            if (observers[i].group)
                observers[i].group(observers[i].context, desired);
        hm_group_effective = desired;
    }
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].packet)
            observers[i].packet(observers[i].context, words, count);
}
void hm_render_end(void) {
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].end)
            observers[i].end(observers[i].context);
    hm_capturing = 0;
}
void hm_render_invalidate(void) {
    hm_capturing = 0;
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].invalidate)
            observers[i].invalidate(observers[i].context);
}
void hm_render_transfer(s32 x, s32 y, u32 width, u32 height) {
    if (!width || !height || hm_transfer_command_depth)
        return;
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].transfer)
            observers[i].transfer(observers[i].context, x, y, width, height);
}
void hm_render_transfer_command_begin(void) { ++hm_transfer_command_depth; }
void hm_render_transfer_command_end(void) {
    if (hm_transfer_command_depth)
        --hm_transfer_command_depth;
}
int hm_render_direct_transfer_begin(void) {
    if (!hm_capturing || hm_transfer_command_depth)
        return 0;
    ++hm_transfer_command_depth;
    return 1;
}
void hm_render_direct_transfer_command(const u32 *words, u32 count, int payload) {
    if (!hm_capturing || hm_transfer_command_depth != 1 || (!words && count))
        return;
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].direct_command)
            observers[i].direct_command(observers[i].context, words, count, payload);
}
void hm_render_direct_transfer_end(int active) {
    if (active && hm_transfer_command_depth)
        --hm_transfer_command_depth;
}
void hm_render_direct_draw(s32 x, s32 y, u32 width, u32 height) {
    if (!hm_capturing)
        hm_render_transfer(x, y, width, height);
}
