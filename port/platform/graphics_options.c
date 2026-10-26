#include "platform/graphics_options.h"
#include "platform/settings_json.h"
#include "platform/user_data.h"
#ifdef HMBTN_ENH_INTERPOLATION
#include "enhance/interpolation/interpolation.h"
#endif
static void hm_validate_rate(hm_graphics_options *options);

void hm_graphics_defaults(hm_graphics_options *o) {
    *o = (hm_graphics_options){.width = 640, .height = 480, .vsync = 1, .fps_denominator = 1};
}
void hm_graphics_load(hm_graphics_options *o) {
    hm_graphics_defaults(o);
    char *text = SDL_malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object;
    size_t length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "graphics", &object, &length) == 1) {
        hm_graphics_options candidate = *o;
        hm_json_integer(object, length, "monitor", &candidate.monitor);
        hm_json_integer(object, length, "window_mode", &candidate.mode);
        hm_json_integer(object, length, "width", &candidate.width);
        hm_json_integer(object, length, "height", &candidate.height);
        hm_json_integer(object, length, "bilinear", &candidate.linear);
        hm_json_integer(object, length, "vsync", &candidate.vsync);
        hm_json_integer(object, length, "fps_numerator", &candidate.fps_numerator);
        hm_json_integer(object, length, "fps_denominator", &candidate.fps_denominator);
        if (candidate.monitor >= 0 && candidate.monitor < 64 && candidate.mode >= 0 &&
            candidate.mode <= 2 && candidate.width >= 320 && candidate.width <= 16384 &&
            candidate.height >= 240 && candidate.height <= 16384 && candidate.linear >= 0 &&
            candidate.linear <= 1 && candidate.vsync >= 0 && candidate.vsync <= 1 &&
            candidate.fps_numerator >= -1 && candidate.fps_denominator > 0 &&
            candidate.fps_denominator <= 1000000 &&
            (candidate.fps_numerator <= 0 ||
             (candidate.fps_numerator >= candidate.fps_denominator &&
              (u64)candidate.fps_numerator <= (u64)candidate.fps_denominator * 1000)))
            *o = candidate;
    }
#ifdef HMBTN_ENH_INTERPOLATION
    const char *value;
    size_t value_length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1 &&
        hm_json_property(object, length, "interpolation", &value, &value_length) == 1 &&
        value_length == 4 && !SDL_memcmp(value, "true", 4))
        o->interpolation = 1;
#endif
    SDL_free(text);
    hm_validate_rate(o);
}
int hm_graphics_save(const hm_graphics_options *o) {
    char json[256];
    SDL_snprintf(json, sizeof(json),
                 "{\"monitor\":%d,\"window_mode\":%d,\"width\":%d,\"height\":%d,\"bilinear\":%d,"
                 "\"vsync\":%d,\"fps_numerator\":%d,\"fps_denominator\":%d}",
                 o->monitor, o->mode, o->width, o->height, o->linear, o->vsync, o->fps_numerator,
                 o->fps_denominator);
    return hm_user_data_set_setting("graphics", json) &&
           hm_user_data_set_nested_setting("enhance", "interpolation",
                                           o->interpolation ? "true" : "false");
}
int hm_pause_config_enabled(void) {
    int enabled = 1; /* The owner requests ESC throughout the native game. */
    char *text = SDL_malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object, *value;
    size_t length, value_length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1 &&
        hm_json_property(object, length, "pause_menu", &value, &value_length) == 1) {
        if (value_length == 5 && !SDL_memcmp(value, "false", 5))
            enabled = 0;
    }
    SDL_free(text);
    return enabled;
}
static SDL_DisplayID hm_display(int ordinal) {
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    SDL_DisplayID result =
        displays && count ? displays[ordinal >= 0 && ordinal < count ? ordinal : 0] : 0;
    SDL_free(displays);
    return result;
}
int hm_graphics_apply(SDL_Window *window, const hm_graphics_options *o) {
    SDL_DisplayID display = hm_display(o->monitor);
    if (!display || o->mode < 0 || o->mode > 2 || o->width < 320 || o->height < 240)
        return 0;
    if (!SDL_SetWindowFullscreen(window, false) || !SDL_SetWindowFullscreenMode(window, NULL) ||
        !SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                               SDL_WINDOWPOS_CENTERED_DISPLAY(display)))
        return 0;
    if (o->mode == 0)
        return SDL_SetWindowSize(window, o->width, o->height) &&
               SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                                     SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    if (o->mode == 2)
        return SDL_SetWindowFullscreen(window, true);
    int count;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display, &count);
    int ok = 0;
    for (int i = 0; modes && i < count; ++i) {
        if (modes[i]->w == o->width && modes[i]->h == o->height) {
            ok = SDL_SetWindowFullscreenMode(window, modes[i]) &&
                 SDL_SetWindowFullscreen(window, true);
            break;
        }
    }
    SDL_free(modes);
    return ok;
}
hm_present_rate hm_graphics_present_rate(const hm_graphics_options *o) {
    if (o->fps_numerator == -1)
        return (hm_present_rate){0, 1};
    return o->fps_numerator ? (hm_present_rate){(u32)o->fps_numerator, (u32)o->fps_denominator}
                            : (hm_present_rate){30, 1};
}
int hm_graphics_swapchain(SDL_GPUDevice *device, SDL_Window *window, const hm_graphics_options *o) {
    SDL_GPUPresentMode mode = o->vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
    if (o->vsync && SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX))
        mode = SDL_GPU_PRESENTMODE_MAILBOX;
    return SDL_WindowSupportsGPUPresentMode(device, window, mode) &&
           SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
}
static int hm_refresh_rate(const SDL_DisplayMode *mode, hm_present_rate *rate) {
    if (!mode)
        return 0;
    u32 n, d;
    if (mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0) {
        n = (u32)mode->refresh_rate_numerator;
        d = (u32)mode->refresh_rate_denominator;
    } else {
        if (!(mode->refresh_rate > 0 && mode->refresh_rate <= 1000))
            return 0;
        n = (u32)(mode->refresh_rate * 1000.0f + 0.5f);
        d = 1000;
    }
    u32 a = n, b = d;
    while (b) {
        u32 rest = a % b;
        a = b;
        b = rest;
    }
    n /= a;
    d /= a;
    if (!d || d > 1000000 || n < d || (u64)n > (u64)d * 1000)
        return 0;
    *rate = (hm_present_rate){n, d};
    return 1;
}
hm_present_rate hm_graphics_effective_rate(SDL_Window *window, const hm_graphics_options *o) {
    hm_present_rate wanted = hm_graphics_present_rate(o), actual;
    if (o->vsync &&
        hm_refresh_rate(SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window)), &actual) &&
        (!wanted.numerator ||
         (u64)actual.numerator * wanted.denominator < (u64)wanted.numerator * actual.denominator))
        return actual;
    return wanted;
}
void hm_graphics_sync_monitor(SDL_Window *window, hm_graphics_options *o) {
    SDL_DisplayID current = SDL_GetDisplayForWindow(window);
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    for (int i = 0; displays && i < count; ++i)
        if (displays[i] == current) {
            o->monitor = i;
            break;
        }
    SDL_free(displays);
    hm_validate_rate(o);
}
static int hm_rates(const hm_graphics_options *o, hm_present_rate rates[256]) {
    SDL_DisplayID display = hm_display(o->monitor);
    const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(display);
    int width = o->mode == 1 || !desktop ? o->width : desktop->w;
    int height = o->mode == 1 || !desktop ? o->height : desktop->h;
    rates[0] = (hm_present_rate){30, 1};
    rates[1] = (hm_present_rate){0, 1}; /* Off (Unlimited). */
    int used = 2, count = 0;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display, &count);
    for (int i = 0; modes && i < count && used < 256; ++i) {
        hm_present_rate rate;
        if (modes[i]->w != width || modes[i]->h != height || !hm_refresh_rate(modes[i], &rate))
            continue;
        int duplicate = 0;
        for (int j = 0; j < used; ++j)
            if (hm_present_rate_equal(rate, rates[j]))
                duplicate = 1;
        if (!duplicate)
            rates[used++] = rate;
    }
    SDL_free(modes);
    for (int i = 3; i < used; ++i) {
        hm_present_rate rate = rates[i];
        int j = i;
        while (j > 2 && (u64)rate.numerator * rates[j - 1].denominator <
                            (u64)rates[j - 1].numerator * rate.denominator) {
            rates[j] = rates[j - 1];
            --j;
        }
        rates[j] = rate;
    }
    return used;
}
static int hm_cycle_rate(hm_graphics_options *o, int step) {
    hm_present_rate rates[256];
    int count = hm_rates(o, rates), current = 0;
    if (o->fps_numerator)
        for (int i = 1; i < count; ++i)
            if (hm_present_rate_equal(hm_graphics_present_rate(o), rates[i]))
                current = i;
    int next = (current + count + step) % count;
    o->fps_numerator = next == 1 ? -1 : next ? (int)rates[next].numerator : 0;
    o->fps_denominator = next ? (int)rates[next].denominator : 1;
    return count > 1;
}
static void hm_validate_rate(hm_graphics_options *o) {
    if (o->fps_numerator <= 0) {
        o->fps_denominator = 1;
        return;
    }
    hm_present_rate rates[256];
    int count = hm_rates(o, rates);
    for (int i = 1; i < count; ++i)
        if (hm_present_rate_equal(hm_graphics_present_rate(o), rates[i]))
            return;
    o->fps_numerator = 0;
    o->fps_denominator = 1;
}
int hm_graphics_change(SDL_GPUDevice *device, SDL_Window *window, hm_graphics_options *o, int item,
                       int direction) {
    hm_graphics_options candidate = *o;
    int step = direction < 0 ? -1 : 1;
    if (item == 6) {
        o->linear = !o->linear;
        return 1;
    }
    if (item == 7)
        candidate.vsync = !candidate.vsync;
    else if (item == 8) {
        if (!hm_cycle_rate(&candidate, step))
            return 0;
    } else if (item == 0) {
        int count;
        SDL_DisplayID *displays = SDL_GetDisplays(&count);
        if (!displays || !count) {
            SDL_free(displays);
            return 0;
        }
        candidate.monitor = (o->monitor + count + step) % count;
        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(displays[candidate.monitor]);
        if (desktop && candidate.mode == 1) {
            candidate.width = desktop->w;
            candidate.height = desktop->h;
        }
        SDL_free(displays);
    } else if (item == 1) {
        candidate.mode = (candidate.mode + 3 + step) % 3;
        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(hm_display(candidate.monitor));
        if (desktop && candidate.mode == 1) {
            candidate.width = desktop->w;
            candidate.height = desktop->h;
        }
    } else if (item == 2) {
        if (candidate.mode == 2)
            return 0; /* Borderless follows the desktop. */
        int count;
        SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(hm_display(o->monitor), &count);
        int current = -1, unique = 0, widths[256], heights[256];
        for (int i = 0; modes && i < count && unique < 256; ++i) {
            int w = modes[i]->w, h = modes[i]->h, duplicate = 0;
            if (w < 320 || h < 240)
                continue;
            for (int j = 0; j < unique; ++j)
                if (widths[j] == w && heights[j] == h)
                    duplicate = 1;
            if (duplicate)
                continue;
            widths[unique] = w;
            heights[unique] = h;
            if (w == o->width && h == o->height)
                current = unique;
            ++unique;
        }
        SDL_free(modes);
        if (!unique)
            return 0;
        current = current < 0 ? (step < 0 ? unique - 1 : 0) : (current + unique + step) % unique;
        candidate.width = widths[current];
        candidate.height = heights[current];
    }
#ifdef HMBTN_ENH_INTERPOLATION
    else if (item == 9) {
        candidate.interpolation = !candidate.interpolation;
        if (!hm_interpolation_enable(candidate.interpolation))
            return 0;
    }
#endif
    else
        return 0; /* Renderer enhancements remain unavailable until implemented. */
    int window_changed = item >= 0 && item <= 2;
    if (window_changed && !hm_graphics_apply(window, &candidate)) {
        hm_graphics_apply(window, o);
        return 0;
    }
    if ((window_changed || candidate.vsync != o->vsync) &&
        !hm_graphics_swapchain(device, window, &candidate)) {
        if (window_changed)
            hm_graphics_apply(window, o);
        hm_graphics_swapchain(device, window, o);
        return 0;
    }
    if (window_changed)
        hm_validate_rate(&candidate);
    *o = candidate;
    return 1;
}
void hm_graphics_value(const hm_graphics_options *o, int item, char *text, size_t capacity) {
    static const char *const modes[] = {"Windowed", "Fullscreen", "Borderless Fullscreen"};
    const char *monitor_name = SDL_GetDisplayName(hm_display(o->monitor));
    if (item == 0)
        SDL_snprintf(text, capacity, "Monitor %d - %s", o->monitor + 1,
                     monitor_name ? monitor_name : "Unavailable");
    else if (item == 1)
        SDL_snprintf(text, capacity, "%s", modes[o->mode]);
    else if (item == 2) {
        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(hm_display(o->monitor));
        SDL_snprintf(text, capacity, "%d x %d", o->mode == 2 && desktop ? desktop->w : o->width,
                     o->mode == 2 && desktop ? desktop->h : o->height);
    } else if (item == 7)
        SDL_snprintf(text, capacity, "%s",
                     o->vsync ? "On - synchronized display" : "Off - tearing possible");
    else if (item == 8) {
        if (o->fps_numerator == -1)
            SDL_snprintf(text, capacity, "Off - Unlimited FPS");
        else if (!o->fps_numerator)
            SDL_snprintf(text, capacity, "Native - 30 FPS");
        else {
            u64 scaled = ((u64)o->fps_numerator * 100 + (u32)o->fps_denominator / 2) /
                         (u32)o->fps_denominator;
            if (scaled % 100)
                SDL_snprintf(text, capacity, "%u.%02u FPS limit", (u32)(scaled / 100),
                             (u32)(scaled % 100));
            else
                SDL_snprintf(text, capacity, "%u FPS limit", (u32)(scaled / 100));
        }
    } else if (item == 9)
        SDL_snprintf(text, capacity, "%s", o->interpolation ? "On - world motion" : "Off");
    else if (item == 6)
        SDL_snprintf(text, capacity, "%s", o->linear ? "Bilinear" : "Nearest Neighbor");
    else
        SDL_snprintf(text, capacity, "Off - not available yet");
}
void hm_graphics_row_value(const hm_graphics_options *o, int item, char *text, size_t capacity) {
    if (item == 0)
        SDL_snprintf(text, capacity, "%d", o->monitor + 1);
    else if (item == 1) {
        static const char *const names[] = {"Windowed", "Fullscreen", "Borderless"};
        SDL_snprintf(text, capacity, "%s", names[o->mode]);
    } else if (item == 2)
        hm_graphics_value(o, item, text, capacity);
    else if (item == 7)
        SDL_snprintf(text, capacity, "%s", o->vsync ? "On" : "Off");
    else if (item == 8) {
        if (o->fps_numerator == -1)
            SDL_snprintf(text, capacity, "Off");
        else if (!o->fps_numerator)
            SDL_snprintf(text, capacity, "Native (30)");
        else {
            u64 scaled = ((u64)o->fps_numerator * 100 + (u32)o->fps_denominator / 2) /
                         (u32)o->fps_denominator;
            if (scaled % 100)
                SDL_snprintf(text, capacity, "%u.%02u", (u32)(scaled / 100), (u32)(scaled % 100));
            else
                SDL_snprintf(text, capacity, "%u", (u32)(scaled / 100));
        }
    } else if (item == 9)
        SDL_snprintf(text, capacity, "%s", o->interpolation ? "On" : "Off");
    else if (item == 6)
        SDL_snprintf(text, capacity, "%s", o->linear ? "Bilinear" : "Nearest");
    else
        SDL_snprintf(text, capacity, "Off");
}
