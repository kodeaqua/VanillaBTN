#include "ps1_pgxp.h"

#include <string.h>

#define PGXP_CACHE_CAPACITY 65536u
#define PGXP_CACHE_MASK     (PGXP_CACHE_CAPACITY - 1)

typedef struct PgxpEntry {
    uintptr_t addr;
    Ps1PgxpVertex vertex;
} PgxpEntry;

static PgxpEntry s_cache[PGXP_CACHE_CAPACITY];
static bool s_vertex_precision = true;
static bool s_perspective_texture = true;

static inline size_t pgxp_hash(uintptr_t addr) {
    /* Murmur/Fibonacci 64-bit mixer for 4-byte aligned pointers. */
    uint64_t v = (uint64_t)addr >> 2;
    v ^= v >> 16;
    v *= 0x45d9f3b;
    v ^= v >> 16;
    return (size_t)(v & PGXP_CACHE_MASK);
}

void ps1_pgxp_init(void) {
    ps1_pgxp_clear();
    s_vertex_precision = true;
    s_perspective_texture = true;
}

void ps1_pgxp_clear(void) {
    memset(s_cache, 0, sizeof(s_cache));
}

void ps1_pgxp_set_enabled(bool vertex_precision, bool perspective_texture) {
    if (s_vertex_precision != vertex_precision) {
        ps1_pgxp_clear();
    }
    s_vertex_precision = vertex_precision;
    s_perspective_texture = perspective_texture;
    if (!vertex_precision && !perspective_texture) {
        ps1_pgxp_clear();
    }
}

bool ps1_pgxp_is_vertex_precision_enabled(void) {
    return s_vertex_precision;
}

bool ps1_pgxp_is_perspective_texture_enabled(void) {
    return s_perspective_texture;
}

void ps1_pgxp_register(const void *dest_addr, float x, float y, float z, float w, uint32_t raw_sxy) {
    if (dest_addr == NULL) return;

    const bool has_subpixel = s_vertex_precision;
    if (!has_subpixel) {
        /* Preserve provenance even without PGXP so 3D packets are not mistaken
         * for 2D HUD geometry by widescreen presentation transforms. */
        x = (float)(int16_t)(raw_sxy & 0xFFFFu);
        y = (float)(int16_t)(raw_sxy >> 16);
        z = 1.0f;
        w = 1.0f;
    }

    uintptr_t addr = (uintptr_t)dest_addr;
    size_t base = pgxp_hash(addr);

    /* Packet arenas are reused every frame, and a busy farm scene can project
     * considerably more than 8K distinct packet vertices.  A four-slot probe
     * window used to evict an unrelated entry when it collided, silently
     * turning that primitive into screen-space 2D at draw time.  That caused
     * individual 3D objects to jump, and adjacent polygons to leave holes.
     * Use ordinary linear probing so every live address remains discoverable. */
    for (size_t i = 0; i < PGXP_CACHE_CAPACITY; ++i) {
        size_t idx = (base + i) & PGXP_CACHE_MASK;
        if (!s_cache[idx].vertex.valid || s_cache[idx].addr == addr) {
            s_cache[idx].addr = addr;
            s_cache[idx].vertex.x = x;
            s_cache[idx].vertex.y = y;
            s_cache[idx].vertex.z = z;
            s_cache[idx].vertex.w = w;
            s_cache[idx].vertex.raw_sxy = raw_sxy;
            s_cache[idx].vertex.has_subpixel = has_subpixel;
            s_cache[idx].vertex.valid = true;
            return;
        }
    }
    /* A full cache must not evict a valid vertex: preserving old entries is
     * safer than corrupting already-built primitives.  The frame-local cache
     * and 65,536 slots make this a defensive, practically unreachable case. */
}

const Ps1PgxpVertex *ps1_pgxp_lookup(const void *src_addr, uint32_t expected_raw_sxy) {
    if (src_addr == NULL) return NULL;

    uintptr_t addr = (uintptr_t)src_addr;
    size_t base = pgxp_hash(addr);

    for (size_t i = 0; i < PGXP_CACHE_CAPACITY; ++i) {
        size_t idx = (base + i) & PGXP_CACHE_MASK;
        if (!s_cache[idx].vertex.valid) {
            return NULL;
        }
        if (s_cache[idx].addr == addr && s_cache[idx].vertex.valid) {
            if (s_cache[idx].vertex.raw_sxy == expected_raw_sxy) {
                return &s_cache[idx].vertex;
            }
            /* Memory was modified/overwritten with different integer coordinates. */
            return NULL;
        }
    }

    return NULL;
}

