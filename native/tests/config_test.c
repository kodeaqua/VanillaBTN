#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

static void test_defaults(void) {
    AppConfig cfg;
    config_set_defaults(&cfg);

    assert(cfg.preset == PRESET_MODERN);
    assert(cfg.display.window_mode == WINDOW_MODE_WINDOWED);
    assert(cfg.display.window_width == 1280);
    assert(cfg.display.window_height == 960);
    assert(cfg.display.backend == BACKEND_AUTO);
    assert(cfg.display.vsync == true);
    assert(cfg.display.fps_limit == 0);
    assert(cfg.display.widescreen == WIDESCREEN_AUTO);

    /* TriStates should be AUTO by default */
    assert(cfg.fixes.pgxp_vertex_precision == TRISTATE_AUTO);
    assert(cfg.fixes.true_depth_buffer == TRISTATE_AUTO);
    assert(cfg.fixes.true_color_32bit == TRISTATE_AUTO);
    assert(cfg.fixes.dithering == DITHER_AUTO);
    assert(cfg.fixes.expand_voice_polyphony == TRISTATE_AUTO);

    /* Resolution with modern preset */
    assert(config_resolve_bool(cfg.fixes.pgxp_vertex_precision, cfg.preset, true) == true);
    assert(config_resolve_bool(cfg.fixes.true_color_32bit, cfg.preset, true) == true);
    assert(config_resolve_dither(cfg.fixes.dithering, cfg.preset) == DITHER_OFF);
    assert(config_resolve_spu_interp(cfg.audio.spu_interpolation, cfg.preset) == SPU_INTERP_GAUSSIAN);
    assert(config_resolve_anti_aliasing(cfg.fixes.anti_aliasing, cfg.preset) == AA_MSAA_4X);

    /* Resolution with authentic PS1 preset */
    assert(config_resolve_bool(cfg.fixes.pgxp_vertex_precision, PRESET_AUTHENTIC_PS1, true) == false);
    assert(config_resolve_bool(cfg.fixes.true_color_32bit, PRESET_AUTHENTIC_PS1, true) == false);
    assert(config_resolve_dither(cfg.fixes.dithering, PRESET_AUTHENTIC_PS1) == DITHER_NATIVE);
    assert(config_resolve_spu_interp(cfg.audio.spu_interpolation, PRESET_AUTHENTIC_PS1) == SPU_INTERP_LINEAR);
    assert(config_resolve_anti_aliasing(cfg.fixes.anti_aliasing, PRESET_AUTHENTIC_PS1) == AA_OFF);

    /* Explicit overrides take precedence regardless of preset */
    assert(config_resolve_bool(TRISTATE_ON, PRESET_AUTHENTIC_PS1, true) == true);
    assert(config_resolve_bool(TRISTATE_OFF, PRESET_MODERN, true) == false);
}

static void test_save_and_load(void) {
    const char *test_ini = "test_vanillabtn.ini";
    SDL_RemovePath(test_ini);

    AppConfig original;
    config_set_defaults(&original);
    original.display.window_width = 1920;
    original.display.window_height = 1080;
    original.display.window_mode = WINDOW_MODE_BORDERLESS;
    original.display.backend = BACKEND_VULKAN;
    original.display.widescreen = WIDESCREEN_ON;
    original.fixes.pgxp_vertex_precision = TRISTATE_ON;
    original.fixes.dithering = DITHER_DEBAND;
    original.graphics.render_scale = 4;
    original.graphics.texture_filter = FILTER_BILINEAR;
    original.input.analog_deadzone = 0.15f;
    strncpy(original.input.turbo_hotkey, "Space", sizeof(original.input.turbo_hotkey) - 1);

    assert(config_save(&original, test_ini) == true);

    AppConfig loaded;
    assert(config_load(&loaded, test_ini) == true);

    assert(loaded.preset == PRESET_MODERN);
    assert(loaded.display.window_width == 1920);
    assert(loaded.display.window_height == 1080);
    assert(loaded.display.window_mode == WINDOW_MODE_BORDERLESS);
    assert(loaded.display.backend == BACKEND_VULKAN);
    assert(loaded.display.widescreen == WIDESCREEN_ON);
    assert(loaded.fixes.pgxp_vertex_precision == TRISTATE_ON);
    assert(loaded.fixes.dithering == DITHER_DEBAND);
    assert(loaded.graphics.render_scale == 4);
    assert(loaded.graphics.texture_filter == FILTER_BILINEAR);
    assert(loaded.input.analog_deadzone > 0.14f && loaded.input.analog_deadzone < 0.16f);
    assert(strcmp(loaded.input.turbo_hotkey, "Space") == 0);

    SDL_RemovePath(test_ini);
}

static void test_presets_parsing(void) {
    const char *test_ini = "test_presets.ini";
    FILE *f = fopen(test_ini, "w");
    assert(f != NULL);
    fprintf(f, "[Preset]\nmode = authentic_ps1\n");
    fclose(f);

    AppConfig cfg;
    assert(config_load(&cfg, test_ini) == true);
    assert(cfg.preset == PRESET_AUTHENTIC_PS1);
    SDL_RemovePath(test_ini);

    f = fopen(test_ini, "w");
    assert(f != NULL);
    fprintf(f, "[Preset]\nmode = custom\n[PS1_Fixes]\npgxp_vertex_precision = on\n");
    fclose(f);

    assert(config_load(&cfg, test_ini) == true);
    assert(cfg.preset == PRESET_CUSTOM);
    assert(cfg.fixes.pgxp_vertex_precision == TRISTATE_ON);
    SDL_RemovePath(test_ini);
}

static void test_robust_fallback(void) {
    const char *test_ini = "test_corrupt.ini";
    FILE *f = fopen(test_ini, "w");
    assert(f != NULL);
    fprintf(f, "garbage random text\n[Display\n===bad===\nwindow_width = -500\n");
    fclose(f);

    AppConfig cfg;
    assert(config_load(&cfg, test_ini) == true);
    /* Should retain sane defaults on corrupt input */
    assert(cfg.display.window_width == 1280);
    assert(cfg.preset == PRESET_MODERN);

    SDL_RemovePath(test_ini);
}

int main(void) {
    test_defaults();
    test_save_and_load();
    test_presets_parsing();
    test_robust_fallback();
    printf("config_test: all tests passed\n");
    return 0;
}
