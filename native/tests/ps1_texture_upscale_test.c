#include "ps1_texture_cache.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); \
} } while (0)

static void test_content_key_is_address_independent(void) {
    const uint32_t a[4] = {0xFFFF0000u, 0, 0, 0xFFFF0000u};
    uint32_t b[4];
    memcpy(b, a, sizeof(a));
    uint64_t key = ps1_texture_content_key(a, 2, 2, 2);
    CHECK(key != 0);
    CHECK(key == ps1_texture_content_key(b, 2, 2, 2));
    b[3] = 0xFF00FF00u;
    CHECK(key != ps1_texture_content_key(b, 2, 2, 2));
    CHECK(key != ps1_texture_content_key(a, 1, 4, 1));
}

static void test_xbrz_reconstructs_diagonal_without_halo(void) {
    const uint32_t source[9] = {
        0xFFFFFFFFu, 0, 0,
        0, 0xFFFFFFFFu, 0,
        0, 0, 0xFFFFFFFFu
    };
    for (int factor = 2; factor <= 6; ++factor) {
        const int width = 3 * factor;
        uint32_t *scaled = calloc((size_t)width * width, sizeof(*scaled));
        CHECK(scaled != NULL);
        CHECK(ps1_texture_scale_xbrz(source, 3, 3, 3, factor,
                                     scaled, (size_t)width));
        bool differs_from_nearest = false;
        for (int y = 0; y < width; ++y) {
            for (int x = 0; x < width; ++x) {
                uint32_t pixel = scaled[(size_t)y * width + x];
                uint32_t nearest = source[(y / factor) * 3 + x / factor];
                differs_from_nearest |= pixel != nearest;
                /* Transparent neighbors must not contribute opaque black. */
                CHECK((pixel & 0xFF000000u) != 0 || (pixel & 0x00FFFFFFu) == 0);
            }
        }
        CHECK(differs_from_nearest);
        free(scaled);
    }
}

static void test_cache_is_bounded_and_reuses_hits(void) {
    Ps1TextureCache cache;
    ps1_texture_cache_init(&cache, 1024);
    uint32_t pixels[16];
    memset(pixels, 0xFF, sizeof(pixels));
    CHECK(ps1_texture_cache_store(&cache, 1, PS1_CACHE_XBRZ, 2,
                                  pixels, 4, 4));
    const Ps1CachedTexture *first = ps1_texture_cache_find(
        &cache, 1, PS1_CACHE_XBRZ, 2);
    CHECK(first != NULL);
    uint64_t stamp = first->last_use;
    CHECK(ps1_texture_cache_find(&cache, 1, PS1_CACHE_XBRZ, 2) == first);
    CHECK(first->last_use > stamp);

    for (uint64_t key = 2; key < 40; ++key) {
        CHECK(ps1_texture_cache_store(&cache, key, PS1_CACHE_XBRZ, 2,
                                      pixels, 4, 4));
        CHECK(cache.bytes <= cache.byte_limit);
    }
    CHECK(ps1_texture_cache_find(&cache, 1, PS1_CACHE_XBRZ, 2) == NULL);
    ps1_texture_cache_destroy(&cache);
}

int main(void) {
    test_content_key_is_address_independent();
    test_xbrz_reconstructs_diagonal_without_halo();
    test_cache_is_bounded_and_reuses_hits();
    puts("ps1_texture_upscale_test: passed");
    return 0;
}
