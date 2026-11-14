#ifndef VANILLABTN_HOST_RENDERER_H
#define VANILLABTN_HOST_RENDERER_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "config.h"

/* Query available render driver names in SDL3 */
int host_renderer_get_driver_count(void);
const char *host_renderer_get_driver_name(int index);
bool host_renderer_is_driver_available(const char *driver_name);

/* Map BackendType enum to driver string identifier */
const char *host_renderer_driver_for_backend(BackendType backend);

/* Create renderer with specified backend and automatic fallback chain:
 * 1. Requested backend
 * 2. System default (NULL)
 * 3. Direct3D 11 (Windows)
 * 4. OpenGL (Cross-platform)
 * 5. Software fallback */
SDL_Renderer *host_renderer_create(SDL_Window *window, BackendType backend, bool vsync);

/* Query the active backend driver name of an initialized renderer */
const char *host_renderer_get_active_name(SDL_Renderer *renderer);

#endif
