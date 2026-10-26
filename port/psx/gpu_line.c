#include "psx/error.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_raster.h"

static s32 hm_line_signed11(u32 value) {
    value &= 2047;
    return value < 1024 ? (s32)value : (s32)value - 2048;
}
static s32 hm_line_abs(s32 value) { return value < 0 ? -value : value; }
static void hm_line_segment(u32 xy0, u32 xy1, u32 color0, u32 color1, u32 opcode, int skip_first) {
    const DRAWENV *draw = &hm_gpu_environment_state()->draw;
    s32 x0 = hm_line_signed11(xy0) + draw->ofs[0], y0 = hm_line_signed11(xy0 >> 16) + draw->ofs[1];
    s32 x1 = hm_line_signed11(xy1) + draw->ofs[0], y1 = hm_line_signed11(xy1 >> 16) + draw->ofs[1];
    s32 dx = hm_line_abs(x1 - x0), dy = hm_line_abs(y1 - y0);
    if (dx > 1023 || dy > 511)
        return;
    s32 sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, error = dx - dy;
    s32 steps = dx > dy ? dx : dy;
    static const s8 dither[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    for (s32 at = 0;; ++at) {
        if (!skip_first || at) {
            u16 color = 0;
            for (u32 channel = 0; channel < 3; ++channel) {
                s32 first = (color0 >> (channel * 8)) & 255;
                s32 last = (color1 >> (channel * 8)) & 255;
                s32 value = steps ? (first * (steps - at) + last * at + steps / 2) / steps : first;
                if (draw->dtd)
                    value += dither[(u32)y0 & 3][(u32)x0 & 3];
                if (value < 0)
                    value = 0;
                if (value > 255)
                    value = 255;
                color |= (u16)((value >> 3) << (channel * 5));
            }
            hm_gpu_pixel(x0, y0, color, (opcode & 2) != 0);
        }
        if (x0 == x1 && y0 == y1)
            break;
        s32 doubled = error * 2;
        if (doubled > -dy) {
            error -= dy;
            x0 += sx;
        }
        if (doubled < dx) {
            error += dx;
            y0 += sy;
        }
    }
}
u32 hm_gpu_line(const u32 *words, u32 available) {
    u32 opcode = words[0] >> 24;
    int shaded = (opcode & 16) != 0, polyline = (opcode & 8) != 0;
    u32 minimum = shaded ? 4 : 3;
    if (available < minimum)
        hm_runtime_error("DrawOTag", "truncated line");
    u32 at = 2, previous_xy = words[1], previous_color = words[0];
    int skip_first = 0;
    for (;;) {
        if (at >= available)
            hm_runtime_error("DrawOTag", "unterminated polyline");
        if (polyline && skip_first && (words[at] & 0xF000F000u) == 0x50005000u)
            return at + 1;
        u32 color = shaded ? words[at++] : previous_color;
        if (at >= available)
            hm_runtime_error("DrawOTag", "truncated shaded line");
        u32 xy = words[at++];
        hm_line_segment(previous_xy, xy, previous_color, color, opcode, skip_first);
        if (!polyline)
            return at;
        previous_xy = xy;
        previous_color = color;
        skip_first = 1;
    }
}
