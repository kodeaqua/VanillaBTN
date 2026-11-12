#ifndef VANILLABTN_PS1_GTE_H
#define VANILLABTN_PS1_GTE_H

/*
 * ps1_gte: Geometry Transformation Engine (GTE) compatibility layer.
 *
 * Implements fixed-point 3D vector rotation, translation, perspective
 * projection (RTPS / RTPT), 2D normal clipping (NCLIP), and depth averaging
 * (AVSZ3 / AVSZ4) for the PlayStation Cop2 operations used by
 * model.c, ground.c, and camera.c. Projection uses the UNR reciprocal, IR/SZ/SXY
 * saturation and signed fixed-point shifts. Full 44-bit MAC overflow handling
 * and depth-cue register behavior remain under audit.
 *
 * Fixed-point formats:
 *   Rotation matrix elements: signed 16-bit 1.3.12 (4096 = 1.0).
 *   Translation vector:       signed 32-bit (world / view coordinates).
 *   Screen distance H:        unsigned 16-bit register (typically 1000).
 *   Screen offset OFX, OFY:   signed 32-bit (pixel origin).
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct Ps1Vector3 {
    int16_t x, y, z;
    int16_t pad;
} Ps1Vector3;

typedef struct Ps1Matrix {
    int16_t m[3][3]; /* 1.3.12 fixed point (4096 = 1.0) */
    int16_t pad;
    int32_t t[3];    /* Translation vector (tx, ty, tz) */
} Ps1Matrix;

typedef struct Ps1ScreenCoord {
    int16_t x, y;
} Ps1ScreenCoord;

typedef struct Ps1SubpixelCoord {
    float x;
    float y;
    float z;
    float w;
    bool valid;
} Ps1SubpixelCoord;

typedef struct Ps1Gte {
    int16_t rot[3][3];     /* Current rotation matrix */
    int32_t trans[3];      /* Current translation vector */
    int32_t ofx, ofy;      /* Screen offset */
    int32_t h;             /* Screen distance */
    int16_t zsf3, zsf4;     /* Signed 1.3.12 average-depth scale registers. */

    /* FIFOs and result registers */
    int32_t sz[4];         /* Screen Z FIFO (SZ0, SZ1, SZ2, SZ3) */
    Ps1ScreenCoord sxy[3]; /* Screen XY FIFO (SXY0, SXY1, SXY2) */
    int32_t opz;           /* 2D cross-product result from NCLIP */
    int32_t flag;          /* Hardware status flags; bit 31 summarizes selected errors. */
    int32_t otz;           /* Average depth from AVSZ3 / AVSZ4 */

    /* PGXP subpixel and depth FIFOs */
    Ps1SubpixelCoord sub_sxy[3]; /* High-precision subpixel coords for SXY0..2 */
    float sub_sz[4];             /* Camera space depth for SZ0..3 */
} Ps1Gte;

void ps1_gte_init(Ps1Gte *gte);
void ps1_gte_set_float_math(bool enabled);
bool ps1_gte_get_float_math(void);
void ps1_gte_set_widescreen_scale(float scale);
float ps1_gte_get_widescreen_scale(void);
int32_t ps1_gte_scale_frustum_half_angle(int32_t base_half_angle);
void ps1_gte_set_geom_offset(Ps1Gte *gte, int32_t ofx, int32_t ofy);
void ps1_gte_set_geom_screen(Ps1Gte *gte, int32_t h);
void ps1_gte_set_rot_matrix(Ps1Gte *gte, const int16_t m[3][3]);
void ps1_gte_set_trans_matrix(Ps1Gte *gte, const int32_t t[3]);
void ps1_gte_set_matrix(Ps1Gte *gte, const Ps1Matrix *matrix);
void ps1_gte_get_sub_sxy(const Ps1Gte *gte, int index, Ps1SubpixelCoord *out);

/* RTPS: Rotate, translate, and perspective-project 1 vector into SXY2 and SZ3.
 * Returns false when a hardware summary error occurs (near-plane overflow,
 * IR1/IR2 overflow, SZ/SXY saturation or MAC0 overflow). */
bool ps1_gte_rtps(Ps1Gte *gte, const Ps1Vector3 *v);

/* RTPT: Rotate, translate, and perspective-project 3 vectors into SXY0..2 and SZ1..3.
 * Returns false if any vertex raises a hardware summary error. */
bool ps1_gte_rtpt(Ps1Gte *gte, const Ps1Vector3 *v0, const Ps1Vector3 *v1, const Ps1Vector3 *v2);

/* NCLIP: 2D normal clip on current screen coordinates (SXY0, SXY1, SXY2).
 * Returns OPZ (> 0 front-facing, <= 0 back-facing). */
int32_t ps1_gte_nclip(Ps1Gte *gte);

/* AVSZ3: Average screen Z of 3 points (SZ1, SZ2, SZ3). */
int32_t ps1_gte_avsz3(Ps1Gte *gte);

/* AVSZ4: Average screen Z of 4 points (SZ0, SZ1, SZ2, SZ3). */
int32_t ps1_gte_avsz4(Ps1Gte *gte);

#endif /* VANILLABTN_PS1_GTE_H */
