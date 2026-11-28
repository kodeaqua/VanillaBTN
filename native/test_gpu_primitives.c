#include <stdio.h>
#include <string.h>
#include "gpu_primitives.h"

static u_long drawn[12];
static int calls;

int Draw_PushPrim(u_long *packet, int words) {
    memcpy(drawn, packet, words * sizeof(*packet));
    ++calls;
    return words;
}

int main(void) {
    u_long triangle[] = {0x20000000, 0, 1023, 511u << 16};
    if (vanillabtn_gpu_push_prim(triangle, 4) != 4 || calls != 1) {
        fputs("Triangle at the PS1 size limit should draw\n", stderr);
        return 1;
    }
    triangle[3] = 512u << 16;
    if (vanillabtn_gpu_push_prim(triangle, 4) != 4 || calls != 1) {
        fputs("Oversized triangle should be consumed without drawing\n", stderr);
        return 1;
    }

    u_long quad[] = {0x2c000000, 0, 0x12340000, 1, 0x56780000, 1u << 16, 0, 513u << 16, 0};
    if (vanillabtn_gpu_push_prim(quad, 9) != 9 || calls != 2 || drawn[7] != quad[3] ||
        drawn[1] != quad[1] || drawn[2] != quad[2] || drawn[4] != quad[4] ||
        quad[7] != (513u << 16)) {
        fputs("Only the oversized half of the textured quad should be collapsed\n", stderr);
        return 1;
    }

    quad[1] = 513u << 16;
    quad[7] = 1u | (1u << 16);
    if (vanillabtn_gpu_push_prim(quad, 9) != 9 || calls != 3 || drawn[1] != quad[3] ||
        drawn[7] != quad[7]) {
        fputs("A valid second half should still draw when the first half is oversized\n", stderr);
        return 1;
    }

    u_long sprite[] = {0x64000000, 0, 0, (8u << 16) | 8u};
    if (vanillabtn_gpu_push_prim(sprite, 3) != 3 || calls != 3) {
        fputs("Truncated sprite should be consumed without reading its missing size\n", stderr);
        return 1;
    }
    if (vanillabtn_gpu_push_prim(sprite, 4) != 4 || calls != 4) {
        fputs("Complete sprite should still draw\n", stderr);
        return 1;
    }
    return 0;
}
