#ifndef VANILLABTN_PS1_GTE_H
#define VANILLABTN_PS1_GTE_H

/*
 * ps1_gte: Geometry Transformation Engine (GTE) compatibility layer.
 *
 * Implements fixed-point 3D vector rotation, translation, perspective
 * projection (RTPS / RTPT), 2D normal clipping (NCLIP), and depth averaging
 * (AVSZ3 / AVSZ4) matching the PlayStation Cop2 hardware behaviors used by
 * model.c, ground.c, and camera.c.
 *
 * Fixed-point formats:
 *   Rotation matrix elements: signed 16-bit 1.3.12 (4096 = 1.0).
 *   Translation vector:       signed 32-bit (world / view coordinates).
 *   Screen distance H:        signed 32-bit (typically 1000).
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

typedef struct Ps1Gte {
    int16_t rot[3][3];     /* Current rotation matrix */
    int32_t trans[3];      /* Current translation vector */
    int32_t ofx, ofy;      /* Screen offset */
    int32_t h;             /* Screen distance */

    /* FIFOs and result registers */
    int32_t sz[4];         /* Screen Z FIFO (SZ0, SZ1, SZ2, SZ3) */
    Ps1ScreenCoord sxy[3]; /* Screen XY FIFO (SXY0, SXY1, SXY2) */
    int32_t opz;           /* 2D cross-product result from NCLIP */
    int32_t flag;          /* Status / clip flag (< 0 indicates behind camera) */
    int32_t otz;           /* Average depth from AVSZ3 / AVSZ4 */
} Ps1Gte;

void ps1_gte_init(Ps1Gte *gte);
void ps1_gte_set_geom_offset(Ps1Gte *gte, int32_t ofx, int32_t ofy);
void ps1_gte_set_geom_screen(Ps1Gte *gte, int32_t h);
void ps1_gte_set_rot_matrix(Ps1Gte *gte, const int16_t m[3][3]);
void ps1_gte_set_trans_matrix(Ps1Gte *gte, const int32_t t[3]);
void ps1_gte_set_matrix(Ps1Gte *gte, const Ps1Matrix *matrix);

/* RTPS: Rotate, translate, and perspective-project 1 vector into SXY2 and SZ3.
 * Returns false and sets flag < 0 if vertex is behind the camera (Z <= 0). */
bool ps1_gte_rtps(Ps1Gte *gte, const Ps1Vector3 *v);

/* RTPT: Rotate, translate, and perspective-project 3 vectors into SXY0..2 and SZ1..3.
 * Returns false and sets flag < 0 if any vertex is behind the camera. */
bool ps1_gte_rtpt(Ps1Gte *gte, const Ps1Vector3 *v0, const Ps1Vector3 *v1, const Ps1Vector3 *v2);

/* NCLIP: 2D normal clip on current screen coordinates (SXY0, SXY1, SXY2).
 * Returns OPZ (> 0 front-facing, <= 0 back-facing). */
int32_t ps1_gte_nclip(Ps1Gte *gte);

/* AVSZ3: Average screen Z of 3 points (SZ1, SZ2, SZ3). */
int32_t ps1_gte_avsz3(Ps1Gte *gte);

/* AVSZ4: Average screen Z of 4 points (SZ0, SZ1, SZ2, SZ3). */
int32_t ps1_gte_avsz4(Ps1Gte *gte);

#endif /* VANILLABTN_PS1_GTE_H */
