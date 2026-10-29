#ifndef HMBTN_PLATFORM_AUDIO_RING_H
#define HMBTN_PLATFORM_AUDIO_RING_H
#include "compat/types.h"
#include <stdatomic.h>
enum { HM_AUDIO_RING_FRAMES = 32768 };
typedef struct {
    s16 samples[HM_AUDIO_RING_FRAMES][2];
    atomic_uint read, write;
    u32 prebuffer, phase, rate;
    s16 current[2], next[2];
    int primed;
    u64 dropped, underflows, rendered;
} hm_audio_ring;
void hm_audio_ring_init(hm_audio_ring *ring, u32 prebuffer);
u32 hm_audio_ring_push(hm_audio_ring *ring, const s16 *stereo, u32 frames);
void hm_audio_ring_read(hm_audio_ring *ring, float *output, u32 frames, u32 channels, u32 rate);
#endif
