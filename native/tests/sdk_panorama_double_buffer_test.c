/* Integration of real original-game draw/display env sequencing with OT.
 * The displayed framebuffer is opposite to the one being redrawn. */
#include "sdk_gpu.h"
#include <libgpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;
static u_long ot[4];
static u_long tile[4];

static int draw_frame(int dest_y, int x, unsigned gp0_rgb, uint32_t *frame, size_t width) {
    DRAWENV draw;
    DISPENV display;
    sdk_gpu_begin_frame();
    /* The game displays the other page while rendering this page. */
    if (!SetDefDispEnv(&display, 0, dest_y ? 0 : 240, 320, 240) ||
        !SetDefDrawEnv(&draw, 0, dest_y, 320, 240) ||
        !PutDispEnv(&display) || !PutDrawEnv(&draw)) return 1;
    ClearOTagR(ot, 4);
    tile[0] = 3u << 24;
    tile[1] = (gp0_rgb & 0xFFFFFFu) | 0x60000000u;
    tile[2] = (uint16_t)x | ((uint32_t)35 << 16);
    tile[3] = (8u << 16) | 20u;
    sdk_gpu_set_2d_widescreen_expansion(true);
    addPrim(ot + 3, tile);
    sdk_gpu_set_2d_widescreen_expansion(false);
    DrawOTag(ot + 3);
    if (!sdk_gpu_ok()) return 2;
    SdkDisplay disp = sdk_gpu_display();
    if (!ps1_video_copy_panorama(&video, disp.x, disp.y, disp.width, disp.height,
                                 frame, width)) return 3;
    return 0;
}

int main(void) {
    sdk_gpu_bind(&video);
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    if (!sdk_gpu_map(ot, sizeof(ot)) || !sdk_gpu_map(tile, sizeof(tile))) return 2;
    size_t width = (size_t)ps1_video_panorama_width(320);
    uint32_t *frame = calloc(width * 240, sizeof(uint32_t));
    if (!frame) return 3;
    const size_t r = (size_t)35 * width;
    const size_t left = (width - 320) / 2;
    /* First render A (right red), B remains empty and is displayed. */
    if (draw_frame(0, 329, 0x0000FFu, frame, width)) return 4;
    if (frame[r + left + 333] != 0xFF000000u) return 5;
    if (!ps1_video_copy_panorama(&video, 0, 0, 320, 240, frame, width)
        || frame[r + left + 333] != 0xFFFF0000u) return 6;
    /* Render B (left green), display A. A must retain the red side pixel. */
    if (draw_frame(240, -26, 0x00FF00u, frame, width)) return 7;
    if (frame[r + left + 333] != 0xFFFF0000u) return 8;
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)
        || frame[r + left - 20] != 0xFF00FF00u) return 9;
    /* Frame A is reused and cleared; previously displayed B stays visible. */
    if (draw_frame(0, 329, 0xFF0000u, frame, width)) return 10;
    if (frame[r + left - 20] != 0xFF00FF00u) return 11;
    if (!ps1_video_copy_panorama(&video, 0, 0, 320, 240, frame, width)
        || frame[r + left + 333] != 0xFF0000FFu) return 12;
    free(frame);
    ps1_video_configure_panorama(0, 0, 1.0f);
    sdk_gpu_bind(NULL);
    puts("sdk_panorama_double_buffer_test: OT draw/display swap keeps previous page");
    return 0;
}
