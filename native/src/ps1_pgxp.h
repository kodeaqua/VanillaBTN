#ifndef VANILLABTN_PS1_PGXP_H
#define VANILLABTN_PS1_PGXP_H

/*
 * ps1_pgxp: Precision Guided eXecution Pipeline for native PS1 rendering.
 *
 * Implements high-precision floating-point vertex caching and perspective-correct
 * texturing metadata tracking. Coordinates projected by GTE preserve fractional
 * subpixel offsets and depth reciprocal (1/W) across packet memory boundaries,
 * eliminating 3D polygon wobbling and affine texture warping.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct Ps1PgxpVertex {
    float x;           /* High-precision subpixel screen X */
    float y;           /* High-precision subpixel screen Y */
    float z;           /* Camera-space depth Z */
    float w;           /* Reciprocal depth (1.0f / z) */
    uint32_t raw_sxy;  /* Packed integer coordinate for memory validation */
    bool valid;
} Ps1PgxpVertex;

/* Initialize or reset the PGXP subsystem and clear cache entries. */
void ps1_pgxp_init(void);
void ps1_pgxp_clear(void);

/* Enable / disable PGXP precision features. */
void ps1_pgxp_set_enabled(bool vertex_precision, bool perspective_texture);
bool ps1_pgxp_is_vertex_precision_enabled(void);
bool ps1_pgxp_is_perspective_texture_enabled(void);

/* Register a vertex into the PGXP cache by destination memory address. */
void ps1_pgxp_register(const void *dest_addr, float x, float y, float z, float w, uint32_t raw_sxy);

/* Look up a vertex from the PGXP cache by source memory address and expected integer SXY.
 * Returns NULL if the address is not registered, or if raw_sxy does not match. */
const Ps1PgxpVertex *ps1_pgxp_lookup(const void *src_addr, uint32_t expected_raw_sxy);

#endif /* VANILLABTN_PS1_PGXP_H */
