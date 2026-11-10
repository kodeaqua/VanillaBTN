#include "ps1_gte.h"
#include "host_display.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "Assertion failed at line %d: %s\n", __LINE__, #cond); \
    exit(1); \
} } while (0)

static void test_widescreen_scale_get_set(void) {
    /* Default scale should be 1.0f (4:3 authentic) */
    ps1_gte_set_widescreen_scale(1.0f);
    CHECK(fabsf(ps1_gte_get_widescreen_scale() - 1.0f) < 1e-5f);

    /* Set 16:9 scale factor (16/9 / 4/3 = 1.333333) */
    ps1_gte_set_widescreen_scale(1.333333f);
    CHECK(fabsf(ps1_gte_get_widescreen_scale() - 1.333333f) < 1e-5f);

    /* Set 21:9 ultra-wide scale factor (21/9 / 4/3 = 1.75) */
    ps1_gte_set_widescreen_scale(1.75f);
    CHECK(fabsf(ps1_gte_get_widescreen_scale() - 1.75f) < 1e-5f);

    /* Out of bounds values fallback safely to 1.0f */
    ps1_gte_set_widescreen_scale(0.1f);
    CHECK(fabsf(ps1_gte_get_widescreen_scale() - 1.0f) < 1e-5f);

    ps1_gte_set_widescreen_scale(10.0f);
    CHECK(fabsf(ps1_gte_get_widescreen_scale() - 1.0f) < 1e-5f);
}

static void test_hor_plus_3d_projection(void) {
    Ps1Gte gte;
    ps1_gte_init(&gte);
    ps1_gte_set_geom_offset(&gte, 160, 120); /* Screen center (160, 120) */
    ps1_gte_set_geom_screen(&gte, 1000);     /* Focal length h = 1000 */

    Ps1Vector3 v = {100, 50, 1000, 0}; /* 3D point in front of camera */

    /* 1. Authentic 4:3 projection (scale = 1.0f) */
    ps1_gte_set_widescreen_scale(1.0f);
    CHECK(ps1_gte_rtps(&gte, &v));

    Ps1SubpixelCoord sub43;
    ps1_gte_get_sub_sxy(&gte, 2, &sub43);
    float offset_x_43 = sub43.x - 160.0f;
    float offset_y_43 = sub43.y - 120.0f;

    CHECK(offset_x_43 > 0.0f);
    CHECK(offset_y_43 > 0.0f);

    /* 2. 16:9 Hor+ projection (scale = 1.333333f) */
    float scale_16_9 = (16.0f / 9.0f) / (4.0f / 3.0f);
    ps1_gte_set_widescreen_scale(scale_16_9);
    CHECK(ps1_gte_rtps(&gte, &v));

    Ps1SubpixelCoord sub169;
    ps1_gte_get_sub_sxy(&gte, 2, &sub169);
    float offset_x_169 = sub169.x - 160.0f;
    float offset_y_169 = sub169.y - 120.0f;

    /* Hor+ FOV expansion compresses horizontal screen-space coordinates by 1/scale
     * so that a 33% wider 3D angle is captured across the view */
    float expected_offset_x = offset_x_43 / scale_16_9;
    CHECK(fabsf(offset_x_169 - expected_offset_x) < 0.1f);

    /* Vertical coordinates MUST remain completely unaffected (pure Hor+, no Vert- cropping) */
    CHECK(fabsf(offset_y_169 - offset_y_43) < 0.01f);

    /* Depth (Z and W) MUST remain identical */
    CHECK(fabsf(sub169.z - sub43.z) < 0.01f);
    CHECK(fabsf(sub169.w - sub43.w) < 1e-6f);

    /* Reset scale to 1.0f */
    ps1_gte_set_widescreen_scale(1.0f);
}

static void test_host_display_widescreen_modes(void) {
    host_display_set_widescreen_mode(WIDESCREEN_OFF);
    CHECK(host_display_get_widescreen_mode() == WIDESCREEN_OFF);

    host_display_set_widescreen_mode(WIDESCREEN_ON);
    CHECK(host_display_get_widescreen_mode() == WIDESCREEN_ON);

    host_display_set_widescreen_mode(WIDESCREEN_STRETCH);
    CHECK(host_display_get_widescreen_mode() == WIDESCREEN_STRETCH);

    host_display_set_widescreen_mode(WIDESCREEN_AUTO);
    CHECK(host_display_get_widescreen_mode() == WIDESCREEN_AUTO);
}

int main(void) {
    test_widescreen_scale_get_set();
    test_hor_plus_3d_projection();
    test_host_display_widescreen_modes();

    printf("ps1_widescreen_test: All True Widescreen Hor+ FOV tests passed successfully.\n");
    return 0;
}
