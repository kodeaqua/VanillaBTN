#include "ps1_gte.h"
#include "host_display.h"
#include "config.h"
#include "sdk_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "Assertion failed at line %d: %s\n", __LINE__, #cond); \
    exit(1); \
} } while (0)

static Ps1Video video;

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

static void test_projected_sprite_visibility_band(void) {
    ps1_gte_set_widescreen_scale(1.0f);
    CHECK(ps1_gte_screen_band_contains(-49, -49, 419));
    CHECK(ps1_gte_screen_band_contains(369, -49, 419));
    CHECK(!ps1_gte_screen_band_contains(-50, -49, 419));
    CHECK(!ps1_gte_screen_band_contains(370, -49, 419));

    ps1_gte_set_widescreen_scale(4.0f / 3.0f);
    CHECK(ps1_gte_screen_band_contains(-100, -49, 419));
    CHECK(ps1_gte_screen_band_contains(420, -49, 419));
    CHECK(!ps1_gte_screen_band_contains(-103, -49, 419));
    CHECK(!ps1_gte_screen_band_contains(423, -49, 419));

    ps1_gte_set_widescreen_scale(4.0f);
    CHECK(ps1_gte_screen_band_contains(-529, -49, 419));
    CHECK(!ps1_gte_screen_band_contains(-530, -49, 419));
    ps1_gte_set_widescreen_scale(1.0f);
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

    /* Hor+ extends the visible world bounds, never changes the projected
     * position of an existing rock/player vertex. R2's inverse squeezing
     * caused world objects to drift relative to the shipping bin. */
    CHECK(fabsf(offset_x_169 - offset_x_43) < 0.01f);

    /* Vertical coordinates MUST remain completely unaffected (pure Hor+, no Vert- cropping) */
    CHECK(fabsf(offset_y_169 - offset_y_43) < 0.01f);

    /* Depth (Z and W) MUST remain identical */
    CHECK(fabsf(sub169.z - sub43.z) < 0.01f);
    CHECK(fabsf(sub169.w - sub43.w) < 1e-6f);

    /* Reset scale to 1.0f */
    ps1_gte_set_widescreen_scale(1.0f);
}

static void test_fixed_point_hor_plus_projection(void) {
    Ps1Gte gte;
    Ps1Vector3 v = {100, 50, 1000, 0};
    Ps1SubpixelCoord base, wide;
    const float scale = (16.0f / 9.0f) / (4.0f / 3.0f);

    ps1_gte_init(&gte);
    ps1_gte_set_geom_offset(&gte, 160, 120);
    ps1_gte_set_geom_screen(&gte, 1000);
    ps1_gte_set_float_math(false);

    ps1_gte_set_widescreen_scale(1.0f);
    CHECK(ps1_gte_rtps(&gte, &v));
    ps1_gte_get_sub_sxy(&gte, 2, &base);
    ps1_gte_set_widescreen_scale(scale);
    CHECK(ps1_gte_rtps(&gte, &v));
    ps1_gte_get_sub_sxy(&gte, 2, &wide);
    CHECK(fabsf(wide.x - base.x) < 0.01f);

    ps1_gte_set_float_math(true);
    ps1_gte_set_widescreen_scale(1.0f);
}

static void test_horizontal_frustum_expands_with_output_aspect(void) {
    const int32_t base_half_angle = 108;
    const double units_per_radian = 4096.0 / (2.0 * 3.14159265358979323846);
    const double base_radians = (double)base_half_angle / units_per_radian;
    const float scale = (16.0f / 9.0f) / (4.0f / 3.0f);

    ps1_gte_set_widescreen_scale(1.0f);
    CHECK(ps1_gte_scale_frustum_half_angle(base_half_angle) == base_half_angle);
    ps1_gte_set_widescreen_scale(scale);
    int32_t wide_half_angle = ps1_gte_scale_frustum_half_angle(base_half_angle);
    CHECK(wide_half_angle > base_half_angle);
    double wide_radians = (double)wide_half_angle / units_per_radian;
    CHECK(fabs(tan(wide_radians) - tan(base_radians) * (double)scale) < 0.003);
    ps1_gte_set_widescreen_scale(1.0f);
}

static void test_host_display_applies_widescreen_before_render(void) {
    CHECK(SDL_Init(0));
    SDL_Surface *surface = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_ARGB8888);
    CHECK(surface != NULL);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    CHECK(renderer != NULL);
    sdk_gpu_bind(&video);

    host_display_set_widescreen_mode(WIDESCREEN_ON);
    CHECK(host_display_update_widescreen(renderer));
    float expected_scale = (1280.0f / 720.0f) / (4.0f / 3.0f);
    CHECK(fabsf(ps1_gte_get_widescreen_scale() - expected_scale) < 1e-5f);
    CHECK(fabsf(ps1_video_get_widescreen_scale() - 1.0f) < 1e-5f);
    CHECK(ps1_video_panorama_width(320) == 427);

    host_display_set_widescreen_mode(WIDESCREEN_OFF);
    CHECK(host_display_update_widescreen(renderer));
    CHECK(fabsf(ps1_gte_get_widescreen_scale() - 1.0f) < 1e-5f);
    CHECK(fabsf(ps1_video_get_widescreen_scale() - 1.0f) < 1e-5f);
    CHECK(ps1_video_panorama_width(320) == 320);

    host_display_set_widescreen_mode(WIDESCREEN_AUTO);
    CHECK(host_display_update_widescreen(renderer));
    CHECK(fabsf(ps1_gte_get_widescreen_scale() - expected_scale) < 1e-5f);
    CHECK(ps1_video_panorama_width(320) == 427);
    CHECK(fabsf(ps1_video_get_widescreen_scale() - 1.0f) < 1e-5f);

    ps1_video_configure_panorama(0, 0, 1.0f);
    sdk_gpu_bind(NULL);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    SDL_Quit();
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

    /* 16:9 aspect ratio (e.g. 1280x720) resolves to an extended frustum,
     * not an anamorphic projection of the PS1 viewport. */
    float aspect_16_9 = 1280.0f / 720.0f;
    CHECK(aspect_16_9 >= 1.55f);
    float fov_scale = aspect_16_9 / (4.0f / 3.0f);
    CHECK(fabsf(fov_scale - 1.333333f) < 0.001f);

    /* 4:3 aspect ratio (e.g. 1280x960) remains 1.0f */
    float aspect_4_3 = 1280.0f / 960.0f;
    CHECK(aspect_4_3 < 1.55f);
}

int main(void) {
    test_widescreen_scale_get_set();
    test_projected_sprite_visibility_band();
    test_hor_plus_3d_projection();
    test_fixed_point_hor_plus_projection();
    test_horizontal_frustum_expands_with_output_aspect();
    test_host_display_widescreen_modes();
    test_host_display_applies_widescreen_before_render();

    printf("ps1_widescreen_test: All True Widescreen Hor+ FOV tests passed successfully.\n");
    return 0;
}
