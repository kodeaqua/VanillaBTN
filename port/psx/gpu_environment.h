#ifndef HMBTN_PSX_GPU_ENVIRONMENT_H
#define HMBTN_PSX_GPU_ENVIRONMENT_H

#include <libgpu.h>

typedef struct {
    DRAWENV draw;
    DISPENV display;
    s32 width, height, debug;
    int display_enabled;
} hm_gpu_environment;
const hm_gpu_environment *hm_gpu_environment_state(void);
void hm_gpu_environment_command(u32 word);

#endif
