/* SDK-level regression: the title's already-authored 3x3 scrolling sprites
 * are opted into the panorama but must not be repeated per packet. Other
 * explicitly marked menu backgrounds retain legacy repetition behavior. */
#include "sdk_gpu.h"
#include <libgpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;
static u_long arena[64 + 12 * 5];

static void add_sprite(u_long *p, int x) {
    p[0] = 4u << 24;
    p[1] = 0x66808080u; /* semitransparent textured SPRT */
    p[2] = (u_long)(u_short)x | (40u << 16);
    p[3] = 0;
    p[4] = (64u << 16) | 64u;
    addPrim(arena + 36, p);
}

int main(void) {
    sdk_gpu_bind(&video);
    if (!sdk_gpu_map(arena, sizeof(arena)) ||
        !ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    DRAWENV draw;
    if (!SetDefDrawEnv(&draw, 0, 240, 320, 240)) return 2;
    draw.tpage = 3u | (2u << 7);
    if (!PutDrawEnv(&draw)) return 3;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            video.vram[y][192 + x] = 0xFFFFu;
    const uint32_t background = 0x206020u;
    RECT rect = {0, 240, 320, 240};
    if (ClearImage(&rect, 0x20, 0x60, 0x20) != 0) return 4;

    sdk_gpu_begin_frame();
    ClearOTagR(arena, 64);
    sdk_gpu_set_2d_widescreen_expansion(true);
    add_sprite(arena + 64, -64);
    add_sprite(arena + 69, 256);
    for (int i = 2; i < 12; ++i) add_sprite(arena + 64 + i * 5, 700);
    sdk_gpu_set_2d_widescreen_expansion(false);
    DrawOTag(arena + 63);
    if (!sdk_gpu_ok()) return 5;
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 6;
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) return 7;
    int pad = (width - 320) / 2;
    const uint32_t left = frame[60u * width + (unsigned)(pad - 20)];
    const uint32_t right = frame[60u * width + (unsigned)(pad + 280)];
    if (left != right || left == 0xFF000000u || left == (0xFF000000u | background)) {
        fprintf(stderr, "title OT overlay mismatch %08x vs %08x\n", left, right);
        return 8;
    }
    free(frame);
    ps1_video_configure_panorama(0, 0, 1.0f);
    sdk_gpu_bind(NULL);
    puts("sdk_title_scroll_ot_test: authored title panorama no double blend");
    return 0;
}
