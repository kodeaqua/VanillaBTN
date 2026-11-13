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

/* Add a small output reserve only when a newly playing music sequence first
 * produces PCM. Silent loading ticks must not consume the one-time reserve. */
typedef struct HostAudioMusicOnset {
    bool primed;
} HostAudioMusicOnset;

size_t host_audio_music_onset_padding(HostAudioMusicOnset *onset,
                                      bool sequence_playing, bool pcm_nonzero,
                                      int queued_bytes, size_t tick_bytes);

#endif
