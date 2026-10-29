#include "platform/audio_ring.h"
#include <string.h>
_Static_assert(sizeof(unsigned int) == 4 && ATOMIC_INT_LOCK_FREE == 2, "lock-free audio indices");
void hm_audio_ring_init(hm_audio_ring *ring, u32 prebuffer) {
    memset(ring, 0, sizeof(*ring));
    atomic_init(&ring->read, 0);
    atomic_init(&ring->write, 0);
    ring->prebuffer = prebuffer < 2                      ? 2
                      : prebuffer > HM_AUDIO_RING_FRAMES ? HM_AUDIO_RING_FRAMES
                                                         : prebuffer;
}
u32 hm_audio_ring_push(hm_audio_ring *ring, const s16 *stereo, u32 frames) {
    u32 write = atomic_load_explicit(&ring->write, memory_order_relaxed);
    u32 read = atomic_load_explicit(&ring->read, memory_order_acquire);
    u32 room = HM_AUDIO_RING_FRAMES - (write - read);
    u32 count = frames < room ? frames : room;
    for (u32 i = 0; i < count; ++i) {
        u32 index = (write + i) & (HM_AUDIO_RING_FRAMES - 1);
        ring->samples[index][0] = stereo[i * 2];
        ring->samples[index][1] = stereo[i * 2 + 1];
    }
    atomic_store_explicit(&ring->write, write + count, memory_order_release);
    ring->dropped += frames - count;
    return count;
}
void hm_audio_ring_read(hm_audio_ring *ring, float *output, u32 frames, u32 channels, u32 rate) {
    if (!channels || !rate)
        return;
    memset(output, 0, (size_t)frames * channels * sizeof(float));
    u32 read = atomic_load_explicit(&ring->read, memory_order_relaxed);
    u32 write = atomic_load_explicit(&ring->write, memory_order_acquire);
    if (!ring->primed) {
        if (write - read < ring->prebuffer)
            return;
        ring->current[0] = ring->samples[read & (HM_AUDIO_RING_FRAMES - 1)][0];
        ring->current[1] = ring->samples[read & (HM_AUDIO_RING_FRAMES - 1)][1];
        ++read;
        ring->next[0] = ring->samples[read & (HM_AUDIO_RING_FRAMES - 1)][0];
        ring->next[1] = ring->samples[read & (HM_AUDIO_RING_FRAMES - 1)][1];
        ++read;
        ring->primed = 1;
    }
    if (ring->rate && ring->rate != rate)
        ring->phase = (u32)((u64)ring->phase * rate / ring->rate);
    ring->rate = rate;
    for (u32 i = 0; i < frames; ++i) {
        float fraction = (float)ring->phase / (float)rate;
        float left =
            ((float)ring->current[0] + ((float)ring->next[0] - ring->current[0]) * fraction) *
            (1.0f / 32768.0f);
        float right =
            ((float)ring->current[1] + ((float)ring->next[1] - ring->current[1]) * fraction) *
            (1.0f / 32768.0f);
        output[i * channels] = channels == 1 ? (left + right) * 0.5f : left;
        if (channels > 1)
            output[i * channels + 1] = right;
        ring->phase += 44100;
        while (ring->phase >= rate) {
            ring->phase -= rate;
            ring->current[0] = ring->next[0];
            ring->current[1] = ring->next[1];
            if (read == write) {
                ring->next[0] = ring->next[1] = 0;
                ++ring->underflows;
            } else {
                ring->next[0] = ring->samples[read & (HM_AUDIO_RING_FRAMES - 1)][0];
                ring->next[1] = ring->samples[read & (HM_AUDIO_RING_FRAMES - 1)][1];
                ++read;
            }
        }
    }
    atomic_store_explicit(&ring->read, read, memory_order_release);
    ring->rendered += frames;
}
