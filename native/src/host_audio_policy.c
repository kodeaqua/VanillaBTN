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
