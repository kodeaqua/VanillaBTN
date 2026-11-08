/* ps1_audio.h — host-side audio system integrating PS1 SPU and CD-XA with SDL3.
 *
 * Coordinates:
 *   - Ps1Spu (24-voice SPU + VAB banks)
 *   - Ps1XaPlayer (CD-ROM XA streams)
 *   - SDL3 AudioStream output at 44 100 Hz, stereo 16-bit signed PCM
 *   - Headless / no-device fallback when audio hardware is absent
 */

#ifndef PS1_AUDIO_H
#define PS1_AUDIO_H

#include "ps1_spu.h"
#include "ps1_xa.h"
#include "ps1_seq.h"
#include <stdbool.h>
#include <stdint.h>

struct SDL_AudioStream;

typedef struct {
    Ps1Spu                  spu;
    Ps1XaPlayer             xa;
    Ps1Seq                  sequence;
    struct SDL_AudioStream *stream;
    bool                    has_output;
} Ps1Audio;

/* Initialize the audio system.
 * Opens an SDL3 audio stream if an output device is available.
 * If no device is available, runs in headless mode gracefully. */
bool ps1_audio_init(Ps1Audio *audio);

/* Shut down audio system and release resources */
void ps1_audio_shutdown(Ps1Audio *audio);

/* Advance audio by 1 tick (at 60 Hz = 735 frames of 44.1 kHz audio).
 * Mixes SPU and CD-XA streams, pushing PCM to SDL3 audio stream. */
void ps1_audio_tick(Ps1Audio *audio, int speed_mode);

/* Direct mix helper: mixes frames stereo 16-bit samples (SPU + XA) into out. */
void ps1_audio_mix(Ps1Audio *audio, int16_t *out, int frames);

#endif /* PS1_AUDIO_H */
