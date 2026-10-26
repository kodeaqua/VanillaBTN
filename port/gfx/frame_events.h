#ifndef HMBTN_GFX_FRAME_EVENTS_H
#define HMBTN_GFX_FRAME_EVENTS_H
#include "compat/types.h"
#include <stddef.h>

typedef struct {
    u64 tick, key;
    int eligible;
    int bypass; /* Direct/loading presentation cannot replay an older OT. */
    const void *arena[2];
    size_t arena_bytes[2];
} hm_render_frame;
typedef struct {
    void *context;
    void (*begin)(void *context, const hm_render_frame *frame);
    void (*group)(void *context, int world);
    void (*packet)(void *context, const u32 *words, u32 count);
    void (*end)(void *context);
    void (*invalidate)(void *context);
} hm_render_observer;
/* Single game-thread, read-only observation of native command execution. */
int hm_render_subscribe(hm_render_observer observer);
void hm_render_unsubscribe(void *context);
void hm_render_begin(const hm_render_frame *frame);
void hm_render_group(int world);
void hm_render_packet(const u32 *words, u32 count);
void hm_render_end(void);
void hm_render_invalidate(void);
#endif
