#ifndef HMBTN_PLATFORM_GRAPHICS_OPTIONS_H
#define HMBTN_PLATFORM_GRAPHICS_OPTIONS_H
#include <SDL3/SDL.h>
typedef struct {
    int monitor, mode, width, height, linear;
} hm_graphics_options;
void hm_graphics_defaults(hm_graphics_options *options);
void hm_graphics_load(hm_graphics_options *options);
int hm_graphics_save(const hm_graphics_options *options);
int hm_pause_config_enabled(void);
int hm_graphics_apply(SDL_Window *window, const hm_graphics_options *options);
int hm_graphics_change(SDL_Window *window, hm_graphics_options *options, int item, int direction);
void hm_graphics_value(const hm_graphics_options *options, int item, char *text, size_t capacity);
void hm_graphics_row_value(const hm_graphics_options *options, int item, char *text,
                           size_t capacity);
#endif
