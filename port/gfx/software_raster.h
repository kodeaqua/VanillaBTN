#ifndef HMBTN_SOFTWARE_RASTER_H
#define HMBTN_SOFTWARE_RASTER_H
#include "gfx/replay_sink.h"
#include "psx/gpu_environment.h"
#include "psx/vram.h"
typedef struct {
    u16 *pixels;
    u32 width, height;
    RECT area;
    int begun;
    int texture_linear;
    u64 revision;
    int logical_x[4096], logical_y[4096];
    const hm_gpu_environment *environment;
    const hm_vram_context *vram;
} hm_software_surface;
hm_replay_sink hm_software_sink(hm_software_surface *surface);
#endif
