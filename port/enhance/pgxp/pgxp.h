#ifndef HMBTN_ENH_PGXP_H
#define HMBTN_ENH_PGXP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(HMBTN_ENH_PGXP) && HMBTN_ENH_PGXP

void hm_pgxp_set_enabled(int enabled);
int hm_pgxp_enabled(void);
void hm_pgxp_frame_begin(void);
void hm_pgxp_store_vertex(const void *dest, uint32_t integer_xy, float x, float y, float z);
int hm_pgxp_lookup_vertex(const void *dest, uint32_t integer_xy, float *out_x, float *out_y,
                          float *out_z);
void hm_pgxp_reset(void);

#else

static inline void hm_pgxp_set_enabled(int enabled) { (void)enabled; }
static inline int hm_pgxp_enabled(void) { return 0; }
static inline void hm_pgxp_frame_begin(void) {}
static inline void hm_pgxp_store_vertex(const void *dest, uint32_t integer_xy, float x, float y,
                                        float z) {
    (void)dest;
    (void)integer_xy;
    (void)x;
    (void)y;
    (void)z;
}
static inline int hm_pgxp_lookup_vertex(const void *dest, uint32_t integer_xy, float *out_x,
                                        float *out_y, float *out_z) {
    (void)dest;
    (void)integer_xy;
    (void)out_x;
    (void)out_y;
    (void)out_z;
    return 0;
}
static inline void hm_pgxp_reset(void) {}

#endif

#ifdef __cplusplus
}
#endif

#endif
