#ifndef HMBTN_AUDIO_OPTIONS_H
#define HMBTN_AUDIO_OPTIONS_H

#include <stddef.h>

typedef struct {
    int bgm_volume;    /* 0..100% (default 100) */
    int sfx_volume;    /* 0..100% (default 100) */
    int hq_resampler;  /* 0 = PS1 Gaussian, 1 = Studio Cubic (default 0) */
} hm_audio_options;

void hm_audio_defaults(hm_audio_options *options);
void hm_audio_load(hm_audio_options *options);
int hm_audio_save(const hm_audio_options *options);
int hm_audio_change(hm_audio_options *options, int item, int direction);
void hm_audio_value(const hm_audio_options *options, int item, char *text, size_t capacity);
void hm_audio_row_value(const hm_audio_options *options, int item, char *text, size_t capacity);

int hm_audio_bgm_volume(void);
int hm_audio_sfx_volume(void);
int hm_audio_hq_resampler(void);

#endif
