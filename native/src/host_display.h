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

/* Configure and query widescreen presentation mode */
void host_display_set_widescreen_mode(WidescreenMode mode);
/* CRT NTSC adjusts output pixel shape, not game-space coordinates. */
void host_display_set_pixel_aspect_ratio(PixelAspectRatio ratio);
/* Explicit integer scaling is opt-in; true Hor+ remains output-ratio aware. */
void host_display_set_integer_scaling(bool enabled);
/* 0 = match desktop height, 1..16 = requested 3D raster scale. */
void host_display_set_render_scale(int scale);
WidescreenMode host_display_get_widescreen_mode(void);
/* Apply GTE Hor+ and 2D safe-area transforms before the next game frame. */
bool host_display_update_widescreen(SDL_Renderer *renderer);

/* Present the SDK-selected VRAM rectangle, retaining original internal size.
 * Replaces the owned streaming texture when the game changes display size. */
bool host_present_frame(const Ps1Video *video, SDL_Renderer *renderer, SDL_Texture **texture);

#endif
