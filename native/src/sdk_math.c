#include "ps1_gte.h"
#include <libgte.h>
#include <libgs.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

MATRIX GsIDMATRIX = {
    { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } },
    { 0, 0, 0 }
};

static Ps1Gte g_sdk_gte = {
    .rot = { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } },
    .h = 1000,
    .zsf3 = 341, .zsf4 = 256
};
static Ps1Vector3 g_gte_v0;
static Ps1Vector3 g_gte_v1;
static Ps1Vector3 g_gte_v2;
static int16_t g_gte_ir[4];
static int32_t g_gte_mac[4];
static int16_t g_gte_color_matrix[3][3];
static int16_t g_gte_light_matrix[3][3];
static int32_t g_gte_back_color[3];
static int32_t g_gte_far_color[3];
static int32_t g_gte_fog_near;
static int32_t g_gte_fog_h;

typedef struct GteMatrixStackEntry {
    int16_t rot[3][3];
    int32_t trans[3];
} GteMatrixStackEntry;

static GteMatrixStackEntry g_matrix_stack[32];
static unsigned g_matrix_sp;

static int rsin_quadrant(unsigned angle) {
    /* First-quadrant 1.12 fixed-point sine (4096 units per turn, 4096 = 1.0). */
    return (int)(sin((double)angle * (3.14159265358979323846 / 2048.0)) * 4096.0 + 0.5);
}

static int rsin_positive(unsigned angle) {
    angle &= 0xFFFu;
    if (angle <= 1024u) return rsin_quadrant(angle);
    if (angle <= 2048u) return rsin_quadrant(2048u - angle);
    if (angle <= 3072u) return -rsin_quadrant(angle - 2048u);
    return -rsin_quadrant(4096u - angle);
}

int rsin(int angle) {
    if (angle < 0) return -rsin_positive((unsigned)(-(int64_t)angle));
    return rsin_positive((unsigned)angle);
}

int rcos(int angle) {
    unsigned a = (unsigned)(angle < 0 ? -(int64_t)angle : angle) & 0xFFFu;
    if (a <= 1024u) return rsin_quadrant(1024u - a);
    if (a <= 2048u) return -rsin_quadrant(a - 1024u);
    if (a <= 3072u) return -rsin_quadrant(3072u - a);
    return rsin_quadrant(a - 3072u);
}

int csin(int angle) {
    return rsin(angle);
}

int ccos(int angle) {
    return rcos(angle);
}

/* Numerical SDK-function results, generated/checked by the optional reference
 * microtest tool. No proprietary SDK source or binaries are included. */
static const unsigned short atan_octant[1025] = {
#include "sdk_atan_table.inc"
};

long ratan2(long y, long x) {
    if (x == 0 && y == 0) return 0;
    int negative_x = x < 0, negative_y = y < 0;
    uint32_t ax = (uint32_t)(x < 0 ? -(int64_t)x : x);
    uint32_t ay = (uint32_t)(y < 0 ? -(int64_t)y : y);
    uint32_t small = ax < ay ? ax : ay, large = ax < ay ? ay : ax;
    /* Match SDK normalization: for large operands, shift the denominator
     * before dividing instead of overflowing a 32-bit shifted numerator. */
    unsigned ratio = (small & 0x7FE00000u) ? small / (large >> 10)
        : (unsigned)(((uint64_t)small << 10) / large);
    if (ratio > 1024) ratio = 1024;
    long angle = ax > ay ? atan_octant[ratio] : 1024 - atan_octant[ratio];
    if (negative_x) angle = 2048 - angle;
    return negative_y ? -angle : angle;
}

int csqrt(int value) {
    if (value <= 0) return 0;
    unsigned leading = 0;
    for (uint32_t bits = (uint32_t)value; !(bits & 0x80000000u); bits <<= 1) ++leading;
    int exponent = 8 - (int)leading;
    int scale = exponent >= 0 ? exponent / 2 : -((-exponent + 1) / 2) + 1;
    int normalized = scale >= 0 ? value >> (scale * 2) : value << (-scale * 2);
    /* SDK fixed-point hyperbolic CORDIC: six shift stages, repeating stage 4.
     * Preserve intermediate truncation; a generic sqrt changes tilt levels. */
    int x = normalized + 0x5D50AD, y = normalized - 0x5D50AD;
    for (int shift = 1; shift <= 6; ++shift) {
        int repetitions = shift == 4 ? 2 : 1;
        while (repetitions-- > 0) {
            int old_x = x;
            if (y < 0) { x += y >> shift; y += old_x >> shift; }
            else { x -= y >> shift; y -= old_x >> shift; }
        }
    }
    return x >> (6 - scale);
}

long SquareRoot0(long value) {
    if (value <= 0) return 0;
    return (long)(sqrt((double)value) + 0.5);
}

static int16_t clamp_ir(int64_t val) {
    if (val > 32767) return 32767;
    if (val < -32768) return -32768;
    return (int16_t)val;
}

static uint32_t pack_sxy(Ps1ScreenCoord c) {
    return (uint32_t)(uint16_t)c.x | ((uint32_t)(uint16_t)c.y << 16);
}

void SetGeomOffset(long ofx, long ofy) {
    ps1_gte_set_geom_offset(&g_sdk_gte, (int32_t)ofx, (int32_t)ofy);
}

void SetGeomScreen(long h) {
    ps1_gte_set_geom_screen(&g_sdk_gte, (int32_t)h);
}

void SetFogNear(long a, long h) {
    g_gte_fog_near = (int32_t)a;
    g_gte_fog_h = (int32_t)h;
}

void SetFarColor(long rfc, long gfc, long bfc) {
    g_gte_far_color[0] = (int32_t)rfc;
    g_gte_far_color[1] = (int32_t)gfc;
    g_gte_far_color[2] = (int32_t)bfc;
}

void PushMatrix(void) {
    if (g_matrix_sp < 32u) {
        memcpy(g_matrix_stack[g_matrix_sp].rot, g_sdk_gte.rot, sizeof(g_sdk_gte.rot));
        memcpy(g_matrix_stack[g_matrix_sp].trans, g_sdk_gte.trans, sizeof(g_sdk_gte.trans));
        ++g_matrix_sp;
    }
}

void PopMatrix(void) {
    if (g_matrix_sp > 0u) {
        --g_matrix_sp;
        memcpy(g_sdk_gte.rot, g_matrix_stack[g_matrix_sp].rot, sizeof(g_sdk_gte.rot));
        memcpy(g_sdk_gte.trans, g_matrix_stack[g_matrix_sp].trans, sizeof(g_sdk_gte.trans));
    }
}

MATRIX *gte_MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2) {
    if (!m0 || !m1 || !m2) return m2;
    short out[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            int64_t sum = (int64_t)m0->m[i][0] * m1->m[0][j]
                        + (int64_t)m0->m[i][1] * m1->m[1][j]
                        + (int64_t)m0->m[i][2] * m1->m[2][j];
            out[i][j] = clamp_ir(sum >> 12);
        }
    }
    memcpy(m2->m, out, sizeof(out));
    return m2;
}

MATRIX *RotMatrix(SVECTOR *r, MATRIX *m) {
    if (!r || !m) return m;
    int32_t t0 = m->t[0], t1 = m->t[1], t2 = m->t[2];
    *m = GsIDMATRIX;
    m->t[0] = t0;
    m->t[1] = t1;
    m->t[2] = t2;
    RotMatrixZ(r->vz, m);
    RotMatrixY(r->vy, m);
    RotMatrixX(r->vx, m);
    return m;
}

MATRIX *RotMatrixX(long r, MATRIX *m) {
    if (!m) return NULL;
    int s = rsin((int)r);
    int c = rcos((int)r);
    MATRIX rx = GsIDMATRIX;
    rx.m[1][1] = (short)c;
    rx.m[1][2] = (short)(-s);
    rx.m[2][1] = (short)s;
    rx.m[2][2] = (short)c;
    return gte_MulMatrix0(&rx, m, m);
}

MATRIX *RotMatrixY(long r, MATRIX *m) {
    if (!m) return NULL;
    int s = rsin((int)r);
    int c = rcos((int)r);
    MATRIX ry = GsIDMATRIX;
    ry.m[0][0] = (short)c;
    ry.m[0][2] = (short)s;
    ry.m[2][0] = (short)(-s);
    ry.m[2][2] = (short)c;
    return gte_MulMatrix0(&ry, m, m);
}

MATRIX *RotMatrixZ(long r, MATRIX *m) {
    if (!m) return NULL;
    int s = rsin((int)r);
    int c = rcos((int)r);
    MATRIX rz = GsIDMATRIX;
    rz.m[0][0] = (short)c;
    rz.m[0][1] = (short)(-s);
    rz.m[1][0] = (short)s;
    rz.m[1][1] = (short)c;
    return gte_MulMatrix0(&rz, m, m);
}

void SetRotMatrix(MATRIX *m) {
    gte_SetRotMatrix(m);
}

void SetTransMatrix(MATRIX *m) {
    gte_SetTransMatrix(m);
}

void RotTrans(SVECTOR *v0, VECTOR *v1, long *flag) {
    if (!v0 || !v1) return;
    int64_t vx = v0->vx, vy = v0->vy, vz = v0->vz;
    for (int i = 0; i < 3; ++i) {
        int64_t sum = ((int64_t)g_sdk_gte.trans[i] << 12)
                    + (int64_t)g_sdk_gte.rot[i][0] * vx
                    + (int64_t)g_sdk_gte.rot[i][1] * vy
                    + (int64_t)g_sdk_gte.rot[i][2] * vz;
        g_gte_mac[i + 1] = (int32_t)(sum >> 12);
        g_gte_ir[i + 1] = clamp_ir(g_gte_mac[i + 1]);
    }
    v1->vx = g_gte_mac[1];
    v1->vy = g_gte_mac[2];
    v1->vz = g_gte_mac[3];
    if (flag) {
        *flag = (long)g_sdk_gte.flag;
    }
}

VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1) {
    if (!m || !v0 || !v1) return v1;
    int64_t vx = v0->vx, vy = v0->vy, vz = v0->vz;
    v1->vx = (long)(((int64_t)m->m[0][0] * vx + (int64_t)m->m[0][1] * vy + (int64_t)m->m[0][2] * vz) >> 12);
    v1->vy = (long)(((int64_t)m->m[1][0] * vx + (int64_t)m->m[1][1] * vy + (int64_t)m->m[1][2] * vz) >> 12);
    v1->vz = (long)(((int64_t)m->m[2][0] * vx + (int64_t)m->m[2][1] * vy + (int64_t)m->m[2][2] * vz) >> 12);
    return v1;
}

void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2) {
    if (!v0 || !v1 || !v2) return;
    int64_t x0 = v0->vx, y0 = v0->vy, z0 = v0->vz;
    int64_t x1 = v1->vx, y1 = v1->vy, z1 = v1->vz;
    v2->vx = (long)(y0 * z1 - z0 * y1);
    v2->vy = (long)(z0 * x1 - x0 * z1);
    v2->vz = (long)(x0 * y1 - y0 * x1);
}

void gte_OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2) {
    if (!v0 || !v1 || !v2) return;
    int64_t x0 = v0->vx, y0 = v0->vy, z0 = v0->vz;
    int64_t x1 = v1->vx, y1 = v1->vy, z1 = v1->vz;
    v2->vx = (long)((y0 * z1 - z0 * y1) >> 12);
    v2->vy = (long)((z0 * x1 - x0 * z1) >> 12);
    v2->vz = (long)((x0 * y1 - y0 * x1) >> 12);
}

long VectorNormal(VECTOR *v0, VECTOR *v1) {
    if (!v0 || !v1) return 0;
    int64_t x = v0->vx, y = v0->vy, z = v0->vz;
    int64_t sq = x * x + y * y + z * z;
    if (sq <= 0) {
        v1->vx = 0;
        v1->vy = 0;
        v1->vz = 0;
        return 0;
    }
    double len = sqrt((double)sq);
    v1->vx = (long)((x * 4096.0) / len);
    v1->vy = (long)((y * 4096.0) / len);
    v1->vz = (long)((z * 4096.0) / len);
    return (long)sq;
}

long VectorNormalS(VECTOR *v0, SVECTOR *v1) {
    if (!v0 || !v1) return 0;
    VECTOR tmp = { 0, 0, 0, 0 };
    long sq = VectorNormal(v0, &tmp);
    v1->vx = clamp_ir(tmp.vx);
    v1->vy = clamp_ir(tmp.vy);
    v1->vz = clamp_ir(tmp.vz);
    return sq;
}

long VectorNormalSS(SVECTOR *v0, SVECTOR *v1) {
    if (!v0 || !v1) return 0;
    VECTOR in = { v0->vx, v0->vy, v0->vz, 0 };
    return VectorNormalS(&in, v1);
}

void gte_SetRotMatrix(const MATRIX *m) {
    if (!m) return;
    ps1_gte_set_rot_matrix(&g_sdk_gte, m->m);
}

void gte_ReadRotMatrix(MATRIX *m) {
    if (!m) return;
    memcpy(m->m, g_sdk_gte.rot, sizeof(g_sdk_gte.rot));
}

void gte_SetTransMatrix(const MATRIX *m) {
    if (!m) return;
    int32_t t[3] = { (int32_t)m->t[0], (int32_t)m->t[1], (int32_t)m->t[2] };
    ps1_gte_set_trans_matrix(&g_sdk_gte, t);
}

void gte_SetColorMatrix(const MATRIX *m) {
    if (!m) return;
    memcpy(g_gte_color_matrix, m->m, sizeof(g_gte_color_matrix));
}

void gte_SetLightMatrix(const MATRIX *m) {
    if (!m) return;
    memcpy(g_gte_light_matrix, m->m, sizeof(g_gte_light_matrix));
}

void gte_SetBackColor(long rbk, long gbk, long bbk) {
    g_gte_back_color[0] = (int32_t)rbk;
    g_gte_back_color[1] = (int32_t)gbk;
    g_gte_back_color[2] = (int32_t)bbk;
}

void gte_ldv0(const void *v0) {
    const SVECTOR *v = (const SVECTOR *)v0;
    if (!v) return;
    g_gte_v0 = (Ps1Vector3){ v->vx, v->vy, v->vz, v->pad };
}

void gte_ldv3(const void *v0, const void *v1, const void *v2) {
    const SVECTOR *a = (const SVECTOR *)v0;
    const SVECTOR *b = (const SVECTOR *)v1;
    const SVECTOR *c = (const SVECTOR *)v2;
    if (!a || !b || !c) return;
    g_gte_v0 = (Ps1Vector3){ a->vx, a->vy, a->vz, a->pad };
    g_gte_v1 = (Ps1Vector3){ b->vx, b->vy, b->vz, b->pad };
    g_gte_v2 = (Ps1Vector3){ c->vx, c->vy, c->vz, c->pad };
}

void gte_ldlvl(const void *v) {
    const VECTOR *vec = (const VECTOR *)v;
    if (!vec) return;
    g_gte_ir[1] = clamp_ir(vec->vx);
    g_gte_ir[2] = clamp_ir(vec->vy);
    g_gte_ir[3] = clamp_ir(vec->vz);
}

void gte_ldlv0(const void *v) {
    const long *words = (const long *)v;
    if (!words) return;
    g_gte_v0 = (Ps1Vector3){ clamp_ir(words[0]), clamp_ir(words[1]), clamp_ir(words[2]), 0 };
}

void gte_ldclmv(const void *p) {
    const int16_t *col = (const int16_t *)p;
    if (!col) return;
    g_gte_ir[1] = col[0];
    g_gte_ir[2] = col[3];
    g_gte_ir[3] = col[6];
}

void gte_rtps(void) {
    ps1_gte_rtps(&g_sdk_gte, &g_gte_v0);
}

void gte_rtpt(void) {
    ps1_gte_rtpt(&g_sdk_gte, &g_gte_v0, &g_gte_v1, &g_gte_v2);
}

void gte_nclip(void) {
    ps1_gte_nclip(&g_sdk_gte);
}

void gte_avsz3(void) {
    ps1_gte_avsz3(&g_sdk_gte);
}

void gte_avsz4(void) {
    ps1_gte_avsz4(&g_sdk_gte);
}

void gte_sqr0(void) {
    g_gte_mac[1] = (int32_t)g_gte_ir[1] * (int32_t)g_gte_ir[1];
    g_gte_mac[2] = (int32_t)g_gte_ir[2] * (int32_t)g_gte_ir[2];
    g_gte_mac[3] = (int32_t)g_gte_ir[3] * (int32_t)g_gte_ir[3];
    g_gte_ir[1] = clamp_ir(g_gte_mac[1]);
    g_gte_ir[2] = clamp_ir(g_gte_mac[2]);
    g_gte_ir[3] = clamp_ir(g_gte_mac[3]);
}

void gte_op12(void) {
    int64_t r1 = g_sdk_gte.rot[0][0], r2 = g_sdk_gte.rot[1][1], r3 = g_sdk_gte.rot[2][2];
    int64_t i1 = g_gte_ir[1], i2 = g_gte_ir[2], i3 = g_gte_ir[3];
    g_gte_mac[1] = (int32_t)((r2 * i3 - r3 * i2) >> 12);
    g_gte_mac[2] = (int32_t)((r3 * i1 - r1 * i3) >> 12);
    g_gte_mac[3] = (int32_t)((r1 * i2 - r2 * i1) >> 12);
    g_gte_ir[1] = clamp_ir(g_gte_mac[1]);
    g_gte_ir[2] = clamp_ir(g_gte_mac[2]);
    g_gte_ir[3] = clamp_ir(g_gte_mac[3]);
}

void gte_rtv0(void) {
    int64_t x = g_gte_v0.x, y = g_gte_v0.y, z = g_gte_v0.z;
    for (int i = 0; i < 3; ++i) {
        int64_t rot = ((int64_t)g_sdk_gte.rot[i][0] * x
                     + (int64_t)g_sdk_gte.rot[i][1] * y
                     + (int64_t)g_sdk_gte.rot[i][2] * z) >> 12;
        g_gte_mac[i + 1] = (int32_t)(g_sdk_gte.trans[i] + rot);
        g_gte_ir[i + 1] = clamp_ir(g_gte_mac[i + 1]);
    }
}

void gte_rtir(void) {
    int64_t x = g_gte_ir[1], y = g_gte_ir[2], z = g_gte_ir[3];
    for (int i = 0; i < 3; ++i) {
        int64_t rot = ((int64_t)g_sdk_gte.rot[i][0] * x
                     + (int64_t)g_sdk_gte.rot[i][1] * y
                     + (int64_t)g_sdk_gte.rot[i][2] * z) >> 12;
        g_gte_mac[i + 1] = (int32_t)rot;
        g_gte_ir[i + 1] = clamp_ir(rot);
    }
}

void gte_stflg(void *out) {
    if (!out) return;
    *(int32_t *)out = g_sdk_gte.flag;
}

void gte_stopz(void *out) {
    if (!out) return;
    *(int32_t *)out = g_sdk_gte.opz;
}

void gte_stotz(void *out) {
    if (!out) return;
    *(int32_t *)out = g_sdk_gte.otz;
}

void gte_stszotz(void *out) {
    if (!out) return;
    *(int32_t *)out = g_sdk_gte.sz[3] >> 2;
}

void gte_stsxy(void *out) {
    if (!out) return;
    *(uint32_t *)out = pack_sxy(g_sdk_gte.sxy[2]);
}

void gte_stsxy2(void *out) {
    if (!out) return;
    *(uint32_t *)out = pack_sxy(g_sdk_gte.sxy[2]);
}

void gte_stsxy3_f3(void *packet) {
    uint8_t *p = (uint8_t *)packet;
    *(uint32_t *)(p + 0x08) = pack_sxy(g_sdk_gte.sxy[0]);
    *(uint32_t *)(p + 0x0C) = pack_sxy(g_sdk_gte.sxy[1]);
    *(uint32_t *)(p + 0x10) = pack_sxy(g_sdk_gte.sxy[2]);
}

void gte_stsxy3_g3(void *packet) {
    uint8_t *p = (uint8_t *)packet;
    *(uint32_t *)(p + 0x08) = pack_sxy(g_sdk_gte.sxy[0]);
    *(uint32_t *)(p + 0x10) = pack_sxy(g_sdk_gte.sxy[1]);
    *(uint32_t *)(p + 0x18) = pack_sxy(g_sdk_gte.sxy[2]);
}

void gte_stsxy3_ft3(void *packet) {
    uint8_t *p = (uint8_t *)packet;
    *(uint32_t *)(p + 0x08) = pack_sxy(g_sdk_gte.sxy[0]);
    *(uint32_t *)(p + 0x10) = pack_sxy(g_sdk_gte.sxy[1]);
    *(uint32_t *)(p + 0x18) = pack_sxy(g_sdk_gte.sxy[2]);
}

void gte_stsxy3_gt3(void *packet) {
    uint8_t *p = (uint8_t *)packet;
    *(uint32_t *)(p + 0x08) = pack_sxy(g_sdk_gte.sxy[0]);
    *(uint32_t *)(p + 0x14) = pack_sxy(g_sdk_gte.sxy[1]);
    *(uint32_t *)(p + 0x20) = pack_sxy(g_sdk_gte.sxy[2]);
}

void gte_stsxy3_f4(void *packet) {
    gte_stsxy3_f3(packet);
}

void gte_stsxy3_g4(void *packet) {
    gte_stsxy3_g3(packet);
}

void gte_stsxy3_ft4(void *packet) {
    gte_stsxy3_ft3(packet);
}

void gte_stsxy3_gt4(void *packet) {
    gte_stsxy3_gt3(packet);
}

void gte_stlvnl(void *out) {
    long *words = (long *)out;
    if (!words) return;
    words[0] = g_gte_mac[1];
    words[1] = g_gte_mac[2];
    words[2] = g_gte_mac[3];
}

void gte_stlvl(void *out) {
    gte_stlvnl(out);
}

void gte_stclmv(void *out) {
    int16_t *col = (int16_t *)out;
    if (!col) return;
    col[0] = g_gte_ir[1];
    col[3] = g_gte_ir[2];
    col[6] = g_gte_ir[3];
}

void InitGeom(void) {
    memset(&g_sdk_gte, 0, sizeof(g_sdk_gte));
    memcpy(g_sdk_gte.rot, GsIDMATRIX.m, sizeof(g_sdk_gte.rot));
    g_sdk_gte.h = 1000;
    g_sdk_gte.zsf3 = 341;
    g_sdk_gte.zsf4 = 256;
    memset(g_gte_ir, 0, sizeof(g_gte_ir));
    memset(g_gte_mac, 0, sizeof(g_gte_mac));
    memset(g_gte_color_matrix, 0, sizeof(g_gte_color_matrix));
    memset(g_gte_light_matrix, 0, sizeof(g_gte_light_matrix));
    memset(g_gte_back_color, 0, sizeof(g_gte_back_color));
    memset(g_gte_far_color, 0, sizeof(g_gte_far_color));
    g_matrix_sp = 0;
}

MATRIX *TransMatrix(MATRIX *m, VECTOR *v) {
    if (!m || !v) return m;
    m->t[0] = v->vx;
    m->t[1] = v->vy;
    m->t[2] = v->vz;
    return m;
}

MATRIX *RotMatrixZXY(SVECTOR *r, MATRIX *m) {
    if (!r || !m) return m;
    int sx = rsin(r->vx), cx = rcos(r->vx);
    int sy = rsin(r->vy), cy = rcos(r->vy);
    int sz = rsin(r->vz), cz = rcos(r->vz);
    int xy = (sx * sy) >> 12;
    int xc = (sx * cy) >> 12;
    /* Z*X*Y uses staged 1.12 products. Rounding each term before adding
     * differs by one unit from composing generic matrix multiplications. */
    m->m[0][0] = (short)(((cz * cy) >> 12) - ((xy * sz) >> 12));
    m->m[0][1] = (short)((-sz * cx) >> 12);
    m->m[0][2] = (short)(((cz * sy) >> 12) + ((xc * sz) >> 12));
    m->m[1][0] = (short)(((sz * cy) >> 12) + ((xy * cz) >> 12));
    m->m[1][1] = (short)((cz * cx) >> 12);
    m->m[1][2] = (short)(((sz * sy) >> 12) - ((xc * cz) >> 12));
    m->m[2][0] = (short)((-cx * sy) >> 12);
    m->m[2][1] = (short)sx;
    m->m[2][2] = (short)((cx * cy) >> 12);
    return m;
}

int GsSetFlatLight(int id, GsF_LIGHT *light) {
    if (id < 0 || id >= 3 || !light) return -1;
    VECTOR direction = {light->vx, light->vy, light->vz, 0}, normal;
    if (!VectorNormal(&direction, &normal)) return -1;
    g_gte_light_matrix[id][0] = (short)normal.vx;
    g_gte_light_matrix[id][1] = (short)normal.vy;
    g_gte_light_matrix[id][2] = (short)normal.vz;
    g_gte_color_matrix[0][id] = (short)(light->r << 4);
    g_gte_color_matrix[1][id] = (short)(light->g << 4);
    g_gte_color_matrix[2][id] = (short)(light->b << 4);
    return 0;
}

void GsSetAmbient(long r, long g, long b) {
    gte_SetBackColor(r >> 4, g >> 4, b >> 4);
}

void GsSetLightMode(int mode) {
    /* Original farm selects normal parallel lighting. Fog mode needs a
     * separate implementation; never report it as silently supported. */
    if (mode != 0) abort();
}

void GsSetProjection(long distance) { SetGeomScreen(distance); }
