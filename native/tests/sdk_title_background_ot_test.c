/* The title queues its scrolling backdrop in 64-slot OT bucket 36 and its
 * matching color/background layer as SPRT parts in bucket 37. Both layers
 * must reach the host panorama, while bucket 34 logo/HUD stays untouched. */
#include "sdk_gpu.h"
#include <libgpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;
static u_long arena[64 + 14 * 5];
static u_long *ot = arena;
static u_long *packets = &arena[64];

static void sprite_packet(u_long *p, int x, int y) {
    p[0] = 4u << 24;
    p[1] = 0x65808080u;
    p[2] = (uint16_t)x | ((u_long)(uint16_t)y << 16);
    p[3] = 0;
    p[4] = (4u << 16) | 4u;
}

static uint32_t sample_point(int x, int y) {
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 0;
    uint32_t result = 0;
    if (ps1_video_copy_panorama(&video, 0, 0, 320, 240, frame, width))
        result = frame[(size_t)y * width + (width - 320) / 2 + x];
    free(frame);
    return result;
}

int main(void) {
    sdk_gpu_bind(&video);
    if (!sdk_gpu_map(arena, sizeof(arena)) ||
        !ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    DRAWENV env;
    if (!SetDefDrawEnv(&env, 0, 0, 320, 240)) return 2;
    env.tpage = 3u | (2u << 7);
    if (!PutDrawEnv(&env)) return 3;
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) video.vram[y][192 + x] = 0x03E0u;
    sdk_gpu_begin_frame();
    ClearOTagR(ot, 64);
    sdk_gpu_set_2d_widescreen_expansion(true);
    for (int i = 0; i < 12; ++i) {
        sprite_packet(&packets[i * 5], 150, 70);
        addPrim(ot + 36, &packets[i * 5]);
    }
    sdk_gpu_set_2d_widescreen_expansion(false);
    sprite_packet(&packets[12 * 5], 10, 35);
    addPrim(ot + 37, &packets[12 * 5]);
    /* Bucket 37 may contain multiple kinds of background packets, not just
     * a small SPRT fixture. The native marker must follow the entire layer. */
    u_long *color_tile = &packets[13 * 5];
    color_tile[0] = 3u << 24;
    color_tile[1] = 0x60FF0000u; /* GP0 BGR byte order: blue tile */
    color_tile[2] = (u_long)(uint16_t)331 | ((u_long)(uint16_t)55 << 16);
    color_tile[3] = (7u << 16) | 8u;
    addPrim(ot + 37, color_tile);
    DrawOTag(ot + 63);
    if (!sdk_gpu_ok() || sample_point(330, 36) != 0xFF00FF00u ||
        sample_point(333, 57) != 0xFF0000FFu) {
        fprintf(stderr, "title: ok=%d green=%08x blue=%08x\n", sdk_gpu_ok(), sample_point(330,36), sample_point(333,57));
        return 4;
    }
    /* Same packet and bucket without the marked backdrop must not leak its
     * previous frame's expansion attribute. */
    sdk_gpu_begin_frame();
    ps1_video_panorama_clear_rows(0, 240);
    ClearOTagR(ot, 64);
    sprite_packet(&packets[12 * 5], 10, 35);
    addPrim(ot + 37, &packets[12 * 5]);
    DrawOTag(ot + 63);
    if (!sdk_gpu_ok() || sample_point(330, 36) != 0xFF000000u) return 5;
    ps1_video_configure_panorama(0, 0, 1.0f);
    sdk_gpu_bind(NULL);
    puts("sdk_title_background_ot_test: title color/backdrop bucket follows 2D extension");
    return 0;
}
