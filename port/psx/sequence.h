#ifndef HMBTN_PSX_SEQUENCE_H
#define HMBTN_PSX_SEQUENCE_H
#include "compat/types.h"
typedef struct {
    const u8 *data;
    u32 bytes, at;
    u8 running;
} hm_seq_cursor;
typedef struct {
    u32 delta, bytes;
    const u8 *payload;
    u8 status, meta;
} hm_seq_event;
/* Returns 1 for an event, 0 at end, -1 for malformed/truncated data. */
int hm_seq_next(hm_seq_cursor *cursor, hm_seq_event *event);
void hm_sound_sequence_register(const void *data, u32 bytes);
void hm_sound_sequences_reset(void);
void hm_sound_sequence_tick(void);
void hm_sound_sequence_forget_voice(u32 voice);
#endif
