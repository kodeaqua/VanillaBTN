#ifndef HMBTN_COMPAT_LIBGTE_H
#define HMBTN_COMPAT_LIBGTE_H

#include "compat/types.h"

/* Source-facing declarations inferred from game call sites, not SDK headers. */
typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    u8 r, g, b, cd;
} CVECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

_Static_assert(sizeof(VECTOR) == 16, "Four PS1 words per VECTOR");
_Static_assert(__builtin_offsetof(VECTOR, vx) == 0, "VECTOR x offset");
_Static_assert(__builtin_offsetof(VECTOR, vy) == 4, "VECTOR y offset");
_Static_assert(__builtin_offsetof(VECTOR, vz) == 8, "VECTOR z offset");
_Static_assert(__builtin_offsetof(VECTOR, pad) == 12, "VECTOR padding offset");
_Static_assert(sizeof(SVECTOR) == 8, "SVECTOR layout");
_Static_assert(__builtin_offsetof(SVECTOR, pad) == 6, "SVECTOR padding offset");
_Static_assert(sizeof(CVECTOR) == 4, "CVECTOR layout");
_Static_assert(__builtin_offsetof(CVECTOR, cd) == 3, "CVECTOR command offset");
_Static_assert(sizeof(MATRIX) == 32, "MATRIX layout");
_Static_assert(__builtin_offsetof(MATRIX, t) == 20, "MATRIX translation offset");

s32 rsin(s32 angle);
s32 rcos(s32 angle);
s32 SquareRoot0(s32 value);
s32 VectorNormal(VECTOR *input, VECTOR *output);
void InitGeom(void);
void SetGeomOffset(s32 x, s32 y);
void SetGeomScreen(s32 distance);
s32 ratan2(s32 y, s32 x);
void OuterProduct0(VECTOR *first, VECTOR *second, VECTOR *result);
extern const MATRIX GsIDMATRIX;
void PushMatrix(void);
void PopMatrix(void);
MATRIX *RotMatrix(const SVECTOR *angle, MATRIX *matrix);
MATRIX *RotMatrixZXY(const SVECTOR *angle, MATRIX *matrix);
MATRIX *RotMatrixX(s32 angle, MATRIX *matrix);
MATRIX *RotMatrixY(s32 angle, MATRIX *matrix);
MATRIX *RotMatrixZ(s32 angle, MATRIX *matrix);
MATRIX *TransMatrix(MATRIX *matrix, const VECTOR *translation);
void SetRotMatrix(const MATRIX *matrix);
void SetTransMatrix(const MATRIX *matrix);
VECTOR *ApplyMatrixLV(const MATRIX *matrix, const VECTOR *input, VECTOR *output);
void VectorNormalS(const VECTOR *input, SVECTOR *output);
void VectorNormalSS(const SVECTOR *input, SVECTOR *output);
void RotTrans(const SVECTOR *input, VECTOR *output, s32 *flags);

#endif
