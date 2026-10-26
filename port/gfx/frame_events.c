#include "gfx/frame_events.h"
static hm_render_observer hm_observer;
void hm_render_subscribe(hm_render_observer observer) { hm_observer = observer; }
void hm_render_begin(const hm_render_frame *frame) {
    if (hm_observer.begin)
        hm_observer.begin(hm_observer.context, frame);
}
void hm_render_group(int world) {
    if (hm_observer.group)
        hm_observer.group(hm_observer.context, world);
}
void hm_render_packet(const u32 *words, u32 count) {
    if (hm_observer.packet)
        hm_observer.packet(hm_observer.context, words, count);
}
void hm_render_end(void) {
    if (hm_observer.end)
        hm_observer.end(hm_observer.context);
}
void hm_render_invalidate(void) {
    if (hm_observer.invalidate)
        hm_observer.invalidate(hm_observer.context);
}
