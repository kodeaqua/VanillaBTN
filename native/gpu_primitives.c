#include <stdlib.h>
#include <string.h>
#include "gpu_primitives.h"

enum {
    GPU_COMMAND_SHIFT = 24,
    GPU_COMMAND_CLASS_MASK = 0xe0,
    GPU_POLYGON = 0x20,
    GPU_TEXTURED = 0x04,
    GPU_QUAD = 0x08,
    GPU_GOURAUD = 0x10,
    GPU_COORD_SIGN = 1 << 10,
    GPU_COORD_RANGE = GPU_COORD_SIGN * 2,
    GPU_MAX_TRIANGLE_WIDTH = 1023,
    GPU_MAX_TRIANGLE_HEIGHT = 511,
    GPU_MAX_POLYGON_WORDS = 12,
};

static int gpu_coord(u_long word) {
    int coord = word & (GPU_COORD_RANGE - 1);
    return coord < GPU_COORD_SIGN ? coord : coord - GPU_COORD_RANGE;
}

static int gpu_axis_fits(int a, int b, int c, int limit) {
    return abs(a - b) <= limit && abs(a - c) <= limit && abs(b - c) <= limit;
}

static int gpu_triangle_fits(u_long a, u_long b, u_long c) {
    return gpu_axis_fits(gpu_coord(a), gpu_coord(b), gpu_coord(c), GPU_MAX_TRIANGLE_WIDTH) &&
           gpu_axis_fits(gpu_coord(a >> 16), gpu_coord(b >> 16), gpu_coord(c >> 16),
                         GPU_MAX_TRIANGLE_HEIGHT);
}

int vanillabtn_gpu_push_prim(u_long *packet, int available_words) {
    unsigned int code = (packet[0] >> GPU_COMMAND_SHIFT) & 0xff;
    if ((code & GPU_COMMAND_CLASS_MASK) != GPU_POLYGON) {
        return Draw_PushPrim(packet, available_words);
    }

    int quad = (code & GPU_QUAD) != 0;
    int gouraud = (code & GPU_GOURAUD) != 0;
    int stride = 1 + ((code & GPU_TEXTURED) != 0) + gouraud;
    int words = (3 + quad) * stride + !gouraud;
    if (available_words < words) {
        return available_words;
    }

    int first = gpu_triangle_fits(packet[1], packet[1 + stride], packet[1 + 2 * stride]);
    int second = quad && gpu_triangle_fits(packet[1 + stride], packet[1 + 2 * stride],
                                           packet[1 + 3 * stride]);
    if (first && (!quad || second)) {
        return Draw_PushPrim(packet, available_words);
    }
    if (first || second) {
        u_long clipped[GPU_MAX_POLYGON_WORDS];
        memcpy(clipped, packet, words * sizeof(*packet));
        /* Collapse the rejected half without changing the shared vertices or texture state. */
        clipped[first ? 1 + 3 * stride : 1] = packet[1 + stride];
        return Draw_PushPrim(clipped, words);
    }
    return words;
}
