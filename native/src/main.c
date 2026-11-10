#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string.h>
#ifdef VANILLABTN_BOOT_TEST
#include <stdio.h>
#include "farm_state.h"
#include "main.h"
extern int main_scene;
#endif

#include "ps1_audio.h"
#include "ps1_card.h"
#include "ps1_input.h"
#include "ps1_video.h"
#include "ps1_pgxp.h"
#include "ps1_gte.h"
#include "config.h"
#include "sdk_gpu.h"
#include "host_display.h"
#include "host_pacing.h"
#include "sdk_pad.h"
#include "sdk_card.h"
#include "sdk_cd.h"
#include "sdk_memory.h"
#include "sdk_sound.h"
#include "sdk_sync.h"
#include "host_gamepad.h"
#include <libetc.h>
#include <libcd.h>
#include <libpad.h>

_Static_assert(sizeof(void *) == 8, "The Windows port requires x64");

static Ps1Video video;
static Ps1Audio audio;
#ifndef VANILLABTN_BOOT_TEST
static Ps1CardManager card_mgr;
#endif

typedef struct HostGame {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Gamepad *gamepads[2];
    bool running;
    bool paused;
    AppConfig config;
    HostPacing pacing;
    uint32_t keyboard_buttons, controller_buttons[2];
    uint16_t keyboard_keys[SDL_SCANCODE_COUNT];
    int16_t stick_x[2], stick_y[2];
    int16_t trigger_left[2], trigger_right[2];
#ifdef VANILLABTN_BOOT_TEST
    unsigned vblanks, vblank_limit, colored_frames, audible_frames;
    bool cycle_required, visited_farm, returned_title, new_game_required;
#endif
} HostGame;

static void send_rumble(unsigned slot, uint8_t small, uint8_t large, void *context) {
    HostGame *host = (HostGame *)context;
    if (slot < 2 && host && host->gamepads[slot] != NULL) {
        bool hd = config_resolve_bool(host->config.fixes.hd_rumble, host->config.preset, true);
        if (hd) {
            /* HD Rumble: graded tactile curve rather than 100% full-blast small motor buzzing */
            Uint16 small_freq = small ? 26000 : 0;
            float l_norm = (float)large / 255.0f;
            Uint16 large_freq = (Uint16)(powf(l_norm, 1.4f) * 65535.0f);
            SDL_RumbleGamepad(host->gamepads[slot], large_freq, small_freq, 100);
        } else {
            /* Authentic PS1: binary 1-bit small motor (full 65535 blast) and linear large */
            SDL_RumbleGamepad(host->gamepads[slot], (Uint16)(large * 257u), small ? 65535 : 0, 100);
        }
    }
}

static uint32_t map_key_to_ps1_button(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP:
    case SDLK_W:
        return PS1_PAD_UP;
    case SDLK_DOWN:
    case SDLK_S:
        return PS1_PAD_DOWN;
    case SDLK_LEFT:
    case SDLK_A:
        return PS1_PAD_LEFT;
    case SDLK_RIGHT:
    case SDLK_D:
        return PS1_PAD_RIGHT;
    case SDLK_X:
    case SDLK_K:
    case SDLK_RETURN:
        return PS1_PAD_CROSS;
    case SDLK_Z:
    case SDLK_J:
        return PS1_PAD_CIRCLE;
    case SDLK_C:
    case SDLK_U:
        return PS1_PAD_SQUARE;
    case SDLK_V:
    case SDLK_I:
        return PS1_PAD_TRIANGLE;
    case SDLK_ESCAPE:
    case SDLK_TAB:
        return PS1_PAD_SELECT;
    case SDLK_SPACE:
        return PS1_PAD_START;
    case SDLK_Q:
        return PS1_PAD_L1;
    case SDLK_E:
        return PS1_PAD_R1;
    case SDLK_1:
        return PS1_PAD_L2;
    case SDLK_2:
        return PS1_PAD_R2;
    default:
        return 0;
    }
}



#ifdef VANILLABTN_BOOT_TEST
static bool boot_save_frame(void) {
    SdkDisplay output = sdk_gpu_display();
    if (!output.enabled || output.width <= 0 || output.height <= 0 ||
        output.width > PS1_VRAM_WIDTH || output.height > PS1_VRAM_HEIGHT) return false;
    size_t count = (size_t)output.width * output.height;
    uint32_t *pixels = SDL_malloc(count * sizeof(*pixels));
    if (!pixels) return false;
    SDL_Surface *surface = NULL;
    if (ps1_video_copy_argb(&video, output.x, output.y, output.width,
        output.height, pixels, output.width)) {
        surface = SDL_CreateSurfaceFrom(output.width, output.height,
            SDL_PIXELFORMAT_ARGB8888, pixels, output.width * sizeof(*pixels));
    }
    bool saved = surface && SDL_SaveBMP(surface, "build/native-demo.bmp");
    if (surface) SDL_DestroySurface(surface);
    SDL_free(pixels);
    return saved;
}
#endif

static void host_subtick_pump(void *context) {
    HostGame *host = context;
    if (!host || !host->running || host->paused) return;
    uint64_t now = SDL_GetTicksNS();
    if (host_pacing_should_present(&host->pacing, now)) {
        bool minimized = host->window != NULL &&
            (SDL_GetWindowFlags(host->window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
        if (!minimized) {
            host_present_frame(&video, host->renderer, &host->texture);
        }
        host_pacing_record_presentation(&host->pacing, now);
    }
}

static void host_game_pump(void *context) {
    HostGame *host = context;
    SDL_Event event;
#ifdef VANILLABTN_BOOT_TEST
    if (main_scene == SCENE_FARM && (game_state.globalFlags & GLOBAL_FLAG_INPUT_REPLAY))
        host->visited_farm = true;
    if (host->visited_farm && main_scene == SCENE_TITLE) host->returned_title = true;
    if (host->new_game_required && main_scene == SCENE_FARM
        && !(game_state.globalFlags & GLOBAL_FLAG_INPUT_REPLAY)) host->visited_farm = true;
    if (host->new_game_required && host->vblanks % 1000 == 0)
        fprintf(stderr, "probe: vblank=%u scene=%d entry=%u\n", host->vblanks,
            main_scene, game_state.progress.entryStep);
    if (++host->vblanks >= host->vblank_limit || !sdk_gpu_ok()) {
        if (host->vblank_limit > 900 && host->vblanks == host->vblank_limit)
            fprintf(stderr, "frame snapshot: %d\n", boot_save_frame());
        sdk_sync_stop();
        host->running = false;
        return;
    }
#endif
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            host->running = false;
        } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            host->paused = false;
            int slot = -1;
            if (host->gamepads[0] == NULL) {
                slot = 0;
            } else if (host->gamepads[1] == NULL) {
                slot = 1;
            }
            if (slot >= 0) {
                host->gamepads[slot] = SDL_OpenGamepad(event.gdevice.which);
                if (host->gamepads[slot] != NULL) {
                    HostPadSnapshot state = host_gamepad_snapshot(host->gamepads[slot]);
                    host->controller_buttons[slot] = state.buttons;
                    host->stick_x[slot] = state.x;
                    host->stick_y[slot] = state.y;
                    host->trigger_left[slot] = state.left_trigger;
                    host->trigger_right[slot] = state.right_trigger;
                }
            }
        } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            for (int i = 0; i < 2; ++i) {
                if (host->gamepads[i] != NULL && SDL_GetGamepadID(host->gamepads[i]) == event.gdevice.which) {
                    SDL_CloseGamepad(host->gamepads[i]);
                    host->gamepads[i] = NULL;
                    host->controller_buttons[i] = 0;
                    host->stick_x[i] = host->stick_y[i] = 0;
                    host->trigger_left[i] = host->trigger_right[i] = 0;
                    if (i == 0 && host->config.input.pause_on_disconnect) {
                        host->paused = true;
                    }
                    break;
                }
            }
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            host->keyboard_buttons = 0;
            memset(host->keyboard_keys, 0, sizeof(host->keyboard_keys));
            for (int i = 0; i < 2; ++i) {
                host->controller_buttons[i] = 0;
                host->stick_x[i] = host->stick_y[i] = 0;
                host->trigger_left[i] = host->trigger_right[i] = 0;
            }
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
            /* Input events need not repeat for controls held while unfocused. */
            int numkeys = 0;
            const bool *keys = SDL_GetKeyboardState(&numkeys);
            if (keys) {
                host->keyboard_buttons = 0;
                for (int sc = 1; sc < numkeys && sc < SDL_SCANCODE_COUNT; ++sc) {
                    if (keys[sc]) {
                        SDL_Keycode kc = SDL_GetKeyFromScancode((SDL_Scancode)sc, SDL_KMOD_NONE, false);
                        uint32_t btn = map_key_to_ps1_button(kc);
                        host->keyboard_buttons = ps1_input_keyboard_event(
                            host->keyboard_keys, SDL_SCANCODE_COUNT, (size_t)sc,
                            (uint16_t)btn, true);
                    }
                }
            }
            for (int i = 0; i < 2; ++i) {
                HostPadSnapshot state = host_gamepad_snapshot(host->gamepads[i]);
                host->controller_buttons[i] = state.buttons;
                host->stick_x[i] = state.x;
                host->stick_y[i] = state.y;
                host->trigger_left[i] = state.left_trigger;
                host->trigger_right[i] = state.right_trigger;
            }
        } else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (((event.key.mod & SDL_KMOD_ALT) && (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER))
                    || event.key.key == SDLK_F11) {
                    host_display_toggle_fullscreen(host->window, &host->config.display);
                    continue;
                }
            }
            uint32_t btn = map_key_to_ps1_button(event.key.key);
            if (event.key.scancode > SDL_SCANCODE_UNKNOWN &&
                event.key.scancode < SDL_SCANCODE_COUNT) {
                host->keyboard_buttons = ps1_input_keyboard_event(
                    host->keyboard_keys, SDL_SCANCODE_COUNT, event.key.scancode,
                    (uint16_t)btn, event.type == SDL_EVENT_KEY_DOWN);
            }
        } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                   || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
            int pad_idx = -1;
            for (int i = 0; i < 2; ++i) {
                if (host->gamepads[i] != NULL && SDL_GetGamepadID(host->gamepads[i]) == event.gbutton.which) {
                    pad_idx = i;
                    break;
                }
            }
            if (pad_idx >= 0) {
                uint32_t btn = host_gamepad_button((SDL_GamepadButton)event.gbutton.button);
                if (btn != 0) {
                    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) host->controller_buttons[pad_idx] |= btn;
                    else host->controller_buttons[pad_idx] &= ~btn;
                }
            }
        } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
            int pad_idx = -1;
            for (int i = 0; i < 2; ++i) {
                if (host->gamepads[i] != NULL && SDL_GetGamepadID(host->gamepads[i]) == event.gaxis.which) {
                    pad_idx = i;
                    break;
                }
            }
            if (pad_idx >= 0) {
                if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) host->stick_x[pad_idx] = event.gaxis.value;
                if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) host->stick_y[pad_idx] = event.gaxis.value;
                if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) host->trigger_left[pad_idx] = event.gaxis.value;
                if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) host->trigger_right[pad_idx] = event.gaxis.value;
            }
        } else if (event.type == SDL_EVENT_WINDOW_EXPOSED
                   || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            bool minimized = host->window != NULL &&
                (SDL_GetWindowFlags(host->window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
            if (!minimized) {
                host_present_frame(&video, host->renderer, &host->texture);
            }
        }
    }

    if (!host->running) { sdk_sync_stop(); return; }

    if (host->paused) {
        bool minimized = host->window != NULL &&
            (SDL_GetWindowFlags(host->window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
        if (!minimized) {
            host_present_frame(&video, host->renderer, &host->texture);
        }
        return;
    }

    /* Supply raw samples only: original pad.c owns the analog/repeat rules
     * in the original game. Do not feed folded stick bits. */
#ifdef VANILLABTN_BOOT_TEST
    if (host->new_game_required) {
        unsigned phase = host->vblanks % 120;
        host->keyboard_buttons = phase < 2 || (phase >= 30 && phase < 32) ? PS1_PAD_UP :
            phase >= 24 && phase < 26 ? PS1_PAD_START :
            phase % 12 < 2 ? PS1_PAD_CROSS : 0;
    }
#endif

    bool fix_diag = config_resolve_bool(host->config.fixes.fix_diagonal_running, host->config.preset, true);
    float deadzone = host->config.input.analog_deadzone;
    uint8_t stick0_x, stick0_y, stick1_x, stick1_y;

    ps1_input_normalize_stick(host->stick_x[0], host->stick_y[0], deadzone, fix_diag, &stick0_x, &stick0_y);
    ps1_input_normalize_stick(host->stick_x[1], host->stick_y[1], deadzone, fix_diag, &stick1_x, &stick1_y);

    sdk_pad_sample(0, true, host->gamepads[0] != NULL,
        (uint16_t)(host->keyboard_buttons | host->controller_buttons[0] |
            ps1_input_trigger_buttons(host->trigger_left[0], host->trigger_right[0])),
        stick0_x, stick0_y);

    sdk_pad_sample(1, host->gamepads[1] != NULL, host->gamepads[1] != NULL,
        (uint16_t)(host->controller_buttons[1] |
            ps1_input_trigger_buttons(host->trigger_left[1], host->trigger_right[1])),
        stick1_x, stick1_y);

    /* Original game timing owns the cadence. Submit exactly one 60 Hz audio
     * interval from the same SDK SPU used by sound.c, not the smoke mixer. */
    int16_t samples[735 * 2];
    sdk_sound_mix(samples, 735);
#ifdef VANILLABTN_BOOT_TEST
    for (unsigned i = 0; i < 735 * 2; ++i) {
        if (samples[i]) { ++host->audible_frames; break; }
    }
    SdkDisplay output = sdk_gpu_display();
    bool colored = false;
    if (output.enabled && output.x >= 0 && output.y >= 0 &&
        output.width <= PS1_VRAM_WIDTH - output.x &&
        output.height <= PS1_VRAM_HEIGHT - output.y) {
        for (int y = 0; y < output.height && !colored; ++y) {
            for (int x = 0; x < output.width; ++x) {
                unsigned pixel = video.vram[output.y + y][output.x + x] & 0x7FFF;
                if (pixel && pixel != 0x7FFF) { colored = true; break; }
            }
        }
    }
    if (colored) ++host->colored_frames;
#endif
    int queued = audio.stream ? SDL_GetAudioStreamQueued(audio.stream) : -1;
    if (audio.stream && queued >= 0 &&
        queued < (int)(sizeof(samples) * 4)) {
        SDL_PutAudioStreamData(audio.stream, samples, sizeof(samples));
    }
    host_pacing_record_tick(&host->pacing);
    uint64_t now = SDL_GetTicksNS();
    if (host_pacing_should_present(&host->pacing, now)) {
        bool minimized = host->window != NULL &&
            (SDL_GetWindowFlags(host->window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
        if (!minimized) {
            host_present_frame(&video, host->renderer, &host->texture);
        }
        host_pacing_record_presentation(&host->pacing, now);
    }
}

#ifdef VANILLABTN_BOOT_TEST
void boot_fault_install(void (*hook)(void));
static void boot_fault_state(void) {
    fprintf(stderr, "state: entry=%u frame=%u\n",
        game_state.progress.entryStep, game_state.frameCount);
}

static uint64_t boot_time;
static uint64_t boot_now(void *context) { (void)context; return boot_time; }
static void boot_wait(uint64_t deadline, void *context) {
    (void)context;
    boot_time = deadline;
}
#endif

int game_main(void);

int main(int argc, char **argv) {
    HostGame host = {0};
    int result = 1;
    const char *disc = "build/game-data/disc";
    const char *card_directory = NULL;
    const char *config_path = NULL;
#ifdef VANILLABTN_BOOT_TEST
    (void)card_directory;
    boot_fault_install(boot_fault_state);
    host.vblank_limit = 900;
#endif
    if (argc == 2 && strcmp(argv[1], "--check-build") == 0) return 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) disc = argv[++i];
        else if (strcmp(argv[i], "--card-dir") == 0 && i + 1 < argc) card_directory = argv[++i];
        else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) config_path = argv[++i];
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "VanillaBTN",
                "VanillaBTN - Native Windows Port\n\n"
                "Usage:\n"
                "  VanillaBTN [options]\n\n"
                "Options:\n"
                "  --data <path>      Path to disc directory containing A_FILE.BIN and A_FILE.HDT\n"
                "  --card-dir <path>  Path to directory containing card1.mcr and card2.mcr\n"
                "  --config <path>    Path to custom vanillabtn.ini configuration file\n"
                "  --help, -h         Show this help information", NULL);
            return 0;
        }
#ifdef VANILLABTN_BOOT_TEST
        else if (strcmp(argv[i], "--demo") == 0) host.vblank_limit = 5000;
        else if (strcmp(argv[i], "--new-game") == 0) {
            host.vblank_limit = 18000;
            host.new_game_required = true;
        }
        else if (strcmp(argv[i], "--cycle") == 0) {
            host.vblank_limit = 10000;
            host.cycle_required = true;
        }
#endif
        else return 1;
    }
    config_load(&host.config, config_path);
    bool pgxp_v = config_resolve_bool(host.config.fixes.pgxp_vertex_precision, host.config.preset, true);
    bool pgxp_t = config_resolve_bool(host.config.fixes.pgxp_perspective_texture, host.config.preset, true);
    bool zbuff = config_resolve_bool(host.config.fixes.true_depth_buffer, host.config.preset, true);
    bool w_clip = config_resolve_bool(host.config.fixes.near_plane_w_clipping, host.config.preset, true);
    bool quad_sub = config_resolve_bool(host.config.fixes.quad_subdivision, host.config.preset, true);
    bool float_gte = config_resolve_bool(host.config.fixes.smooth_camera_angles, host.config.preset, true);
    bool true_color = config_resolve_bool(host.config.fixes.true_color_32bit, host.config.preset, true);
    DitherMode dither_cfg = config_resolve_dither(host.config.fixes.dithering, host.config.preset);
    Ps1DitherMode dither_mode = (dither_cfg == DITHER_NATIVE) ? PS1_DITHER_NATIVE :
                                (dither_cfg == DITHER_DEBAND) ? PS1_DITHER_DEBAND : PS1_DITHER_OFF;
    bool tex_bleed = config_resolve_bool(host.config.fixes.texture_bleeding_fix, host.config.preset, true);
    bool alpha_halo = config_resolve_bool(host.config.fixes.alpha_halo_fix, host.config.preset, true);
    bool mipmap = config_resolve_bool(host.config.fixes.mipmapping, host.config.preset, true);
    Ps1TextureFilterMode filter_mode = (host.config.graphics.texture_filter == FILTER_BILINEAR) ? PS1_FILTER_BILINEAR : PS1_FILTER_NEAREST;
    if (host.config.preset == PRESET_AUTHENTIC_PS1) {
        filter_mode = PS1_FILTER_NEAREST;
    }
    AntiAliasingMode aa_cfg = config_resolve_anti_aliasing(host.config.fixes.anti_aliasing, host.config.preset);
    Ps1AntiAliasingMode aa_mode = (aa_cfg == AA_OFF) ? PS1_AA_OFF :
                                  (aa_cfg == AA_SMAA) ? PS1_AA_SMAA : PS1_AA_MSAA_4X;
    bool smooth_fog = config_resolve_bool(host.config.fixes.smooth_fog, host.config.preset, true);
    bool oit_trans = config_resolve_bool(host.config.fixes.oit_transparency, host.config.preset, true);
    bool zero_lag = config_resolve_bool(host.config.fixes.zero_fillrate_lag, host.config.preset, true);
    bool hdr_tone = config_resolve_bool(host.config.fixes.hdr_tonemapping, host.config.preset, true);
    bool atomic_clut = config_resolve_bool(host.config.fixes.atomic_clut, host.config.preset, true);
    bool expand_poly = config_resolve_bool(host.config.fixes.expand_voice_polyphony, host.config.preset, true);
    bool seamless_cd = config_resolve_bool(host.config.fixes.seamless_cd_loop, host.config.preset, true);
    bool pop_suppress = config_resolve_bool(host.config.fixes.audio_pop_suppression, host.config.preset, true);
    bool vag_cross = config_resolve_bool(host.config.fixes.vag_loop_crossfade, host.config.preset, true);
    bool float_mix = config_resolve_bool(host.config.fixes.float_audio_mixing, host.config.preset, true);
    bool hifi_rev = config_resolve_bool(host.config.fixes.hifi_reverb, host.config.preset, true);

    ps1_pgxp_set_enabled(pgxp_v, pgxp_t);
    ps1_video_set_true_depth_buffer(zbuff);
    ps1_video_set_near_plane_clipping(w_clip);
    ps1_video_set_quad_subdivision(quad_sub);
    ps1_gte_set_float_math(float_gte);
    ps1_video_set_true_color(true_color);
    ps1_video_set_dithering(dither_mode);
    ps1_video_set_texture_bleeding_fix(tex_bleed);
    ps1_video_set_alpha_halo_fix(alpha_halo);
    ps1_video_set_mipmapping(mipmap);
    host_display_set_widescreen_mode(host.config.display.widescreen);
    ps1_video_set_texture_filter(filter_mode);
    ps1_video_set_anti_aliasing(aa_mode);
    ps1_video_set_smooth_fog(smooth_fog);
    ps1_video_set_oit_transparency(oit_trans);
    ps1_video_set_zero_fillrate_lag(zero_lag);
    ps1_video_set_hdr_tonemapping(hdr_tone);
    ps1_video_set_atomic_clut(atomic_clut);

    sdk_sound_set_polyphony_expansion(expand_poly);
    sdk_sound_set_pop_suppression(pop_suppress);
    sdk_sound_set_vag_crossfade(vag_cross);
    sdk_sound_set_float_mixing(float_mix);
    sdk_cd_set_seamless_loop(seamless_cd);
    sdk_sound_set_hifi_reverb(hifi_rev);
    sdk_sound_set_equal_power_panning(hifi_rev);
#ifdef VANILLABTN_BOOT_TEST
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) return 1;
    CdlFILE required;
    if (!sdk_cd_mount(disc) ||
        !CdSearchFile(&required, "\\A_FILE.HDT;1") ||
        !CdSearchFile(&required, "\\A_FILE.BIN;1")) {
        if (strcmp(disc, "build/game-data/disc") == 0) {
            if (sdk_cd_mount("disc") &&
                CdSearchFile(&required, "\\A_FILE.HDT;1") &&
                CdSearchFile(&required, "\\A_FILE.BIN;1")) {
                disc = "disc";
                goto disc_ready;
            }
            if (sdk_cd_mount("data") &&
                CdSearchFile(&required, "\\A_FILE.HDT;1") &&
                CdSearchFile(&required, "\\A_FILE.BIN;1")) {
                disc = "data";
                goto disc_ready;
            }
        }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN",
            "Game data was not found. Import your disc and use --data with its disc directory, or place A_FILE.HDT and A_FILE.BIN in a 'disc' or 'data' folder.", NULL);
        goto done;
    }
disc_ready:
    host.window = host_display_create_window(&host.config.display);
    if (!host.window) goto done;
    host.renderer = SDL_CreateRenderer(host.window, NULL);
    if (!host.renderer || !SDL_SetRenderLogicalPresentation(host.renderer,
        320, 240, SDL_LOGICAL_PRESENTATION_LETTERBOX)) goto done;
    sdk_memory_clear();
    sdk_gpu_bind(&video);
    if (!sdk_gpu_map(sdk_main_ram, SDK_RAM_SIZE)) goto done;
    host.texture = SDL_CreateTexture(host.renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 320, 240);
    if (!host.texture || !SDL_SetTextureScaleMode(host.texture, SDL_SCALEMODE_NEAREST)) goto done;

    /* Query monitor refresh rate and initialize decoupled presentation pacing */
    {
        float monitor_hz = 60.0f;
        SDL_DisplayID disp_id = host_display_get_monitor_id(host.config.display.monitor);
        const SDL_DisplayMode *cur_mode = SDL_GetCurrentDisplayMode(disp_id);
        if (cur_mode && cur_mode->refresh_rate >= 20.0f) {
            monitor_hz = cur_mode->refresh_rate;
        }
        host_pacing_init(&host.pacing, &host.config.display, monitor_hz);
        if (host.pacing.target_fps > 60 || host.pacing.target_fps == 0) {
            sdk_sync_subtick_hook(host_subtick_pump, &host);
        } else {
            sdk_sync_subtick_hook(NULL, NULL);
        }
        SDL_SetRenderVSync(host.renderer, host.config.display.vsync ? 1 : 0);
    }
    ps1_audio_init(&audio);
    ps1_spu_set_polyphony_expansion(&audio.spu, expand_poly);
    ps1_spu_set_pop_suppression(&audio.spu, pop_suppress);
    ps1_spu_set_vag_crossfade(&audio.spu, vag_cross);
    ps1_spu_set_float_mixing(&audio.spu, float_mix);
    ps1_xa_set_seamless_loop(&audio.xa, seamless_cd);
    ps1_reverb_set_hifi(&audio.spu.reverb, hifi_rev);
    ps1_seq_set_equal_power_panning(&audio.sequence, hifi_rev);
#ifndef VANILLABTN_BOOT_TEST
    if (card_directory) {
        SDL_PathInfo info;
        if (!*card_directory || strlen(card_directory) >= sizeof(card_mgr.base_dir)
            || !SDL_GetPathInfo(card_directory, &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN",
                "Choose an existing folder for --card-dir.", host.window);
            goto done;
        }
    }
    ps1_card_manager_init(&card_mgr, card_directory);
    sdk_card_bind_manager(&card_mgr);
#else
    /* This probe must never read, recover, create or flush user save files. */
    sdk_card_bind_manager(NULL);
#endif
    sdk_pad_rumble(send_rumble, &host);
    host.running = true;
    sdk_sync_pump(host_game_pump, &host);
#ifdef VANILLABTN_BOOT_TEST
    sdk_sync_bind(boot_now, boot_wait, NULL);
#endif
    sdk_sync_start();
    result = game_main();
#ifdef VANILLABTN_BOOT_TEST
    fprintf(stderr, "boot: vblanks=%u scene=%d gpu=%d colored=%u audible=%u\n",
        host.vblanks, main_scene, sdk_gpu_ok(), host.colored_frames, host.audible_frames);
    if (host.new_game_required) {
        fprintf(stderr, "new game: farm=%d entry=%u frame=%u\n", host.visited_farm,
            game_state.progress.entryStep, game_state.frameCount);
        if (!host.visited_farm) result = 2;
    }
    if (host.cycle_required) {
        fprintf(stderr, "cycle: farm=%d returned_title=%d\n",
            host.visited_farm, host.returned_title);
        if (!host.visited_farm || !host.returned_title) result = 2;
    }
    if (host.vblanks < host.vblank_limit || !sdk_gpu_ok() || !host.colored_frames) result = 2;
#endif

done:
    sdk_sync_subtick_hook(NULL, NULL);
    sdk_sync_stop();
    sdk_sync_pump(NULL, NULL);
    VSyncCallback(NULL);
    PadStopCom();
    sdk_pad_rumble(NULL, NULL);
    sdk_card_bind_manager(NULL);
    sdk_cd_unmount();
    sdk_gpu_bind(NULL);
    for (int i = 0; i < 2; ++i) {
        if (host.gamepads[i]) SDL_CloseGamepad(host.gamepads[i]);
    }
#ifndef VANILLABTN_BOOT_TEST
    ps1_card_manager_shutdown(&card_mgr);
#endif
    ps1_audio_shutdown(&audio);
    if (host.texture) SDL_DestroyTexture(host.texture);
    if (host.renderer) SDL_DestroyRenderer(host.renderer);
    if (host.window) SDL_DestroyWindow(host.window);
    SDL_Quit();
    return result;
}
