#include "host_texture_pack.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char s_pack_directory[260];

bool host_texture_pack_configure(const char *relative_directory) {
    if (!relative_directory || !relative_directory[0]
        || strlen(relative_directory) >= sizeof(s_pack_directory)) return false;
    SDL_strlcpy(s_pack_directory, relative_directory, sizeof(s_pack_directory));
    return true;
}

void host_texture_pack_disable(void) {
    s_pack_directory[0] = '\0';
}

static void release_pixels(const uint32_t *pixels, void *userdata) {
    (void)userdata;
    free((void *)pixels);
}

static SDL_Surface *load_surface(const char *filename) {
    char path[640];
    if (SDL_snprintf(path, sizeof(path), "%s\\%s",
            s_pack_directory, filename) <= 0) return NULL;
    SDL_Surface *surface = SDL_LoadBMP(path);
    if (surface) return surface;
    const char *base = SDL_GetBasePath();
    if (!base || SDL_snprintf(path, sizeof(path), "%s%s\\%s", base,
            s_pack_directory, filename) <= 0) return NULL;
    return SDL_LoadBMP(path);
}

bool host_texture_pack_lookup(uint64_t content_key, int source_width,
    int source_height, Ps1ReplacementImage *out_image, void *userdata) {
    (void)userdata;
    if (!s_pack_directory[0] || !out_image || source_width <= 0
        || source_height <= 0) return false;
    char filename[96];
    SDL_snprintf(filename, sizeof(filename), "%016llx_%dx%d.bmp",
        (unsigned long long)content_key, source_width, source_height);
    SDL_Surface *loaded = load_surface(filename);
    if (!loaded) return false;
    SDL_Surface *surface = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(loaded);
    if (!surface || surface->w <= 0 || surface->h <= 0
        || (size_t)surface->w > SIZE_MAX / (size_t)surface->h / sizeof(uint32_t)) {
        SDL_DestroySurface(surface);
        return false;
    }
    size_t count = (size_t)surface->w * (size_t)surface->h;
    uint32_t *pixels = (uint32_t *)malloc(count * sizeof(*pixels));
    if (!pixels) {
        SDL_DestroySurface(surface);
        return false;
    }
    for (int y = 0; y < surface->h; ++y) {
        memcpy(pixels + (size_t)y * surface->w,
            (const unsigned char *)surface->pixels + (size_t)y * surface->pitch,
            (size_t)surface->w * sizeof(*pixels));
    }
    *out_image = (Ps1ReplacementImage){pixels, surface->w, surface->h,
        (size_t)surface->w, release_pixels, NULL};
    SDL_DestroySurface(surface);
    return true;
}
