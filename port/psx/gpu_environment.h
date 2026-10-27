#ifndef HMBTN_PSX_GPU_ENVIRONMENT_H
#define HMBTN_PSX_GPU_ENVIRONMENT_H

#include <libgpu.h>

typedef struct {
    DRAWENV draw;
    DISPENV display;
    s32 width, height, debug;
    int display_enabled;
} hm_gpu_environment;
/* Solid backdrop outside native VRAM; never repeat an edge texture column. */
static inline u16 hm_gpu_background(const DRAWENV *draw) {
    return draw->isbg ? (u16)((draw->r0 >> 3) | ((draw->g0 >> 3) << 5) | ((draw->b0 >> 3) << 10))
                      : 0;
}
const hm_gpu_environment *hm_gpu_environment_state(void);
hm_gpu_environment *hm_gpu_environment_bind(hm_gpu_environment *environment);
RECT hm_gpu_environment_set_clip(RECT clip);
void hm_gpu_environment_command(u32 word);

#endif
