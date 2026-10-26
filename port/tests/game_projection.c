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
#ifdef HMBTN_ENH_WIDESCREEN
    hm_widescreen_scene(1);
    hm_widescreen_configure(0, 1920, 1080);
    if (near_x(-20) || !near_x(-19) || !near_x(319) || near_x(320))
        return 1;
    hm_widescreen_configure(1, 1920, 1080);
    if (!near_x(-54) || !near_x(373) || near_x(374) || !wide_x(453) || wide_x(454))
        return 1;
    vertex = (SVECTOR){100, -50, 1000, 0};
    if (gte_project_vertex(&screen, &vertex) != 250 || screen.x != 260 || screen.y != 70)
        return 1;
    hm_widescreen_configure(0, 1920, 1080);
#endif
    return 0;
}
