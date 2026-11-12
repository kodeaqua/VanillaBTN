/* Regression: a background flag is captured when the packet is queued.
 * Replaying the OT after the authoring code disables the flag must not
 * squeeze the background into the widescreen 4:3 HUD safe area. */
#include "sdk_gpu.h"
#include <libgpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;
static u_long ot[4];
static u_long packets[8];

int main(void) {
    sdk_gpu_bind(&video);
    if (!sdk_gpu_map(ot, sizeof(ot)) || !sdk_gpu_map(packets, sizeof(packets))) return 1;
    DRAWENV env;
    if (!SetDefDrawEnv(&env, 0, 0, 320, 240) || !PutDrawEnv(&env)) return 2;
    ps1_video_set_widescreen_scale(4.0f / 3.0f, 160.0f);
    sdk_gpu_begin_frame();
    ClearOTagR(ot, 4);

    /* 24x8 red background tile at the physical left screen edge. */
    packets[0] = 3u << 24;
    packets[1] = 0x600000FFu;
    packets[2] = 0u;
    packets[3] = (8u << 16) | 24u;
    sdk_gpu_set_2d_widescreen_expansion(true);
    addPrim(ot + 3, packets);

    /* 24x8 green UI tile at x=100, in the centered HUD safe area. */
    packets[4] = 3u << 24;
    packets[5] = 0x6000FF00u;
    packets[6] = 100u;
    packets[7] = (8u << 16) | 24u;
    sdk_gpu_set_2d_widescreen_expansion(false);
    addPrim(ot + 3, packets + 4);

    /* Both packets are drawn AFTER expansion has been turned off. */
    DrawOTag(ot + 3);
    if (!sdk_gpu_ok()) return 3;
    if (video.vram[2][0] != 0x001Fu || video.vram[2][23] != 0x001Fu) return 4;
    if (video.vram[2][24] != 0) return 5;
    if (video.vram[2][114] != 0 || video.vram[2][115] != 0x03E0u
        || video.vram[2][132] != 0x03E0u || video.vram[2][133] != 0) return 6;

    /* Frame-local packet attributes may not leak into reused packet addresses. */
    sdk_gpu_begin_frame();
    memset(&video.vram[2][0], 0, 320 * sizeof(video.vram[2][0]));
    ClearOTagR(ot, 4);
    packets[0] = 3u << 24;
    addPrim(ot + 3, packets); /* default safe-area behaviour */
    DrawOTag(ot + 3);
    if (!sdk_gpu_ok() || video.vram[2][0] != 0 || video.vram[2][40] != 0x001Fu) return 7;
    /* This is the real OT packet path used by the title/memory-card screens.
     * A queued background tile outside the PS1 clip should enter the native
     * panorama, but its unmarked version must remain absent next frame. */
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 8;
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 9;
    sdk_gpu_begin_frame();
    ClearOTagR(ot, 4);
    packets[0] = 3u << 24;
    packets[1] = 0x600000FFu; /* GP0 RGB is BGR in the packet */
    packets[2] = 2u << 16 | 330u;
    packets[3] = 8u << 16 | 16u;
    sdk_gpu_set_2d_widescreen_expansion(true);
    addPrim(ot + 3, packets);
    sdk_gpu_set_2d_widescreen_expansion(false);
    DrawOTag(ot + 3);
    if (!sdk_gpu_ok() || !ps1_video_copy_panorama(&video, 0, 0, 320, 240,
                                                  frame, (size_t)width)) {
        free(frame); return 10;
    }
    int margin = (width - 320) / 2;
    if (frame[(size_t)4 * width + margin + 334] != 0xFFFF0000u) {
        free(frame); return 11;
    }
    /* The next frame does not implicitly clear framebuffer memory. Clear it
     * explicitly before checking that reused packet IDs no longer extend. */
    RECT clear_page = {0, 0, 320, 240};
    if (ClearImage(&clear_page, 0, 0, 0) != 0) { free(frame); return 13; }
    sdk_gpu_begin_frame();
    ClearOTagR(ot, 4);
    addPrim(ot + 3, packets); /* same address, no background marker */
    DrawOTag(ot + 3);
    if (!sdk_gpu_ok() || !ps1_video_copy_panorama(&video, 0, 0, 320, 240,
                                                  frame, (size_t)width) ||
        frame[(size_t)4 * width + margin + 334] != 0xFF000000u) {
        free(frame); return 12;
    }
    free(frame);
    ps1_video_configure_panorama(0, 0, 1.0f);
    ps1_video_set_widescreen_scale(1.0f, 160.0f);
    sdk_gpu_bind(NULL);
    puts("sdk_background_packet_test: queued background flags and true panorama coverage respected");
    return 0;
}
