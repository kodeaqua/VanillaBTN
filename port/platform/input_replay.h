#ifndef HMBTN_PLATFORM_INPUT_REPLAY_H
#define HMBTN_PLATFORM_INPUT_REPLAY_H
#include "compat/types.h"
typedef struct {
    struct {
        u64 tick;
        u16 held;
    } changes[1024];
    u32 count, cursor;
    u16 held;
} hm_input_replay;
/* Diagnostic replay lines: decimal VBlank tick, hexadecimal PadButton mask. */
int hm_input_replay_line(hm_input_replay *replay, const char *line);
u16 hm_input_replay_sample(hm_input_replay *replay, u64 tick);
int hm_input_replay_load(hm_input_replay *replay, const char *path);
#endif
