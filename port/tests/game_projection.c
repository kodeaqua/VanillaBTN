/* Exercise the original public projection helper while discarding unrelated
 * model-loading/camera paths whose runtime dependencies are still unfinished. */
#include "../../src/main/geometry/model.c"
#include <stdio.h>

/* Test-only traps for unrelated exported functions in model.c. The COFF linker
 * resolves their imports even when those sections are discarded. They are not
 * runtime implementations and must never be reached by this projection test. */
FrameState *current_frame;
void debug_hook_run(s32 value) {
    (void)value;
    hm_runtime_error("game.projection fixture", "unexpected model draw");
}
void VectorNormalS(const VECTOR *in, SVECTOR *out) {
    (void)in;
    (void)out;
    hm_runtime_error("game.projection fixture", "unexpected VectorNormalS");
}
void VectorNormalSS(const SVECTOR *in, SVECTOR *out) {
    (void)in;
    (void)out;
    hm_runtime_error("game.projection fixture", "unexpected VectorNormalSS");
}
VECTOR *ApplyMatrixLV(const MATRIX *matrix, const VECTOR *in, VECTOR *out) {
    (void)matrix;
    (void)in;
    (void)out;
    hm_runtime_error("game.projection fixture", "unexpected ApplyMatrixLV");
}
#define HM_ROTATION_TRAP(name)                                                                     \
    MATRIX *name(s32 angle, MATRIX *matrix) {                                                      \
        (void)angle;                                                                               \
        (void)matrix;                                                                              \
        hm_runtime_error("game.projection fixture", "unexpected " #name);                          \
    }
HM_ROTATION_TRAP(RotMatrixX)
HM_ROTATION_TRAP(RotMatrixY)
HM_ROTATION_TRAP(RotMatrixZ)
#undef HM_ROTATION_TRAP

int main(void) {
    InitGeom();
    SetRotMatrix(&GsIDMATRIX);
    SetTransMatrix(&GsIDMATRIX);
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);
    SVECTOR vertex = {100, -50, 1000, 0};
    FarmScreenXY screen = {0};
    if (gte_project_vertex(&screen, &vertex) != 250 || screen.x != 260 || screen.y != 70) {
        fprintf(stderr, "Original projection produced the wrong XY/depth\n");
        return 1;
    }
    vertex.vz = -1;
    if (gte_project_vertex(&screen, &vertex) != 0) {
        fprintf(stderr, "Original projection accepted a vertex behind the camera\n");
        return 1;
    }
    return 0;
}
