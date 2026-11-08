#include "sdk_gpu.h"
#include <libgpu.h>
#include <string.h>

_Static_assert(sizeof(u_long) == 4, "SDK words must be 32-bit");
_Static_assert(sizeof(RECT) == 8, "SDK rectangles contain four halfwords");

static Ps1Video *vram;

void sdk_gpu_bind(Ps1Video *video) {
    vram = video;
}

static int valid_transfer(const RECT *rect, const void *pixels) {
    return vram != NULL && rect != NULL && pixels != NULL
        && rect->x >= 0 && rect->y >= 0 && rect->w > 0 && rect->h > 0
        && rect->w <= PS1_VRAM_WIDTH - rect->x
        && rect->h <= PS1_VRAM_HEIGHT - rect->y;
}

int LoadImage(RECT *rect, u_long *pixels) {
    if (!valid_transfer(rect, pixels)) return -1;
    /* The SDK pointer is word-typed, but each row contains w halfwords.
     * Copy bytes so odd widths and unaligned bank payloads remain valid. */
    for (int row = 0; row < rect->h; ++row) {
        memcpy(&vram->vram[rect->y + row][rect->x],
               (const unsigned char *)pixels + (size_t)row * rect->w * 2,
               (size_t)rect->w * 2);
    }
    return 0;
}

int StoreImage(RECT *rect, u_long *pixels) {
    if (!valid_transfer(rect, pixels)) return -1;
    for (int row = 0; row < rect->h; ++row) {
        memcpy((unsigned char *)pixels + (size_t)row * rect->w * 2,
               &vram->vram[rect->y + row][rect->x], (size_t)rect->w * 2);
    }
    return 0;
}

int DrawSync(int mode) {
    /* There is no pending queue: transfers above finish before returning.
     * Both waiting (0) and polling (1) therefore report completion. */
    return mode == 0 || mode == 1 ? 0 : -1;
}
