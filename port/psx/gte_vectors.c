#include "psx/integer_math.h"
#include <inline_c.h>
#include <libgte.h>

s32 SquareRoot0(s32 value) {
    if (value < 0)
        hm_runtime_error("SquareRoot0", "negative input");
    return (s32)hm_integer_sqrt((u32)value);
}

s32 VectorNormal(VECTOR *input, VECTOR *output) {
    s32 xyz[3] = {input->vx, input->vy, input->vz};
    u64 sum = 0;
    for (u32 i = 0; i < 3; ++i)
        sum += (u64)((s64)xyz[i] * xyz[i]);
    if (sum > INT32_MAX)
        hm_runtime_error("VectorNormal", "squared length exceeds SDK input range");
    u32 length = hm_integer_sqrt(sum << 24);
    s32 values[3];
    for (u32 i = 0; i < 3; ++i)
        values[i] = length ? (s32)((s64)xyz[i] * 16777216 / length) : 0;
    output->vx = values[0];
    output->vy = values[1];
    output->vz = values[2];
    return (s32)sum;
}

void VectorNormalS(const VECTOR *input, SVECTOR *output) {
    VECTOR source = {input->vx, input->vy, input->vz, 0}, normal;
    VectorNormal(&source, &normal);
    output->vx = (s16)normal.vx;
    output->vy = (s16)normal.vy;
    output->vz = (s16)normal.vz;
}

void VectorNormalSS(const SVECTOR *input, SVECTOR *output) {
    VECTOR source = {input->vx, input->vy, input->vz, 0};
    VectorNormalS(&source, output);
}

void OuterProduct0(VECTOR *first, VECTOR *second, VECTOR *result) {
    s16 diagonal[3] = {hm_gte.matrix[0][0][0], hm_gte.matrix[0][1][1], hm_gte.matrix[0][2][2]};
    s32 a[3] = {first->vx, first->vy, first->vz};
    s32 b[3] = {second->vx, second->vy, second->vz};
    for (u32 i = 0; i < 3; ++i) {
        hm_gte.matrix[0][i][i] = hm_gte_half(a[i]);
        hm_gte.ir[i] = hm_gte_half(b[i]);
    }
    hm_gte_command(&hm_gte, 0x4B70000C);
    gte_stlvnl(result);
    for (u32 i = 0; i < 3; ++i)
        hm_gte.matrix[0][i][i] = diagonal[i];
}

VECTOR *ApplyMatrixLV(const MATRIX *matrix, const VECTOR *input, VECTOR *output) {
    s32 xyz[3] = {input->vx, input->vy, input->vz};
    s32 result[3];
    SetRotMatrix(matrix);
    for (u32 row = 0; row < 3; ++row) {
        s64 sum = 0;
        for (u32 column = 0; column < 3; ++column)
            sum += (s64)matrix->m[row][column] * xyz[column];
        result[row] = hm_gte_word(hm_gte_shift(sum, 12));
    }
    output->vx = result[0];
    output->vy = result[1];
    output->vz = result[2];
    return output;
}
