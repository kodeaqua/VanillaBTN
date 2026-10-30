#include "enhance/interpolation/interpolation.h"
#include "gfx/gpu_scene.h"
#include "gfx/packet_identity.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static u32 polygons[2][10], sprites[2][9];
static u32 xy(int x, int y) { return (u16)x | ((u32)(u16)y << 16); }
static hm_render_frame hm_test_begin(int slot, u64 tick, const void *arena, size_t size) {
    DRAWENV env;
    SetDefDrawEnv(&env, 0, slot * 240, 32, 32);
    env.dtd = 0;
    env.isbg = 1;
    PutDrawEnv(&env);
    hm_packet_identity_reset();
    return (hm_render_frame){
        .key = 1, .tick = tick, .eligible = 1, .arena = {arena, NULL}, .arena_bytes = {size, 0}};
}
static hm_render_frame hm_test_mesh(int slot, u64 tick, int shift, int changed, int inconsistent) {
    hm_render_frame frame = hm_test_begin(slot, tick, polygons[slot], sizeof(polygons[slot]));
    u32 *p = polygons[slot];
    p[0] = 0x280000ff;
    p[5] = changed ? 0x28ff0000 : 0x2800ff00;
    int left = 2 + shift, right = 6 + shift;
    p[1] = xy(left, 2);
    p[2] = xy(left + 4, 2);
    p[3] = xy(left, 6);
    p[4] = xy(left + 4, 6);
    if (inconsistent)
        ++right;
    p[6] = xy(right, 2);
    p[7] = xy(right + 4, 2);
    p[8] = xy(right, 6);
    p[9] = xy(right + 4, 6);
    hm_render_begin(&frame);
    hm_render_group(1);
    hm_gpu_execute_words(p, 10);
    hm_render_packet(p, 5);
    hm_render_packet(p + 5, 5);
    hm_render_end();
    return frame;
}
static hm_render_frame hm_test_cells(int slot, u64 tick) {
    hm_render_frame frame = hm_test_begin(slot, tick, sprites[slot], sizeof(sprites[slot]));
    int count = slot ? 3 : 2;
    for (int i = 0; i < count; ++i) {
        u32 *p = sprites[slot] + i * 3;
        int entity = slot ? (i == 0 ? 3 : i) : i + 1;
        int x = entity == 3 ? 20 : 2 + (entity - 1) * 4 + (slot ? 4 : 0);
        p[0] = 0x600000ff;
        p[1] = xy(x, 2);
        p[2] = xy(4, 4);
        hm_packet_identity_span(p, p + 3, (u64)entity);
    }
    hm_render_begin(&frame);
    hm_render_group(1);
    for (int i = 0; i < count; ++i) {
        hm_gpu_execute_words(sprites[slot] + i * 3, 3);
        hm_render_packet(sprites[slot] + i * 3, 3);
    }
    hm_render_end();
    return frame;
}
static u16 pixel(const hm_interpolated_view *v, int x, int y) {
    return v->pixels[(v->area.y + y) * v->stride + v->area.x + x];
}
static hm_render_frame hm_test_rows(int slot, u64 tick, int changed) {
    hm_render_frame f = hm_test_begin(slot, tick, sprites[slot], sizeof(sprites[slot]));
    for (int i = 0; i < 2; ++i) {
        u32 *p = sprites[slot] + i * 3;
        p[0] = changed && i ? 0x6000ff00 : 0x600000ff;
        p[1] = xy(2, 2 + i * 4 + slot * 4);
        p[2] = xy(8, 4);
        hm_packet_identity_span(p, p + 3, (u64)i + 1);
    }
    hm_render_begin(&f);
    hm_render_group(1);
    for (int i = 0; i < 2; ++i) {
        hm_gpu_execute_words(sprites[slot] + i * 3, 3);
        hm_render_packet(sprites[slot] + i * 3, 3);
    }
    hm_render_end();
    return f;
}
int main(void) {
    hm_vram_reset();
    ResetGraph(0);
    CHECK(hm_interpolation_enable(1));
    hm_interpolated_view view;
    hm_test_mesh(0, 2, 0, 0, 0);
    hm_render_frame live = hm_test_mesh(1, 4, 4, 0, 0);
    CHECK(hm_interpolation_render(83333334, &live, &view));
    CHECK(pixel(&view, 4, 3) == 31 && pixel(&view, 8, 3) == (31 << 5)); /* Both interpolate. */
    hm_render_invalidate();
    hm_test_mesh(0, 2, 0, 0, 0);
    live = hm_test_mesh(1, 4, 4, 1, 0);
    CHECK(hm_interpolation_render(83333334, &live, &view));
    /* A changed neighbor previously stayed current while the left tile moved
     * halfway, exposing black at x=8..9. The whole connected mesh stays current. */
    for (int x = 6; x < 14; ++x)
        CHECK(pixel(&view, x, 3) != 0);
    hm_render_invalidate();
    hm_test_mesh(0, 2, 0, 0, 1);
    live = hm_test_mesh(1, 4, 4, 0, 0);
    CHECK(hm_interpolation_render(83333334, &live, &view));
    for (int x = 6; x < 14; ++x)
        CHECK(pixel(&view, x, 3) != 0); /* Conflicting vertex history. */
    hm_render_invalidate();
    hm_test_cells(0, 2);
    live = hm_test_cells(1, 4);
    CHECK(hm_interpolation_render(83333334, &live, &view));
    CHECK(pixel(&view, 4, 3) == 31 && pixel(&view, 11, 3) == 31 && pixel(&view, 12, 3) == 0);
    hm_gpu_scene *scene = hm_gpu_scene_create();
    CHECK(scene);
    hm_replay_sink previous = hm_gpu_scene_begin(scene, 64, 64);
    CHECK(hm_interpolation_replay(83333334, &live));
    hm_gpu_scene_data data;
    CHECK(hm_gpu_scene_end(scene, previous, &data));
    int motions = 0;
    for (u32 i = 0; i < data.words[10]; ++i)
        if (data.words[data.words[8] + i * HM_SCENE_STRIDE + 4] & 32)
            ++motions;
    CHECK(motions == 2);
    hm_gpu_scene_destroy(scene);
    hm_render_invalidate();
    hm_test_rows(0, 2, 0);
    live = hm_test_rows(1, 4, 1);
    CHECK(hm_interpolation_render(83333334, &live, &view));
    /* A material change on the lower row must not leave a black horizontal
     * strip between its current position and the interpolated upper row. */
    for (int y = 6; y < 14; ++y)
        for (int x = 2; x < 10; ++x)
            CHECK(pixel(&view, x, y) != 0);
    hm_packet_identity_reset();
    CHECK(hm_interpolation_enable(0));
    return 0;
}
