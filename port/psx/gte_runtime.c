#include <inline_c.h>
#include <string.h>

hm_gte_state hm_gte;
const MATRIX GsIDMATRIX = {.m = {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}};
static MATRIX hm_matrix_stack[32];
static u32 hm_matrix_depth;

void InitGeom(void) {
    memset(&hm_gte, 0, sizeof(hm_gte));
    hm_matrix_depth = 0;
    hm_gte.zsf[0] = 341;
    hm_gte.zsf[1] = 256;
}

void SetGeomOffset(s32 x, s32 y) {
    hm_gte.ofx = hm_gte_word((s64)x * 65536);
    hm_gte.ofy = hm_gte_word((s64)y * 65536);
}

void SetGeomScreen(s32 distance) { hm_gte.distance = (u16)(u32)distance; }
void SetFogNear(s32 near_distance, s32 projection_distance) {
    if (!projection_distance)
        hm_runtime_error("SetFogNear", "zero projection distance");
    hm_gte.dqa = hm_gte_half(hm_gte_word(-(s64)near_distance * 320 / projection_distance));
    hm_gte.dqb = 0x01400000;
}
void SetFarColor(s32 r, s32 g, s32 b) {
    hm_gte.translation[2][0] = hm_gte_word((s64)r * 16);
    hm_gte.translation[2][1] = hm_gte_word((s64)g * 16);
    hm_gte.translation[2][2] = hm_gte_word((s64)b * 16);
}
void SetRotMatrix(const MATRIX *matrix) { gte_SetRotMatrix(matrix); }
void SetTransMatrix(const MATRIX *matrix) { gte_SetTransMatrix(matrix); }

void PushMatrix(void) {
    if (hm_matrix_depth == 32)
        hm_runtime_error("PushMatrix", "matrix stack overflow");
    gte_ReadRotMatrix(&hm_matrix_stack[hm_matrix_depth++]);
}

void PopMatrix(void) {
    if (!hm_matrix_depth)
        hm_runtime_error("PopMatrix", "matrix stack underflow");
    const MATRIX *matrix = &hm_matrix_stack[--hm_matrix_depth];
    gte_SetRotMatrix(matrix);
    gte_SetTransMatrix(matrix);
}

void RotTrans(const SVECTOR *input, VECTOR *output, s32 *flags) {
    gte_ldv0(input);
    hm_gte_command(&hm_gte, 0x4A480012);
    gte_stlvnl(output);
    gte_stflg(flags);
}

MATRIX *TransMatrix(MATRIX *matrix, const VECTOR *translation) {
    matrix->t[0] = translation->vx;
    matrix->t[1] = translation->vy;
    matrix->t[2] = translation->vz;
    return matrix;
}
