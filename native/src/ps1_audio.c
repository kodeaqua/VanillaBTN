/* ps1_audio.c — host-side audio system integrating PS1 SPU and CD-XA with SDL3. */

#include "ps1_audio.h"
#include <SDL3/SDL.h>
#include <string.h>

#define FRAMES_PER_60HZ_TICK (PS1_SPU_SAMPLE_RATE / 60) /* 735 frames */
#define MAX_QUEUED_BYTES (4 * FRAMES_PER_60HZ_TICK * 2 * (int)sizeof(int16_t)) /* ~11760 bytes = ~66 ms */

bool ps1_audio_init(Ps1Audio *audio) {
    if (!audio) return false;
    memset(audio, 0, sizeof(*audio));

    ps1_spu_init(&audio->spu);
    ps1_xa_init(&audio->xa);

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 2;
    spec.freq = PS1_SPU_SAMPLE_RATE;

    audio->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (audio->stream) {
        SDL_ResumeAudioStreamDevice(audio->stream);
        audio->has_output = true;
    } else {
        audio->has_output = false;
    }

    return true;
}

void ps1_audio_shutdown(Ps1Audio *audio) {
    if (!audio) return;

    if (audio->stream) {
        SDL_DestroyAudioStream(audio->stream);
        audio->stream = NULL;
    }
    audio->has_output = false;
}

void ps1_audio_mix(Ps1Audio *audio, int16_t *out, int frames) {
    if (!audio || !out || frames <= 0) return;

    /* Mix SPU 24 voices */
    if (audio->sequence.data != NULL) ps1_seq_mix(&audio->sequence, &audio->spu, out, frames);
    else ps1_spu_mix(&audio->spu, out, frames, 0);

    /* Mix CD-XA stream, accumulating on top of SPU */
    ps1_xa_mix(&audio->xa, out, frames,
               audio->spu.cd_vol_left, audio->spu.cd_vol_right, 1);
}

void ps1_audio_tick(Ps1Audio *audio, int speed_mode) {
    if (!audio) return;

    /* Advance CD-XA fade */
    ps1_xa_fade_tick(&audio->xa, speed_mode);

    int16_t buffer[FRAMES_PER_60HZ_TICK * 2];
    ps1_audio_mix(audio, buffer, FRAMES_PER_60HZ_TICK);
    if (!audio->has_output || !audio->stream) return;

    /* Check if audio queue is getting too full to avoid latency build-up */
    int queued = SDL_GetAudioStreamQueued(audio->stream);
    if (queued >= MAX_QUEUED_BYTES) return;

    SDL_PutAudioStreamData(audio->stream, buffer, (int)sizeof(buffer));
}
