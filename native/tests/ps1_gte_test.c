/*
 * ps1_gte_test: synthetic checks for the GTE compatibility layer.
 */
#include "ps1_gte.h"

int main(void) {
    Ps1Gte gte;
    ps1_gte_init(&gte);

    /* 1. Identity transform and perspective divide:
     * v = (100, 50, 1000) with h = 1000, offset = (160, 120).
     * Sx = 160 + (100 * 1000) / 1000 = 260.
     * Sy = 120 + (50 * 1000) / 1000 = 170.
     * Sz = 1000. */
    ps1_gte_set_geom_offset(&gte, 160, 120);
    ps1_gte_set_geom_screen(&gte, 1000);

    Ps1Vector3 v0 = { 100, 50, 1000, 0 };
    if (!ps1_gte_rtps(&gte, &v0)) return 1;
    if (gte.sxy[2].x != 260 || gte.sxy[2].y != 170 || gte.sz[3] != 1000) return 2;
    if (gte.flag < 0) return 3;

    /* 2. RTPT on 3 vertices:
     * v0 = (0, 0, 1000)   -> Sx = 160, Sy = 120
     * v1 = (200, 0, 1000) -> Sx = 360, Sy = 120
     * v2 = (0, 100, 1000) -> Sx = 160, Sy = 220 */
    Ps1Vector3 t0 = { 0, 0, 1000, 0 };
    Ps1Vector3 t1 = { 200, 0, 1000, 0 };
    Ps1Vector3 t2 = { 0, 100, 1000, 0 };

    if (!ps1_gte_rtpt(&gte, &t0, &t1, &t2)) return 4;
    if (gte.sxy[0].x != 160 || gte.sxy[0].y != 120) return 5;
    if (gte.sxy[1].x != 360 || gte.sxy[1].y != 120) return 6;
    if (gte.sxy[2].x != 160 || gte.sxy[2].y != 220) return 7;

    /* 3. NCLIP on front-facing triangle:
     * (160,120), (360,120), (160,220) -> OPZ = (160-360)*(220-120) - (120-120)*(160-360)
     * = -200 * 100 = -20000.
     * In PS1 GTE: OPZ = (x0-x1)*(y2-y1) - (y0-y1)*(x2-x1).
     * Clockwise / counter-clockwise: */
    int32_t opz = ps1_gte_nclip(&gte);
    /* For t0, t1, t2: (160,120) to (360,120) is right, (160,220) is down -> clockwise in screen Y-down */
    if (opz == 0) return 8;

    /* Reverse order should flip sign of OPZ */
    gte.sxy[1] = (Ps1ScreenCoord){ 160, 220 };
    gte.sxy[2] = (Ps1ScreenCoord){ 360, 120 };
    int32_t opz_rev = ps1_gte_nclip(&gte);
    if (opz_rev != -opz) return 9;

    /* 4. Clip flag behind camera (Z <= 0):
     * Point with Z = -10 must fail and set flag < 0. */
    Ps1Vector3 behind = { 0, 0, -10, 0 };
    if (ps1_gte_rtps(&gte, &behind)) return 10;
    if (gte.flag >= 0) return 11;

    /* 5. Depth averaging (AVSZ3 and AVSZ4): */
    gte.sz[0] = 100;
    gte.sz[1] = 200;
    gte.sz[2] = 300;
    gte.sz[3] = 400;

    int32_t av3 = ps1_gte_avsz3(&gte); /* (200 + 300 + 400) / 3 = 300 */
    if (av3 != 300) return 12;

    int32_t av4 = ps1_gte_avsz4(&gte); /* (100 + 200 + 300 + 400) / 4 = 250 */
    if (av4 != 250) return 13;

    /* 6. Translation matrix offset: */
    int32_t trans[3] = { 50, -30, 500 };
    ps1_gte_set_trans_matrix(&gte, trans);
    /* v = (0, 0, 500) -> view coords (50, -30, 1000)
     * Sx = 160 + (50 * 1000) / 1000 = 210.
     * Sy = 120 + (-30 * 1000) / 1000 = 90. */
    Ps1Vector3 v_trans = { 0, 0, 500, 0 };
    if (!ps1_gte_rtps(&gte, &v_trans)) return 14;
    if (gte.sxy[2].x != 210 || gte.sxy[2].y != 90 || gte.sz[3] != 1000) return 15;

    return 0;
}
