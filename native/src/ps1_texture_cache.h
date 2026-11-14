#ifndef VANILLABTN_PS1_TEXTURE_CACHE_H
#define VANILLABTN_PS1_TEXTURE_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum Ps1TextureCacheKind {
    PS1_CACHE_XBRZ = 1,
    PS1_CACHE_REPLACEMENT = 2,
    PS1_CACHE_REPLACEMENT_MISS = 3
} Ps1TextureCacheKind;

typedef struct Ps1CachedTexture {
    uint64_t content_key;
    uint64_t last_use;
    Ps1TextureCacheKind kind;
    int scale;
    int width, height;
    size_t byte_size;
    uint32_t *pixels;
    struct Ps1CachedTexture *next;
} Ps1CachedTexture;

typedef struct Ps1TextureCache {
    Ps1CachedTexture *entries;
    size_t bytes, byte_limit;
    uint64_t use_clock;
} Ps1TextureCache;

uint64_t ps1_texture_content_key(const uint32_t *argb, int width, int height,
                                 size_t stride);

/* Clean-room scale-by-rules implementation compatible with xBRZ's defining
 * behavior: classify diagonal corners, preserve isolated details and blend
 * only across detected edges. It does not contain upstream GPL source. */
bool ps1_texture_scale_xbrz(const uint32_t *source, int width, int height,
                            size_t source_stride, int factor,
                            uint32_t *target, size_t target_stride);

void ps1_texture_cache_init(Ps1TextureCache *cache, size_t byte_limit);
void ps1_texture_cache_destroy(Ps1TextureCache *cache);
const Ps1CachedTexture *ps1_texture_cache_find(Ps1TextureCache *cache,
    uint64_t content_key, Ps1TextureCacheKind kind, int scale);
bool ps1_texture_cache_store(Ps1TextureCache *cache, uint64_t content_key,
    Ps1TextureCacheKind kind, int scale, const uint32_t *pixels,
    int width, int height);

#endif
