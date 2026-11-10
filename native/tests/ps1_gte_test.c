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
    if (gte.sz[0] != 1000) return 21; /* Three pushes retain previous SZ3. */

    /* Hardware NCLIP is the signed shoelace area. In screen Y-down space,
     * this clockwise triangle has positive OPZ = 20000. */
    int32_t opz = ps1_gte_nclip(&gte);
    if (opz != 20000 || gte.flag != 0) return 8;

    /* Reverse order should flip sign of OPZ */
    gte.sxy[1] = (Ps1ScreenCoord){ 160, 220 };
    gte.sxy[2] = (Ps1ScreenCoord){ 360, 120 };
    int32_t opz_rev = ps1_gte_nclip(&gte);
    if (opz_rev != -opz) return 9;
    /* Software writes to SXY preserve all signed 16-bit input bits. MAC0
     * wraps at 32 bits and reports which direction overflowed. */
    gte.sxy[0] = (Ps1ScreenCoord){-32768, -32768};
    gte.sxy[1] = (Ps1ScreenCoord){32767, -32768};
    gte.sxy[2] = (Ps1ScreenCoord){-32768, 32767};
    if (ps1_gte_nclip(&gte) != -131071 || !(gte.flag & (1 << 16)) || gte.flag >= 0) return 22;
    gte.sxy[1] = (Ps1ScreenCoord){-32768, 32767};
    gte.sxy[2] = (Ps1ScreenCoord){32767, -32768};
    if (ps1_gte_nclip(&gte) != 131071 || !(gte.flag & (1 << 15)) ||
        (gte.flag & (1 << 16)) || gte.flag >= 0) return 23;

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

    int32_t av3 = ps1_gte_avsz3(&gte); /* InitGeom: ZSF3=341, (900*341)>>12 = 74. */
    if (av3 != 74) return 12;

    int32_t av4 = ps1_gte_avsz4(&gte); /* InitGeom: ZSF4=256, (1000*256)>>12 = 62. */
    if (av4 != 62) return 13;

    /* 6. Translation matrix offset: */
    int32_t trans[3] = { 50, -30, 500 };
    ps1_gte_set_trans_matrix(&gte, trans);
    /* v = (0, 0, 500) -> view coords (50, -30, 1000)
     * Sx = 160 + (50 * 1000) / 1000 = 210.
     * Sy = 120 + (-30 * 1000) / 1000 = 90. */
    Ps1Vector3 v_trans = { 0, 0, 500, 0 };
    if (!ps1_gte_rtps(&gte, &v_trans)) return 14;
    if (gte.sxy[2].x != 210 || gte.sxy[2].y != 90 || gte.sz[3] != 1000) return 15;

    /* SZ is an unsigned saturated 16-bit hardware FIFO, never raw host Z. */
    trans[0] = trans[1] = 0;
    trans[2] = 1000000;
    ps1_gte_set_trans_matrix(&gte, trans);
    Ps1Vector3 far = {0, 0, 0, 0};
    if (ps1_gte_rtps(&gte, &far)) return 16;
    if (gte.sz[3] != 65535 || !(gte.flag & (1 << 18))) return 17;
    for (int i = 0; i < 4; ++i) gte.sz[i] = 65535;
    if (ps1_gte_avsz3(&gte) != 16367 || ps1_gte_avsz4(&gte) != 16383) return 18;
    gte.zsf3 = 32767;
    if (ps1_gte_avsz3(&gte) != 65535 || !(gte.flag & (1 << 18))) return 19;
    gte.zsf4 = -1;
    if (ps1_gte_avsz4(&gte) != 0 || !(gte.flag & (1 << 18))) return 20;
    /* Hardware SXY lanes saturate to [-1024,1023], rather than wrapping
     * huge projected coordinates into a drawable triangle. */
    ps1_gte_init(&gte);
    int32_t clipped_translation[3] = {2000, -2000, 1000};
    ps1_gte_set_trans_matrix(&gte, clipped_translation);
    Ps1Vector3 zero = {0};
    if (ps1_gte_rtps(&gte, &zero)) return 21;
    if (gte.sxy[2].x != 1023 || gte.sxy[2].y != -1024) return 22;
    if ((gte.flag & 0x80006000u) != 0x80006000u) return 23;
    /* RTPT accumulates clipping from earlier vertices as well. */
    Ps1Vector3 inside = {-2000, 2000, 0, 0};
    if (ps1_gte_rtpt(&gte, &zero, &inside, &inside)) return 24;
    if (gte.sxy[0].x != 1023 || gte.sxy[2].x != 0) return 25;
    if ((gte.flag & 0x6000u) != 0x6000u) return 26;
    /* Near-plane quotient saturates at 0x1FFFF, even with nonzero SZ.
     * Negative projection rounds down, as the hardware arithmetic shift does. */
    ps1_gte_init(&gte);
    Ps1Vector3 near = {1, -1, 500, 0};
    if (ps1_gte_rtps(&gte, &near)) return 27;
    if (gte.flag != (int32_t)0x80020000u || gte.sxy[2].x != 1
        || gte.sxy[2].y != -2 || gte.sz[3] != 500) return 28;
    /* 1/3 quantizes to 21845 in 16.16; (21845*3)>>16 is zero. */
    ps1_gte_set_geom_screen(&gte, 1);
    Ps1Vector3 third = {3, -3, 3, 0};
    if (!ps1_gte_rtps(&gte, &third)) return 29;
    if (gte.sxy[2].x != 0 || gte.sxy[2].y != -1) return 30;
    /* Rotation also uses SAR12, rather than truncating toward zero. */
    gte.rot[0][0] = -1;
    ps1_gte_set_geom_screen(&gte, 1000);
    Ps1Vector3 fraction = {1, 0, 1000, 0};
    if (!ps1_gte_rtps(&gte, &fraction) || gte.sxy[2].x != -1) return 31;
    /* IR3 saturation by itself is excluded from FLAG's summary bit. */
    ps1_gte_init(&gte);
    int32_t far_ir[3] = {0, 0, 40000};
    ps1_gte_set_trans_matrix(&gte, far_ir);
    if (!ps1_gte_rtps(&gte, &zero) || gte.flag != (1 << 22)) return 32;
    /* Projection consumes saturated IR1/IR2, not unbounded translation. */
    ps1_gte_init(&gte);
    ps1_gte_set_geom_screen(&gte, 100);
    int32_t wide_xy[3] = {40000, -40000, 10000};
    ps1_gte_set_trans_matrix(&gte, wide_xy);
    if (ps1_gte_rtps(&gte, &zero)) return 33;
    if (gte.sxy[2].x != 327 || gte.sxy[2].y != -328
        || gte.flag != (int32_t)0x81800000u) return 34;
    /* UNR's final clamp alone must not raise divide overflow. */
    ps1_gte_init(&gte);
    ps1_gte_set_geom_screen(&gte, 0xFE3F);
    Ps1Vector3 reciprocal_edge = {1, 0, 0x7F20, 0};
    if (!ps1_gte_rtps(&gte, &reciprocal_edge) || gte.flag != 0
        || gte.sxy[2].x != 1) return 35;
    return 0;
}
