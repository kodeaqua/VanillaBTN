#include "host_display.h"
#include "sdk_gpu.h"
#include "ps1_gte.h"
#include <string.h>

static WidescreenMode s_widescreen_mode = WIDESCREEN_AUTO;
static bool s_integer_scaling;
/* The stepped PS1 screen fade uses a temporary 640x480 display mode. Keep
 * Hor+ projection for the next 320x240 scene frame while that mode is active;
 * otherwise one frame is projected at 4:3 before the host restores Hor+. */
static float s_last_world_fov_scale = 1.0f;
/* The original screen_fade_step() temporarily switches the PS1 display to
 * 640x480 and draws a fullscreen quad over the already-rendered VRAM pages.
 * The quad is a transition effect, not a new camera. Retain the actual last
 * 320x240 display page to present that same world panorama through the fade. */
static SdkDisplay s_last_game_display;
static bool s_fade_uses_panorama;

void host_display_set_integer_scaling(bool enabled) {
    s_integer_scaling = enabled;
}

void host_display_set_widescreen_mode(WidescreenMode mode) {
    s_widescreen_mode = mode;
}

WidescreenMode host_display_get_widescreen_mode(void) {
    return s_widescreen_mode;
}

/* The viewport width follows the current renderer output, not a hard-coded
 * 16:9 preset. World XY projection retains original 4:3 pixel coordinates;
 * only the frustum and panoramic clipping extend left/right. UI stays at the
 * original 320x240 centered safe area, with zero horizontal resampling. */
bool host_display_update_widescreen(SDL_Renderer *renderer) {
    if (!renderer) return false;

    SdkDisplay display = sdk_gpu_display();
    if (!display.enabled || display.x < 0 || display.y < 0
        || display.width <= 0 || display.height <= 0
        || display.width > PS1_VRAM_WIDTH - display.x
        || display.height > PS1_VRAM_HEIGHT - display.y) {
        s_last_world_fov_scale = 1.0f;
        s_fade_uses_panorama = false;
        s_last_game_display = (SdkDisplay){0};
        ps1_gte_set_widescreen_scale(1.0f);
        ps1_video_set_widescreen_scale(1.0f, 160.0f);
        return ps1_video_configure_panorama(0, 0, 1.0f);
    }

    int output_width = 0;
    int output_height = 0;
    float output_aspect = 4.0f / 3.0f;
    if (SDL_GetRenderOutputSize(renderer, &output_width, &output_height)
        && output_width > 0 && output_height > 0) {
        output_aspect = (float)output_width / (float)output_height;
    }

    WidescreenMode effective_mode = s_widescreen_mode;
    if (effective_mode == WIDESCREEN_AUTO) {
        effective_mode = output_aspect > (4.0f / 3.0f) + 0.01f
            ? WIDESCREEN_ON : WIDESCREEN_OFF;
    }

    float fov_scale = 1.0f;
    SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_LETTERBOX;
    if (effective_mode == WIDESCREEN_STRETCH) {
        /* Explicit legacy anamorphic mode, not true widescreen. */
        presentation = SDL_LOGICAL_PRESENTATION_STRETCH;
    } else if (effective_mode == WIDESCREEN_ON && output_aspect > 4.0f / 3.0f
               && display.width == 320 && display.height == 240) {
        fov_scale = output_aspect / (4.0f / 3.0f);
        if (fov_scale > 4.0f) fov_scale = 4.0f;
        /* The logical canvas itself expands to the real aspect. The final
         * sub-pixel stretch compensates only for integer width rounding:
         * e.g. 427:240 vs an actual 16:9 window, never 320:240 to 16:9. */
        presentation = SDL_LOGICAL_PRESENTATION_STRETCH;
    }

    /* Anamorphic stretch is explicit. Otherwise honor integer scaling
     * without changing the camera or the original 4:3 HUD coordinates. */
    if (s_integer_scaling && effective_mode != WIDESCREEN_STRETCH) {
        presentation = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
    }

    /* screen_fade_step() temporarily selects a 640x480 VRAM rectangle for a
     * fullscreen effect. Keep the prior panoramic camera and display page
     * throughout that effect; this is not a new 4:3 world camera. */
    const bool temporary_fade_mode = display.width == 640 && display.height == 480;
    s_fade_uses_panorama = temporary_fade_mode && effective_mode == WIDESCREEN_ON
        && s_last_game_display.enabled && s_last_world_fov_scale > 1.00001f;
    if (s_fade_uses_panorama) {
        /* The 640x480 image is an effect over two 320x240 VRAM pages. Show
         * the most recently displayed page at its unchanged panoramic width,
         * instead of letting SDL letterbox a different 4:3 logical canvas. */
        fov_scale = s_last_world_fov_scale;
        presentation = SDL_LOGICAL_PRESENTATION_STRETCH;
        if (s_integer_scaling) presentation = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
    } else {
        s_last_world_fov_scale = fov_scale;
        if (display.width == 320 && display.height == 240)
            s_last_game_display = display;
    }
    if (!ps1_video_configure_panorama(display.width, display.height, fov_scale)) {
        return false;
    }
    ps1_gte_set_widescreen_scale(fov_scale);
    /* The R2 inverse-transform compressed the 2D UI and made fonts blurry.
     * R3 displays its original coordinates 1:1 inside the centered safe area. */
    ps1_video_set_widescreen_scale(1.0f,
        (float)display.x + (float)display.width * 0.5f);
    const SdkDisplay presented = s_fade_uses_panorama ? s_last_game_display : display;
    int canvas_width = ps1_video_panorama_width(presented.width);
    int canvas_height = presented.height;
    int active_width = 0, active_height = 0;
    SDL_RendererLogicalPresentation active_presentation;
    /* Setting the logical viewport on every present is expensive on some
     * SDL renderer backends. Resize only when the game or output changes. */
    if (SDL_GetRenderLogicalPresentation(renderer, &active_width, &active_height,
                                         &active_presentation)
        && active_width == canvas_width && active_height == canvas_height
        && active_presentation == presentation) {
        return true;
    }
    return SDL_SetRenderLogicalPresentation(renderer,
        canvas_width, canvas_height, presentation);
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
        if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) &&
            !SDL_SetWindowFullscreen(window, false)) return false;
        if (SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) {
            if (!SDL_RestoreWindow(window)) return false;
        }
        int w = (config && config->window_width >= 320) ? config->window_width : 640;
        int h = (config && config->window_height >= 240) ? config->window_height : 480;
        return SDL_SetWindowSize(window, w, h);
    }
    case WINDOW_MODE_MAXIMIZED:
        return (!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
                || SDL_SetWindowFullscreen(window, false))
            && SDL_MaximizeWindow(window);

    case WINDOW_MODE_BORDERLESS:
        return SDL_SetWindowFullscreenMode(window, NULL)
            && SDL_SetWindowFullscreen(window, true);

    case WINDOW_MODE_FULLSCREEN: {
        SDL_DisplayID disp_id = host_display_get_monitor_id(config ? config->monitor : 0);
        SDL_DisplayMode closest;
        if (config && config->fullscreen_width > 0 && config->fullscreen_height > 0
            && SDL_GetClosestFullscreenDisplayMode(disp_id, config->fullscreen_width,
                                                   config->fullscreen_height,
                                                   (float)config->fullscreen_hz,
                                                   true, &closest)) {
            if (!SDL_SetWindowFullscreenMode(window, &closest)) return false;
        } else {
            if (!SDL_SetWindowFullscreenMode(window, NULL)) return false;
        }
        return SDL_SetWindowFullscreen(window, true);
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
        if (!host_display_apply_mode(window, config)) {
            SDL_DestroyWindow(window);
            return NULL;
        }
    }
    return window;
}

/* Toggle between fullscreen/borderless and windowed mode */
bool host_display_toggle_fullscreen(SDL_Window *window, DisplayConfig *config) {
    if (!window) return false;
    /* Actual SDL window flags are authoritative. The INI preference can be
     * stale after a failed mode change or a platform-initiated transition. */
    bool is_fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    DisplayConfig local_cfg;
    if (config) {
        local_cfg = *config;
    } else {
        memset(&local_cfg, 0, sizeof(local_cfg));
        local_cfg.window_width = 640;
        local_cfg.window_height = 480;
    }
    local_cfg.window_mode = is_fullscreen ? WINDOW_MODE_WINDOWED
        : (config && config->window_mode == WINDOW_MODE_FULLSCREEN
            ? WINDOW_MODE_FULLSCREEN : WINDOW_MODE_BORDERLESS);
    if (!host_display_apply_mode(window, &local_cfg)) return false;
    /* Commit configuration only after SDL accepted the transition. */
    if (config) config->window_mode = local_cfg.window_mode;
    return true;
}

/* Keep PS1 pixels in VRAM; SDL only presents the selected display rectangle. */
bool host_present_frame(const Ps1Video *video, SDL_Renderer *renderer, SDL_Texture **texture) {
    if (!video || !renderer || !texture || !*texture) return false;
    void *pixels;
    int pitch;
    bool copied;
    if (!host_display_update_widescreen(renderer)) return false;
    SdkDisplay display = sdk_gpu_display();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (!display.enabled || display.x < 0 || display.y < 0
        || display.width <= 0 || display.height <= 0
        || display.width > PS1_VRAM_WIDTH - display.x
        || display.height > PS1_VRAM_HEIGHT - display.y) {
        ps1_gte_set_widescreen_scale(1.0f);
        ps1_video_set_widescreen_scale(1.0f, 160.0f);
        return SDL_RenderClear(renderer) && SDL_RenderPresent(renderer);
    }
    float width, height;
    if (!SDL_GetTextureSize(*texture, &width, &height)) return false;

    const SdkDisplay presented = s_fade_uses_panorama ? s_last_game_display : display;
    int presentation_width = ps1_video_panorama_width(presented.width);
    if (width != presentation_width || height != presented.height) {
        SDL_Texture *replacement = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, presentation_width, presented.height);
        if (!replacement) return false;
        if (!SDL_SetTextureScaleMode(replacement, SDL_SCALEMODE_NEAREST)) {
            SDL_DestroyTexture(replacement);
            return false;
        }
        SDL_DestroyTexture(*texture);
        *texture = replacement;
    }

    if (!SDL_LockTexture(*texture, NULL, &pixels, &pitch)) {
        return false;
    }
    if (pitch <= 0 || (pitch % (int)sizeof(uint32_t)) != 0) {
        SDL_UnlockTexture(*texture);
        return false;
    }
    copied = ps1_video_copy_panorama(video, presented.x, presented.y, presented.width, presented.height,
                                     pixels, (size_t)pitch / sizeof(uint32_t));
    SDL_UnlockTexture(*texture);
    return copied && SDL_RenderClear(renderer)
        && SDL_RenderTexture(renderer, *texture, NULL, NULL)
        && SDL_RenderPresent(renderer);
}
