#ifndef VANILLABTN_COMPAT_LIBGTE_H
#define VANILLABTN_COMPAT_LIBGTE_H

/* Only declarations needed by the currently compiled original modules belong
 * here. Unsupported GTE operations must fail compilation rather than no-op. */
#include "sdk_types.h"

typedef struct VECTOR { long vx, vy, vz, pad; } VECTOR;
typedef struct SVECTOR { short vx, vy, vz, pad; } SVECTOR;
typedef struct DVECTOR { short vx, vy; } DVECTOR;
typedef struct CVECTOR { u_char r, g, b, cd; } CVECTOR;
typedef struct MATRIX { short m[3][3]; long t[3]; } MATRIX;

int csqrt(int value);
long SquareRoot0(long value);
int rsin(int angle);
int rcos(int angle);
int csin(int angle);
int ccos(int angle);
long ratan2(long y, long x);

void SetGeomOffset(long ofx, long ofy);
void SetGeomScreen(long h);
void SetFogNear(long a, long h);
void SetFarColor(long rfc, long gfc, long bfc);
void PushMatrix(void);
void PopMatrix(void);

MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixX(long r, MATRIX *m);
MATRIX *RotMatrixY(long r, MATRIX *m);
MATRIX *RotMatrixZ(long r, MATRIX *m);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void RotTrans(SVECTOR *v0, VECTOR *v1, long *flag);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
void gte_OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2);
MATRIX *gte_MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
long VectorNormal(VECTOR *v0, VECTOR *v1);
long VectorNormalS(VECTOR *v0, SVECTOR *v1);
long VectorNormalSS(SVECTOR *v0, SVECTOR *v1);

void gte_SetRotMatrix(const MATRIX *m);
void gte_ReadRotMatrix(MATRIX *m);
void gte_SetTransMatrix(const MATRIX *m);
void gte_SetColorMatrix(const MATRIX *m);
void gte_SetLightMatrix(const MATRIX *m);
void gte_SetBackColor(long rbk, long gbk, long bbk);

void gte_ldv0(const void *v0);
void gte_ldv3(const void *v0, const void *v1, const void *v2);
void gte_ldlvl(const void *v);
void gte_ldlv0(const void *v);
void gte_ldclmv(const void *p);

void gte_rtps(void);
void gte_rtpt(void);
void gte_nclip(void);
void gte_avsz3(void);
void gte_avsz4(void);
void gte_sqr0(void);
void gte_op12(void);
void gte_rtv0(void);
void gte_rtir(void);

void gte_stflg(void *out);
void gte_stopz(void *out);
void gte_stotz(void *out);
void gte_stszotz(void *out);
void gte_stsxy(void *out);
void gte_stsxy2(void *out);
void gte_stsxy3_f3(void *packet);
void gte_stsxy3_g3(void *packet);
void gte_stsxy3_ft3(void *packet);
void gte_stsxy3_gt3(void *packet);
void gte_stsxy3_f4(void *packet);
void gte_stsxy3_g4(void *packet);
void gte_stsxy3_ft4(void *packet);
void gte_stsxy3_gt4(void *packet);
void gte_stlvnl(void *out);
void gte_stlvl(void *out);
void gte_stclmv(void *out);

#endif
