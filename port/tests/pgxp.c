#include "enhance/pgxp/pgxp.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    uint32_t packet_a[8] = {0};
    uint32_t packet_b[8] = {0};
    float x = 0.0f, y = 0.0f, z = 0.0f;

    /* 1. Default / Vanilla Parity: PGXP is disabled */
    hm_pgxp_reset();
    hm_pgxp_set_enabled(0);
    CHECK(hm_pgxp_enabled() == 0);

    /* When disabled, store and lookup must be safe no-ops */
    hm_pgxp_store_vertex(&packet_a[1], 0x00100020, 32.5f, 16.25f, 300.0f);
    CHECK(!hm_pgxp_lookup_vertex(&packet_a[1], 0x00100020, &x, &y, &z));

    /* 2. Enable PGXP */
    hm_pgxp_set_enabled(1);
    CHECK(hm_pgxp_enabled() == 1);

    /* 3. Store and lookup vertex */
    uint32_t int_xy = 0x00140028; /* X = 40, Y = 20 */
    float fx = 40.375f, fy = 20.625f, fz = 512.0f;
    hm_pgxp_store_vertex(&packet_a[1], int_xy, fx, fy, fz);

    CHECK(hm_pgxp_lookup_vertex(&packet_a[1], int_xy, &x, &y, &z));
    CHECK(fabsf(x - fx) < 0.0001f);
    CHECK(fabsf(y - fy) < 0.0001f);
    CHECK(fabsf(z - fz) < 0.0001f);

    /* 4. Mismatched pointer should fail */
    CHECK(!hm_pgxp_lookup_vertex(&packet_b[1], int_xy, &x, &y, &z));

    /* 5. Mismatched integer coordinates should fail (detects stale/reused memory) */
    CHECK(!hm_pgxp_lookup_vertex(&packet_a[1], int_xy + 1, &x, &y, &z));

    /* 6. Frame aging and retention across 1 frame */
    hm_pgxp_frame_begin();
    CHECK(hm_pgxp_lookup_vertex(&packet_a[1], int_xy, &x, &y, &z));

    /* After another frame without refresh, it should expire */
    hm_pgxp_frame_begin();
    CHECK(!hm_pgxp_lookup_vertex(&packet_a[1], int_xy, &x, &y, &z));

    /* 7. Stress / multiple vertices in hash table */
    static uint32_t packet_stress[500];
    for (int i = 0; i < 500; ++i) {
        uint32_t val = (uint32_t)i;
        hm_pgxp_store_vertex(&packet_stress[i], val, (float)i + 0.5f, (float)i + 0.25f,
                             100.0f + (float)i);
    }
    for (int i = 0; i < 500; ++i) {
        uint32_t val = (uint32_t)i;
        CHECK(hm_pgxp_lookup_vertex(&packet_stress[i], val, &x, &y, &z));
        CHECK(fabsf(x - ((float)i + 0.5f)) < 0.0001f);
        CHECK(fabsf(y - ((float)i + 0.25f)) < 0.0001f);
        CHECK(fabsf(z - (100.0f + (float)i)) < 0.0001f);
    }

    /* 8. Reset */
    hm_pgxp_reset();
    CHECK(!hm_pgxp_lookup_vertex(&packet_a[0], 0, &x, &y, &z));

    /* 9. Return to disabled vanilla state */
    hm_pgxp_set_enabled(0);
    CHECK(hm_pgxp_enabled() == 0);

    return 0;
}
