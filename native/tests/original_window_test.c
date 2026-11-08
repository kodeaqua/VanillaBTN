#include "sdk_gpu.h"
#include "sdk_work.h"
#include "window_frame.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

static Ps1Video video;
static u_long work[1024], ot[2];

int main(void) {
    sdk_gpu_bind(&video);
    CHECK(sdk_gpu_map(work, sizeof(work)));
    CHECK(sdk_work_bind(work, sizeof(work)));
    ClearOTagR(ot, 2);
    window_frame_draw(ot, 20, 20, 40, 30);
    DrawOTag(ot + 1);
    CHECK(sdk_gpu_ok());
    CHECK(video.vram[20][22] == 0x6BBF); /* Original light upper edge. */
    CHECK(video.vram[22][59] == 0x2237); /* Original dark right edge. */
    CHECK(video.vram[22][22] == 0x5F7E); /* Original gradient's first row. */
    CHECK(video.vram[45][22] != 0 && video.vram[45][22] != video.vram[22][22]);
    CHECK(video.vram[19][22] == 0 && video.vram[50][22] == 0);
    sdk_gpu_bind(NULL);
    CHECK(sdk_work_bind(NULL, 0));
    return 0;
}
