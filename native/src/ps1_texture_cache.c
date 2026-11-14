#include "ps1_texture_cache.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET UINT64_C(14695981039346656037)
#define FNV_PRIME UINT64_C(1099511628211)

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

uint64_t ps1_texture_content_key(const uint32_t *argb, int width, int height,
                                 size_t stride) {
    if (!argb || width <= 0 || height <= 0 || stride < (size_t)width) return 0;
    uint64_t hash = FNV_OFFSET;
    uint32_t dimensions[2] = {(uint32_t)width, (uint32_t)height};
    hash = hash_bytes(hash, dimensions, sizeof(dimensions));
    for (int y = 0; y < height; ++y) {
        hash = hash_bytes(hash, argb + (size_t)y * stride,
                          (size_t)width * sizeof(*argb));
    }
    return hash ? hash : 1;
}

static unsigned channel(uint32_t color, unsigned shift) {
    return (color >> shift) & 255u;
}

static unsigned color_distance(uint32_t a, uint32_t b) {
    int aa = (int)channel(a, 24), ba = (int)channel(b, 24);
    int ar = (int)channel(a, 16), br = (int)channel(b, 16);
    int ag = (int)channel(a, 8), bg = (int)channel(b, 8);
    int ab = (int)channel(a, 0), bb = (int)channel(b, 0);
    int alpha = aa - ba;
    if (aa == 0 || ba == 0) {
        /* Transparent RGB is undefined and must not create a black fringe. */
        if (aa == 0) ar = ag = ab = 0;
        if (ba == 0) br = bg = bb = 0;
    }
    int y = 77 * (ar - br) + 150 * (ag - bg) + 29 * (ab - bb);
    int cb = -43 * (ar - br) - 85 * (ag - bg) + 128 * (ab - bb);
    int cr = 128 * (ar - br) - 107 * (ag - bg) - 21 * (ab - bb);
    return (unsigned)(y * y / 256 + cb * cb / 512 + cr * cr / 512
        + alpha * alpha * 64);
}

static uint32_t alpha_blend(uint32_t base, uint32_t edge,
                            unsigned edge_weight, unsigned total_weight) {
    if (edge_weight == 0 || base == edge) return base;
    unsigned base_weight = total_weight - edge_weight;
    unsigned ba = channel(base, 24), ea = channel(edge, 24);
    unsigned out_a = (ba * base_weight + ea * edge_weight + total_weight / 2)
        / total_weight;
    if (out_a == 0) return 0;
    uint32_t out = out_a << 24;
    for (unsigned shift = 0; shift < 24; shift += 8) {
        unsigned premul = channel(base, shift) * ba * base_weight
            + channel(edge, shift) * ea * edge_weight;
        unsigned value = (premul + out_a * total_weight / 2)
            / (out_a * total_weight);
        if (value > 255u) value = 255u;
        out |= value << shift;
    }
    return out;
}

static uint32_t source_at(const uint32_t *source, int width, int height,
                          size_t stride, int x, int y) {
    if (x < 0) x = 0;
    if (x >= width) x = width - 1;
    if (y < 0) y = 0;
    if (y >= height) y = height - 1;
    return source[(size_t)y * stride + x];
}

typedef struct CornerRule {
    uint32_t color;
    bool blend;
} CornerRule;

static CornerRule classify_corner(uint32_t center, uint32_t side_a,
                                  uint32_t side_b, uint32_t diagonal,
                                  uint32_t opposite_a, uint32_t opposite_b) {
    CornerRule rule = {0, false};
    unsigned sides = color_distance(side_a, side_b);
    unsigned center_a = color_distance(center, side_a);
    unsigned center_b = color_distance(center, side_b);
    if (center_a == 0 && center_b == 0) return rule;

    /* The two orthogonal samples must describe a more coherent continuation
     * than the center/diagonal axis. This is the scale-by-rules decision that
     * keeps single-pixel details while smoothing staircase diagonals. */
    unsigned crossing = color_distance(center, diagonal)
        + color_distance(side_a, opposite_a)
        + color_distance(side_b, opposite_b);
    unsigned continuation = sides * 2u + center_a + center_b;
    if (sides <= (center_a + center_b) / 2u || continuation < crossing) {
        rule.color = center_a <= center_b ? side_a : side_b;
        if (side_a == side_b) rule.color = side_a;
        rule.blend = rule.color != center;
    }
    return rule;
}

static void blend_corner(uint32_t *block, size_t stride, int factor,
                         CornerRule rule, int rotation) {
    if (!rule.blend) return;
    for (int y = 0; y < factor; ++y) {
        for (int x = 0; x < factor; ++x) {
            int rx = x, ry = y;
            if (rotation == 1) { rx = factor - 1 - y; ry = x; }
            else if (rotation == 2) { rx = factor - 1 - x; ry = factor - 1 - y; }
            else if (rotation == 3) { rx = y; ry = factor - 1 - x; }
            int reach = factor - (x + y);
            if (reach <= 0) continue;
            /* Limit the foreign color to 75%; a detected line stays connected
             * without erasing the source pixel's identity. */
            unsigned weight = (unsigned)(reach * 3);
            unsigned total = (unsigned)(factor * 4);
            uint32_t *pixel = block + (size_t)ry * stride + rx;
            *pixel = alpha_blend(*pixel, rule.color, weight, total);
        }
    }
}

bool ps1_texture_scale_xbrz(const uint32_t *source, int width, int height,
                            size_t source_stride, int factor,
                            uint32_t *target, size_t target_stride) {
    if (!source || !target || width <= 0 || height <= 0
        || source_stride < (size_t)width || factor < 2 || factor > 6
        || width > INT_MAX / factor || height > INT_MAX / factor
        || target_stride < (size_t)width * factor) return false;

    for (int sy = 0; sy < height; ++sy) {
        for (int sx = 0; sx < width; ++sx) {
            uint32_t e = source_at(source, width, height, source_stride, sx, sy);
            uint32_t *block = target + (size_t)sy * factor * target_stride
                + (size_t)sx * factor;
            for (int y = 0; y < factor; ++y)
                for (int x = 0; x < factor; ++x)
                    block[(size_t)y * target_stride + x] = e;

            uint32_t a = source_at(source, width, height, source_stride, sx - 1, sy - 1);
            uint32_t b = source_at(source, width, height, source_stride, sx, sy - 1);
            uint32_t c = source_at(source, width, height, source_stride, sx + 1, sy - 1);
            uint32_t d = source_at(source, width, height, source_stride, sx - 1, sy);
            uint32_t f = source_at(source, width, height, source_stride, sx + 1, sy);
            uint32_t g = source_at(source, width, height, source_stride, sx - 1, sy + 1);
            uint32_t h = source_at(source, width, height, source_stride, sx, sy + 1);
            uint32_t i = source_at(source, width, height, source_stride, sx + 1, sy + 1);
            CornerRule tl = classify_corner(e, d, b, a, g, c);
            CornerRule tr = classify_corner(e, b, f, c, a, i);
            CornerRule br = classify_corner(e, f, h, i, c, g);
            CornerRule bl = classify_corner(e, h, d, g, i, a);
            blend_corner(block, target_stride, factor, tl, 0);
            blend_corner(block, target_stride, factor, tr, 1);
            blend_corner(block, target_stride, factor, br, 2);
            blend_corner(block, target_stride, factor, bl, 3);
        }
    }
    return true;
}

void ps1_texture_cache_init(Ps1TextureCache *cache, size_t byte_limit) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
    cache->byte_limit = byte_limit;
}

void ps1_texture_cache_destroy(Ps1TextureCache *cache) {
    if (!cache) return;
    Ps1CachedTexture *entry = cache->entries;
    while (entry) {
        Ps1CachedTexture *next = entry->next;
        free(entry->pixels);
        free(entry);
        entry = next;
    }
    memset(cache, 0, sizeof(*cache));
}

const Ps1CachedTexture *ps1_texture_cache_find(Ps1TextureCache *cache,
    uint64_t content_key, Ps1TextureCacheKind kind, int scale) {
    if (!cache) return NULL;
    for (Ps1CachedTexture *entry = cache->entries; entry; entry = entry->next) {
        if (entry->content_key == content_key && entry->kind == kind
            && entry->scale == scale) {
            entry->last_use = ++cache->use_clock;
            return entry;
        }
    }
    return NULL;
}

static void evict_oldest(Ps1TextureCache *cache) {
    Ps1CachedTexture **oldest_link = NULL;
    uint64_t oldest_use = UINT64_MAX;
    for (Ps1CachedTexture **link = &cache->entries; *link; link = &(*link)->next) {
        if ((*link)->last_use < oldest_use) {
            oldest_use = (*link)->last_use;
            oldest_link = link;
        }
    }
    if (!oldest_link) return;
    Ps1CachedTexture *oldest = *oldest_link;
    *oldest_link = oldest->next;
    cache->bytes -= oldest->byte_size;
    free(oldest->pixels);
    free(oldest);
}

bool ps1_texture_cache_store(Ps1TextureCache *cache, uint64_t content_key,
    Ps1TextureCacheKind kind, int scale, const uint32_t *pixels,
    int width, int height) {
    if (!cache || !pixels || !content_key || width <= 0 || height <= 0
        || (size_t)width > SIZE_MAX / (size_t)height / sizeof(*pixels)) return false;
    size_t byte_size = (size_t)width * (size_t)height * sizeof(*pixels);
    if (byte_size > cache->byte_limit) return false;
    while (cache->bytes > cache->byte_limit - byte_size) evict_oldest(cache);

    Ps1CachedTexture *entry = (Ps1CachedTexture *)calloc(1, sizeof(*entry));
    uint32_t *copy = (uint32_t *)malloc(byte_size);
    if (!entry || !copy) {
        free(entry);
        free(copy);
        return false;
    }
    memcpy(copy, pixels, byte_size);
    entry->content_key = content_key;
    entry->kind = kind;
    entry->scale = scale;
    entry->width = width;
    entry->height = height;
    entry->byte_size = byte_size;
    entry->pixels = copy;
    entry->last_use = ++cache->use_clock;
    entry->next = cache->entries;
    cache->entries = entry;
    cache->bytes += byte_size;
    return true;
}
