/* End-to-end GP0 packet provenance: native Hor+ only extends actual world
 * geometry, while unprojected HUD packets remain within the 4:3 viewport. */
#include "ps1_gpu.h"
#include "ps1_pgxp.h"
#include "ps1_video.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Video video;

static uint32_t xy(int x, int y) {
    return (uint16_t)x | ((uint32_t)(uint16_t)y << 16);
}

static int run(void) {
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    ps1_video_set_pgxp_vertex_precision(true);
    ps1_video_set_true_color(true);
    ps1_pgxp_init();
    Ps1DrawState state;
    ps1_draw_state_reset(&state);
    state.left = 0;
    state.right = 319;
    state.top = 240;
    state.bottom = 479;
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = calloc((size_t)width * 240, sizeof(*frame));
    if (!frame) return 2;
    int margin = (width - 320) / 2;

    /* A triangle crossing the original viewport: PGXP identifies all three
     * xy packet fields as projected, so pixels entirely outside the old
     * clipping boundary are rasterized as real world geometry. */
    uint32_t triangle[] = {
        0x2000FF00u, xy(322, 275), xy(355, 275), xy(322, 325)
    };
    for (int i = 1; i <= 3; ++i) {
        ps1_pgxp_register(&triangle[i], (float)(int16_t)triangle[i],
            (float)(int16_t)(triangle[i] >> 16), 100.0f, 0.01f, triangle[i]);
    }
    ps1_video_panorama_begin_frame();
    if (!ps1_gpu_exec(&video, &state, triangle, 4) ||
        !ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, (size_t)width)) {
        free(frame);
        return 3;
    }
    if (frame[(size_t)42 * width + (margin + 328)] != 0xFF00FF00u) {
        free(frame);
        return 4;
    }
    if (video.vram[282][328] != 0) { /* Texture VRAM must not be drawn to. */
        free(frame);
        return 5;
    }

    /* The same packet without projected provenance is HUD geometry: keep
     * original 4:3 clipping and do not invent world pixels at x >= 320. */
    ps1_pgxp_clear();
    memset(&video, 0, sizeof(video));
    ps1_video_panorama_begin_frame();
    if (!ps1_gpu_exec(&video, &state, triangle, 4) ||
        !ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, (size_t)width)) {
        free(frame);
        return 6;
    }
    if (frame[(size_t)42 * width + (margin + 328)] == 0xFF00FF00u) {
        free(frame);
        return 7;
    }
    /* 2D packets are allowed into the panorama ONLY when the game marks
     * them as an authored scrolling backdrop. This is what lets the title
     * screen use its real off-screen tiles instead of fake edge colors. */
    ps1_video_panorama_begin_frame();
    uint32_t background_tile[] = {
        0x600000FFu, xy(330, 282), (16u << 16) | 16u
    };
    ps1_video_set_2d_widescreen_expansion(true);
    bool background_ok = ps1_gpu_exec(&video, &state, background_tile, 3);
    ps1_video_set_2d_widescreen_expansion(false);
    if (!background_ok ||
        !ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, (size_t)width)) {
        free(frame); return 8;
    }
    if (frame[(size_t)48 * width + (margin + 336)] != 0xFFFF0000u) {
        free(frame); return 9;
    }
    ps1_video_panorama_begin_frame();
    if (!ps1_gpu_exec(&video, &state, background_tile, 3) ||
        !ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, (size_t)width)) {
        free(frame); return 10;
    }
    if (frame[(size_t)48 * width + (margin + 336)] != 0xFF000000u) {
        free(frame); return 11;
    }
    free(frame);
    ps1_video_configure_panorama(0, 0, 1.0f);
    return 0;
}

int main(void) {
    int code = run();
    if (code != 0) fprintf(stderr, "ps1_horplus_packets_test failed at step %d\n", code);
    else puts("ps1_horplus_packets_test: projected 3D and explicit background 2D extend; HUD stays 4:3");
    return code;
}
