#include "platform/graphics_options.h"
#include "platform/settings_json.h"
#include "platform/user_data.h"

void hm_graphics_defaults(hm_graphics_options *o) { *o = (hm_graphics_options){0, 0, 640, 480, 0}; }
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
        if (candidate.monitor >= 0 && candidate.monitor < 64 && candidate.mode >= 0 &&
            candidate.mode <= 2 && candidate.width >= 320 && candidate.width <= 16384 &&
            candidate.height >= 240 && candidate.height <= 16384 && candidate.linear >= 0 &&
            candidate.linear <= 1)
            *o = candidate;
    }
    SDL_free(text);
}
int hm_graphics_save(const hm_graphics_options *o) {
    char json[256];
    SDL_snprintf(json, sizeof(json),
                 "{\"monitor\":%d,\"window_mode\":%d,\"width\":%d,\"height\":%d,\"bilinear\":%d}",
                 o->monitor, o->mode, o->width, o->height, o->linear);
    return hm_user_data_set_setting("graphics", json);
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
int hm_graphics_change(SDL_Window *window, hm_graphics_options *o, int item, int direction) {
    hm_graphics_options candidate = *o;
    int step = direction < 0 ? -1 : 1;
    if (item == 6) {
        o->linear = !o->linear;
        return 1;
    }
    if (item == 0) {
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
    } else
        return 0; /* Renderer enhancements remain unavailable until implemented. */
    if (!hm_graphics_apply(window, &candidate)) {
        hm_graphics_apply(window, o);
        return 0;
    }
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
    } else if (item == 6)
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
    else if (item == 6)
        SDL_snprintf(text, capacity, "%s", o->linear ? "Bilinear" : "Nearest");
    else
        SDL_snprintf(text, capacity, "Off");
}
