#include "psx/sound_control.h"
#include "platform/audio_options.h"
#include "platform/settings_json.h"
#include "platform/user_data.h"
#include "psx/spu_voice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static hm_audio_options active_audio = {
    .bgm_volume = 100,
    .sfx_volume = 100,
    .hq_resampler = 0,
};

void hm_audio_defaults(hm_audio_options *options) {
    if (!options)
        return;
    options->bgm_volume = 100;
    options->sfx_volume = 100;
    options->hq_resampler = 0;
}

void hm_audio_load(hm_audio_options *options) {
    hm_audio_defaults(options);
    char *text = malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object;
    size_t length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "audio", &object, &length) == 1) {
        int val = 0;
        if (hm_json_integer(object, length, "bgm_volume", &val) == 1) {
            if (val < 0) val = 0; else if (val > 100) val = 100;
            options->bgm_volume = val;
        }
        if (hm_json_integer(object, length, "sfx_volume", &val) == 1) {
            if (val < 0) val = 0; else if (val > 100) val = 100;
            options->sfx_volume = val;
        }
        const char *prop_val;
        size_t prop_len;
        if (hm_json_property(object, length, "hq_resampler", &prop_val, &prop_len) == 1) {
            options->hq_resampler = (prop_len == 4 && !memcmp(prop_val, "true", 4));
        }
    }
    free(text);
    active_audio = *options;
    hm_spu_set_hq(active_audio.hq_resampler);
    hm_sound_set_volumes(active_audio.bgm_volume, active_audio.sfx_volume);
}

int hm_audio_save(const hm_audio_options *options) {
    if (!options)
        return 0;
    active_audio = *options;
    hm_spu_set_hq(active_audio.hq_resampler);
    hm_sound_set_volumes(active_audio.bgm_volume, active_audio.sfx_volume);
    int ok = 1;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", options->bgm_volume);
    ok &= hm_user_data_set_nested_setting("audio", "bgm_volume", buf);
    snprintf(buf, sizeof(buf), "%d", options->sfx_volume);
    ok &= hm_user_data_set_nested_setting("audio", "sfx_volume", buf);
    ok &= hm_user_data_set_nested_setting("audio", "hq_resampler",
                                          options->hq_resampler ? "true" : "false");
    return ok;
}

int hm_audio_change(hm_audio_options *options, int item, int direction) {
    if (!options)
        return 0;
    switch (item) {
    case 0: /* BGM Volume (+/- 10%) */
        options->bgm_volume += direction * 10;
        if (options->bgm_volume < 0) options->bgm_volume = 0;
        if (options->bgm_volume > 100) options->bgm_volume = 100;
        break;
    case 1: /* SFX Volume (+/- 10%) */
        options->sfx_volume += direction * 10;
        if (options->sfx_volume < 0) options->sfx_volume = 0;
        if (options->sfx_volume > 100) options->sfx_volume = 100;
        break;
    case 2: /* HQ Resampler Toggle */
        options->hq_resampler = !options->hq_resampler;
        break;
    default:
        return 0;
    }
    active_audio = *options;
    hm_spu_set_hq(active_audio.hq_resampler);
    hm_sound_set_volumes(active_audio.bgm_volume, active_audio.sfx_volume);
    return 1;
}

void hm_audio_value(const hm_audio_options *options, int item, char *text, size_t capacity) {
    if (!options || !text || !capacity)
        return;
    switch (item) {
    case 0:
        snprintf(text, capacity, "BGM volume level (0 to 100)");
        break;
    case 1:
        snprintf(text, capacity, "Sound effects volume (0 to 100)");
        break;
    case 2:
        snprintf(text, capacity, "Audio resampling quality mode");
        break;
    default:
        text[0] = 0;
        break;
    }
}

void hm_audio_row_value(const hm_audio_options *options, int item, char *text, size_t capacity) {
    if (!options || !text || !capacity)
        return;
    switch (item) {
    case 0:
        snprintf(text, capacity, "%d%%", options->bgm_volume);
        break;
    case 1:
        snprintf(text, capacity, "%d%%", options->sfx_volume);
        break;
    case 2:
        snprintf(text, capacity, "%s", options->hq_resampler ? "Studio HQ" : "PS1 Gaussian");
        break;
    default:
        text[0] = 0;
        break;
    }
}

int hm_audio_bgm_volume(void) {
    return active_audio.bgm_volume;
}

int hm_audio_sfx_volume(void) {
    return active_audio.sfx_volume;
}

int hm_audio_hq_resampler(void) {
    return active_audio.hq_resampler;
}
