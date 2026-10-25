#include "psx/gpu_environment.h"
#include "psx/vram.h"
#include <inline_c.h>
#include <libgs.h>
#include <stdio.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    hm_vram_reset();
    RECT pixel = {0, 0, 1, 1};
    u32 value = 0x1234;
    LoadImage(&pixel, &value);
    CHECK(ResetGraph(0) == 0 && hm_vram_pixel(0, 0) == 0x1234);
    const hm_gpu_environment *state = hm_gpu_environment_state();
    CHECK(!state->display_enabled);
    DRAWENV draw;
    CHECK(SetDefDrawEnv(&draw, 16, 32, 320, 240) == &draw);
    CHECK(draw.clip.x == 16 && draw.clip.h == 240 && draw.ofs[1] == 32 && draw.dtd == 1);
    draw.isbg = 1;
    draw.r0 = 255;
    CHECK(PutDrawEnv(&draw) == &draw && hm_vram_pixel(16, 32) == 31 &&
          hm_vram_pixel(335, 271) == 31);
    CHECK(hm_vram_pixel(0, 0) == 0x1234 && hm_vram_pixel(336, 32) == 0);
    DISPENV display;
    CHECK(SetDefDispEnv(&display, 0, 240, 320, 240) == &display);
    CHECK(PutDispEnv(&display) == &display && state->display.disp.y == 240);
    CHECK(SetDispMask(1) == 0 && state->display_enabled);
    GsInitGraph(320, 240, 0, 0, 0);
    GsSetProjection(160);
    CHECK(hm_gte.distance == 160);
    GsF_LIGHT light = {0, 0, 100, 208, 128, 64, 0};
    GsSetFlatLight(0, &light);
    CHECK(hm_gte.matrix[1][0][2] == -4096 && hm_gte.matrix[1][0][0] == 0);
    CHECK(hm_gte.matrix[2][0][0] == 3328 && hm_gte.matrix[2][1][0] == 2048);
    GsSetAmbient(2048, 2048, 2048);
    CHECK(hm_gte.translation[1][0] == 2048 && hm_gte.translation[1][2] == 2048);
    GsSetLightMode(0);
    GsDefDispBuff(0, 0, 0, 240);
    GsSetOffset(10, 20);
    GsSetDrawBuffOffset();
    CHECK(state->draw.ofs[0] == 10 && state->draw.ofs[1] == 20);
    CHECK(state->width == 320 && state->height == 240);
    u32 work[16];
    GsSetWorkBase(work);
    CHECK(GsGetWorkBase() == work);
    return 0;
}
