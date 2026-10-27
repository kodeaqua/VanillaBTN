#include "enhance/widescreen/widescreen.h"
#include <math.h>
static int requested_margin, world_scene, replay_world;
static RECT native_area;
void hm_widescreen_configure(int enabled, int width, int height) {
    requested_margin = 0;
    if (enabled && width > 0 && height > 0 && (s64)width * 3 > (s64)height * 4) {
        s64 logical = ((s64)240 * width + height / 2) / height;
        if (logical > 1024)
            logical = 1024;
        requested_margin = (int)(logical - 320 + 1) / 2;
    }
}
int hm_widescreen_enabled(void) { return requested_margin != 0; }
void hm_widescreen_scene(int world) { world_scene = !!world; }
int hm_widescreen_margin(void) { return world_scene ? requested_margin : 0; }
int hm_widescreen_angle(int angle) {
    int margin = hm_widescreen_margin();
    if (!margin)
        return angle;
    const double unit = 6.283185307179586 / 4096;
    int base = angle > 1024 ? 2048 : 0;
    return base + (int)lround(atan(tan((angle - base) * unit) * (320 + 2 * margin) / 320) / unit);
}
int hm_widescreen_radius(int radius) {
    return radius + (int)((s64)radius * hm_widescreen_margin() / 160);
}
RECT hm_widescreen_area(RECT native) {
    int margin = native.w == 320 && native.h == 240 ? hm_widescreen_margin() : 0;
    /* The SDK's stepped fades temporarily select the double-sized framebuffer. */
    if (native.w == 640 && native.h == 480) {
        margin = hm_widescreen_margin() * 2;
        if (margin > 192)
            margin = 192;
    }
    native.x -= margin;
    native.w += 2 * margin;
    return native;
}
void hm_widescreen_replay_begin(const RECT *native) {
    native_area = *native;
    replay_world = 0;
}
void hm_widescreen_replay_group(int world) { replay_world = !!world; }
RECT hm_widescreen_clip(RECT clip) {
    if (replay_world && clip.x == native_area.x && clip.w == native_area.w)
        return hm_widescreen_area(clip);
    return clip;
}
int hm_widescreen_overlay(const u32 *words, u32 count, u32 output[5]) {
    RECT expanded = hm_widescreen_area(native_area);
    int margin = native_area.x - expanded.x;
    u32 width = (u16)native_area.w, height = (u32)(u16)native_area.h << 16;
    if (!margin || !count)
        return 0;
    /* Rucksack/name-entry shade uses a variable-size TILE, not a polygon.
     * Return its actual packet length: subsequent commands must stay aligned. */
    if (((words[0] >> 24) & ~2u) == 0x60 && count >= 3 && words[1] == 0 &&
        words[2] == (width | height)) {
        output[0] = words[0];
        output[1] = (u16)-margin;
        output[2] = (width + 2 * margin) | height;
        return 3;
    }
    if (count < 5 || ((words[0] >> 24) & ~2u) != 0x28 || words[1] != 0 || words[2] != width ||
        words[3] != height || words[4] != (width | height))
        return 0;
    output[0] = words[0];
    for (int i = 1; i <= 4; ++i) {
        int x = (s16)words[i];
        x += x ? margin : -margin;
        output[i] = (words[i] & 0xffff0000u) | (u16)x;
    }
    return 5;
}
