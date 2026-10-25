#include <inline_c.h>

static void hm_rotation_product(const MATRIX *a, const MATRIX *b, MATRIX *out) {
    s16 result[3][3];
    for (u32 row = 0; row < 3; ++row)
        for (u32 column = 0; column < 3; ++column) {
            s64 sum = 0;
            for (u32 k = 0; k < 3; ++k)
                sum += (s64)a->m[row][k] * b->m[k][column];
            result[row][column] = hm_gte_half(hm_gte_word(hm_gte_shift(sum, 12)));
        }
    memcpy(out->m, result, sizeof(result));
}

static MATRIX *hm_rotation_angles(const SVECTOR *angle, MATRIX *matrix, int zxy) {
    s32 sx = rsin(angle->vx), cx = rcos(angle->vx);
    s32 sy = rsin(angle->vy), cy = rcos(angle->vy);
    s32 sz = rsin(angle->vz), cz = rcos(angle->vz);
    MATRIX x = {.m = {{4096, 0, 0}, {0, (s16)cx, (s16)-sx}, {0, (s16)sx, (s16)cx}}};
    MATRIX y = {.m = {{(s16)cy, 0, (s16)sy}, {0, 4096, 0}, {(s16)-sy, 0, (s16)cy}}};
    MATRIX z = {.m = {{(s16)cz, (s16)-sz, 0}, {(s16)sz, (s16)cz, 0}, {0, 0, 4096}}};
    MATRIX intermediate;
    if (zxy) {
        hm_rotation_product(&z, &x, &intermediate);
        hm_rotation_product(&intermediate, &y, matrix);
    } else {
        hm_rotation_product(&x, &y, &intermediate);
        hm_rotation_product(&intermediate, &z, matrix);
    }
    return matrix;
}

MATRIX *RotMatrix(const SVECTOR *angle, MATRIX *matrix) {
    return hm_rotation_angles(angle, matrix, 0);
}
MATRIX *RotMatrixZXY(const SVECTOR *angle, MATRIX *matrix) {
    return hm_rotation_angles(angle, matrix, 1);
}

static MATRIX *hm_rotate_axis(s32 angle, MATRIX *matrix, u32 axis) {
    SVECTOR angles = {0};
    s16 value = hm_gte_half(angle);
    if (axis == 0)
        angles.vx = value;
    else if (axis == 1)
        angles.vy = value;
    else
        angles.vz = value;
    MATRIX rotation;
    hm_rotation_angles(&angles, &rotation, 0);
    hm_rotation_product(&rotation, matrix, matrix);
    return matrix;
}
MATRIX *RotMatrixX(s32 angle, MATRIX *matrix) { return hm_rotate_axis(angle, matrix, 0); }
MATRIX *RotMatrixY(s32 angle, MATRIX *matrix) { return hm_rotate_axis(angle, matrix, 1); }
MATRIX *RotMatrixZ(s32 angle, MATRIX *matrix) { return hm_rotate_axis(angle, matrix, 2); }
