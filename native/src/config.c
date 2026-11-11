#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <SDL3/SDL.h>

void config_set_defaults(AppConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));

    config->preset = PRESET_MODERN;

    config->display.window_mode = WINDOW_MODE_WINDOWED;
    config->display.monitor = 0;
    config->display.window_width = 1280;
    config->display.window_height = 960;
    config->display.fullscreen_width = 0;
    config->display.fullscreen_height = 0;
    config->display.fullscreen_hz = 0;
    config->display.backend = BACKEND_AUTO;
    config->display.vsync = true;
    config->display.fps_limit = 0; /* 0 = match active monitor */
    config->display.widescreen = WIDESCREEN_AUTO;
    config->display.integer_scale = false;
    config->display.par = PAR_SQUARE;

    config->fixes.pgxp_vertex_precision = TRISTATE_AUTO;
    config->fixes.pgxp_perspective_texture = TRISTATE_AUTO;
    config->fixes.true_depth_buffer = TRISTATE_AUTO;
    config->fixes.near_plane_w_clipping = TRISTATE_AUTO;
    config->fixes.quad_subdivision = TRISTATE_AUTO;
    config->fixes.smooth_camera_angles = TRISTATE_AUTO;
    config->fixes.skeletal_precision = TRISTATE_AUTO;
    config->fixes.smooth_normals = TRISTATE_AUTO;
    config->fixes.true_color_32bit = TRISTATE_AUTO;
    config->fixes.dithering = DITHER_AUTO;
    config->fixes.texture_bleeding_fix = TRISTATE_AUTO;
    config->fixes.alpha_halo_fix = TRISTATE_AUTO;
    config->fixes.mipmapping = TRISTATE_AUTO;
    config->fixes.anti_aliasing = AA_AUTO;
    config->fixes.smooth_fog = TRISTATE_AUTO;
    config->fixes.oit_transparency = TRISTATE_AUTO;
    config->fixes.zero_fillrate_lag = TRISTATE_AUTO;
    config->fixes.hdr_tonemapping = TRISTATE_AUTO;
    config->fixes.atomic_clut = TRISTATE_AUTO;
    config->fixes.expand_voice_polyphony = TRISTATE_AUTO;
    config->fixes.seamless_cd_loop = TRISTATE_AUTO;
    config->fixes.audio_pop_suppression = TRISTATE_AUTO;
    config->fixes.vag_loop_crossfade = TRISTATE_AUTO;
    config->fixes.float_audio_mixing = TRISTATE_AUTO;
    config->fixes.hifi_reverb = TRISTATE_AUTO;
    config->fixes.fix_diagonal_running = TRISTATE_AUTO;
    config->fixes.hd_rumble = TRISTATE_AUTO;

    config->graphics.texture_filter = FILTER_NEAREST;
    config->graphics.render_scale = 1;
    config->graphics.hd_font = TRISTATE_AUTO;
    config->graphics.extended_draw_distance = TRISTATE_AUTO;

    config->audio.spu_interpolation = SPU_INTERP_AUTO;
    config->audio.master_volume = 100;
    config->audio.mute_on_unfocus = false;

    config->input.analog_deadzone = 0.10f;
    config->input.pause_on_unfocus = false;
    config->input.pause_on_disconnect = true;
    strncpy(config->input.turbo_hotkey, "Tab", sizeof(config->input.turbo_hotkey) - 1);
    config->input.turbo_speed = 3.0f;
}

bool config_resolve_bool(TriState state, PresetMode preset, bool default_for_modern) {
    if (state == TRISTATE_ON) return true;
    if (state == TRISTATE_OFF) return false;
    if (preset == PRESET_AUTHENTIC_PS1) return false;
    return default_for_modern;
}

DitherMode config_resolve_dither(DitherMode mode, PresetMode preset) {
    if (mode != DITHER_AUTO) return mode;
    if (preset == PRESET_AUTHENTIC_PS1) return DITHER_NATIVE;
    return DITHER_OFF;
}

SpuInterpolationMode config_resolve_spu_interp(SpuInterpolationMode mode, PresetMode preset) {
    if (mode != SPU_INTERP_AUTO) return mode;
    if (preset == PRESET_AUTHENTIC_PS1) return SPU_INTERP_LINEAR;
    return SPU_INTERP_GAUSSIAN;
}

AntiAliasingMode config_resolve_anti_aliasing(AntiAliasingMode mode, PresetMode preset) {
    if (mode != AA_AUTO) return mode;
    if (preset == PRESET_AUTHENTIC_PS1) return AA_OFF;
    return AA_MSAA_4X;
}

static void trim_inplace(char *str) {
    if (!str) return;
    char *start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

static bool str_equals_case(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == *b;
}

static bool parse_bool(const char *val, bool def) {
    if (!val) return def;
    if (str_equals_case(val, "true") || str_equals_case(val, "on") ||
        str_equals_case(val, "1") || str_equals_case(val, "yes")) {
        return true;
    }
    if (str_equals_case(val, "false") || str_equals_case(val, "off") ||
        str_equals_case(val, "0") || str_equals_case(val, "no")) {
        return false;
    }
    return def;
}

static TriState parse_tristate(const char *val) {
    if (!val) return TRISTATE_AUTO;
    if (str_equals_case(val, "auto")) return TRISTATE_AUTO;
    if (parse_bool(val, false)) return TRISTATE_ON;
    if (str_equals_case(val, "false") || str_equals_case(val, "off") ||
        str_equals_case(val, "0") || str_equals_case(val, "no")) {
        return TRISTATE_OFF;
    }
    return TRISTATE_AUTO;
}

static const char *tristate_str(TriState s) {
    switch (s) {
    case TRISTATE_ON: return "on";
    case TRISTATE_OFF: return "off";
    default: return "auto";
    }
}

bool config_resolve_path(char *out_path, size_t out_size, const char *custom_path) {
    if (!out_path || out_size == 0) return false;
    if (custom_path && *custom_path) {
        snprintf(out_path, out_size, "%s", custom_path);
        return true;
    }

    /* 1. Try local directory first (portable application folder). */
    FILE *f = fopen(CONFIG_FILENAME, "rb");
    if (f) {
        fclose(f);
        snprintf(out_path, out_size, "%s", CONFIG_FILENAME);
        return true;
    }

    /* 2. Try application base path if available from SDL. */
    const char *base = SDL_GetBasePath();
    if (base && *base) {
        snprintf(out_path, out_size, "%s%s", base, CONFIG_FILENAME);
        f = fopen(out_path, "rb");
        if (f) {
            fclose(f);
            return true;
        }
    }

    /* 3. Fallback to default local filename. */
    snprintf(out_path, out_size, "%s", CONFIG_FILENAME);
    return true;
}

static void parse_kv(AppConfig *config, const char *section, const char *key, const char *val) {
    if (str_equals_case(section, "Preset")) {
        if (str_equals_case(key, "mode")) {
            if (str_equals_case(val, "authentic_ps1") || str_equals_case(val, "ps1") || str_equals_case(val, "classic")) {
                config->preset = PRESET_AUTHENTIC_PS1;
            } else if (str_equals_case(val, "custom")) {
                config->preset = PRESET_CUSTOM;
            } else {
                config->preset = PRESET_MODERN;
            }
        }
    } else if (str_equals_case(section, "Display")) {
        if (str_equals_case(key, "window_mode")) {
            if (str_equals_case(val, "borderless")) config->display.window_mode = WINDOW_MODE_BORDERLESS;
            else if (str_equals_case(val, "fullscreen")) config->display.window_mode = WINDOW_MODE_FULLSCREEN;
            else if (str_equals_case(val, "maximized")) config->display.window_mode = WINDOW_MODE_MAXIMIZED;
            else config->display.window_mode = WINDOW_MODE_WINDOWED;
        } else if (str_equals_case(key, "monitor")) {
            config->display.monitor = atoi(val);
        } else if (str_equals_case(key, "window_width")) {
            int w = atoi(val);
            if (w >= 320) config->display.window_width = w;
        } else if (str_equals_case(key, "window_height")) {
            int h = atoi(val);
            if (h >= 240) config->display.window_height = h;
        } else if (str_equals_case(key, "fullscreen_width")) {
            config->display.fullscreen_width = atoi(val);
        } else if (str_equals_case(key, "fullscreen_height")) {
            config->display.fullscreen_height = atoi(val);
        } else if (str_equals_case(key, "fullscreen_hz")) {
            config->display.fullscreen_hz = atoi(val);
        } else if (str_equals_case(key, "backend")) {
            if (str_equals_case(val, "vulkan") || str_equals_case(val, "vk")) config->display.backend = BACKEND_VULKAN;
            else if (str_equals_case(val, "opengl") || str_equals_case(val, "gl")) config->display.backend = BACKEND_OPENGL;
            else if (str_equals_case(val, "d3d11") || str_equals_case(val, "direct3d11")) config->display.backend = BACKEND_D3D11;
            else if (str_equals_case(val, "d3d12") || str_equals_case(val, "direct3d12")) config->display.backend = BACKEND_D3D12;
            else if (str_equals_case(val, "software") || str_equals_case(val, "sw")) config->display.backend = BACKEND_SOFTWARE;
            else config->display.backend = BACKEND_AUTO;
        } else if (str_equals_case(key, "vsync")) {
            config->display.vsync = parse_bool(val, true);
        } else if (str_equals_case(key, "fps_limit")) {
            if (str_equals_case(val, "monitor")) config->display.fps_limit = 0;
            else if (str_equals_case(val, "unlocked") || str_equals_case(val, "none")) config->display.fps_limit = -1;
            else config->display.fps_limit = atoi(val);
        } else if (str_equals_case(key, "widescreen")) {
            if (str_equals_case(val, "off")) config->display.widescreen = WIDESCREEN_OFF;
            else if (str_equals_case(val, "on")) config->display.widescreen = WIDESCREEN_ON;
            else if (str_equals_case(val, "stretch")) config->display.widescreen = WIDESCREEN_STRETCH;
            else config->display.widescreen = WIDESCREEN_AUTO;
        } else if (str_equals_case(key, "integer_scale")) {
            config->display.integer_scale = parse_bool(val, false);
        } else if (str_equals_case(key, "pixel_aspect_ratio")) {
            if (str_equals_case(val, "crt_ntsc") || str_equals_case(val, "ntsc")) config->display.par = PAR_CRT_NTSC;
            else config->display.par = PAR_SQUARE;
        }
    } else if (str_equals_case(section, "PS1_Fixes")) {
        if (str_equals_case(key, "pgxp_vertex_precision")) config->fixes.pgxp_vertex_precision = parse_tristate(val);
        else if (str_equals_case(key, "pgxp_perspective_texture")) config->fixes.pgxp_perspective_texture = parse_tristate(val);
        else if (str_equals_case(key, "true_depth_buffer")) config->fixes.true_depth_buffer = parse_tristate(val);
        else if (str_equals_case(key, "near_plane_w_clipping")) config->fixes.near_plane_w_clipping = parse_tristate(val);
        else if (str_equals_case(key, "quad_subdivision")) config->fixes.quad_subdivision = parse_tristate(val);
        else if (str_equals_case(key, "smooth_camera_angles")) config->fixes.smooth_camera_angles = parse_tristate(val);
        else if (str_equals_case(key, "skeletal_precision")) config->fixes.skeletal_precision = parse_tristate(val);
        else if (str_equals_case(key, "smooth_normals")) config->fixes.smooth_normals = parse_tristate(val);
        else if (str_equals_case(key, "true_color_32bit")) config->fixes.true_color_32bit = parse_tristate(val);
        else if (str_equals_case(key, "dithering")) {
            if (str_equals_case(val, "off")) config->fixes.dithering = DITHER_OFF;
            else if (str_equals_case(val, "native")) config->fixes.dithering = DITHER_NATIVE;
            else if (str_equals_case(val, "deband")) config->fixes.dithering = DITHER_DEBAND;
            else config->fixes.dithering = DITHER_AUTO;
        } else if (str_equals_case(key, "texture_bleeding_fix")) config->fixes.texture_bleeding_fix = parse_tristate(val);
        else if (str_equals_case(key, "alpha_halo_fix")) config->fixes.alpha_halo_fix = parse_tristate(val);
        else if (str_equals_case(key, "mipmapping")) config->fixes.mipmapping = parse_tristate(val);
        else if (str_equals_case(key, "anti_aliasing")) {
            if (str_equals_case(val, "off")) config->fixes.anti_aliasing = AA_OFF;
            else if (str_equals_case(val, "msaa_4x") || str_equals_case(val, "msaa")) config->fixes.anti_aliasing = AA_MSAA_4X;
            else if (str_equals_case(val, "smaa")) config->fixes.anti_aliasing = AA_SMAA;
            else config->fixes.anti_aliasing = AA_AUTO;
        } else if (str_equals_case(key, "smooth_fog")) config->fixes.smooth_fog = parse_tristate(val);
        else if (str_equals_case(key, "oit_transparency")) config->fixes.oit_transparency = parse_tristate(val);
        else if (str_equals_case(key, "zero_fillrate_lag")) config->fixes.zero_fillrate_lag = parse_tristate(val);
        else if (str_equals_case(key, "hdr_tonemapping")) config->fixes.hdr_tonemapping = parse_tristate(val);
        else if (str_equals_case(key, "atomic_clut")) config->fixes.atomic_clut = parse_tristate(val);
        else if (str_equals_case(key, "expand_voice_polyphony")) config->fixes.expand_voice_polyphony = parse_tristate(val);
        else if (str_equals_case(key, "seamless_cd_loop")) config->fixes.seamless_cd_loop = parse_tristate(val);
        else if (str_equals_case(key, "audio_pop_suppression")) config->fixes.audio_pop_suppression = parse_tristate(val);
        else if (str_equals_case(key, "vag_loop_crossfade")) config->fixes.vag_loop_crossfade = parse_tristate(val);
        else if (str_equals_case(key, "float_audio_mixing")) config->fixes.float_audio_mixing = parse_tristate(val);
        else if (str_equals_case(key, "hifi_reverb")) config->fixes.hifi_reverb = parse_tristate(val);
        else if (str_equals_case(key, "fix_diagonal_running")) config->fixes.fix_diagonal_running = parse_tristate(val);
        else if (str_equals_case(key, "hd_rumble")) config->fixes.hd_rumble = parse_tristate(val);
    } else if (str_equals_case(section, "Graphics_Enhancements")) {
        if (str_equals_case(key, "texture_filter")) {
            if (str_equals_case(val, "bilinear")) config->graphics.texture_filter = FILTER_BILINEAR;
            else if (str_equals_case(val, "xbrz")) config->graphics.texture_filter = FILTER_XBRZ;
            else config->graphics.texture_filter = FILTER_NEAREST;
        } else if (str_equals_case(key, "render_scale")) {
            if (str_equals_case(val, "native")) config->graphics.render_scale = 0;
            else {
                int scale = atoi(val);
                if (scale >= 1 && scale <= 16) config->graphics.render_scale = scale;
            }
        } else if (str_equals_case(key, "hd_font")) {
            config->graphics.hd_font = parse_tristate(val);
        } else if (str_equals_case(key, "extended_draw_distance")) {
            config->graphics.extended_draw_distance = parse_tristate(val);
        }
    } else if (str_equals_case(section, "Audio_Enhancements")) {
        if (str_equals_case(key, "spu_interpolation")) {
            if (str_equals_case(val, "gaussian")) config->audio.spu_interpolation = SPU_INTERP_GAUSSIAN;
            else if (str_equals_case(val, "linear")) config->audio.spu_interpolation = SPU_INTERP_LINEAR;
            else if (str_equals_case(val, "cubic")) config->audio.spu_interpolation = SPU_INTERP_CUBIC;
            else config->audio.spu_interpolation = SPU_INTERP_AUTO;
        } else if (str_equals_case(key, "master_volume")) {
            int vol = atoi(val);
            if (vol < 0) vol = 0;
            if (vol > 100) vol = 100;
            config->audio.master_volume = vol;
        } else if (str_equals_case(key, "mute_on_unfocus")) {
            config->audio.mute_on_unfocus = parse_bool(val, false);
        }
    } else if (str_equals_case(section, "Input_QoL")) {
        if (str_equals_case(key, "analog_deadzone")) {
            float dz = (float)atof(val);
            if (dz < 0.0f) dz = 0.0f;
            if (dz > 0.50f) dz = 0.50f;
            config->input.analog_deadzone = dz;
        } else if (str_equals_case(key, "pause_on_unfocus")) {
            config->input.pause_on_unfocus = parse_bool(val, false);
        } else if (str_equals_case(key, "pause_on_disconnect")) {
            config->input.pause_on_disconnect = parse_bool(val, true);
        } else if (str_equals_case(key, "turbo_hotkey")) {
            strncpy(config->input.turbo_hotkey, val, sizeof(config->input.turbo_hotkey) - 1);
            config->input.turbo_hotkey[sizeof(config->input.turbo_hotkey) - 1] = '\0';
        } else if (str_equals_case(key, "turbo_speed")) {
            float spd = (float)atof(val);
            if (spd < 1.0f) spd = 1.0f;
            if (spd > 10.0f) spd = 10.0f;
            config->input.turbo_speed = spd;
        }
    }
}

bool config_load(AppConfig *config, const char *custom_path) {
    if (!config) return false;
    config_set_defaults(config);

    char filepath[1024];
    if (!config_resolve_path(filepath, sizeof(filepath), custom_path)) return false;

    FILE *f = fopen(filepath, "r");
    if (!f) {
        /* File doesn't exist: generate a default commented configuration file. */
        config_save(config, filepath);
        return true;
    }

    char line[512];
    char current_section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        trim_inplace(line);
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') continue;

        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, line + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
                trim_inplace(current_section);
            }
            continue;
        }

        /* Strip inline comments */
        char *comment = strchr(line, ';');
        if (comment) *comment = '\0';
        comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *sep = strchr(line, '=');
        if (sep) {
            *sep = '\0';
            char *key = line;
            char *val = sep + 1;
            trim_inplace(key);
            trim_inplace(val);
            if (*key) parse_kv(config, current_section, key, val);
        }
    }

    fclose(f);
    return true;
}

bool config_save(const AppConfig *config, const char *path) {
    if (!config || !path || !*path) return false;

    char tmp_path[1050];
    int path_length = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (path_length < 0 || (size_t)path_length >= sizeof(tmp_path)) return false;

    FILE *f = fopen(tmp_path, "w");
    if (!f) return false;

    fprintf(f,
        "; ==============================================================================\n"
        "; VANILLABTN MASTER CONFIGURATION\n"
        "; ==============================================================================\n\n"
        "[Preset]\n"
        "; Master preset mode: modern, authentic_ps1, custom\n"
        "; - modern: ALL 35 PS1 flaw fixes are automatically enabled (Recommended!)\n"
        "; - authentic_ps1: 100%% authentic 1999 console experience (all fixes OFF)\n"
        "; - custom: strictly follows individual switches below\n"
        "mode = %s\n\n",
        config->preset == PRESET_AUTHENTIC_PS1 ? "authentic_ps1" :
        config->preset == PRESET_CUSTOM ? "custom" : "modern");

    fprintf(f,
        "[Display]\n"
        "; Window mode: windowed, borderless, fullscreen, maximized\n"
        "window_mode = %s\n\n"
        "; Active monitor index (0 = primary display, 1 = secondary, etc.)\n"
        "monitor = %d\n\n"
        "; Windowed resolution\n"
        "window_width = %d\n"
        "window_height = %d\n\n"
        "; Fullscreen target resolution (0x0 = native desktop resolution)\n"
        "fullscreen_width = %d\n"
        "fullscreen_height = %d\n"
        "fullscreen_hz = %d\n\n"
        "; Graphics backend: auto, vulkan, opengl, d3d11, d3d12, software\n"
        "backend = %s\n\n"
        "; V-Sync: on, off\n"
        "vsync = %s\n\n"
        "; FPS Limiter: 30, 60, monitor (matches active monitor Hz), unlocked, or custom integer\n"
        "fps_limit = %s\n\n"
        "; Widescreen: auto, off, on, stretch\n"
        "widescreen = %s\n\n"
        "; Integer scaling: on, off\n"
        "integer_scale = %s\n\n"
        "; Pixel Aspect Ratio: square (1:1), crt_ntsc (1.092 original NTSC CRT)\n"
        "pixel_aspect_ratio = %s\n\n",
        config->display.window_mode == WINDOW_MODE_BORDERLESS ? "borderless" :
        config->display.window_mode == WINDOW_MODE_FULLSCREEN ? "fullscreen" :
        config->display.window_mode == WINDOW_MODE_MAXIMIZED ? "maximized" : "windowed",
        config->display.monitor,
        config->display.window_width, config->display.window_height,
        config->display.fullscreen_width, config->display.fullscreen_height, config->display.fullscreen_hz,
        config->display.backend == BACKEND_VULKAN ? "vulkan" :
        config->display.backend == BACKEND_OPENGL ? "opengl" :
        config->display.backend == BACKEND_D3D11 ? "d3d11" :
        config->display.backend == BACKEND_D3D12 ? "d3d12" :
        config->display.backend == BACKEND_SOFTWARE ? "software" : "auto",
        config->display.vsync ? "on" : "off",
        config->display.fps_limit == 0 ? "monitor" :
        config->display.fps_limit == -1 ? "unlocked" : "60",
        config->display.widescreen == WIDESCREEN_OFF ? "off" :
        config->display.widescreen == WIDESCREEN_ON ? "on" :
        config->display.widescreen == WIDESCREEN_STRETCH ? "stretch" : "auto",
        config->display.integer_scale ? "on" : "off",
        config->display.par == PAR_CRT_NTSC ? "crt_ntsc" : "square");

    fprintf(f,
        "; ==============================================================================\n"
        "; INDIVIDUAL SWITCHES (Leave as 'auto' to follow the master preset above)\n"
        "; ==============================================================================\n\n"
        "[PS1_Fixes]\n"
        "pgxp_vertex_precision = %s\n"
        "pgxp_perspective_texture = %s\n"
        "true_depth_buffer = %s\n"
        "near_plane_w_clipping = %s\n"
        "quad_subdivision = %s\n"
        "smooth_camera_angles = %s\n"
        "skeletal_precision = %s\n"
        "smooth_normals = %s\n"
        "true_color_32bit = %s\n"
        "dithering = %s\n"
        "texture_bleeding_fix = %s\n"
        "alpha_halo_fix = %s\n"
        "mipmapping = %s\n"
        "anti_aliasing = %s\n"
        "smooth_fog = %s\n"
        "oit_transparency = %s\n"
        "zero_fillrate_lag = %s\n"
        "hdr_tonemapping = %s\n"
        "atomic_clut = %s\n"
        "expand_voice_polyphony = %s\n"
        "seamless_cd_loop = %s\n"
        "audio_pop_suppression = %s\n"
        "vag_loop_crossfade = %s\n"
        "float_audio_mixing = %s\n"
        "hifi_reverb = %s\n"
        "fix_diagonal_running = %s\n"
        "hd_rumble = %s\n\n",
        tristate_str(config->fixes.pgxp_vertex_precision),
        tristate_str(config->fixes.pgxp_perspective_texture),
        tristate_str(config->fixes.true_depth_buffer),
        tristate_str(config->fixes.near_plane_w_clipping),
        tristate_str(config->fixes.quad_subdivision),
        tristate_str(config->fixes.smooth_camera_angles),
        tristate_str(config->fixes.skeletal_precision),
        tristate_str(config->fixes.smooth_normals),
        tristate_str(config->fixes.true_color_32bit),
        config->fixes.dithering == DITHER_OFF ? "off" :
        config->fixes.dithering == DITHER_NATIVE ? "native" :
        config->fixes.dithering == DITHER_DEBAND ? "deband" : "auto",
        tristate_str(config->fixes.texture_bleeding_fix),
        tristate_str(config->fixes.alpha_halo_fix),
        tristate_str(config->fixes.mipmapping),
        config->fixes.anti_aliasing == AA_OFF ? "off" :
        config->fixes.anti_aliasing == AA_MSAA_4X ? "msaa_4x" :
        config->fixes.anti_aliasing == AA_SMAA ? "smaa" : "auto",
        tristate_str(config->fixes.smooth_fog),
        tristate_str(config->fixes.oit_transparency),
        tristate_str(config->fixes.zero_fillrate_lag),
        tristate_str(config->fixes.hdr_tonemapping),
        tristate_str(config->fixes.atomic_clut),
        tristate_str(config->fixes.expand_voice_polyphony),
        tristate_str(config->fixes.seamless_cd_loop),
        tristate_str(config->fixes.audio_pop_suppression),
        tristate_str(config->fixes.vag_loop_crossfade),
        tristate_str(config->fixes.float_audio_mixing),
        tristate_str(config->fixes.hifi_reverb),
        tristate_str(config->fixes.fix_diagonal_running),
        tristate_str(config->fixes.hd_rumble));

    fprintf(f,
        "[Graphics_Enhancements]\n"
        "texture_filter = %s\n"
        "render_scale = %d\n"
        "hd_font = %s\n"
        "extended_draw_distance = %s\n\n",
        config->graphics.texture_filter == FILTER_BILINEAR ? "bilinear" :
        config->graphics.texture_filter == FILTER_XBRZ ? "xbrz" : "nearest",
        config->graphics.render_scale,
        tristate_str(config->graphics.hd_font),
        tristate_str(config->graphics.extended_draw_distance));

    fprintf(f,
        "[Audio_Enhancements]\n"
        "spu_interpolation = %s\n"
        "master_volume = %d\n"
        "mute_on_unfocus = %s\n\n",
        config->audio.spu_interpolation == SPU_INTERP_GAUSSIAN ? "gaussian" :
        config->audio.spu_interpolation == SPU_INTERP_LINEAR ? "linear" :
        config->audio.spu_interpolation == SPU_INTERP_CUBIC ? "cubic" : "auto",
        config->audio.master_volume,
        config->audio.mute_on_unfocus ? "on" : "off");

    fprintf(f,
        "[Input_QoL]\n"
        "analog_deadzone = %.2f\n"
        "pause_on_unfocus = %s\n"
        "pause_on_disconnect = %s\n"
        "turbo_hotkey = %s\n"
        "turbo_speed = %.1f\n",
        config->input.analog_deadzone,
        config->input.pause_on_unfocus ? "on" : "off",
        config->input.pause_on_disconnect ? "on" : "off",
        config->input.turbo_hotkey[0] ? config->input.turbo_hotkey : "none",
        config->input.turbo_speed);

    bool write_failed = ferror(f) != 0;
    if (fclose(f) != 0) write_failed = true;
    if (write_failed) {
        SDL_RemovePath(tmp_path);
        return false;
    }

    /* The temporary file is next to its destination, so replacement stays on
     * one volume. MoveFileEx replaces the old configuration without the
     * delete-then-rename window that could lose it on a failed rename. */
    if (!MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        SDL_RemovePath(tmp_path);
        return false;
    }
    return true;
}
