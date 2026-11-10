#include "game_graphics.h"

#include <stdlib.h>

static unsigned u16(const uint8_t *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static uint32_t u32(const uint8_t *p) {
    return (uint32_t)u16(p) | (uint32_t)u16(p + 2) << 16;
}

static int s16(const uint8_t *p) {
    unsigned value = u16(p);
    return value < 32768 ? (int)value : (int)value - 65536;
}

static bool span(size_t offset, size_t count, size_t size) {
    return offset <= size && count <= size - offset;
}

typedef struct RunReader {
    const uint8_t *data;
    size_t size, offset;
    unsigned remaining;
    bool repeat;
    uint8_t fill;
} RunReader;

/* Source behavior: negative control copies literals, nonnegative repeats a byte. */
static bool run_byte(RunReader *reader, uint8_t *value) {
    if (reader->remaining == 0) {
        int control;
        if (reader->offset == reader->size) return false;
        control = reader->data[reader->offset++];
        if (control >= 128) control -= 256;
        reader->repeat = control >= 0;
        reader->remaining = control < 0 ? (unsigned)-control : (unsigned)control;
        if (reader->remaining == 0) reader->remaining = 1;
        if (reader->repeat) {
            if (reader->offset == reader->size) return false;
            reader->fill = reader->data[reader->offset++];
        }
    }
    if (reader->repeat) *value = reader->fill;
    else {
        if (reader->offset == reader->size) return false;
        *value = reader->data[reader->offset++];
    }
    --reader->remaining;
    return true;
}

bool game_texture_pack_upload(Ps1Video *video, const uint8_t *data, size_t size) {
    size_t offset = 4;
    if (video == NULL || data == NULL || size < 4 || data[0] != 0x0D) return false;
    for (unsigned block = 0; block < u16(data + 2); ++block) {
        size_t bytes, width, height, pixels;
        unsigned flags, x, y;
        uint8_t *decoded;
        uint16_t *words;
        bool valid = true;
        if (!span(offset, 12, size)) return false;
        bytes = u32(data + offset) & 0xFFFFFFu;
        flags = data[offset + 3];
        x = u16(data + offset + 4);
        y = u16(data + offset + 6);
        width = u16(data + offset + 8);
        height = u16(data + offset + 10);
        offset += 12;
        if (width == 0 || height == 0 || width > 1024 || height > 512
            || x + width > 1024 || y + height > 512 || !span(offset, bytes, size)) return false;
        pixels = width * height;
        decoded = malloc(pixels * 2);
        words = malloc(pixels * sizeof(*words));
        if (decoded == NULL || words == NULL) {
            free(decoded);
            free(words);
            return false;
        }
        if ((flags & 0xCu) == 4u) {
            valid = bytes >= pixels * 2;
            if (valid) {
                for (size_t i = 0; i < pixels * 2; ++i) decoded[i] = data[offset + i];
            }
        } else {
            RunReader reader = {data + offset, bytes, 0, 0, false, 0};
            if (flags & 8u) {
                /* Column RLE stores all low bytes, then high bytes, per column. */
                for (size_t col = 0; col < width && valid; ++col) {
                    for (size_t half = 0; half < 2 && valid; ++half) {
                        for (size_t row = 0; row < height && valid; ++row) {
                            valid = run_byte(&reader, &decoded[(row * width + col) * 2 + half]);
                        }
                    }
                }
            } else {
                for (size_t i = 0; i < pixels * 2 && valid; ++i) valid = run_byte(&reader, &decoded[i]);
            }
        }
        if (valid) {
            for (size_t i = 0; i < pixels; ++i) words[i] = (uint16_t)u16(decoded + i * 2);
            valid = ps1_video_upload(video, (int)x, (int)y, (int)width, (int)height, words, width);
        }
        free(decoded);
        free(words);
        if (!valid) return false;
        offset += (bytes + 3u) & ~(size_t)3u;
        if (offset > size) return false;
    }
    return true;
}

static bool animation_draw(Ps1Video *video, const Ps1DrawState *draw,
                          const uint8_t *data, size_t size, unsigned animation,
                          unsigned frame, int x, int y, bool quads) {
    size_t images, lists, parts, ranges, frames, range, current, list;
    unsigned count;
    if (video == NULL || draw == NULL || data == NULL || size < 36 || u32(data) != 0x0E) return false;
    images = u32(data + 16);
    lists = u32(data + 20);
    parts = u32(data + 24);
    ranges = u32(data + 28);
    frames = u32(data + 32);
    range = ranges + (size_t)animation * 4;
    if (!span(range, 4, size) || frame >= u16(data + range)) return false;
    current = frames + u16(data + range + 2) + (size_t)frame * 10;
    if (!span(current, 10, size)) return false;
    if (u16(data + current) > 0xFFFDu) return true;
    list = lists + (size_t)u16(data + current) * 4;
    if (!span(list, 4, size)) return false;
    count = u16(data + list);
    parts += u16(data + list + 2);
    if (!span(parts, (size_t)count * 12, size)) return false;
    /* OT insertion reverses the source's backward part loop into this painter order. */
    for (unsigned i = 0; i < count; ++i) {
        const uint8_t *part = data + parts + (size_t)i * 12;
        size_t image = images + (size_t)u16(part) * 8;
        size_t palette = images + (size_t)u16(part + 2) * 8;
        unsigned ix, iy, depth;
        Ps1Sprite sprite = {0};
        if (!span(image, 8, size) || !span(palette, 8, size)) return false;
        ix = u16(data + image);
        iy = u16(data + image + 2);
        depth = part[8] >> 7;
        sprite.x = x + s16(data + current + 2) + s16(part + 4);
        sprite.y = y + s16(data + current + 4) + s16(part + 6);
        sprite.width = (int)(u16(data + image + 4) << (2 - depth));
        sprite.height = (int)u16(data + image + 6);
        sprite.u = (ix & 63u) << (2 - depth);
        sprite.v = iy & 255u;
        sprite.tpage = (depth << 7) | ((iy & 256u) >> 4) | ((ix & 1023u) >> 6);
        sprite.clut = (u16(data + palette + 2) << 6) | (u16(data + palette) >> 4);
        sprite.rgb24 = 0x808080u;
        sprite.semi_transparent = (part[8] & 16u) != 0;
        if (!quads) {
            if (!ps1_video_draw_sprite(video, draw, &sprite)) return false;
        } else {
            if (sprite.width <= 0 || sprite.height <= 0) return false;
            unsigned left = sprite.u, right = (sprite.u + (unsigned)sprite.width - 1) & 255u;
            unsigned top = sprite.v, bottom = (sprite.v + (unsigned)sprite.height - 1) & 255u;
            if (part[8] & 1u) { unsigned swap = left; left = right; right = swap; }
            if (part[8] & 2u) { unsigned swap = top; top = bottom; bottom = swap; }
            Ps1Vertex vertices[4] = {
                {sprite.x,sprite.y,left,top}, {sprite.x + sprite.width,sprite.y,right,top},
                {sprite.x,sprite.y + sprite.height,left,bottom},
                {sprite.x + sprite.width,sprite.y + sprite.height,right,bottom}
            };
            if (!ps1_video_draw_quad(video, draw, vertices, vertices + 1, vertices + 2,
                vertices + 3, sprite.rgb24, sprite.tpage, sprite.clut, true, false,
                sprite.semi_transparent)) return false;
        }
    }
    return true;
}

bool game_animation_draw(Ps1Video *video, const Ps1DrawState *draw,
                          const uint8_t *data, size_t size, unsigned animation,
                          unsigned frame, int x, int y) {
    return animation_draw(video, draw, data, size, animation, frame, x, y, false);
}

bool game_animation_draw_quads(Ps1Video *video, const Ps1DrawState *draw,
                                const uint8_t *data, size_t size, unsigned animation,
                                unsigned frame, int x, int y) {
    return animation_draw(video, draw, data, size, animation, frame, x, y, true);
}

bool game_animation_tick(GameAnimation *state, const uint8_t *data, size_t size) {
    size_t range, frame;
    unsigned duration, control, count;
    if (state == NULL || data == NULL || size < 36 || u32(data) != 0x0E) return false;
    range = u32(data + 28) + (size_t)state->animation * 4;
    if (!span(range, 4, size)) return false;
    count = u16(data + range);
    if (state->frame >= count) return false;
    frame = u32(data + 32) + u16(data + range + 2) + (size_t)state->frame * 10;
    if (!span(frame, 10, size)) return false;
    duration = (data[frame + 6] + 1u) >> 1;
    control = data[frame + 7];
    if (++state->ticks >= duration) {
        state->ticks = 0;
        ++state->frame;
        if (state->frame >= count && (control & 0x80u) == 0) state->frame = control & 0x7Fu;
        if (control == 255) {
            state->ticks = duration;
            --state->frame;
        } else if (control & 128u) state->frame = control & 127u;
        if (state->frame >= count) return false;
    }
    return true;
}
