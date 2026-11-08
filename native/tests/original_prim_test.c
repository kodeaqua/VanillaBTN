#include "sdk_gpu.h"
#include "prim.h"
#include "sdk_work.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static Ps1Video video;
static u_long packets[64], ot[4];

int main(void) {
    _Static_assert(sizeof(TILE) == 16, "TILE wire layout");
    _Static_assert(sizeof(POLY_FT4) == 40, "FT4 wire layout");
    _Static_assert(sizeof(SPRT) == 20, "SPRT wire layout");
    sdk_gpu_bind(&video);
    assert(sdk_gpu_map(packets, sizeof(packets)));
    assert(sdk_gpu_map(ot, sizeof(ot)));
    assert(!sdk_gpu_map(packets + 1, 4));
    ClearOTagR(ot, 4);
    prim_make_tile((TILE *)packets, ot + 3, 10, 10, 4, 4, 0xFF0000, 0);
    prim_make_tile((TILE *)(packets + 4), ot, 11, 11, 1, 1, 0x0000FF, 0);
    prim_make_pixel((struct Tile1 *)(packets + 8), ot, 12, 12, 0x00FF00, 0);
    DrawOTag(ot + 3);
    assert(sdk_gpu_ok());
    assert(video.vram[10][10] == 0x001F);
    assert(video.vram[11][11] == 0x7C00);
    assert(video.vram[12][12] == 0x03E0);
    assert((packets[0] >> 24) == 3);
    assert(packets[1] == 0x600000FFul); /* Original builder's actual GP0 bytes. */
    /* Newest packet in a bucket is drawn first; older packets cover it. */
    ClearOTagR(ot, 4);
    prim_make_tile((TILE *)packets, ot, 20, 20, 1, 1, 0xFF0000, 0);
    prim_make_tile((TILE *)(packets + 4), ot, 20, 20, 1, 1, 0x0000FF, 0);
    DrawOTag(ot + 3);
    assert(video.vram[20][20] == 0x001F);
    ClearOTagR(ot, 4);
    assert(sdk_work_bind(packets, sizeof(packets)));
    prim_add_tile(ot, 30, 30, 2, 2, 0x00FF00, 0);
    prim_add_pixel(ot, 33, 33, 0xFF0000, 0);
    DrawOTag(ot + 3);
    assert(sdk_gpu_ok() && video.vram[30][30] == 0x03E0);
    assert(video.vram[33][33] == 0x001F);
    /* Neither forged links, oversized packets nor cycles may escape mappings. */
    ot[3] = 0x123456;
    DrawOTag(ot + 3);
    assert(!sdk_gpu_ok());
    sdk_gpu_reset_packets();
    assert(sdk_gpu_map(ot, sizeof(ot)));
    ClearOTagR(ot, 4);
    ot[1] = ot[2]; /* Self link to ot[1]. */
    DrawOTag(ot + 3);
    assert(!sdk_gpu_ok());
    sdk_gpu_reset_packets();
    assert(sdk_gpu_map(packets, 4));
    assert(sdk_gpu_map(ot, sizeof(ot)));
    ClearOTagR(ot, 4);
    packets[0] = 3ul << 24;
    addPrim(ot, packets);
    assert(!sdk_gpu_ok());
    sdk_gpu_bind(NULL);
    assert(sdk_work_bind(NULL, 0));
    return 0;
}
