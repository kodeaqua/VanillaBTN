#ifndef HMBTN_REPLAY_SINK_H
#define HMBTN_REPLAY_SINK_H
#include "compat/types.h"
#include <libgpu.h>
typedef struct {
    void *context;
    void (*begin)(void *context, const RECT *area);
    void (*command)(void *context, const u32 *words, u32 available, int after);
} hm_replay_sink;
/* Only explicit private replay invokes these hooks, never normal DrawOTag. */
hm_replay_sink hm_replay_bind(hm_replay_sink sink);
void hm_replay_begin(const RECT *area);
void hm_replay_command(const u32 *words, u32 available, int after);
#endif
