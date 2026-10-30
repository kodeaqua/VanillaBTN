#ifndef HMBTN_GFX_FRAME_EVENTS_H
#define HMBTN_GFX_FRAME_EVENTS_H
#include "compat/types.h"
#include <stddef.h>

typedef struct {
    u64 tick, key;
    int eligible;
    int world_view; /* Farm world view, including its UI overlays. */
    int bypass;     /* Direct/loading presentation cannot replay an older OT. */
    int retain;     /* Direct fade adds commands to the existing framebuffer. */
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
    void (*transfer)(void *context, s32 x, s32 y, u32 width, u32 height);
} hm_render_observer;
/* Single game-thread, read-only observation of native command execution. */
int hm_render_subscribe(hm_render_observer observer);
void hm_render_unsubscribe(void *context);
void hm_render_begin(const hm_render_frame *frame);
void hm_render_group(int world);
void hm_render_packet(const u32 *words, u32 count);
void hm_render_end(void);
void hm_render_invalidate(void);
/* Native VRAM changes outside captured geometry. Rectangles do not wrap. */
void hm_render_transfer(s32 x, s32 y, u32 width, u32 height);
void hm_render_transfer_command_begin(void);
void hm_render_transfer_command_end(void);
void hm_render_direct_draw(s32 x, s32 y, u32 width, u32 height);
#endif
