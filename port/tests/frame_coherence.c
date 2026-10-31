#include "gfx/frame_replay.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_links.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static hm_vram_context private_vram;
static hm_render_frame capture(void) {
    hm_vram_reset();
    ResetGraph(0);
    DRAWENV draw;
    DISPENV display;
    SetDefDrawEnv(&draw, 0, 0, 32, 32);
    draw.dfe = 1;
    PutDrawEnv(&draw);
    SetDefDispEnv(&display, 0, 0, 32, 32);
    PutDispEnv(&display);
    hm_render_frame live = {.key = 7, .tick = 2};
    u32 tile[] = {0x600000ff, 0, 0x00100010};
    hm_render_begin(&live);
    hm_gpu_execute_words(tile, 3);
    hm_render_packet(tile, 3);
    hm_render_end();
    return live;
}
int main(void) {
    hm_frame_replay *replay = hm_frame_replay_acquire();
    CHECK(replay);
    hm_render_frame live = capture();
    u64 serial = hm_frame_replay_serial(replay, &live);
    CHECK(serial);
    live.tick = 600; /* Long CD/audio wait without a framebuffer change. */
    CHECK(hm_frame_replay_serial(replay, &live) == serial);
    RECT viewport = {0, 0, 32, 32}, atlas = {896, 490, 1, 1}, other = {0, 240, 32, 32};
    u32 pixels[2] = {0x12341234, 0x12341234}, readback[512];
    LoadImage(&atlas, pixels);
    MoveImage(&atlas, 900, 490);
    ClearImage(&other, 0, 0, 0);
    StoreImage(&viewport, readback);
    CHECK(hm_frame_replay_serial(replay, &live) == serial);
    hm_vram_snapshot(&private_vram);
    hm_vram_context *native = hm_vram_bind(&private_vram);
    ClearImage(&viewport, 0, 0, 0);
    LoadImage(&viewport, readback);
    MoveImage(&atlas, 0, 0);
    hm_vram_reset();
    hm_vram_bind(native);
    CHECK(hm_frame_replay_serial(replay, &live) == serial);
    ClearImage(&viewport, 0, 0, 0);
    CHECK(!hm_frame_replay_serial(replay, &live));
    live = capture();
    RECT pixel = {0, 0, 1, 1};
    LoadImage(&pixel, pixels);
    CHECK(!hm_frame_replay_serial(replay, &live));
    live = capture();
    MoveImage(&atlas, 0, 0);
    CHECK(!hm_frame_replay_serial(replay, &live));
    live = capture();
    RECT wrapped = {1023, 511, 2, 2};
    LoadImage(&wrapped, pixels);
    CHECK(!hm_frame_replay_serial(replay, &live));
    /* Direct VRAM operations inside a captured OT are preserved as GP0 events. */
    live = capture();
    hm_render_begin(&live);
    RECT direct_rect = {0, 0, 16, 16};
    ClearImage(&direct_rect, 0, 0, 0);
    u32 nop = 0;
    hm_render_packet(&nop, 1);
    hm_render_end();
    CHECK(hm_frame_replay_serial(replay, &live));
    /* Captures grow past the former 64K-word and 64-group fixed limits. */
    hm_render_frame large = {.key = 8, .tick = 4};
    u32 nops[1024] = {0};
    hm_render_begin(&large);
    for (int i = 0; i < 140; ++i) {
        hm_render_group(i & 1);
        hm_render_packet(nops, 1024);
    }
    hm_render_end();
    CHECK(hm_frame_replay_serial(replay, &large));
    hm_render_frame bypass = {.key = 9, .tick = 6, .bypass = 1};
    hm_render_begin(&bypass);
    hm_render_packet(nops, 3);
    hm_render_end();
    CHECK(hm_frame_replay_serial(replay, &bypass));
    live = capture();
    u32 direct[] = {3u << 24 | HM_GPU_LINK_END, 0x6000ff00, 0, 0x00010001};
    DrawOTag(direct); /* Loading draws outside the normal capture hook. */
    CHECK(hm_frame_replay_serial(replay, &live));
    live = capture();
    hm_vram_reset();
    CHECK(!hm_frame_replay_serial(replay, &live));
    const char *failure = hm_frame_replay_failure(replay);
    CHECK(failure && strstr(failure, "no complete captured scene") != NULL);
    hm_frame_replay_release(replay);
    return 0;
}
