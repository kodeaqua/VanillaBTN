/*
 * ps1_float_gte_test: synthetic checks for 32-bit floating-point continuous GTE math.
 */

#include "ps1_gte.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int test_float_math_controls(void) {
    ps1_gte_set_float_math(true);
    if (!ps1_gte_get_float_math()) return 1;

    ps1_gte_set_float_math(false);
    if (ps1_gte_get_float_math()) return 2;

    ps1_gte_set_float_math(true);
    return 0;
}

static int test_smooth_rotation_precision(void) {
    Ps1Gte gte;
    ps1_gte_init(&gte);
    ps1_gte_set_geom_offset(&gte, 160, 120);
    ps1_gte_set_geom_screen(&gte, 1000);
    ps1_gte_set_float_math(true);

    /* 45-degree rotation around Y: cos(45) = sin(45) = 2896 in 1.3.12 (4096 = 1.0) */
    int16_t rot[3][3] = {
        {  2896, 0, 2896 },
        {     0, 4096, 0 },
        { -2896, 0, 2896 }
    };
    ps1_gte_set_rot_matrix(&gte, rot);
    int32_t trans[3] = { 0, 0, 1000 };
    ps1_gte_set_trans_matrix(&gte, trans);

    /* Test vertex at (50, 0, 0) */
    Ps1Vector3 v = { 50, 0, 0, 0 };
    if (!ps1_gte_rtps(&gte, &v)) return 10;

    Ps1SubpixelCoord sub;
    ps1_gte_get_sub_sxy(&gte, 2, &sub);
    if (!sub.valid) return 11;

    /*
     * Theoretical values:
     * cx = 50 * (2896 / 4096.0) = 35.3515625
     * cz = -50 * (2896 / 4096.0) + 1000 = 1000 - 35.3515625 = 964.6484375
     * expected screen_x = 160 + (35.3515625 * 1000) / 964.6484375 = 160 + 36.64709 = 196.64709
     */
    float expected_x = 160.0f + (35.3515625f * 1000.0f) / 964.6484375f;
    if (fabsf(sub.x - expected_x) > 0.05f) return 12;

    /* Integer coordinates in SXY FIFO must also be consistent with fixed-point truncation */
    if (abs((int)gte.sxy[2].x - (int)expected_x) > 1) return 13;

    return 0;
}

static int test_smooth_continuous_sweep(void) {
    Ps1Gte gte;
    ps1_gte_init(&gte);
    ps1_gte_set_geom_offset(&gte, 160, 120);
    ps1_gte_set_geom_screen(&gte, 1000);
    ps1_gte_set_float_math(true);

    /* Sweep translation Z from 1000 to 1500 */
    float prev_x = 0.0f;
    for (int step = 0; step < 50; ++step) {
        int32_t tz = 1000 + step * 10;
        int32_t trans[3] = { 0, 0, tz };
        ps1_gte_set_trans_matrix(&gte, trans);

        Ps1Vector3 v = { 50, 0, 0, 0 };
        if (!ps1_gte_rtps(&gte, &v)) return 20;

        Ps1SubpixelCoord sub;
        ps1_gte_get_sub_sxy(&gte, 2, &sub);
        if (!sub.valid) return 21;

        if (step > 0) {
            /* As Z increases, perspective X must decrease strictly monotonically */
            if (sub.x >= prev_x) return 22;
            /* Step change must be small and smooth (< 1.5 pixels per step) */
            if (fabsf(sub.x - prev_x) > 1.5f) return 23;
        }
        prev_x = sub.x;
    }
    return 0;
}

static int test_fallback_when_disabled(void) {
    Ps1Gte gte;
    ps1_gte_init(&gte);
    ps1_gte_set_geom_offset(&gte, 160, 120);
    ps1_gte_set_geom_screen(&gte, 1000);

    int32_t trans[3] = { 0, 0, 723 };
    ps1_gte_set_trans_matrix(&gte, trans);
    Ps1Vector3 v = { 35, 0, 0, 0 };

    /* Float math disabled */
    ps1_gte_set_float_math(false);
    if (!ps1_gte_rtps(&gte, &v)) return 30;

    Ps1SubpixelCoord sub_fixed;
    ps1_gte_get_sub_sxy(&gte, 2, &sub_fixed);

    /* Float math enabled */
    ps1_gte_set_float_math(true);
    if (!ps1_gte_rtps(&gte, &v)) return 31;

    Ps1SubpixelCoord sub_float;
    ps1_gte_get_sub_sxy(&gte, 2, &sub_float);

    /* Float math produces sub-pixel continuous value close to fixed-point but smoother */
    if (fabsf(sub_float.x - sub_fixed.x) > 1.0f) return 32;

    return 0;
}

int main(void) {
    int res;
    if ((res = test_float_math_controls()) != 0) {
        fprintf(stderr, "test_float_math_controls failed with %d\n", res);
        return 1;
    }
    if ((res = test_smooth_rotation_precision()) != 0) {
        fprintf(stderr, "test_smooth_rotation_precision failed with %d\n", res);
        return 2;
    }
    if ((res = test_smooth_continuous_sweep()) != 0) {
        fprintf(stderr, "test_smooth_continuous_sweep failed with %d\n", res);
        return 3;
    }
    if ((res = test_fallback_when_disabled()) != 0) {
        fprintf(stderr, "test_fallback_when_disabled failed with %d\n", res);
        return 4;
    }

    printf("ps1_float_gte_test: all tests passed\n");
    return 0;
}
