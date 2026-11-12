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

#endif
