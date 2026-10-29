#include "platform/audio_output.h"
#include "platform/audio_ring.h"
#include "psx/sound_voice.h"
#include <SDL3/SDL.h>
#include <stdio.h>
static SDL_AudioDeviceID hm_audio_device;
static hm_audio_ring hm_audio;
static void hm_audio_submit(void *context, const s16 *stereo, u32 frames) {
    hm_audio_ring_push(context, stereo, frames);
}
static void SDLCALL hm_audio_callback(void *context, const SDL_AudioSpec *spec, float *buffer,
                                      int bytes) {
    if (spec->channels <= 0 || spec->freq <= 0 || bytes <= 0)
        return;
    hm_audio_ring_read(context, buffer, (u32)bytes / ((u32)spec->channels * sizeof(float)),
                       (u32)spec->channels, (u32)spec->freq);
}
int hm_audio_output_start(void) {
#if defined(__ANDROID__)
    hm_audio_ring_init(&hm_audio, 2940);
#else
    hm_audio_ring_init(&hm_audio, 1470);
#endif
    SDL_AudioSpec desired = {.format = SDL_AUDIO_F32, .channels = 2, .freq = 44100};
    hm_audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
    if (!hm_audio_device ||
        !SDL_SetAudioPostmixCallback(hm_audio_device, hm_audio_callback, &hm_audio))
        return 0;
    SDL_AudioSpec actual;
    int frames;
    if (SDL_GetAudioDeviceFormat(hm_audio_device, &actual, &frames)) {
        printf("Native audio device: %d Hz, %d channels, %d frames\n", actual.freq, actual.channels,
               frames);
        fflush(stdout);
    }
    hm_sound_set_sink(hm_audio_submit, &hm_audio);
    return 1;
}
void hm_audio_output_stop(void) {
    hm_sound_set_sink(NULL, NULL);
    if (hm_audio_device) {
        SDL_CloseAudioDevice(hm_audio_device);
        hm_audio_device = 0;
        fprintf(stderr, "Audio output frames=%llu underflows=%llu dropped=%llu\n",
                (unsigned long long)hm_audio.rendered, (unsigned long long)hm_audio.underflows,
                (unsigned long long)hm_audio.dropped);
    }
}
void hm_audio_output_pause(int paused) {
    if (hm_audio_device) {
        if (paused)
            SDL_PauseAudioDevice(hm_audio_device);
        else
            SDL_ResumeAudioDevice(hm_audio_device);
    }
}
