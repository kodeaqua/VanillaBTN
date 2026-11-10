#ifndef VANILLABTN_HOST_DISPLAY_H
#define VANILLABTN_HOST_DISPLAY_H

#include "ps1_video.h"
#include "config.h"
#include <SDL3/SDL.h>

/* Query connected displays count and IDs */
int host_display_get_monitor_count(void);
SDL_DisplayID host_display_get_monitor_id(int monitor_index);
bool host_display_get_monitor_bounds(int monitor_index, SDL_Rect *bounds);

/* Create host window configured according to DisplayConfig */
SDL_Window *host_display_create_window(const DisplayConfig *config);

/* Apply or change window mode and monitor position at runtime */
bool host_display_apply_mode(SDL_Window *window, const DisplayConfig *config);

/* Toggle between fullscreen/borderless and windowed mode */
bool host_display_toggle_fullscreen(SDL_Window *window, DisplayConfig *config);

/* Present the SDK-selected VRAM rectangle, retaining original internal size.
 * Replaces the owned streaming texture when the game changes display size. */
bool host_present_frame(const Ps1Video *video, SDL_Renderer *renderer, SDL_Texture **texture);

#endif
