#include "host_display.h"
#include "sdk_gpu.h"
#include "ps1_gte.h"
#include <string.h>

static WidescreenMode s_widescreen_mode = WIDESCREEN_AUTO;

void host_display_set_widescreen_mode(WidescreenMode mode) {
    s_widescreen_mode = mode;
}

WidescreenMode host_display_get_widescreen_mode(void) {
    return s_widescreen_mode;
}

/* Query connected displays count */
int host_display_get_monitor_count(void) {
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    if (displays) {
        SDL_free(displays);
    }
    return count > 0 ? count : 0;
}

/* Query display ID for a zero-based monitor index */
SDL_DisplayID host_display_get_monitor_id(int monitor_index) {
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    SDL_DisplayID id = 0;
    if (displays && count > 0) {
        if (monitor_index >= 0 && monitor_index < count) {
            id = displays[monitor_index];
        } else {
            id = displays[0];
        }
        SDL_free(displays);
    }
    return id;
}

/* Query desktop bounds for a monitor */
bool host_display_get_monitor_bounds(int monitor_index, SDL_Rect *bounds) {
    if (!bounds) return false;
    SDL_DisplayID id = host_display_get_monitor_id(monitor_index);
    if (!id) {
        bounds->x = 0;
        bounds->y = 0;
        bounds->w = 640;
        bounds->h = 480;
        return false;
    }
    return SDL_GetDisplayBounds(id, bounds);
}

/* Apply or change window mode and monitor position at runtime */
bool host_display_apply_mode(SDL_Window *window, const DisplayConfig *config) {
    if (!window) return false;
    WindowMode mode = config ? config->window_mode : WINDOW_MODE_WINDOWED;

    switch (mode) {
    case WINDOW_MODE_WINDOWED: {
        SDL_SetWindowFullscreen(window, false);
        SDL_RestoreWindow(window);
        int w = (config && config->window_width >= 320) ? config->window_width : 640;
        int h = (config && config->window_height >= 240) ? config->window_height : 480;
        SDL_SetWindowSize(window, w, h);
        return true;
    }
    case WINDOW_MODE_MAXIMIZED:
        SDL_SetWindowFullscreen(window, false);
        SDL_MaximizeWindow(window);
        return true;

    case WINDOW_MODE_BORDERLESS:
        SDL_SetWindowFullscreenMode(window, NULL);
        SDL_SetWindowFullscreen(window, true);
        return true;

    case WINDOW_MODE_FULLSCREEN: {
        SDL_DisplayID disp_id = host_display_get_monitor_id(config ? config->monitor : 0);
        SDL_DisplayMode closest;
        if (config && config->fullscreen_width > 0 && config->fullscreen_height > 0
            && SDL_GetClosestFullscreenDisplayMode(disp_id, config->fullscreen_width,
                                                   config->fullscreen_height,
                                                   (float)config->fullscreen_hz,
                                                   true, &closest)) {
            SDL_SetWindowFullscreenMode(window, &closest);
        } else {
            SDL_SetWindowFullscreenMode(window, NULL);
        }
        SDL_SetWindowFullscreen(window, true);
        return true;
    }
    default:
        return false;
    }
}

/* Create host window configured according to DisplayConfig */
SDL_Window *host_display_create_window(const DisplayConfig *config) {
    int win_w = (config && config->window_width >= 320) ? config->window_width : 640;
    int win_h = (config && config->window_height >= 240) ? config->window_height : 480;

    SDL_Window *window = SDL_CreateWindow("VanillaBTN", win_w, win_h, SDL_WINDOW_RESIZABLE);
    if (!window) return NULL;

    if (config) {
        if (config->monitor > 0) {
            SDL_Rect bounds;
            if (host_display_get_monitor_bounds(config->monitor, &bounds)) {
                int pos_x = bounds.x + (bounds.w - win_w) / 2;
                int pos_y = bounds.y + (bounds.h - win_h) / 2;
                SDL_SetWindowPosition(window, pos_x, pos_y);
            }
        }
        host_display_apply_mode(window, config);
    }
    return window;
}

/* Toggle between fullscreen/borderless and windowed mode */
bool host_display_toggle_fullscreen(SDL_Window *window, DisplayConfig *config) {
    if (!window) return false;
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    bool is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if (!is_fullscreen && config && (config->window_mode == WINDOW_MODE_FULLSCREEN || config->window_mode == WINDOW_MODE_BORDERLESS)) {
        is_fullscreen = true;
    }

    if (is_fullscreen) {
        if (config) config->window_mode = WINDOW_MODE_WINDOWED;
        DisplayConfig local_cfg;
        if (config) {
            local_cfg = *config;
        } else {
            memset(&local_cfg, 0, sizeof(local_cfg));
            local_cfg.window_width = 640;
            local_cfg.window_height = 480;
        }
        local_cfg.window_mode = WINDOW_MODE_WINDOWED;
        return host_display_apply_mode(window, &local_cfg);
    } else {
        WindowMode target = (config && config->window_mode == WINDOW_MODE_FULLSCREEN)
                            ? WINDOW_MODE_FULLSCREEN : WINDOW_MODE_BORDERLESS;
        if (config) config->window_mode = target;
        DisplayConfig local_cfg;
        if (config) {
            local_cfg = *config;
        } else {
            memset(&local_cfg, 0, sizeof(local_cfg));
        }
        local_cfg.window_mode = target;
        return host_display_apply_mode(window, &local_cfg);
    }
}

/* Keep PS1 pixels in VRAM; SDL only presents the selected display rectangle. */
bool host_present_frame(const Ps1Video *video, SDL_Renderer *renderer, SDL_Texture **texture) {
    if (!video || !renderer || !texture || !*texture) return false;
    void *pixels;
    int pitch;
    bool copied;
    SdkDisplay display = sdk_gpu_display();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (!display.enabled || display.x < 0 || display.y < 0
        || display.width <= 0 || display.height <= 0
        || display.width > PS1_VRAM_WIDTH - display.x
        || display.height > PS1_VRAM_HEIGHT - display.y) {
        return SDL_RenderClear(renderer) && SDL_RenderPresent(renderer);
    }
    float width, height;
    if (!SDL_GetTextureSize(*texture, &width, &height)) return false;

    int win_w = 0, win_h = 0;
    float window_aspect = 4.0f / 3.0f;
    if (SDL_GetRenderOutputSize(renderer, &win_w, &win_h) && win_h > 0) {
        window_aspect = (float)win_w / (float)win_h;
    }

    WidescreenMode effective_mode = s_widescreen_mode;
    if (effective_mode == WIDESCREEN_AUTO) {
        effective_mode = (window_aspect > 1.34f) ? WIDESCREEN_ON : WIDESCREEN_OFF;
    }

    SDL_RendererLogicalPresentation pres_mode = SDL_LOGICAL_PRESENTATION_LETTERBOX;
    if (effective_mode == WIDESCREEN_ON) {
        float fov_scale = window_aspect / (4.0f / 3.0f);
        if (fov_scale < 1.0f) fov_scale = 1.0f;
        ps1_gte_set_widescreen_scale(fov_scale);
        pres_mode = SDL_LOGICAL_PRESENTATION_STRETCH;
    } else if (effective_mode == WIDESCREEN_STRETCH) {
        ps1_gte_set_widescreen_scale(1.0f);
        pres_mode = SDL_LOGICAL_PRESENTATION_STRETCH;
    } else {
        ps1_gte_set_widescreen_scale(1.0f);
        pres_mode = SDL_LOGICAL_PRESENTATION_LETTERBOX;
    }

    if (width != display.width || height != display.height) {
        SDL_Texture *replacement = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, display.width, display.height);
        if (!replacement) return false;
        if (!SDL_SetTextureScaleMode(replacement, SDL_SCALEMODE_NEAREST)
            || !SDL_SetRenderLogicalPresentation(renderer, display.width, display.height, pres_mode)) {
            SDL_DestroyTexture(replacement);
            return false;
        }
        SDL_DestroyTexture(*texture);
        *texture = replacement;
    } else {
        SDL_SetRenderLogicalPresentation(renderer, display.width, display.height, pres_mode);
    }

    if (!SDL_LockTexture(*texture, NULL, &pixels, &pitch)) {
        return false;
    }
    if (pitch <= 0 || (pitch % (int)sizeof(uint32_t)) != 0) {
        SDL_UnlockTexture(*texture);
        return false;
    }
    copied = ps1_video_copy_argb(video, display.x, display.y, display.width, display.height, pixels,
                                 (size_t)pitch / sizeof(uint32_t));
    SDL_UnlockTexture(*texture);
    return copied && SDL_RenderClear(renderer)
        && SDL_RenderTexture(renderer, *texture, NULL, NULL)
        && SDL_RenderPresent(renderer);
}
