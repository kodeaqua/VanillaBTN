#include "host_audio_policy.h"
#include <string.h>

void host_audio_apply_volume(int16_t *samples, size_t count, int volume_percent,
                             bool muted) {
    if (!samples || count == 0) return;
    if (muted || volume_percent <= 0) {
        memset(samples, 0, count * sizeof(*samples));
        return;
    }
    if (volume_percent >= 100) return;

    /* 32-bit arithmetic accommodates INT16_MIN * 99 without overflow.
     * Truncation toward zero preserves sign and never amplifies the signal. */
    for (size_t i = 0; i < count; ++i) {
        samples[i] = (int16_t)((int32_t)samples[i] * volume_percent / 100);
    }
}

int host_audio_queue_frames(int queued_bytes, int generated_frames, int *start_frame) {
    if (start_frame) *start_frame = 0;
    if (queued_bytes < 0 || generated_frames <= 0 || !start_frame) return 0;

    /* Round up if SDL reports a partial sample frame: never overrun the cap. */
    int queued_frames = queued_bytes / (int)(2 * sizeof(int16_t));
    if (queued_bytes % (int)(2 * sizeof(int16_t))) ++queued_frames;
    int room = HOST_AUDIO_QUEUE_LIMIT_FRAMES - queued_frames;
    if (room <= 0) return 0;

    int count = generated_frames < room ? generated_frames : room;
    /* When catch-up produces more PCM than SDL can consume, discard the
     * oldest new frames rather than replaying stale audio behind the game. */
    *start_frame = generated_frames - count;
    return count;
}

void host_audio_clock_reset(HostAudioClock *clock) {
    if (clock) memset(clock, 0, sizeof(*clock));
}

int host_audio_clock_frames(HostAudioClock *clock, uint64_t now_ns, bool turbo) {
    if (!clock) return HOST_AUDIO_TICK_FRAMES;
    if (!clock->initialized || now_ns < clock->last_ns || turbo) {
        clock->initialized = true;
        clock->last_ns = now_ns;
        clock->remainder = 0;
        return HOST_AUDIO_TICK_FRAMES;
    }

    if (now_ns == clock->last_ns) return 0;
    uint64_t elapsed = now_ns - clock->last_ns;
    clock->last_ns = now_ns;
    /* Cap catch-up at four ticks; never let an OS pause turn into seconds of
     * heavy SPU mixing or a large SDL queue. The remainder is discarded after
     * a truncated gap so the next normal tick starts with a clean clock. */
    const uint64_t max_ns = 1000000000ULL * HOST_AUDIO_MAX_FRAMES / 44100ULL;
    if (elapsed >= max_ns) {
        clock->remainder = 0;
        return HOST_AUDIO_MAX_FRAMES;
    }
    uint64_t scaled = elapsed * 44100ULL + clock->remainder;
    int frames = (int)(scaled / 1000000000ULL);
    clock->remainder = scaled % 1000000000ULL;
    return frames;
}

size_t host_audio_music_onset_padding(HostAudioMusicOnset *onset,
                                      bool sequence_playing, bool pcm_nonzero,
                                      int queued_bytes, size_t tick_bytes) {
    if (!onset) return 0;
    if (!sequence_playing) {
        onset->primed = false;
        return 0;
    }
    if (!pcm_nonzero || onset->primed || queued_bytes < 0 || tick_bytes == 0 ||
        tick_bytes > SIZE_MAX / 2) return 0;

    onset->primed = true;
    /* Leave two 60 Hz ticks of silence ahead of the first musical samples.
     * This absorbs an isolated slow loading tick without advancing the SPU
     * or increasing latency on every subsequent game tick. */
    size_t target = tick_bytes * 2;
    if ((size_t)queued_bytes >= target) return 0;
    size_t missing = target - (size_t)queued_bytes;
    /* SDL S16 stereo input must contain complete 4-byte sample frames. */
    return (missing + 3) & ~(size_t)3;
}
