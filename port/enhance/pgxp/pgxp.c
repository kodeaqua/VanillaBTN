#include "enhance/pgxp/pgxp.h"

#if defined(HMBTN_ENH_PGXP) && HMBTN_ENH_PGXP

#define HM_PGXP_TABLE_SIZE 32768
#define HM_PGXP_TABLE_MASK (HM_PGXP_TABLE_SIZE - 1)

typedef struct {
    uintptr_t key;
    uint32_t integer_xy;
    float x;
    float y;
    float z;
    uint32_t frame;
} hm_pgxp_entry;

static hm_pgxp_entry s_pgxp_table[HM_PGXP_TABLE_SIZE];
static uint32_t s_pgxp_frame = 1;
static int s_pgxp_enabled = 0;

void hm_pgxp_set_enabled(int enabled) {
    s_pgxp_enabled = !!enabled;
}

int hm_pgxp_enabled(void) {
    return s_pgxp_enabled;
}

void hm_pgxp_frame_begin(void) {
    s_pgxp_frame++;
    if (!s_pgxp_frame) {
        s_pgxp_frame = 1;
    }
}

void hm_pgxp_reset(void) {
    for (size_t i = 0; i < HM_PGXP_TABLE_SIZE; ++i) {
        s_pgxp_table[i].key = 0;
        s_pgxp_table[i].frame = 0;
    }
}

void hm_pgxp_store_vertex(const void *dest, uint32_t integer_xy, float x, float y, float z) {
    if (!dest || !s_pgxp_enabled) {
        return;
    }
    uintptr_t key = (uintptr_t)dest;
    uint32_t h = (uint32_t)(((key >> 2) * 2654435761u) & HM_PGXP_TABLE_MASK);
    for (uint32_t probe = 0; probe < 16; ++probe) {
        uint32_t idx = (h + probe) & HM_PGXP_TABLE_MASK;
        if (s_pgxp_table[idx].key == key || s_pgxp_table[idx].key == 0 ||
            (s_pgxp_frame - s_pgxp_table[idx].frame) > 1) {
            s_pgxp_table[idx].key = key;
            s_pgxp_table[idx].integer_xy = integer_xy;
            s_pgxp_table[idx].x = x;
            s_pgxp_table[idx].y = y;
            s_pgxp_table[idx].z = z;
            s_pgxp_table[idx].frame = s_pgxp_frame;
            return;
        }
    }
}

int hm_pgxp_lookup_vertex(const void *dest, uint32_t integer_xy, float *out_x, float *out_y,
                          float *out_z) {
    if (!dest || !s_pgxp_enabled) {
        return 0;
    }
    uintptr_t key = (uintptr_t)dest;
    uint32_t h = (uint32_t)(((key >> 2) * 2654435761u) & HM_PGXP_TABLE_MASK);
    for (uint32_t probe = 0; probe < 16; ++probe) {
        uint32_t idx = (h + probe) & HM_PGXP_TABLE_MASK;
        if (s_pgxp_table[idx].key == key && (s_pgxp_frame - s_pgxp_table[idx].frame) <= 1) {
            if (s_pgxp_table[idx].integer_xy == integer_xy) {
                if (out_x)
                    *out_x = s_pgxp_table[idx].x;
                if (out_y)
                    *out_y = s_pgxp_table[idx].y;
                if (out_z)
                    *out_z = s_pgxp_table[idx].z;
                return 1;
            }
            return 0;
        }
        if (s_pgxp_table[idx].key == 0) {
            return 0;
        }
    }
    return 0;
}

#endif
