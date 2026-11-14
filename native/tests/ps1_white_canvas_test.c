/* Pure-white 4:3 UI canvases may extend neutral white to the unused margins
 * without mirroring a map, while actual world pixels remain authoritative. */
#include "ps1_video.h"
#include <stdio.h>
#include <stdlib.h>

static Ps1Video video;
static uint32_t sample(const uint32_t *frame, int width, int x, int y) {
    return frame[(size_t)y * width + (width - 320) / 2 + x];
}

int main(void) {
    if (!ps1_video_configure_panorama(320, 240, 4.0f / 3.0f)) return 1;
    ps1_video_set_true_color(true);
    ps1_video_panorama_clear_rows(240, 240);
    int width = ps1_video_panorama_width(320);
    uint32_t *frame = malloc((size_t)width * 240 * sizeof(*frame));
    if (!frame) return 2;
    for (int y = 240; y < 480; ++y) {
        for (int x = 0; x < 320; ++x)
            video.color32[y][x] = 0xFFFFFFFFu;
        video.color32[y][100] = 0xFF0000FFu; /* non-repeatable map art */
        if ((y - 240) % 4 == 0 && (y - 240) >= 160)
            video.color32[y][319] = 0xFF33AA77u;
    }
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) return 3;
    if (sample(frame, width, -20, 80) != 0xFFFFFFFFu ||
        sample(frame, width, 330, 80) != 0xFFFFFFFFu ||
        sample(frame, width, 100, 80) != 0xFF0000FFu) return 4;
    /* A colorful border must never result in an invented white canvas. */
    for (int y = 240; y < 480; ++y) video.color32[y][0] = 0xFF004400u;
    if (!ps1_video_copy_panorama(&video, 0, 240, 320, 240, frame, width)) return 5;
    if (sample(frame, width, -20, 80) != 0xFF000000u) return 6;
    free(frame);
    ps1_video_configure_panorama(0, 0, 1.0f);
    puts("ps1_white_canvas_test: white map backdrop without mirrored world art");
    return 0;
}
