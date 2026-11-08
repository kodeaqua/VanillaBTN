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

static bool transform_vertex(const Ps1Gte *gte, const Ps1Vector3 *v,
                             int32_t *out_sx, int32_t *out_sy, int32_t *out_sz,
                             uint32_t *flags) {
    int64_t x = v->x;
    int64_t y = v->y;
    int64_t z = v->z;

    int64_t rx = (gte->rot[0][0] * x + gte->rot[0][1] * y + gte->rot[0][2] * z) / 4096;
    int64_t ry = (gte->rot[1][0] * x + gte->rot[1][1] * y + gte->rot[1][2] * z) / 4096;
    int64_t rz = (gte->rot[2][0] * x + gte->rot[2][1] * y + gte->rot[2][2] * z) / 4096;

    int64_t cx = rx + gte->trans[0];
    int64_t cy = ry + gte->trans[1];
    int64_t cz = rz + gte->trans[2];

    /* SZ is a saturated unsigned 16-bit register, not the unbounded host
     * translation result. Far values must not escape into an OT address. */
    if (cz < 0 || cz > 65535) *flags |= 1u << 18;
    *out_sz = cz < 0 ? 0 : cz > 65535 ? 65535 : (int32_t)cz;
    if (*out_sz == 0) {
        *flags |= 1u << 17;
        return false;
    }
    cz = *out_sz;

    int64_t sx = gte->ofx + (cx * (int64_t)gte->h) / cz;
    int64_t sy = gte->ofy + (cy * (int64_t)gte->h) / cz;
    /* SXY is a saturated signed 11-bit FIFO. These flags let original model
     * code reject overflowed geometry instead of drawing wrapped triangles.
     * See psx-spx, GTE Saturation (FLAG bits 14 and 13). */
    if (sx < -1024 || sx > 1023) *flags |= 1u << 14;
    if (sy < -1024 || sy > 1023) *flags |= 1u << 13;
    *out_sx = sx < -1024 ? -1024 : sx > 1023 ? 1023 : (int32_t)sx;
    *out_sy = sy < -1024 ? -1024 : sy > 1023 ? 1023 : (int32_t)sy;
    return *flags == 0;
}

bool ps1_gte_rtps(Ps1Gte *gte, const Ps1Vector3 *v) {
    if (gte == NULL || v == NULL) return false;

    /* Push FIFOs: SZ0<-SZ1<-SZ2<-SZ3; SXY0<-SXY1<-SXY2 */
    gte->sz[0] = gte->sz[1];
    gte->sz[1] = gte->sz[2];
    gte->sz[2] = gte->sz[3];

    gte->sxy[0] = gte->sxy[1];
    gte->sxy[1] = gte->sxy[2];

    int32_t sx = 0, sy = 0, sz = 0;
    uint32_t flags = 0;
    transform_vertex(gte, v, &sx, &sy, &sz, &flags);
    gte->flag = (int32_t)(flags ? flags | (1u << 31) : 0);
    gte->sz[3] = sz;
    gte->sxy[2] = (Ps1ScreenCoord){(int16_t)sx, (int16_t)sy};
    return flags == 0;
}

bool ps1_gte_rtpt(Ps1Gte *gte, const Ps1Vector3 *v0, const Ps1Vector3 *v1, const Ps1Vector3 *v2) {
    if (gte == NULL || v0 == NULL || v1 == NULL || v2 == NULL) return false;

    gte->sz[0] = gte->sz[3];

    int32_t sx0 = 0, sy0 = 0, sz0 = 0;
    int32_t sx1 = 0, sy1 = 0, sz1 = 0;
    int32_t sx2 = 0, sy2 = 0, sz2 = 0;
    uint32_t flags = 0;
    transform_vertex(gte, v0, &sx0, &sy0, &sz0, &flags);
    transform_vertex(gte, v1, &sx1, &sy1, &sz1, &flags);
    transform_vertex(gte, v2, &sx2, &sy2, &sz2, &flags);
    gte->flag = (int32_t)(flags ? flags | (1u << 31) : 0);
    gte->sxy[0] = (Ps1ScreenCoord){(int16_t)sx0, (int16_t)sy0};
    gte->sxy[1] = (Ps1ScreenCoord){(int16_t)sx1, (int16_t)sy1};
    gte->sxy[2] = (Ps1ScreenCoord){(int16_t)sx2, (int16_t)sy2};
    gte->sz[1] = sz0;
    gte->sz[2] = sz1;
    gte->sz[3] = sz2;
    return flags == 0;
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
