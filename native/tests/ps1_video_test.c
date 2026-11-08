#include "ps1_video.h"

#include <stdint.h>

static Ps1Video video;

int main(void) {
    const uint16_t source[2][3] = {
        {0x001F, 0x03E0, 0x7C00},
        {0x7FFF, 0x801F, 0x0000},
    };
    uint32_t target[2][4] = {{0}};

    if (!ps1_video_upload(&video, 2, 240, 3, 2, &source[0][0], 3)
        || !ps1_video_copy_argb(&video, 2, 240, 3, 2, &target[0][0], 4)) {
        return 1;
    }
    if (target[0][0] != 0xFFFF0000u || target[0][1] != 0xFF00FF00u
        || target[0][2] != 0xFF0000FFu || target[1][0] != 0xFFFFFFFFu
        || target[1][1] != 0xFFFF0000u || target[1][2] != 0xFF000000u
        || target[0][3] != 0 || target[1][3] != 0) {
        return 2;
    }
    if (ps1_video_upload(&video, 1023, 0, 2, 1, &source[0][0], 3)
        || ps1_video_upload(&video, 0, 0, 3, 1, &source[0][0], 2)
        || ps1_video_copy_argb(&video, -1, 0, 1, 1, &target[0][0], 4)
        || ps1_video_copy_argb(&video, 0, 0, 3, 1, &target[0][0], 2)) {
        return 3;
    }
    return 0;
}
