#include "gfx/frame_events.h"
#include "gfx/replay_sink.h"
#include "psx/error.h"
#include "psx/gpu_environment.h"
#include "psx/gpu_links.h"
#include "psx/gpu_raster.h"
#include "psx/vram.h"
#include <libgpu.h>
#include <stdio.h>

static s32 hm_signed11(u32 value) {
    value &= 2047;
    return value < 1024 ? (s32)value : (s32)value - 2048;
}

static s32 hm_limit(s32 value) { return value < 0 ? 0 : value > 31 ? 31 : value; }

void hm_gpu_pixel(s32 x, s32 y, u16 color, int blend) {
    const hm_gpu_environment *state = hm_gpu_environment_state();
    const RECT *clip = &state->draw.clip;
    if (x < 0 || x >= 1024 || y < 0 || y >= 512 || x < clip->x || y < clip->y ||
        x >= clip->x + clip->w || y >= clip->y + clip->h)
        return;
    const RECT *display = &state->display.disp;
    if (!state->draw.dfe && state->display_enabled && x >= display->x && y >= display->y &&
        x < display->x + display->w && y < display->y + display->h)
        return;
    if (blend) {
        u16 background = hm_vram_pixel((u32)x, (u32)y);
        u16 result = color & 0x8000;
        u32 mode = (state->draw.tpage >> 5) & 3;
        for (u32 shift = 0; shift < 15; shift += 5) {
            s32 back = (background >> shift) & 31, front = (color >> shift) & 31, mixed;
            switch (mode) {
            case 0:
                mixed = back / 2 + front / 2;
                break;
            case 1:
                mixed = back + front;
                break;
            case 2:
                mixed = back - front;
                break;
            default:
                mixed = back + front / 4;
                break;
            }
            result |= (u16)(hm_limit(mixed) << shift);
        }
        color = result;
    }
    hm_vram_draw_pixel((u32)x, (u32)y, color);
}

u16 hm_gpu_texel(u32 u, u32 v, u16 clut) {
    const DRAWENV *draw = &hm_gpu_environment_state()->draw;
    u32 mask_x = (256u - (u32)draw->tw.w) & 248;
    u32 mask_y = (256u - (u32)draw->tw.h) & 248;
    u = ((u & ~mask_x) | ((u32)draw->tw.x & mask_x)) & 255;
    v = ((v & ~mask_y) | ((u32)draw->tw.y & mask_y)) & 255;
    u32 page = draw->tpage, x = (page & 15) * 64, y = (page & 16) * 16;
    u32 depth = (page >> 7) & 3, index;
    if (depth == 0)
        index = (hm_vram_pixel(x + u / 4, y + v) >> ((u & 3) * 4)) & 15;
    else if (depth == 1)
        index = (hm_vram_pixel(x + u / 2, y + v) >> ((u & 1) * 8)) & 255;
    else if (depth == 2)
        return hm_vram_pixel(x + u, y + v);
    else
        hm_runtime_error("DrawOTag", "reserved texture depth");
    return hm_vram_pixel((clut & 63) * 16 + index, (clut >> 6) & 511);
}

static u16 hm_gpu_color(u32 rgb) {
    return (u16)(((rgb >> 3) & 31) | ((rgb >> 6) & 0x3E0) | ((rgb >> 9) & 0x7C00));
}

static void hm_gpu_rectangle(const u32 *words, u32 opcode, u32 width, u32 height, int textured) {
    const DRAWENV *draw = &hm_gpu_environment_state()->draw;
    s32 x0 = hm_signed11(words[1]) + draw->ofs[0];
    s32 y0 = hm_signed11(words[1] >> 16) + draw->ofs[1];
    s32 clip_x0 = draw->clip.x > 0 ? draw->clip.x : 0;
    s32 clip_y0 = draw->clip.y > 0 ? draw->clip.y : 0;
    s32 clip_x1 = draw->clip.x + draw->clip.w < 1024 ? draw->clip.x + draw->clip.w : 1024;
    s32 clip_y1 = draw->clip.y + draw->clip.h < 512 ? draw->clip.y + draw->clip.h : 512;
    s32 x_start = x0 < clip_x0 ? clip_x0 - x0 : 0;
    s32 y_start = y0 < clip_y0 ? clip_y0 - y0 : 0;
    s32 x_end = x0 + (s32)width > clip_x1 ? clip_x1 - x0 : (s32)width;
    s32 y_end = y0 + (s32)height > clip_y1 ? clip_y1 - y0 : (s32)height;
    if (x_start >= x_end || y_start >= y_end)
        return;
    u16 color = hm_gpu_color(words[0]);
    u32 u0 = textured ? (words[2] & 255) : 0;
    u32 v0 = textured ? ((words[2] >> 8) & 255) : 0;
    u16 clut = textured ? (u16)(words[2] >> 16) : 0;
    u32 light_r = words[0] & 255, light_g = (words[0] >> 8) & 255, light_b = (words[0] >> 16) & 255;
    for (s32 y = y_start; y < y_end; ++y)
        for (s32 x = x_start; x < x_end; ++x) {
            u16 pixel = color;
            int blend = (opcode & 2) != 0;
            if (textured) {
                pixel = hm_gpu_texel(u0 + (u32)x, v0 + (u32)y, clut);
                if (!pixel)
                    continue;
                blend = blend && (pixel & 0x8000);
                if (!(opcode & 1)) {
                    u32 r = ((pixel & 31) * light_r) / 128;
                    u32 g = (((pixel >> 5) & 31) * light_g) / 128;
                    u32 b = (((pixel >> 10) & 31) * light_b) / 128;
                    pixel = (u16)((pixel & 0x8000) | (r > 31 ? 31 : r) |
                                  ((g > 31 ? 31 : g) << 5) | ((b > 31 ? 31 : b) << 10));
                }
            }
            hm_gpu_pixel(x0 + x, y0 + y, pixel, blend);
        }
}

static u32 hm_gpu_command(const u32 *words, u32 available) {
    u32 opcode = words[0] >> 24;
    if (opcode == 0 || opcode == 1)
        return 1; /* NOP / cache invalidate: direct VRAM reads have no stale cache. */
    if (opcode >= 0xE1 && opcode <= 0xE6) {
        hm_gpu_environment_command(words[0]);
        return 1;
    }
    if (opcode == 0xA0 || opcode == 2) {
        if (available < 3)
            return available;
        RECT area = {(s16)words[1], (s16)(words[1] >> 16), (s16)words[2], (s16)(words[2] >> 16)};
        if (opcode == 2) {
            ClearImage(&area, (u8)words[0], (u8)(words[0] >> 8), (u8)(words[0] >> 16));
            return 3;
        }
        u32 width = (((u32)area.w - 1) & 1023) + 1;
        u32 height = (((u32)area.h - 1) & 511) + 1;
        u32 length = 3 + (width * height + 1) / 2;
        if (length > available)
            return available;
        LoadImage(&area, words + 3);
        return length;
    }
    if ((opcode & 0xE0) == 0x20)
        return hm_gpu_polygon(words, available);
    if ((opcode & 0xE0) == 0x40)
        return hm_gpu_line(words, available);
    if ((opcode & 0xE0) == 0x60) {
        int textured = (opcode & 4) != 0;
        u32 size = (opcode >> 3) & 3;
        u32 length = 2 + (u32)textured + (size == 0);
        if (available < length)
            return available;
        u32 width = size == 1 ? 1 : size == 2 ? 8 : 16, height = width;
        if (size == 0) {
            width = words[length - 1] & 1023;
            height = (words[length - 1] >> 16) & 511;
        }
        hm_gpu_rectangle(words, opcode, width, height, textured);
        return length;
    }
    return 1; /* Skip unknown opcode gracefully */
}

void hm_gpu_execute_words(const u32 *words, u32 count) {
    if (!words && count)
        hm_runtime_error("hm_gpu_execute_words", "null command array");
    for (u32 at = 0; at < count;) {
        u32 consumed = hm_replay_consume(words + at, count - at);
        if (consumed) {
            if (consumed > count - at)
                hm_runtime_error("hm_gpu_execute_words", "invalid backend command length");
            at += consumed;
            continue;
        }
        hm_replay_command(words + at, count - at, 0);
        u32 length = hm_gpu_command(words + at, count - at);
        hm_replay_command(words + at, length, 1);
        at += length;
    }
}
void DrawOTag(u32 *table) {
    if (!table || ((uintptr_t)table & 3))
        hm_runtime_error("DrawOTag", "null or unaligned OT");
    if (hm_vram_is_native()) {
        const RECT *clip = &hm_gpu_environment_state()->draw.clip;
        hm_render_direct_draw(clip->x, clip->y, (u32)clip->w, (u32)clip->h);
    }
    const u32 *packet = table;
    const u32 *slow = table;
    u32 visited = 0;
    for (; packet && visited < HM_GPU_LINK_CAPACITY; ++visited) {
        u32 tag = packet[0], count = tag >> 24;
        for (u32 at = 0; at < count;) {
            u32 length = hm_gpu_command(packet + 1 + at, count - at);
            hm_render_packet(packet + 1 + at, length);
            at += length;
        }
        packet = hm_gpu_link_decode(tag);
        if (!packet)
            return;

        /* Floyd's cycle-finding: advance tortoise every 2 steps */
        if (visited & 1) {
            u32 slow_tag = slow[0];
            slow = hm_gpu_link_decode(slow_tag);
            if (slow == packet) {
                /* Gracefully break detected cycle without crashing process */
                return;
            }
        }
    }
}
