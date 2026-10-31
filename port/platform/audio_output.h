#ifndef HMBTN_PLATFORM_AUDIO_OUTPUT_H
#define HMBTN_PLATFORM_AUDIO_OUTPUT_H
#include "compat/types.h"
typedef struct {
    u32 queued_frames;
    u64 rendered_frames, underflow_frames, dropped_frames;
} hm_audio_output_stats;
int hm_audio_output_start(void);
void hm_audio_output_stop(void);
void hm_audio_output_pause(int paused);
void hm_audio_output_get_stats(hm_audio_output_stats *stats);
#endif
