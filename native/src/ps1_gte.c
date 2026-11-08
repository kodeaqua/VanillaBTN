#include "ps1_gte.h"

#include <string.h>

void ps1_gte_init(Ps1Gte *gte) {
    if (gte == NULL) return;
    memset(gte, 0, sizeof(*gte));

    /* Identity rotation matrix (1.3.12 fixed point, 4096 = 1.0). */
    gte->rot[0][0] = 4096;
    gte->rot[1][1] = 4096;
    gte->rot[2][2] = 4096;

    gte->h = 1000;
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
                             int32_t *out_sx, int32_t *out_sy, int32_t *out_sz) {
    int64_t x = v->x;
    int64_t y = v->y;
    int64_t z = v->z;

    int64_t rx = (gte->rot[0][0] * x + gte->rot[0][1] * y + gte->rot[0][2] * z) / 4096;
    int64_t ry = (gte->rot[1][0] * x + gte->rot[1][1] * y + gte->rot[1][2] * z) / 4096;
    int64_t rz = (gte->rot[2][0] * x + gte->rot[2][1] * y + gte->rot[2][2] * z) / 4096;

    int64_t cx = rx + gte->trans[0];
    int64_t cy = ry + gte->trans[1];
    int64_t cz = rz + gte->trans[2];

    if (cz <= 0) {
        return false;
    }

    *out_sx = (int32_t)(gte->ofx + (cx * (int64_t)gte->h) / cz);
    *out_sy = (int32_t)(gte->ofy + (cy * (int64_t)gte->h) / cz);
    *out_sz = (int32_t)cz;
    return true;
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
    if (!transform_vertex(gte, v, &sx, &sy, &sz)) {
        gte->flag |= (int32_t)(1u << 31); /* negative flag = clip/behind camera */
        gte->sz[3] = 0;
        gte->sxy[2] = (Ps1ScreenCoord){0, 0};
        return false;
    }

    gte->flag &= ~(int32_t)(1u << 31);
    gte->sz[3] = sz;
    gte->sxy[2] = (Ps1ScreenCoord){(int16_t)sx, (int16_t)sy};
    return true;
}

bool ps1_gte_rtpt(Ps1Gte *gte, const Ps1Vector3 *v0, const Ps1Vector3 *v1, const Ps1Vector3 *v2) {
    if (gte == NULL || v0 == NULL || v1 == NULL || v2 == NULL) return false;

    gte->sz[0] = gte->sz[1];

    int32_t sx0, sy0, sz0;
    int32_t sx1, sy1, sz1;
    int32_t sx2, sy2, sz2;

    bool ok0 = transform_vertex(gte, v0, &sx0, &sy0, &sz0);
    bool ok1 = transform_vertex(gte, v1, &sx1, &sy1, &sz1);
    bool ok2 = transform_vertex(gte, v2, &sx2, &sy2, &sz2);

    if (!ok0 || !ok1 || !ok2) {
        gte->flag |= (int32_t)(1u << 31);
        return false;
    }

    gte->flag &= ~(int32_t)(1u << 31);

    gte->sxy[0] = (Ps1ScreenCoord){(int16_t)sx0, (int16_t)sy0};
    gte->sxy[1] = (Ps1ScreenCoord){(int16_t)sx1, (int16_t)sy1};
    gte->sxy[2] = (Ps1ScreenCoord){(int16_t)sx2, (int16_t)sy2};

    gte->sz[1] = sz0;
    gte->sz[2] = sz1;
    gte->sz[3] = sz2;
    return true;
}

int32_t ps1_gte_nclip(Ps1Gte *gte) {
    if (gte == NULL) return 0;
    int64_t x0 = gte->sxy[0].x, y0 = gte->sxy[0].y;
    int64_t x1 = gte->sxy[1].x, y1 = gte->sxy[1].y;
    int64_t x2 = gte->sxy[2].x, y2 = gte->sxy[2].y;

    /* OPZ = (x0 - x1) * (y2 - y1) - (y0 - y1) * (x2 - x1) */
    int32_t opz = (int32_t)((x0 - x1) * (y2 - y1) - (y0 - y1) * (x2 - x1));
    gte->opz = opz;
    return opz;
}

int32_t ps1_gte_avsz3(Ps1Gte *gte) {
    if (gte == NULL) return 0;
    int32_t otz = (gte->sz[1] + gte->sz[2] + gte->sz[3]) / 3;
    gte->otz = otz;
    return otz;
}

int32_t ps1_gte_avsz4(Ps1Gte *gte) {
    if (gte == NULL) return 0;
    int32_t otz = (gte->sz[0] + gte->sz[1] + gte->sz[2] + gte->sz[3]) / 4;
    gte->otz = otz;
    return otz;
}
