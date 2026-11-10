#ifndef VANILLABTN_CONFIG_H
#define VANILLABTN_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONFIG_FILENAME "vanillabtn.ini"

typedef enum PresetMode {
    PRESET_MODERN = 0,      /* Default: All 35 PS1 flaws fixed automatically */
    PRESET_AUTHENTIC_PS1,   /* 100% 1999 original PS1 console behavior */
    PRESET_CUSTOM           /* Explicit per-setting configuration */
} PresetMode;

typedef enum WindowMode {
    WINDOW_MODE_WINDOWED = 0,
    WINDOW_MODE_BORDERLESS,
    WINDOW_MODE_FULLSCREEN,
    WINDOW_MODE_MAXIMIZED
} WindowMode;

typedef enum WidescreenMode {
    WIDESCREEN_AUTO = 0,
    WIDESCREEN_OFF,
    WIDESCREEN_ON,
    WIDESCREEN_STRETCH
} WidescreenMode;

typedef enum BackendType {
    BACKEND_AUTO = 0,
    BACKEND_VULKAN,
    BACKEND_OPENGL,
    BACKEND_D3D11,
    BACKEND_D3D12,
    BACKEND_SOFTWARE
} BackendType;

typedef enum TriState {
    TRISTATE_AUTO = 0,
    TRISTATE_OFF = 1,
    TRISTATE_ON = 2
} TriState;

typedef enum DitherMode {
    DITHER_AUTO = 0,
    DITHER_OFF = 1,
    DITHER_NATIVE = 2,
    DITHER_DEBAND = 3
} DitherMode;

typedef enum TextureFilterMode {
    FILTER_NEAREST = 0,
    FILTER_BILINEAR = 1,
    FILTER_XBRZ = 2
} TextureFilterMode;

typedef enum SpuInterpolationMode {
    SPU_INTERP_AUTO = 0,
    SPU_INTERP_GAUSSIAN = 1,
    SPU_INTERP_LINEAR = 2,
    SPU_INTERP_CUBIC = 3
} SpuInterpolationMode;

typedef enum AntiAliasingMode {
    AA_AUTO = 0,
    AA_OFF = 1,
    AA_MSAA_4X = 2,
    AA_SMAA = 3
} AntiAliasingMode;

typedef enum PixelAspectRatio {
    PAR_SQUARE = 0,         /* 1:1 square pixel aspect ratio */
    PAR_CRT_NTSC = 1        /* 1.092 original NTSC CRT aspect ratio */
} PixelAspectRatio;

typedef struct DisplayConfig {
    WindowMode window_mode;
    int monitor;
    int window_width;
    int window_height;
    int fullscreen_width;
    int fullscreen_height;
    int fullscreen_hz;
    BackendType backend;
    bool vsync;
    int fps_limit;          /* 0 = monitor, -1 = unlocked, or explicit integer (e.g. 30, 60, 144) */
    WidescreenMode widescreen;
    bool integer_scale;
    PixelAspectRatio par;
} DisplayConfig;

typedef struct Ps1FixesConfig {
    TriState pgxp_vertex_precision;
    TriState pgxp_perspective_texture;
    TriState true_depth_buffer;
    TriState near_plane_w_clipping;
    TriState quad_subdivision;
    TriState smooth_camera_angles;
    TriState skeletal_precision;
    TriState smooth_normals;
    TriState true_color_32bit;
    DitherMode dithering;
    TriState texture_bleeding_fix;
    TriState alpha_halo_fix;
    TriState mipmapping;
    AntiAliasingMode anti_aliasing;
    TriState smooth_fog;
    TriState oit_transparency;
    TriState zero_fillrate_lag;
    TriState expand_voice_polyphony;
    TriState seamless_cd_loop;
    TriState audio_pop_suppression;
    TriState vag_loop_crossfade;
    TriState float_audio_mixing;
    TriState hifi_reverb;
    TriState fix_diagonal_running;
    TriState hd_rumble;
} Ps1FixesConfig;

typedef struct GraphicsConfig {
    TextureFilterMode texture_filter;
    int render_scale;       /* 1, 2, 4, 8, or 0 for native display resolution */
    TriState hd_font;
    TriState extended_draw_distance;
} GraphicsConfig;

typedef struct AudioConfig {
    SpuInterpolationMode spu_interpolation;
    int master_volume;      /* 0 - 100 */
    bool mute_on_unfocus;
} AudioConfig;

typedef struct InputConfig {
    float analog_deadzone;  /* 0.00 - 0.50 */
    bool pause_on_unfocus;
    bool pause_on_disconnect;
    char turbo_hotkey[16];  /* e.g. "Tab", "R2", "none" */
    float turbo_speed;      /* e.g. 3.0 */
} InputConfig;

typedef struct AppConfig {
    PresetMode preset;
    DisplayConfig display;
    Ps1FixesConfig fixes;
    GraphicsConfig graphics;
    AudioConfig audio;
    InputConfig input;
} AppConfig;

/* Initialize config with recommended modern defaults. */
void config_set_defaults(AppConfig *config);

/* Resolve a TriState boolean value based on the master preset mode. */
bool config_resolve_bool(TriState state, PresetMode preset, bool default_for_modern);

/* Resolve dithering mode based on preset. */
DitherMode config_resolve_dither(DitherMode mode, PresetMode preset);

/* Resolve SPU interpolation mode based on preset. */
SpuInterpolationMode config_resolve_spu_interp(SpuInterpolationMode mode, PresetMode preset);

/* Resolve anti-aliasing mode based on preset. */
AntiAliasingMode config_resolve_anti_aliasing(AntiAliasingMode mode, PresetMode preset);

/* Load configuration from an INI file. Generates a default file if none exists. */
bool config_load(AppConfig *config, const char *custom_path);

/* Save configuration to an INI file. */
bool config_save(const AppConfig *config, const char *path);

/* Resolve path to vanillabtn.ini (executable directory with pref path fallback). */
bool config_resolve_path(char *out_path, size_t out_size, const char *custom_path);

#endif /* VANILLABTN_CONFIG_H */
