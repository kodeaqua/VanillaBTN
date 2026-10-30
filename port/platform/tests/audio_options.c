#include "platform/audio_options.h"
#include "platform/user_data.h"
#include "psx/sound_control.h"
#include "psx/spu_voice.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(int argc, char **argv) {
    CHECK(argc == 2 && SDL_Init(0));
    char *root = NULL;
    CHECK(SDL_asprintf(&root, "%s-%llu", argv[1], (unsigned long long)SDL_GetPerformanceCounter()) >= 0);
    CHECK(hm_user_data_start_at(root));

    hm_audio_options opt;
    hm_audio_defaults(&opt);

    CHECK(opt.bgm_volume == 100);
    CHECK(opt.sfx_volume == 100);
    CHECK(opt.hq_resampler == 0);
    hm_spu_set_hq(1);
    hm_audio_load(&opt);
    CHECK(opt.hq_resampler == 0);
    CHECK(hm_spu_get_hq() == 0);

    char val[64];
    hm_audio_row_value(&opt, 0, val, sizeof(val));
    CHECK(!strcmp(val, "100%"));
    hm_audio_row_value(&opt, 1, val, sizeof(val));
    CHECK(!strcmp(val, "100%"));
    hm_audio_row_value(&opt, 2, val, sizeof(val));
    CHECK(!strcmp(val, "PS1 Gaussian"));

    /* Test volume stepping and clamping */
    CHECK(hm_audio_change(&opt, 0, -1) == 1);
    CHECK(opt.bgm_volume == 90);
    CHECK(hm_sound_get_bgm_volume() == 90);

    CHECK(hm_audio_change(&opt, 1, -2) == 1);
    CHECK(opt.sfx_volume == 80);
    CHECK(hm_sound_get_sfx_volume() == 80);

    CHECK(hm_audio_change(&opt, 2, 1) == 1);
    CHECK(opt.hq_resampler == 1);
    hm_audio_row_value(&opt, 2, val, sizeof(val));
    CHECK(!strcmp(val, "Studio HQ"));

    /* Test save & load persistence */
    CHECK(hm_audio_save(&opt) == 1);

    hm_audio_options loaded;
    hm_audio_load(&loaded);
    CHECK(loaded.bgm_volume == 90);
    CHECK(loaded.sfx_volume == 80);
    CHECK(loaded.hq_resampler == 1);
    CHECK(hm_spu_get_hq() == 1);
    CHECK(hm_sound_get_bgm_volume() == 90);
    CHECK(hm_sound_get_sfx_volume() == 80);

    /* Clamp test */
    for (int i = 0; i < 15; ++i)
        hm_audio_change(&opt, 0, -1);
    CHECK(opt.bgm_volume == 0);
    CHECK(hm_sound_get_bgm_volume() == 0);

    for (int i = 0; i < 15; ++i)
        hm_audio_change(&opt, 0, 1);
    CHECK(opt.bgm_volume == 100);
    CHECK(hm_sound_get_bgm_volume() == 100);

    /* Check helper descriptions fit 34-char font box */
    for (int i = 0; i < 3; ++i) {
        char desc[128];
        hm_audio_value(&opt, i, desc, sizeof(desc));
        CHECK(strlen(desc) <= 34);
    }

    SDL_free(root);
    SDL_Quit();
    return 0;
}
