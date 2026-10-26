#include "gfx/frame_events.h"
enum { OBSERVERS = 4 };
static hm_render_observer observers[OBSERVERS];
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
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].begin)
            observers[i].begin(observers[i].context, frame);
}
void hm_render_group(int world) {
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].group)
            observers[i].group(observers[i].context, world);
}
void hm_render_packet(const u32 *words, u32 count) {
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].packet)
            observers[i].packet(observers[i].context, words, count);
}
void hm_render_end(void) {
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].end)
            observers[i].end(observers[i].context);
}
void hm_render_invalidate(void) {
    for (int i = 0; i < OBSERVERS; ++i)
        if (observers[i].invalidate)
            observers[i].invalidate(observers[i].context);
}
