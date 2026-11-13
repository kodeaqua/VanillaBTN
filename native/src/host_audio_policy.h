#ifndef VANILLABTN_HOST_AUDIO_POLICY_H
#define VANILLABTN_HOST_AUDIO_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Apply host volume only after the authoritative PS1 mixer has produced PCM.
 * No DSP/game state changes; 100% is bit-identical, and muting does not
 * prevent the original SPU from advancing. */
void host_audio_apply_volume(int16_t *samples, size_t count, int volume_percent,
                             bool muted);

/* Follow the playback sample clock rather than assuming every VSync callback
 * arrives exactly 1/60 s apart. A late/skipped callback must not permanently
 * omit its elapsed PCM; long stalls are bounded to avoid runaway catch-up.
 * Turbo intentionally keeps one game-tick of sound per accelerated tick. */
#define HOST_AUDIO_TICK_FRAMES 735
#define HOST_AUDIO_MAX_FRAMES (HOST_AUDIO_TICK_FRAMES * 4)
/* Bound stream latency even when a delayed callback emits multiple ticks. */
#define HOST_AUDIO_QUEUE_LIMIT_FRAMES (HOST_AUDIO_TICK_FRAMES * 4)

/* Select the newest generated frames that fit the remaining SDL queue room.
 * Mixing still advances the original SPU by the full generated frame count. */
int host_audio_queue_frames(int queued_bytes, int generated_frames, int *start_frame);

typedef struct HostAudioClock {
    uint64_t last_ns;
    uint64_t remainder;
    bool initialized;
} HostAudioClock;

void host_audio_clock_reset(HostAudioClock *clock);
int host_audio_clock_frames(HostAudioClock *clock, uint64_t now_ns, bool turbo);

/* Add a small output reserve only when a newly playing music sequence first
 * produces PCM. Silent loading ticks must not consume the one-time reserve. */
typedef struct HostAudioMusicOnset {
    bool primed;
} HostAudioMusicOnset;

size_t host_audio_music_onset_padding(HostAudioMusicOnset *onset,
                                      bool sequence_playing, bool pcm_nonzero,
                                      int queued_bytes, size_t tick_bytes);

#endif
