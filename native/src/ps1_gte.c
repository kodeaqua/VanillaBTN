#include "ps1_gte.h"

#include <string.h>
#include <limits.h>

void ps1_gte_init(Ps1Gte *gte) {
    if (gte == NULL) return;
    memset(gte, 0, sizeof(*gte));

    /* Identity rotation matrix (1.3.12 fixed point, 4096 = 1.0). */
    gte->rot[0][0] = 4096;
    gte->rot[1][1] = 4096;
    gte->rot[2][2] = 4096;

    gte->h = 1000;
    /* Retail SDK InitGeom depth scales: quarter-depth ordering values. */
    gte->zsf3 = 341;
    gte->zsf4 = 256;
}

void ps1_gte_set_geom_offset(Ps1Gte *gte, int32_t ofx, int32_t ofy) {
    if (gte == NULL) return;
    gte->ofx = ofx;
    gte->ofy = ofy;
}

void ps1_gte_set_geom_screen(Ps1Gte *gte, int32_t h) {
    if (gte == NULL) return;
    gte->h = h;
}

void ps1_gte_set_rot_matrix(Ps1Gte *gte, const int16_t m[3][3]) {
    if (gte == NULL || m == NULL) return;
    memcpy(gte->rot, m, sizeof(gte->rot));
}

void ps1_gte_set_trans_matrix(Ps1Gte *gte, const int32_t t[3]) {
    if (gte == NULL || t == NULL) return;
    gte->trans[0] = t[0];
    gte->trans[1] = t[1];
    gte->trans[2] = t[2];
}

void ps1_gte_set_matrix(Ps1Gte *gte, const Ps1Matrix *matrix) {
    if (gte == NULL || matrix == NULL) return;
    ps1_gte_set_rot_matrix(gte, matrix->m);
    ps1_gte_set_trans_matrix(gte, matrix->t);
}

/* Hardware UNR reciprocal, generated from its seed formula instead of an SDK
 * table. The final clamp can occur without divide overflow (e.g. FE3F/7F20).
 * Reference: https://psx-spx.consoledev.net/geometrytransformationenginegte/
 */
static uint32_t perspective_quotient(uint16_t h, uint16_t sz, uint32_t *flags) {
    if ((uint32_t)h >= (uint32_t)sz * 2u) {
        if (flags) *flags |= 1u << 17;
        return 0x1FFFFu;
    }
    unsigned shift = 0;
    uint32_t d = sz;
    while (d < 0x8000u) { d <<= 1; ++shift; }
    uint32_t n = (uint32_t)h << shift;
    unsigned index = (d - 0x7FC0u) >> 7;
    int seed = (int)((0x40000u / (index + 0x100u) + 1u) / 2u) - 0x101;
    if (seed < 0) seed = 0;
    uint32_t u = (uint32_t)seed + 0x101u;
    d = (0x2000080u - d * u) >> 8;
    d = (0x80u + d * u) >> 8;
    uint64_t result = ((uint64_t)n * d + 0x8000u) >> 16;
    return result > 0x1FFFFu ? 0x1FFFFu : (uint32_t)result;
}

static int32_t projection_ir(int64_t value, unsigned bit, uint32_t *flags) {
    if (flags && (value < -32768 || value > 32767)) *flags |= 1u << bit;
    return value < -32768 ? -32768 : value > 32767 ? 32767 : (int32_t)value;
}

static int32_t projection_flag(uint32_t flags) {
    /* IR3 saturation (bit 22) alone does not set the summary bit. */
    return (int32_t)(flags | ((flags & 0x7F87E000u) ? 0x80000000u : 0));
}

static void transform_vertex(const Ps1Gte *gte, const Ps1Vector3 *v,
                             int32_t *out_sx, int32_t *out_sy, int32_t *out_sz,
                             Ps1SubpixelCoord *out_sub,
                             uint32_t *flags) {
    int64_t x = v->x;
    int64_t y = v->y;
    int64_t z = v->z;

    int64_t rx = (gte->rot[0][0] * x + gte->rot[0][1] * y + gte->rot[0][2] * z) >> 12;
    int64_t ry = (gte->rot[1][0] * x + gte->rot[1][1] * y + gte->rot[1][2] * z) >> 12;
    int64_t rz = (gte->rot[2][0] * x + gte->rot[2][1] * y + gte->rot[2][2] * z) >> 12;

    int64_t cx = rx + gte->trans[0];
    int64_t cy = ry + gte->trans[1];
    int64_t cz = rz + gte->trans[2];

    /* SZ is a saturated unsigned 16-bit register, not the unbounded host
     * translation result. Far values must not escape into an OT address. */
    if (cz < 0 || cz > 65535) *flags |= 1u << 18;
    *out_sz = cz < 0 ? 0 : cz > 65535 ? 65535 : (int32_t)cz;
    int32_t ir_x = projection_ir(cx, 24, flags);
    int32_t ir_y = projection_ir(cy, 23, flags);
    projection_ir(cz, 22, flags);
    uint32_t quotient = perspective_quotient((uint16_t)gte->h, (uint16_t)*out_sz, flags);
    int64_t screen_x = (int64_t)gte->ofx * 65536 + (int64_t)ir_x * quotient;
    int64_t screen_y = (int64_t)gte->ofy * 65536 + (int64_t)ir_y * quotient;
    if (screen_x > INT32_MAX || screen_y > INT32_MAX) *flags |= 1u << 16;
    if (screen_x < INT32_MIN || screen_y < INT32_MIN) *flags |= 1u << 15;
    int64_t sx = screen_x >> 16;
    int64_t sy = screen_y >> 16;
    /* SXY is a saturated signed 11-bit FIFO. These flags let original model
     * code reject overflowed geometry instead of drawing wrapped triangles.
     * See psx-spx, GTE Saturation (FLAG bits 14 and 13). */
    if (sx < -1024 || sx > 1023) *flags |= 1u << 14;
    if (sy < -1024 || sy > 1023) *flags |= 1u << 13;
    *out_sx = sx < -1024 ? -1024 : sx > 1023 ? 1023 : (int32_t)sx;
    *out_sy = sy < -1024 ? -1024 : sy > 1023 ? 1023 : (int32_t)sy;

    if (out_sub != NULL) {
        float sub_x = (float)screen_x / 65536.0f;
        float sub_y = (float)screen_y / 65536.0f;
        if (sub_x < -1024.0f) sub_x = -1024.0f;
        else if (sub_x > 1023.0f) sub_x = 1023.0f;
        if (sub_y < -1024.0f) sub_y = -1024.0f;
        else if (sub_y > 1023.0f) sub_y = 1023.0f;

        float sub_z = (cz > 0) ? (float)cz : (float)*out_sz;
        float sub_w = (sub_z > 0.001f) ? (1.0f / sub_z) : 1.0f;
        out_sub->x = sub_x;
        out_sub->y = sub_y;
        out_sub->z = sub_z;
        out_sub->w = sub_w;
        out_sub->valid = true;
    }
}

bool ps1_gte_rtps(Ps1Gte *gte, const Ps1Vector3 *v) {
    if (gte == NULL || v == NULL) return false;

    /* Push FIFOs: SZ0<-SZ1<-SZ2<-SZ3; SXY0<-SXY1<-SXY2 */
    gte->sz[0] = gte->sz[1];
    gte->sz[1] = gte->sz[2];
    gte->sz[2] = gte->sz[3];

    gte->sxy[0] = gte->sxy[1];
    gte->sxy[1] = gte->sxy[2];

    gte->sub_sz[0] = gte->sub_sz[1];
    gte->sub_sz[1] = gte->sub_sz[2];
    gte->sub_sz[2] = gte->sub_sz[3];

    gte->sub_sxy[0] = gte->sub_sxy[1];
    gte->sub_sxy[1] = gte->sub_sxy[2];

    int32_t sx = 0, sy = 0, sz = 0;
    Ps1SubpixelCoord sub = {0};
    uint32_t flags = 0;
    transform_vertex(gte, v, &sx, &sy, &sz, &sub, &flags);
    gte->flag = projection_flag(flags);
    gte->sz[3] = sz;
    gte->sxy[2] = (Ps1ScreenCoord){(int16_t)sx, (int16_t)sy};
    gte->sub_sz[3] = sub.z;
    gte->sub_sxy[2] = sub;
    return gte->flag >= 0;
}

bool ps1_gte_rtpt(Ps1Gte *gte, const Ps1Vector3 *v0, const Ps1Vector3 *v1, const Ps1Vector3 *v2) {
    if (gte == NULL || v0 == NULL || v1 == NULL || v2 == NULL) return false;

    gte->sz[0] = gte->sz[3];
    gte->sub_sz[0] = gte->sub_sz[3];

    int32_t sx0 = 0, sy0 = 0, sz0 = 0;
    int32_t sx1 = 0, sy1 = 0, sz1 = 0;
    int32_t sx2 = 0, sy2 = 0, sz2 = 0;
    Ps1SubpixelCoord sub0 = {0}, sub1 = {0}, sub2 = {0};
    uint32_t flags = 0;
    transform_vertex(gte, v0, &sx0, &sy0, &sz0, &sub0, &flags);
    transform_vertex(gte, v1, &sx1, &sy1, &sz1, &sub1, &flags);
    transform_vertex(gte, v2, &sx2, &sy2, &sz2, &sub2, &flags);
    gte->flag = projection_flag(flags);
    gte->sxy[0] = (Ps1ScreenCoord){(int16_t)sx0, (int16_t)sy0};
    gte->sxy[1] = (Ps1ScreenCoord){(int16_t)sx1, (int16_t)sy1};
    gte->sxy[2] = (Ps1ScreenCoord){(int16_t)sx2, (int16_t)sy2};
    gte->sz[1] = sz0;
    gte->sz[2] = sz1;
    gte->sz[3] = sz2;
    gte->sub_sxy[0] = sub0;
    gte->sub_sxy[1] = sub1;
    gte->sub_sxy[2] = sub2;
    gte->sub_sz[1] = sub0.z;
    gte->sub_sz[2] = sub1.z;
    gte->sub_sz[3] = sub2.z;
    return gte->flag >= 0;
}

void ps1_gte_get_sub_sxy(const Ps1Gte *gte, int index, Ps1SubpixelCoord *out) {
    if (gte == NULL || out == NULL || index < 0 || index >= 3) return;
    *out = gte->sub_sxy[index];
}

int32_t ps1_gte_nclip(Ps1Gte *gte) {
    if (gte == NULL) return 0;
    int64_t x0 = gte->sxy[0].x, y0 = gte->sxy[0].y;
    int64_t x1 = gte->sxy[1].x, y1 = gte->sxy[1].y;
    int64_t x2 = gte->sxy[2].x, y2 = gte->sxy[2].y;

    /* Hardware MAC0 uses this winding; reversing it culls the visible sides
     * of original TMD terrain while leaving 2D sprites unaffected. */
    int64_t area = x0 * y1 + x1 * y2 + x2 * y0
        - x0 * y2 - x1 * y0 - x2 * y1;
    uint32_t flags = 0;
    if (area > INT32_MAX) flags |= 1u << 16;
    if (area < INT32_MIN) flags |= 1u << 15;
    gte->flag = (int32_t)(flags ? flags | (1u << 31) : 0);
    gte->opz = (int32_t)(uint32_t)area;
    return gte->opz;
}

static int32_t average_depth(Ps1Gte *gte, uint32_t sum, int16_t scale) {
    int64_t mac0 = (int64_t)sum * scale;
    int64_t depth = mac0 >> 12;
    uint32_t flags = 0;
    if (mac0 > INT32_MAX) flags |= 1u << 16;
    if (mac0 < INT32_MIN) flags |= 1u << 15;
    if (depth < 0 || depth > 65535) flags |= 1u << 18;
    gte->otz = depth < 0 ? 0 : depth > 65535 ? 65535 : (int32_t)depth;
    gte->flag = (int32_t)(flags ? flags | (1u << 31) : 0);
    return gte->otz;
}

int32_t ps1_gte_avsz3(Ps1Gte *gte) {
    if (!gte) return 0;
    uint32_t sum = (uint16_t)gte->sz[1] + (uint16_t)gte->sz[2] + (uint16_t)gte->sz[3];
    return average_depth(gte, sum, gte->zsf3);
}

int32_t ps1_gte_avsz4(Ps1Gte *gte) {
    if (!gte) return 0;
    uint32_t sum = (uint16_t)gte->sz[0] + (uint16_t)gte->sz[1]
        + (uint16_t)gte->sz[2] + (uint16_t)gte->sz[3];
    return average_depth(gte, sum, gte->zsf4);
}
