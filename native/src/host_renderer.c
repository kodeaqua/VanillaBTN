#include "host_renderer.h"
#include <string.h>

int host_renderer_get_driver_count(void) {
    int count = SDL_GetNumRenderDrivers();
    return count > 0 ? count : 0;
}

const char *host_renderer_get_driver_name(int index) {
    int count = host_renderer_get_driver_count();
    if (index < 0 || index >= count) return NULL;
    return SDL_GetRenderDriver(index);
}

bool host_renderer_is_driver_available(const char *driver_name) {
    if (!driver_name) return false;
    int count = host_renderer_get_driver_count();
    for (int i = 0; i < count; ++i) {
        const char *name = SDL_GetRenderDriver(i);
        if (name && SDL_strcasecmp(name, driver_name) == 0) {
            return true;
        }
    }
    return false;
}

const char *host_renderer_driver_for_backend(BackendType backend) {
    switch (backend) {
    case BACKEND_VULKAN:
        return "vulkan";
    case BACKEND_D3D11:
        return "direct3d11";
    case BACKEND_D3D12:
        return "direct3d12";
    case BACKEND_OPENGL:
        return "opengl";
    case BACKEND_SOFTWARE:
        return "software";
    case BACKEND_AUTO:
    default:
        return NULL;
    }
}

SDL_Renderer *host_renderer_create(SDL_Window *window, BackendType backend, bool vsync) {
    if (!window) return NULL;

    const char *primary = host_renderer_driver_for_backend(backend);
    const char *fallback_chain[5];
    int chain_count = 0;

    if (primary != NULL) {
        fallback_chain[chain_count++] = primary;
    }
    fallback_chain[chain_count++] = NULL;           /* SDL default best backend */
    fallback_chain[chain_count++] = "direct3d11";   /* D3D11 stable Windows fallback */
    fallback_chain[chain_count++] = "opengl";       /* OpenGL cross-platform fallback */
    fallback_chain[chain_count++] = "software";     /* Software CPU fallback */

    SDL_Renderer *renderer = NULL;
    for (int i = 0; i < chain_count; ++i) {
        const char *target = fallback_chain[i];
        bool duplicate = false;
        for (int j = 0; j < i; ++j) {
            if ((target == NULL && fallback_chain[j] == NULL) ||
                (target != NULL && fallback_chain[j] != NULL &&
                 SDL_strcasecmp(target, fallback_chain[j]) == 0)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        renderer = SDL_CreateRenderer(window, target);
        if (renderer != NULL) {
            break;
        }
    }

    if (renderer) {
        SDL_SetRenderLogicalPresentation(renderer, 320, 240, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderVSync(renderer, vsync ? 1 : 0);
    }
    return renderer;
}

const char *host_renderer_get_active_name(SDL_Renderer *renderer) {
    if (!renderer) return NULL;
    return SDL_GetRendererName(renderer);
}
