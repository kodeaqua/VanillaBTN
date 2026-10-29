#include "farm_state.h"
#include "platform/gpu_present.h"
#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#include "gfx/game_frame.h"
#include "gfx/software_output.h"
#endif
#ifdef HMBTN_ENH_FILTERING
#include "gfx/game_frame.h"
#include "gfx/software_output.h"
#endif
#ifdef HMBTN_GPU_RASTER
#include "gfx/frame_replay.h"
#include "gfx/game_frame.h"
#include "platform/gpu_renderer.h"
#endif
#ifdef HMBTN_ENH_SCALING
#include "enhance/scaling/scaling.h"
#include "gfx/game_frame.h"
#endif
#ifdef HMBTN_ENH_INTERPOLATION
#include "enhance/interpolation/interpolation.h"
#include "gfx/game_frame.h"
#endif
#include "platform/audio_output.h"
#include "platform/game_host.h"
#include "platform/input_replay.h"
#include "platform/presentation_clock.h"
#include "platform/user_data.h"
#include "platform/version.h"
#include "platform/window_icon.h"
#include "platform/mouse_input.h"
#ifdef HMBTN_ENH_PAUSE_MENU
#include "enhance/pause_menu/assets.h"
#include "enhance/pause_menu/canvas.h"
#include "enhance/pause_menu/menu.h"
#include "platform/graphics_options.h"
#ifdef HMBTN_ENH_PGXP
#include "enhance/pgxp/pgxp.h"
#ifdef HMBTN_ENH_DITHERING
#include "enhance/dithering/dithering.h"
#endif
#endif
#include "platform/controls_options.h"
#include "platform/audio_options.h"
#ifdef HMBTN_ENH_GAMEPLAY
#include "platform/gameplay_options.h"
#endif
#ifdef HMBTN_ENH_ANTIALIASING
#include "platform/fxaa.h"
#define HM_HOST_AA 1
#endif
#endif
#include "psx/gpu_environment.h"
#include "psx/pad.h"
#include "psx/spu_voice.h"
#include "psx/timing.h"
#include "psx/vram.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Window *hm_window;
static SDL_GPUDevice *hm_device;
static SDL_GPUTexture *hm_texture;
static SDL_GPUTransferBuffer *hm_upload;
static u32 hm_image_width, hm_image_height, hm_upload_bytes;
static Uint64 hm_start;
static hm_input_replay hm_replay;
static int hm_replay_enabled;
static u64 hm_host_tick_limit;
static hm_controls_options hm_controls;
static hm_audio_options hm_audio;
static SDL_Gamepad *hm_active_gamepad = NULL;
static int hm_mouse_wheel_up = 0;
static int hm_mouse_wheel_down = 0;
static int hm_trace_camera;
static hm_present_clock hm_presentation;
#ifdef HMBTN_ENH_PAUSE_MENU
static hm_pause_assets hm_assets;
static hm_graphics_options hm_options;
#ifdef HMBTN_ENH_GAMEPLAY
static hm_gameplay_options hm_gameplay;
#endif
#ifdef HM_HOST_AA
static hm_fxaa *hm_fxaa_output;
#endif
static int hm_ignore_held;
static int hm_pause_enabled;
static int hm_host_in_pause;
static u8 hm_pause_background[320 * 240 * 4];
static u8 hm_pause_frozen[1024 * 240 * 4];
#endif
#ifdef HMBTN_GPU_RASTER
static hm_gpu_renderer *hm_gpu_output;
static hm_gpu_scene *hm_gpu_commands;
static hm_frame_replay *hm_gpu_replay;
static hm_gpu_scene_data hm_gpu_cached;
static u64 hm_gpu_cached_serial;
static int hm_gpu_fallback_reported;
#endif

#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
static hm_software_output *hm_native_output;
#endif
enum { HM_SCAN_WIDTH = 1024, HM_SCAN_HEIGHT = 512 };

static void hm_host_fail(void) {
    fprintf(stderr, "Native window: %s\n", SDL_GetError());
    hm_game_host_stop();
    exit(1);
}

/* Texture bounds match the image so linear sampling cannot bleed from a stale
 * border. Transfer capacity only grows; SDL defers released queued resources. */
static int hm_host_image_size(u32 width, u32 height) {
    if (hm_texture && width == hm_image_width && height == hm_image_height)
        return 1;
    if (!width || !height || width > 4096 || height > 4096)
        return 0;
    SDL_GPUTextureCreateInfo texture = {.type = SDL_GPU_TEXTURETYPE_2D,
                                        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                        .width = width,
                                        .height = height,
                                        .layer_count_or_depth = 1,
                                        .num_levels = 1};
    SDL_GPUTexture *image = SDL_CreateGPUTexture(hm_device, &texture);
    if (!image)
        return 0;
    u32 bytes = width * height * 4;
    SDL_GPUTransferBuffer *upload = hm_upload;
    if (!upload || bytes > hm_upload_bytes) {
        SDL_GPUTransferBufferCreateInfo transfer = {.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                    .size = bytes};
        upload = SDL_CreateGPUTransferBuffer(hm_device, &transfer);
        if (!upload) {
            SDL_ReleaseGPUTexture(hm_device, image);
            return 0;
        }
    }
    if (hm_texture)
        SDL_ReleaseGPUTexture(hm_device, hm_texture);
    if (hm_upload && upload != hm_upload)
        SDL_ReleaseGPUTransferBuffer(hm_device, hm_upload);
    hm_texture = image;
    hm_upload = upload;
    hm_image_width = width;
    hm_image_height = height;
    if (bytes > hm_upload_bytes)
        hm_upload_bytes = bytes;
    return 1;
}
#ifdef HMBTN_ENH_SCALING
static void hm_host_scaling_unavailable(void) {
#ifdef HMBTN_ENH_PAUSE_MENU
    hm_options.scaling = 0;
#endif
    hm_scaling_enable(0);
    Uint64 before = SDL_GetTicksNS();
    hm_audio_output_pause(1);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Back to Nature",
                             "Enhance Scaling could not allocate the render size (maximum 4096 per "
                             "axis). Native rendering remains active.",
                             hm_window);
    hm_start += SDL_GetTicksNS() - before;
    hm_audio_output_pause(0);
}
#ifdef HMBTN_ENH_PAUSE_MENU
static void hm_host_output_size(int *w, int *h);
static int hm_host_scaling_prepare(void) {
    int w, h;
    if (!SDL_GetWindowSizeInPixels(hm_window, &w, &h) || w <= 0 || h <= 0)
        return 0;
    hm_host_output_size(&w, &h);
#ifdef HMBTN_ENH_FILTERING
    hm_scaling_filter(hm_options.linear);
#endif
    if (hm_scaling_prepare((u32)w, (u32)h))
        return 1;
    hm_host_scaling_unavailable();
    return 0;
}
#endif
#endif

static hm_present_rate hm_host_present_rate(void) {
#ifdef HMBTN_ENH_PAUSE_MENU
    return hm_graphics_effective_rate(hm_window, &hm_options);
#else
    return (hm_present_rate){60, 1};
#endif
}
static void hm_host_reset_presentation(void) {
    if (!hm_present_clock_init(&hm_presentation, SDL_GetTicksNS(), hm_host_present_rate())) {
        SDL_SetError("Invalid presentation rate");
        hm_host_fail();
    }
}
static void hm_host_display_event(Uint32 type) {
#ifdef HMBTN_ENH_PAUSE_MENU
    if (type == SDL_EVENT_WINDOW_DISPLAY_CHANGED ||
        type == SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED ||
        type == SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED) {
        hm_graphics_sync_monitor(hm_window, &hm_options);
        if (!hm_graphics_swapchain(hm_device, hm_window, &hm_options)) {
            hm_options.vsync = 1;
            if (!hm_graphics_swapchain(hm_device, hm_window, &hm_options))
                hm_host_fail();
        }
        hm_host_reset_presentation();
    }
#else
    (void)type;
#endif
}
static int hm_host_wide_active(void) {
#ifdef HMBTN_ENH_WIDESCREEN
    return hm_widescreen_margin() != 0;
#else
    return 0;
#endif
}
#if defined(HMBTN_ENH_SCALING) && defined(HMBTN_ENH_PAUSE_MENU)
static void hm_host_output_size(int *w, int *h) {
    if (hm_host_wide_active())
        return;
    if ((s64)*w * 3 > (s64)*h * 4)
        *w = *h * 4 / 3;
    else
        *h = *w * 3 / 4;
}
#endif
static void hm_host_blit(SDL_GPUCommandBuffer *command, SDL_GPUTexture *image, u32 image_width,
                         u32 image_height, SDL_GPUTexture *target, Uint32 width, Uint32 height,
                         int linear, int expanded) {
    int enabled = 0;
#ifdef HMBTN_ENH_WIDESCREEN
    enabled = hm_widescreen_enabled();
#endif
#ifdef HM_HOST_AA
    if (hm_options.antialiasing == HM_AA_FXAA && !hm_host_in_pause) {
        SDL_GPUTexture *filtered = hm_fxaa_encode(hm_fxaa_output, command, image, image_width, image_height);
        if (filtered) image = filtered;
        else {
            hm_options.antialiasing = HM_AA_OFF;
            fprintf(stderr, "FXAA output unavailable; anti-aliasing disabled\n");
        }
    }
#endif
    hm_gpu_present_image(command, image, image_width, image_height, target, width, height, linear,
                         expanded, enabled);
}
static int hm_host_submit(u32 image_width, u32 image_height, int linear, int wide) {
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(hm_device);
    if (!command)
        hm_host_fail();
    SDL_GPUTexture *swapchain = NULL;
    Uint32 width, height;
    if (!SDL_AcquireGPUSwapchainTexture(command, hm_window, &swapchain, &width, &height)) {
        SDL_CancelGPUCommandBuffer(command);
        hm_host_fail();
    }
    if (!swapchain) {
        /* Skip a full frame-in-flight queue rather than explicitly wait on it. */
        if (!SDL_CancelGPUCommandBuffer(command))
            hm_host_fail();
        return 0;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command);
    SDL_GPUTextureTransferInfo source = {.transfer_buffer = hm_upload,
                                         .pixels_per_row = image_width,
                                         .rows_per_layer = image_height};
    SDL_GPUTextureRegion region = {
        .texture = hm_texture, .w = image_width, .h = image_height, .d = 1};
    SDL_UploadToGPUTexture(copy, &source, &region, true);
    SDL_EndGPUCopyPass(copy);
    hm_host_blit(command, hm_texture, image_width, image_height, swapchain, width, height, linear,
                 wide);
    if (!SDL_SubmitGPUCommandBuffer(command))
        hm_host_fail();
    return 1;
}

#ifdef HMBTN_GPU_RASTER
static int hm_host_gpu_present(const RECT *native_area) {
    if (!hm_gpu_output || !hm_gpu_commands || native_area->w <= 0 || native_area->h <= 0)
        return 0;
#ifdef HMBTN_ENH_PAUSE_MENU
    if (hm_options.renderer) {
        hm_frame_replay_release(hm_gpu_replay);
        hm_gpu_replay = NULL;
        hm_gpu_cached_serial = 0;
        return 0;
    }
#endif
    if (!hm_gpu_replay) {
        hm_gpu_replay = hm_frame_replay_acquire();
        hm_gpu_cached_serial = 0;
        if (!hm_gpu_replay)
            return 0;
    }
    /* Diagnostic capture deliberately uses the software reference. */
    if (SDL_getenv("HMBTN_CAPTURE_FRAME"))
        return 0;
    RECT output_area = *native_area;
#ifdef HMBTN_ENH_WIDESCREEN
    output_area = hm_widescreen_area(output_area);
#endif
    u32 width = (u32)output_area.w, height = (u32)output_area.h;
#if defined(HMBTN_ENH_SCALING) && defined(HMBTN_ENH_PAUSE_MENU)
    if (hm_options.scaling) {
        int w, h;
        if (!SDL_GetWindowSizeInPixels(hm_window, &w, &h) || w <= 0 || h <= 0)
            return 0;
        hm_host_output_size(&w, &h);
        if (w > 4096 || h > 4096) {
            hm_host_scaling_unavailable();
            return 0;
        }
        width = (u32)w;
        height = (u32)h;
    }
#endif
    hm_render_frame live = hm_game_render_frame();
    u64 serial = hm_frame_replay_serial(hm_gpu_replay, &live);
    int interpolate = 0;
#if defined(HMBTN_ENH_INTERPOLATION) && defined(HMBTN_ENH_PAUSE_MENU)
    interpolate = hm_options.interpolation && live.eligible;
#endif
    u32 texture_filter = 0;
    unsigned multisamples = 1;
#ifdef HM_HOST_AA
    multisamples = hm_antialiasing_samples(hm_options.antialiasing);
#endif
#if defined(HMBTN_ENH_FILTERING) && defined(HMBTN_ENH_PAUSE_MENU)
    texture_filter = (u32)hm_options.linear;
#endif
    u64 cache_serial = interpolate ? 0 : serial;
    if (!cache_serial || hm_gpu_cached_serial != cache_serial || hm_gpu_cached.width != width ||
        hm_gpu_cached.height != height ||
        (hm_gpu_cached.words && (hm_gpu_cached.words[11] != texture_filter ||
                                hm_gpu_cached.words[15] != multisamples))) {
        hm_gpu_cached_serial = 0;
        hm_gpu_cached = (hm_gpu_scene_data){0};
        hm_gpu_scene_texture_filter(hm_gpu_commands, texture_filter);
        hm_gpu_scene_multisamples(hm_gpu_commands, multisamples);
        hm_replay_sink previous = hm_gpu_scene_begin(hm_gpu_commands, width, height);
        int replayed = 0;
#ifdef HMBTN_ENH_INTERPOLATION
        if (interpolate)
            replayed = hm_interpolation_replay(SDL_GetTicksNS() - hm_start, &live);
#endif
        if (!replayed)
            replayed = hm_frame_replay_draw(hm_gpu_replay, &live);
        if (!hm_gpu_scene_end(hm_gpu_commands, previous, &hm_gpu_cached) || !replayed) {
            if (serial && !hm_gpu_fallback_reported) {
                fprintf(stderr, "GPU scene fallback: %s\n", hm_gpu_scene_fallback(hm_gpu_commands));
                hm_gpu_fallback_reported = 1;
            }
            return 0;
        }
        hm_gpu_cached_serial = cache_serial;
    }
    SDL_GPUCommandBuffer *command = SDL_AcquireGPUCommandBuffer(hm_device);
    if (!command)
        hm_host_fail();
    SDL_GPUTexture *swapchain = NULL;
    Uint32 window_width, window_height;
    if (!SDL_AcquireGPUSwapchainTexture(command, hm_window, &swapchain, &window_width,
                                        &window_height)) {
        SDL_CancelGPUCommandBuffer(command);
        hm_host_fail();
    }
    if (!swapchain) {
        SDL_CancelGPUCommandBuffer(command);
        return 1;
    }
    SDL_GPUTexture *image = hm_gpu_renderer_encode(hm_gpu_output, command, &hm_gpu_cached);
    if (!image) {
        SDL_CancelGPUCommandBuffer(command);
        if (!hm_gpu_fallback_reported) {
            fprintf(stderr, "GPU output unavailable; using software rendering\n");
            hm_gpu_fallback_reported = 1;
        }
        return 0;
    }
    int linear = 0;
#ifdef HMBTN_ENH_PAUSE_MENU
    linear = hm_options.linear;
#endif
    hm_host_blit(command, image, width, height, swapchain, window_width, window_height, linear,
                 hm_host_wide_active());
    if (!SDL_SubmitGPUCommandBuffer(command))
        hm_host_fail();
    return 1;
}
#endif

#ifdef HMBTN_ENH_PAUSE_MENU
static void hm_host_pause(void) {
    hm_host_in_pause = 1;
    hm_pause_menu menu;
    hm_pause_open(&menu);
    Uint64 before = SDL_GetTicksNS();
    hm_audio_output_pause(1);
    hm_host_reset_presentation();
    const hm_gpu_environment *state = hm_gpu_environment_state();
    const RECT *area = &state->display.disp;
#ifdef HMBTN_ENH_INTERPOLATION
    hm_interpolated_view interpolated = {0};
    hm_render_frame live = hm_game_render_frame();
    if (state->display_enabled &&
        hm_interpolation_render(SDL_GetTicksNS() - hm_start, &live, &interpolated))
        area = &interpolated.area;
#endif
    u32 pause_width = 320;
#ifdef HMBTN_ENH_WIDESCREEN
    hm_software_view wide = {0};
    hm_software_output *frozen_output = NULL;
    RECT wide_area = hm_widescreen_area(*area);
    if (state->display_enabled && wide_area.w > 320 && wide_area.h == 240) {
        frozen_output = hm_software_output_create();
        if (frozen_output && hm_software_output_prepare(frozen_output, wide_area.w, 240)) {
            hm_render_frame frozen_frame = hm_game_render_frame();
            hm_replay_sink previous = hm_software_output_begin(frozen_output);
            int replayed = 0;
#ifdef HMBTN_ENH_INTERPOLATION
            if (hm_options.interpolation && frozen_frame.eligible)
                replayed = hm_interpolation_replay(SDL_GetTicksNS() - hm_start, &frozen_frame);
#endif
            if (!replayed)
                replayed = hm_software_output_replay(frozen_output, &frozen_frame);
            int completed = hm_software_output_end(frozen_output, previous, &wide);
            if (replayed && completed)
                pause_width = wide.width;
            else
                wide = (hm_software_view){0};
        }
    }
#endif
    for (unsigned y = 0; y < 240; ++y)
        for (unsigned x = 0; x < pause_width; ++x) {
            u16 color = state->display_enabled && area->w > 0 && area->h > 0
                            ? hm_vram_pixel((u32)area->x + x * (u32)area->w / 320,
                                            (u32)area->y + y * (u32)area->h / 240)
                            : 0;
#ifdef HMBTN_ENH_INTERPOLATION
            if (interpolated.pixels)
                color = interpolated
                            .pixels[((u32)area->y + y * (u32)area->h / 240) * interpolated.stride +
                                    (u32)area->x + x * (u32)area->w / 320];
#endif
#ifdef HMBTN_ENH_WIDESCREEN
            if (wide.pixels)
                color = wide.pixels[y * pause_width + x];
#endif
            u8 *out = hm_pause_frozen + (y * pause_width + x) * 4;
            for (unsigned k = 0; k < 3; ++k) {
                unsigned value = (color >> (k * 5)) & 31;
                out[k] = (u8)((value << 3) | (value >> 2));
            }
            out[3] = 255;
        }
#ifdef HMBTN_ENH_WIDESCREEN
    hm_software_output_destroy(frozen_output);
#endif
    if (!hm_host_image_size(pause_width, 240))
        hm_host_fail();
    int running = 1;
    const char *notice = "";
    char value[128] = "";
    char rows[HM_PAUSE_MAX_VIEW_ITEMS][32] = {{0}};
    hm_pause_view view = {.detail = value};
    for (int i = 0; i < HM_PAUSE_MAX_VIEW_ITEMS; ++i)
        view.values[i] = rows[i];
    int value_dirty = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            value_dirty = 1;
            if (event.type == SDL_EVENT_QUIT) {
                hm_game_host_stop();
                exit(0);
            }
            hm_host_display_event(event.type);
            hm_pause_input input;
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                switch (event.gbutton.button) {
                case SDL_GAMEPAD_BUTTON_DPAD_UP:
                    input = HM_PAUSE_UP; break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
                    input = HM_PAUSE_DOWN; break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
                    input = HM_PAUSE_LEFT; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                    input = HM_PAUSE_RIGHT; break;
                case SDL_GAMEPAD_BUTTON_SOUTH:
                    input = HM_PAUSE_CONFIRM; break;
                case SDL_GAMEPAD_BUTTON_EAST:
                case SDL_GAMEPAD_BUTTON_START:
                    input = HM_PAUSE_BACK; break;
                default:
                    continue;
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                int win_w = 0, win_h = 0;
                if (SDL_GetWindowSizeInPixels(hm_window, &win_w, &win_h) && win_w > 0 && win_h > 0) {
                    int vx, vy;
                    if (hm_mouse_calc_virtual_pos(event.motion.x, event.motion.y, win_w, win_h, &vx, &vy)) {
                        if (menu.page == HM_PAUSE_GRAPHICS || menu.page == HM_PAUSE_GAMEPLAY ||
                            menu.page == HM_PAUSE_CONTROLS || menu.page == HM_PAUSE_AUDIO) {
                            if (vx >= 20 && vx < 300 && vy >= 45 && vy < 45 + HM_PAUSE_VISIBLE_ROWS * 20) {
                                int row = (vy - 45) / 20;
                                int idx = menu.first_visible + row;
                                if (idx < hm_pause_count(&menu) && menu.selected != idx) {
                                    menu.selected = idx;
                                    value_dirty = 1;
                                }
                            }
                        } else {
                            int root = menu.page == HM_PAUSE_ROOT;
                            int top = root ? 54 : 38;
                            int row_start = top + (root ? 44 : 42);
                            int count = hm_pause_count(&menu);
                            if (vx >= 80 && vx < 240) {
                                for (int i = 0; i < count; ++i) {
                                    int ry = row_start + i * 24 - 4;
                                    if (vy >= ry && vy < ry + 22) {
                                        if (menu.selected != i) {
                                            menu.selected = i;
                                            value_dirty = 1;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                continue;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    input = HM_PAUSE_BACK;
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    int win_w = 0, win_h = 0;
                    if (SDL_GetWindowSizeInPixels(hm_window, &win_w, &win_h) && win_w > 0 && win_h > 0) {
                        int vx, vy;
                        if (hm_mouse_calc_virtual_pos(event.button.x, event.button.y, win_w, win_h, &vx, &vy)) {
                            if (menu.page == HM_PAUSE_GRAPHICS || menu.page == HM_PAUSE_GAMEPLAY ||
                                menu.page == HM_PAUSE_CONTROLS || menu.page == HM_PAUSE_AUDIO) {
                                if (vx >= 20 && vx < 300 && vy >= 45 && vy < 45 + HM_PAUSE_VISIBLE_ROWS * 20) {
                                    int row = (vy - 45) / 20;
                                    int idx = menu.first_visible + row;
                                    if (idx < hm_pause_count(&menu)) {
                                        menu.selected = idx;
                                        if (vx >= 192) {
                                            input = (vx < 246) ? HM_PAUSE_LEFT : HM_PAUSE_RIGHT;
                                        } else {
                                            input = HM_PAUSE_CONFIRM;
                                        }
                                    } else {
                                        continue;
                                    }
                                } else {
                                    continue;
                                }
                            } else {
                                int root = menu.page == HM_PAUSE_ROOT;
                                int top = root ? 54 : 38;
                                int row_start = top + (root ? 44 : 42);
                                int count = hm_pause_count(&menu);
                                int found = 0;
                                if (vx >= 80 && vx < 240) {
                                    for (int i = 0; i < count; ++i) {
                                        int ry = row_start + i * 24 - 4;
                                        if (vy >= ry && vy < ry + 22) {
                                            menu.selected = i;
                                            input = HM_PAUSE_CONFIRM;
                                            found = 1;
                                            break;
                                        }
                                    }
                                }
                                if (!found)
                                    continue;
                            }
                        } else {
                            continue;
                        }
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                if (event.wheel.y > 0)
                    input = HM_PAUSE_UP;
                else if (event.wheel.y < 0)
                    input = HM_PAUSE_DOWN;
                else
                    continue;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                switch (event.key.key) {
                case SDLK_ESCAPE:
                case SDLK_BACKSPACE:
                case SDLK_J:
                    input = HM_PAUSE_BACK;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_X:
                case SDLK_K:
                case SDLK_SPACE:
                    input = HM_PAUSE_CONFIRM;
                    break;
                case SDLK_UP:
                case SDLK_W:
                    input = HM_PAUSE_UP;
                    break;
                case SDLK_DOWN:
                case SDLK_S:
                    input = HM_PAUSE_DOWN;
                    break;
                case SDLK_LEFT:
                case SDLK_A:
                    input = HM_PAUSE_LEFT;
                    break;
                case SDLK_RIGHT:
                case SDLK_D:
                    input = HM_PAUSE_RIGHT;
                    break;
                default:
                    continue;
                }
            } else {
                continue;
            }
            hm_pause_action action = hm_pause_handle(&menu, input);
            notice = "";
            if (action == HM_PAUSE_RESUME) {
                running = 0;
                break;
            }
            if (action == HM_PAUSE_EXIT) {
                hm_game_host_stop();
                exit(0);
            }
            if (action == HM_PAUSE_CHANGE_GRAPHICS) {
                if (hm_graphics_change(hm_device, hm_window, &hm_options, menu.selected,
                                       input == HM_PAUSE_LEFT ? -1 : 1)) {
#ifdef HMBTN_ENH_WIDESCREEN
                    int changed_w, changed_h;
                    if (SDL_GetWindowSizeInPixels(hm_window, &changed_w, &changed_h))
                        hm_widescreen_configure(hm_options.widescreen, changed_w, changed_h);
#endif
                    hm_host_reset_presentation();
                    notice = hm_graphics_save(&hm_options) ? "" : "Cannot save settings";
                } else {
                    notice = "Option unavailable";
#ifdef HM_HOST_AA
                    if (menu.selected == 10 && hm_options.antialiasing >= HM_AA_MSAA2)
                        notice = "MSAA requires GPU Auto";
#endif
                }
            }
#ifdef HMBTN_ENH_GAMEPLAY
            if (action == HM_PAUSE_CHANGE_GAMEPLAY) {
                if (hm_gameplay_change(&hm_gameplay, menu.selected,
                                       input == HM_PAUSE_LEFT ? -1 : 1)) {
                    notice = hm_gameplay_save(&hm_gameplay) ? "" : "Cannot save settings";
                }
            }
#else
            if (action == HM_PAUSE_CHANGE_GAMEPLAY) {
                notice = "Option unavailable";
            }
#endif
            if (action == HM_PAUSE_CHANGE_CONTROLS) {
                if (hm_controls_change(&hm_controls, menu.selected,
                                       input == HM_PAUSE_LEFT ? -1 : 1)) {
                    notice = hm_controls_save(&hm_controls) ? "" : "Cannot save settings";
                }
            }
            if (action == HM_PAUSE_CHANGE_AUDIO) {
                if (hm_audio_change(&hm_audio, menu.selected,
                                    input == HM_PAUSE_LEFT ? -1 : 1)) {
                    notice = hm_audio_save(&hm_audio) ? "" : "Cannot save settings";
                }
            }
        }
        if (!running)
            break;
        if (hm_present_clock_due(&hm_presentation, SDL_GetTicksNS())) {
            u8 *pixels = SDL_MapGPUTransferBuffer(hm_device, hm_upload, true);
            if (!pixels)
                hm_host_fail();
            hm_pause_canvas_extract(hm_pause_background, hm_pause_frozen, pause_width);
            if (value_dirty) {
                value[0] = 0;
                for (int i = 0; i < HM_PAUSE_MAX_VIEW_ITEMS; ++i)
                    rows[i][0] = 0;
                if (menu.page == HM_PAUSE_GRAPHICS)
                    hm_graphics_value(&hm_options, menu.selected, value, sizeof(value));
                if (menu.page == HM_PAUSE_GRAPHICS)
                    for (int i = 0; i < HM_PAUSE_GRAPHICS_ITEMS; ++i)
                        hm_graphics_row_value(&hm_options, i, rows[i], sizeof(rows[i]));
#ifdef HMBTN_ENH_GAMEPLAY
                if (menu.page == HM_PAUSE_GAMEPLAY)
                    hm_gameplay_value(&hm_gameplay, menu.selected, value, sizeof(value));
                if (menu.page == HM_PAUSE_GAMEPLAY)
                    for (int i = 0; i < HM_PAUSE_GAMEPLAY_ITEMS; ++i)
                        hm_gameplay_row_value(&hm_gameplay, i, rows[i], sizeof(rows[i]));
#endif
                if (menu.page == HM_PAUSE_CONTROLS)
                    hm_controls_value(&hm_controls, menu.selected, value, sizeof(value));
                if (menu.page == HM_PAUSE_CONTROLS)
                    for (int i = 0; i < HM_PAUSE_CONTROLS_ITEMS; ++i)
                        hm_controls_row_value(&hm_controls, i, rows[i], sizeof(rows[i]));
                if (menu.page == HM_PAUSE_AUDIO)
                    hm_audio_value(&hm_audio, menu.selected, value, sizeof(value));
                if (menu.page == HM_PAUSE_AUDIO)
                    for (int i = 0; i < HM_PAUSE_AUDIO_ITEMS; ++i)
                        hm_audio_row_value(&hm_audio, i, rows[i], sizeof(rows[i]));
                value_dirty = 0;
            }
            view.notice = notice;
            hm_pause_draw(&hm_assets, hm_pause_background, &menu, &view);
            hm_pause_canvas_compose(pixels, hm_pause_frozen, pause_width, hm_pause_background);
            SDL_UnmapGPUTransferBuffer(hm_device, hm_upload);
            hm_host_submit(pause_width, 240, 0, pause_width > 320);
        }
        Uint64 now = SDL_GetTicksNS();
        if (hm_presentation.next_ns > now) {
            Uint64 wait = hm_presentation.next_ns - now;
            SDL_DelayPrecise(wait < 5000000 ? wait : 5000000);
        }
    }
    hm_start += SDL_GetTicksNS() - before;
    hm_host_reset_presentation();
    hm_ignore_held = 1;
    hm_audio_output_pause(0);
    hm_host_in_pause = 0;
}
#endif

static void hm_host_present_scanout(u64 tick) {
    if (hm_trace_camera && tick % 120 == 0) {
        const FarmPlayer *player = &game_state.player;
        const FarmScroll *scroll = &farm_area.mode.scroll;
        fprintf(stderr,
                "Camera tick=%llu area=%d place=%d,%d player=%d,%d,%d centre=%d,%d,%d "
                "eye=%d,%d,%d yaw=%d distance=%d height=%d global=%08x debug=%08x "
                "event=%d view=%d entry=%d,%d\n",
                (unsigned long long)tick, farm_area.mode.area, farm_area.mode.place.value,
                farm_area.mode.place.location, player->position.vx, player->position.vy,
                player->position.vz, scroll->centre.vx, scroll->centre.vy, scroll->centre.vz,
                scroll->a18, scroll->b1A, scroll->c1C, scroll->yaw, scroll->z, scroll->x,
                game_state.globalFlags, game_state.debugFlags, farm_area.mode.eventMode,
                farm_area.mode.view, game_state.progress.entryStep,
                game_state.progress.entrySubStep);
    }
    const hm_gpu_environment *state = hm_gpu_environment_state();
    const RECT *area = &state->display.disp;
#if (defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)) && defined(HMBTN_ENH_PAUSE_MENU)
    int private_output = 0;
#ifdef HMBTN_ENH_FILTERING
    private_output |= hm_options.linear;
#endif
#ifdef HMBTN_ENH_WIDESCREEN
    private_output |= hm_widescreen_margin() != 0;
#endif
    if (private_output && !hm_native_output) {
        hm_native_output = hm_software_output_create();
        if (!hm_native_output) {
            hm_options.linear = 0;
#ifdef HMBTN_ENH_WIDESCREEN
            hm_options.widescreen = 0;
            hm_widescreen_configure(0, 0, 0);
#endif
            fprintf(stderr, "Private rendering unavailable; using native output\n");
        }
    }
    if (!private_output && hm_native_output) {
        hm_software_output_destroy(hm_native_output);
        hm_native_output = NULL;
    }
#endif

#ifdef HMBTN_ENH_PAUSE_MENU
    hm_options.gpu_active = 0;
#endif
#ifdef HMBTN_GPU_RASTER
    if (state->display_enabled && hm_host_gpu_present(area)) {
#ifdef HMBTN_ENH_PAUSE_MENU
        hm_options.gpu_active = 1;
#endif
        return;
    }
#endif
#if defined(HMBTN_ENH_SCALING) || defined(HMBTN_ENH_INTERPOLATION) ||                              \
    defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    hm_render_frame live = hm_game_render_frame();
#endif
#ifdef HMBTN_ENH_SCALING
    hm_scaled_view scaled = {0};
    int scaling_active = 0;
    hm_replay_sink previous_sink = {0};
#if defined(HMBTN_ENH_PAUSE_MENU)
    /* Auto never stalls simulation with a viewport-sized CPU retry. The
     * explicit Software renderer remains the full-resolution reference. */
    scaling_active = state->display_enabled && hm_options.scaling && hm_options.renderer &&
                     hm_host_scaling_prepare();
#endif
    if (scaling_active)
        previous_sink = hm_scaling_begin();
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    hm_software_view native_filtered = {0};
    int native_filter_active = 0;
    hm_replay_sink native_previous = {0};
    if (state->display_enabled && hm_native_output && area->w > 0 && area->h > 0
#ifdef HMBTN_ENH_SCALING
        && !scaling_active
#endif
    ) {
        int filter = 0;
#if defined(HMBTN_ENH_FILTERING) && defined(HMBTN_ENH_PAUSE_MENU)
        filter = hm_options.linear;
#endif
        hm_software_output_filter(hm_native_output, filter);
        RECT output_area = *area;
#ifdef HMBTN_ENH_WIDESCREEN
        output_area = hm_widescreen_area(output_area);
#endif
        native_filter_active =
            hm_software_output_prepare(hm_native_output, (u32)output_area.w, (u32)output_area.h);
        if (native_filter_active)
            native_previous = hm_software_output_begin(hm_native_output);
        else {
            hm_software_output_destroy(hm_native_output);
            hm_native_output = NULL;
#ifdef HMBTN_ENH_PAUSE_MENU
            hm_options.linear = 0;
#ifdef HMBTN_ENH_WIDESCREEN
            hm_options.widescreen = 0;
            hm_widescreen_configure(0, 0, 0);
#endif
#endif
            fprintf(stderr, "Private output allocation failed; using native output\n");
        }
    }
#endif
#ifdef HMBTN_ENH_INTERPOLATION
    hm_interpolated_view interpolated = {0};
    if (state->display_enabled &&
        hm_interpolation_render(SDL_GetTicksNS() - hm_start, &live, &interpolated))
        area = &interpolated.area;
#endif
#ifdef HMBTN_ENH_SCALING
    if (scaling_active) {
#ifdef HMBTN_ENH_INTERPOLATION
        if (!interpolated.pixels)
#endif
            hm_scaling_replay(&live);
        hm_scaling_end(previous_sink, &scaled);
    }
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    if (native_filter_active) {
#ifdef HMBTN_ENH_INTERPOLATION
        if (!interpolated.pixels)
#endif
            hm_software_output_replay(hm_native_output, &live);
        hm_software_output_end(hm_native_output, native_previous, &native_filtered);
    }
#endif
    /* A direct/loading scanout has no replayable scene. Keep its native pixels
     * centred on the same wide canvas rather than changing the output aspect. */
    RECT source_area = *area;
#ifdef HMBTN_ENH_WIDESCREEN
    RECT scanout_area = hm_widescreen_area(source_area);
    int wide_scanout = 0;
#ifdef HMBTN_ENH_SCALING
    if (!scaled.pixels)
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
        if (!native_filtered.pixels)
#endif
        {
            wide_scanout = scanout_area.w != source_area.w;
            area = &scanout_area;
        }
#endif
    if (SDL_getenv("HMBTN_TRACE_SCANOUT") && tick % 60 == 0) {
        fprintf(stderr, "Scanout tick %llu: enabled=%d area=%d,%d %dx%d\n",
                (unsigned long long)tick, state->display_enabled, area->x, area->y, area->w,
                area->h);
    }
    if (!area->w || !area->h)
        return;
    if (area->w < 0 || area->w > HM_SCAN_WIDTH || area->h < 0 || area->h > HM_SCAN_HEIGHT) {
        SDL_SetError("unsupported display dimensions");
        hm_host_fail();
    }
    u32 output_width = (u32)area->w, output_height = (u32)area->h;
#ifdef HMBTN_ENH_SCALING
    if (scaled.pixels) {
        output_width = scaled.width;
        output_height = scaled.height;
    }
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    if (native_filtered.pixels) {
        output_width = native_filtered.width;
        output_height = native_filtered.height;
    }
#endif
    if (!hm_host_image_size(output_width, output_height)) {
#ifdef HMBTN_ENH_SCALING
        if (scaled.pixels) {
            hm_host_scaling_unavailable();
            scaled = (hm_scaled_view){0};
            output_width = (u32)area->w;
            output_height = (u32)area->h;
        }
#endif
        if (!hm_host_image_size(output_width, output_height))
            hm_host_fail();
    }
    u8 *pixels = SDL_MapGPUTransferBuffer(hm_device, hm_upload, true);
    if (!pixels)
        hm_host_fail();
    u16 backdrop = state->display_enabled ? hm_gpu_background(&state->draw) : 0;
    for (u32 y = 0; y < output_height; ++y)
        for (u32 x = 0; x < output_width; ++x) {
            u16 color = 0;
            int source_x = area->x + (int)x, source_y = area->y + (int)y;
            int source_visible = source_x >= source_area.x && source_y >= source_area.y &&
                                 source_x < source_area.x + source_area.w &&
                                 source_y < source_area.y + source_area.h;
#ifdef HMBTN_ENH_SCALING
            if (scaled.pixels)
                color = scaled.pixels[(size_t)y * output_width + x];
            else
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
                if (native_filtered.pixels)
                color = native_filtered.pixels[(size_t)y * output_width + x];
            else
#endif
                color = state->display_enabled && source_visible
                            ? hm_vram_pixel((u32)source_x, (u32)source_y)
                            : backdrop;
#ifdef HMBTN_ENH_INTERPOLATION
            if (interpolated.pixels && source_visible
#ifdef HMBTN_ENH_SCALING
                && !scaled.pixels
#endif
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
                && !native_filtered.pixels
#endif
            )
                color = interpolated
                            .pixels[((u32)area->y + y) * interpolated.stride + (u32)area->x + x];
#endif
            u8 *out = pixels + (y * output_width + x) * 4;
            s32 dither_val = (s32)(((x * 13u + y * 7u) ^ (x * y)) & 3u) - 1;
            for (u32 channel = 0; channel < 3; ++channel) {
                u32 component = (color >> (channel * 5)) & 31;
                s32 byte_val = (s32)((component << 3) | (component >> 2)) + dither_val;
                out[channel] = (u8)(byte_val < 0 ? 0 : byte_val > 255 ? 255 : byte_val);
            }
            out[3] = 255;
        }
    const char *capture = SDL_getenv("HMBTN_CAPTURE_FRAME");
    if (capture) {
        SDL_Surface *surface =
            SDL_CreateSurfaceFrom((int)output_width, (int)output_height, SDL_PIXELFORMAT_RGBA32,
                                  pixels, (int)output_width * 4);
        if (!surface || !SDL_SaveBMP(surface, capture))
            hm_host_fail();
        SDL_DestroySurface(surface);
    }
    SDL_UnmapGPUTransferBuffer(hm_device, hm_upload);
#ifdef HMBTN_ENH_PAUSE_MENU
    hm_host_submit(output_width, output_height, hm_options.linear,
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
                   native_filtered.pixels ? hm_host_wide_active() :
#endif
#ifdef HMBTN_ENH_SCALING
                   scaled.pixels ? hm_host_wide_active()
                                 :
#endif
#ifdef HMBTN_ENH_WIDESCREEN
                                 wide_scanout);
#else
                   0);
#endif
#else
    hm_host_submit(output_width, output_height, 0, 0);
#endif
}

static void hm_host_events(void) {
#if defined(HMBTN_ENH_WIDESCREEN) && defined(HMBTN_ENH_PAUSE_MENU)
    int output_w, output_h;
    if (SDL_GetWindowSizeInPixels(hm_window, &output_w, &output_h))
        hm_widescreen_configure(hm_options.widescreen, output_w, output_h);
#endif
    int current_win_w = 0, current_win_h = 0;
    float current_mx = 0.0f, current_my = 0.0f;
    if (SDL_GetWindowSizeInPixels(hm_window, &current_win_w, &current_win_h) && current_win_w > 0 && current_win_h > 0) {
        SDL_GetMouseState(&current_mx, &current_my);
        hm_mouse_update_pos(current_mx, current_my, current_win_w, current_win_h);
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            hm_game_host_stop();
            exit(0);
        }
        hm_host_display_event(event.type);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event.button.button == SDL_BUTTON_LEFT)
                hm_mouse_button_down(HM_MOUSE_BTN_LEFT);
            else if (event.button.button == SDL_BUTTON_RIGHT)
                hm_mouse_button_down(HM_MOUSE_BTN_RIGHT);
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (event.button.button == SDL_BUTTON_LEFT)
                hm_mouse_button_up(HM_MOUSE_BTN_LEFT);
            else if (event.button.button == SDL_BUTTON_RIGHT)
                hm_mouse_button_up(HM_MOUSE_BTN_RIGHT);
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            if (event.wheel.y > 0)
                hm_mouse_wheel(1);
            else if (event.wheel.y < 0)
                hm_mouse_wheel(-1);
        }
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            if (!hm_active_gamepad)
                hm_active_gamepad = SDL_OpenGamepad(event.gdevice.which);
        } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            if (hm_active_gamepad && SDL_GetGamepadID(hm_active_gamepad) == event.gdevice.which) {
                SDL_CloseGamepad(hm_active_gamepad);
                hm_active_gamepad = NULL;
            }
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            if (event.wheel.y > 0)
                hm_mouse_wheel_up = 4;
            else if (event.wheel.y < 0)
                hm_mouse_wheel_down = 4;
        }
#ifdef HMBTN_ENH_PAUSE_MENU
        if (hm_pause_enabled && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            event.key.key == SDLK_ESCAPE)
            hm_host_pause();
#endif
    }
}

static void hm_host_tick(void *context, u64 tick) {
    (void)context;
    if (hm_host_tick_limit && tick > hm_host_tick_limit) {
        printf("Native window completed %llu VBlanks\n", (unsigned long long)hm_host_tick_limit);
        hm_game_host_stop();
        exit(0);
    }
    for (;;) {
        hm_host_events();
        Uint64 now = SDL_GetTicksNS();
        Uint64 deadline = hm_start + tick * SDL_NS_PER_SECOND / 60;
        /* Let an overdue simulation tick run before doing more visual work. */
        if (now >= deadline)
            break;
        if (hm_present_clock_due(&hm_presentation, now))
            hm_host_present_scanout(tick);
        now = SDL_GetTicksNS();
        /* A recoverable display error can exclude modal time from the epoch. */
        deadline = hm_start + tick * SDL_NS_PER_SECOND / 60;
        if (now >= deadline)
            break;
        Uint64 wake = hm_presentation.next_ns < deadline ? hm_presentation.next_ns : deadline;
        if (wake > now)
            SDL_DelayPrecise(wake - now);
    }
    u16 held = 0;
    if (hm_controls.keyboard_layout == HM_KEYBOARD_LAYOUT_ARROWS) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        static const SDL_Scancode mapping[16] = {
            SDL_SCANCODE_Q,         SDL_SCANCODE_E,       SDL_SCANCODE_A,       SDL_SCANCODE_S,
            SDL_SCANCODE_W,         SDL_SCANCODE_D,       SDL_SCANCODE_X,       SDL_SCANCODE_Z,
            SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_RETURN,
            SDL_SCANCODE_UP,        SDL_SCANCODE_RIGHT,   SDL_SCANCODE_DOWN,    SDL_SCANCODE_LEFT};
        for (u32 i = 0; i < 16; ++i)
            if (mapping[i] != SDL_SCANCODE_UNKNOWN && keys[mapping[i]])
                held |= (u16)(1u << i);
    } else if (hm_controls.keyboard_layout == HM_KEYBOARD_LAYOUT_WASD) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        static const SDL_Scancode mapping_wasd[16] = {
            SDL_SCANCODE_Y,         SDL_SCANCODE_P,       SDL_SCANCODE_O,       SDL_SCANCODE_U,
            SDL_SCANCODE_I,         SDL_SCANCODE_L,       SDL_SCANCODE_K,       SDL_SCANCODE_J,
            SDL_SCANCODE_SPACE,     SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_RETURN,
            SDL_SCANCODE_W,         SDL_SCANCODE_D,       SDL_SCANCODE_S,       SDL_SCANCODE_A};
        for (u32 i = 0; i < 16; ++i)
            if (mapping_wasd[i] != SDL_SCANCODE_UNKNOWN && keys[mapping_wasd[i]])
                held |= (u16)(1u << i);
    }
    if (hm_controls.controller_enabled && hm_active_gamepad) {
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))
            held |= HM_PAD_UP;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))
            held |= HM_PAD_DOWN;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))
            held |= HM_PAD_LEFT;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
            held |= HM_PAD_RIGHT;

        Sint16 stick_x = SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        Sint16 stick_y = SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
        if (stick_x < -12000) held |= HM_PAD_LEFT;
        if (stick_x > 12000) held |= HM_PAD_RIGHT;
        if (stick_y < -12000) held |= HM_PAD_UP;
        if (stick_y > 12000) held |= HM_PAD_DOWN;

        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_SOUTH))
            held |= HM_PAD_CROSS;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_EAST))
            held |= HM_PAD_CIRCLE;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_WEST))
            held |= HM_PAD_SQUARE;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_NORTH))
            held |= HM_PAD_TRIANGLE;

        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
            held |= HM_PAD_L1;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
            held |= HM_PAD_R1;
        if (SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 12000)
            held |= HM_PAD_L2;
        if (SDL_GetGamepadAxis(hm_active_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 12000)
            held |= HM_PAD_R2;

        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_START))
            held |= HM_PAD_START;
        if (SDL_GetGamepadButton(hm_active_gamepad, SDL_GAMEPAD_BUTTON_BACK))
            held |= HM_PAD_SELECT;
    }
    if (hm_controls.mouse_mode != HM_MOUSE_MODE_OFF) {
        float mx, my;
        Uint32 mbuttons = SDL_GetMouseState(&mx, &my);
        if (mbuttons & SDL_BUTTON_LMASK)
            held |= HM_PAD_CROSS;
        if (mbuttons & SDL_BUTTON_RMASK)
            held |= HM_PAD_SQUARE;
        if (mbuttons & SDL_BUTTON_MMASK)
            held |= HM_PAD_TRIANGLE;
        if (hm_mouse_wheel_up > 0) {
            held |= HM_PAD_R1;
            hm_mouse_wheel_up--;
        }
        if (hm_mouse_wheel_down > 0) {
            held |= HM_PAD_R2;
            hm_mouse_wheel_down--;
        }
        if (hm_controls.mouse_mode == HM_MOUSE_MODE_MOVE && (mbuttons & SDL_BUTTON_LMASK)) {
            int win_w = 0, win_h = 0;
            if (SDL_GetWindowSize(hm_window, &win_w, &win_h) && win_w > 0 && win_h > 0) {
                float center_x = win_w * 0.5f;
                float center_y = win_h * 0.5f;
                float dx = mx - center_x;
                float dy = my - center_y;
                if (dx > 40.0f) held |= HM_PAD_RIGHT;
                if (dx < -40.0f) held |= HM_PAD_LEFT;
                if (dy > 40.0f) held |= HM_PAD_DOWN;
                if (dy < -40.0f) held |= HM_PAD_UP;
            }
        }
    }
    if (hm_replay_enabled)
        held = hm_input_replay_sample(&hm_replay, tick);
#ifdef HMBTN_ENH_PAUSE_MENU
    if (hm_ignore_held) {
        if (!held)
            hm_ignore_held = 0;
        held = 0;
    }
#endif
    hm_pad_sample(0, 1, held);
}

int hm_game_host_start(hm_disc *disc) {
#ifndef HMBTN_ENH_PAUSE_MENU
    (void)disc;
#endif
    const char *limit = SDL_getenv("HMBTN_BOOT_TICKS");
    if (limit) {
        char *end;
        hm_host_tick_limit = strtoull(limit, &end, 10);
        if (!*limit || *end || !hm_host_tick_limit || hm_host_tick_limit > 1000000) {
            fprintf(stderr, "Invalid diagnostic tick limit\n");
            return 0;
        }
    }
    const char *replay_path = SDL_getenv("HMBTN_INPUT_REPLAY");
    if (replay_path) {
        if (!hm_input_replay_load(&hm_replay, replay_path)) {
            fprintf(stderr, "Cannot load input replay\n");
            return 0;
        }
        hm_replay_enabled = 1;
    }
    SDL_SetMainReady();
    hm_trace_camera = SDL_getenv("HMBTN_TRACE_CAMERA") != NULL;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
        return 0;
    /* The launcher starts user data first to read the remembered disc path. */
    if (!hm_user_data_root() && !hm_user_data_start()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN",
                                 "Cannot open Documents/VanillaBTN. Check folder permissions and "
                                 "close any other game instance.",
                                 NULL);
        return 0;
    }
    hm_window = SDL_CreateWindow(hm_version_window_title(), 640, 480, 0);
    if (hm_window) {
        SDL_Surface *icon = SDL_CreateSurfaceFrom(
            HMBTN_ICON_WIDTH, HMBTN_ICON_HEIGHT, SDL_PIXELFORMAT_RGBA32,
            (void *)hm_window_icon_rgba, HMBTN_ICON_WIDTH * 4);
        if (icon) {
            SDL_SetWindowIcon(hm_window, icon);
            SDL_DestroySurface(icon);
        }
    }
    hm_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                        SDL_GPU_SHADERFORMAT_MSL,
                                    false, NULL);
    if (!hm_window || !hm_device || !SDL_ClaimWindowForGPUDevice(hm_device, hm_window) ||
        !SDL_SetGPUAllowedFramesInFlight(hm_device, 2))
        return 0;
    if (!hm_host_image_size(HM_SCAN_WIDTH, HM_SCAN_HEIGHT))
        return 0;
    if (!hm_audio_output_start()) {
        fprintf(stderr, "Native audio: %s\n", SDL_GetError());
        return 0;
    }
    hm_spu_set_hq(1); /* Modern high-fidelity Cubic Hermite audio resampling */
#ifdef HMBTN_ENH_PAUSE_MENU
    hm_pause_enabled = hm_pause_config_enabled();
    if (hm_pause_enabled && !hm_pause_assets_load(&hm_assets, disc)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Back to Nature",
                                 "Cannot load the original game font for the pause menu.",
                                 hm_window);
        return 0;
    }
    hm_graphics_load(&hm_options);
#ifdef HMBTN_ENH_GAMEPLAY
    hm_gameplay_load(&hm_gameplay);
#endif
    if (!hm_graphics_apply(hm_window, &hm_options)) {
        hm_graphics_defaults(&hm_options);
        if (!hm_graphics_apply(hm_window, &hm_options))
            return 0;
    }
    if (!hm_graphics_swapchain(hm_device, hm_window, &hm_options)) {
        hm_options.vsync = 1;
        if (!hm_graphics_swapchain(hm_device, hm_window, &hm_options))
            return 0;
    }
#endif
#if defined(HMBTN_ENH_WIDESCREEN) && defined(HMBTN_ENH_PAUSE_MENU)
    int initial_w, initial_h;
    if (SDL_GetWindowSizeInPixels(hm_window, &initial_w, &initial_h))
        hm_widescreen_configure(hm_options.widescreen, initial_w, initial_h);
#endif
#if defined(HMBTN_ENH_INTERPOLATION) && defined(HMBTN_ENH_PAUSE_MENU)
    if (!hm_interpolation_enable(hm_options.interpolation)) {
        SDL_SetError("Cannot allocate visual interpolation buffers");
        return 0;
    }
#endif
#if defined(HMBTN_ENH_SCALING) && defined(HMBTN_ENH_PAUSE_MENU)
    if (!hm_scaling_enable(hm_options.scaling)) {
        SDL_SetError("Cannot allocate scaling history");
        return 0;
    }
#endif
#if defined(HMBTN_ENH_PGXP)
    hm_pgxp_set_enabled(1); /* Auto-applied: 3D subpixel precision without wobble */
#endif
#if defined(HMBTN_ENH_DITHERING)
    hm_dither_set_enabled(0); /* Auto-applied: clean 32-bit smooth colors without dither noise */
#endif
#ifdef HMBTN_GPU_RASTER
    hm_gpu_output = hm_gpu_renderer_create(hm_device);
    if (hm_gpu_output) {
        hm_gpu_commands = hm_gpu_scene_create();
        hm_gpu_replay = hm_frame_replay_acquire();
    }
    if (!hm_gpu_output || !hm_gpu_commands || !hm_gpu_replay) {
        hm_frame_replay_release(hm_gpu_replay);
        hm_gpu_replay = NULL;
        hm_gpu_scene_destroy(hm_gpu_commands);
        hm_gpu_commands = NULL;
        hm_gpu_renderer_destroy(hm_gpu_output);
        hm_gpu_output = NULL;
        fprintf(stderr, "GPU scene renderer unavailable; using software rendering\n");
    }
#endif
#ifdef HM_HOST_AA
    hm_fxaa_output = hm_fxaa_create(hm_device);
    hm_options.aa_capabilities = 1u | (hm_fxaa_output ? 1u << HM_AA_FXAA : 0);
#ifdef HMBTN_GPU_RASTER
    if (hm_gpu_output && hm_gpu_commands)
        hm_options.aa_capabilities |= (1u << HM_AA_MSAA2) | (1u << HM_AA_MSAA4);
#endif
    if (!(hm_options.aa_capabilities & (1u << hm_options.antialiasing)))
        hm_options.antialiasing = HM_AA_OFF;
#endif
    hm_controls_defaults(&hm_controls);
    hm_controls_load(&hm_controls);
    hm_audio_load(&hm_audio);
    int pad_count = 0;
    SDL_JoystickID *pads = SDL_GetGamepads(&pad_count);
    if (pads && pad_count > 0) {
        hm_active_gamepad = SDL_OpenGamepad(pads[0]);
        SDL_free(pads);
    }
    hm_start = SDL_GetTicksNS();
    hm_host_reset_presentation();
    hm_timing_reset((hm_timing_host){NULL, hm_host_tick, NULL});
    printf("Native game window using SDL_GPU %s\n", SDL_GetGPUDeviceDriver(hm_device));
    fflush(stdout);
    return 1;
}

void hm_game_host_stop(void) {
    if (hm_active_gamepad) {
        SDL_CloseGamepad(hm_active_gamepad);
        hm_active_gamepad = NULL;
    }
#if defined(HMBTN_ENH_FILTERING) || defined(HMBTN_ENH_WIDESCREEN)
    hm_software_output_destroy(hm_native_output);
    hm_native_output = NULL;
#endif
#ifdef HMBTN_ENH_SCALING
    hm_scaling_enable(0);
#endif
#ifdef HMBTN_ENH_INTERPOLATION
    hm_interpolation_enable(0);
#ifdef HMBTN_ENH_PGXP
    hm_pgxp_reset();
#endif
#endif
#ifdef HMBTN_GPU_RASTER
    hm_frame_replay_release(hm_gpu_replay);
    hm_gpu_replay = NULL;
    hm_gpu_scene_destroy(hm_gpu_commands);
    hm_gpu_commands = NULL;
    hm_gpu_cached = (hm_gpu_scene_data){0};
    hm_gpu_cached_serial = 0;
    hm_gpu_fallback_reported = 0;
#endif
    hm_audio_output_stop();
    if (hm_device) {
        SDL_WaitForGPUIdle(hm_device);
#ifdef HM_HOST_AA
        hm_fxaa_destroy(hm_fxaa_output);
        hm_fxaa_output = NULL;
#endif
#ifdef HMBTN_GPU_RASTER
        hm_gpu_renderer_destroy(hm_gpu_output);
        hm_gpu_output = NULL;
#endif
        if (hm_upload)
            SDL_ReleaseGPUTransferBuffer(hm_device, hm_upload);
        if (hm_texture)
            SDL_ReleaseGPUTexture(hm_device, hm_texture);
        if (hm_window)
            SDL_ReleaseWindowFromGPUDevice(hm_device, hm_window);
        SDL_DestroyGPUDevice(hm_device);
    }
    if (hm_window)
        SDL_DestroyWindow(hm_window);
    hm_device = NULL;
    hm_window = NULL;
    hm_upload = NULL;
    hm_texture = NULL;
    hm_image_width = hm_image_height = hm_upload_bytes = 0;
    hm_user_data_stop();
    SDL_Quit();
}
