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
