#include "psx/gpu_environment.h"
#include "psx/error.h"
#include "psx/vram.h"
#include <inline_c.h>
#include <libgs.h>
#include <string.h>

static hm_gpu_environment hm_environment;
static void *hm_gs_work;
static s32 hm_gs_x, hm_gs_y;
static s16 hm_gs_buffers[2][2];

const hm_gpu_environment *hm_gpu_environment_state(void) { return &hm_environment; }

s32 ResetGraph(s32 mode) {
    if (mode != 0)
        hm_runtime_error("ResetGraph", "unsupported graph reset mode");
    memset(&hm_environment, 0, sizeof(hm_environment));
    hm_vram_mask(0, 0);
    /* Hardware reset changes registers, not VRAM contents or native OT handles. */
    return 0;
}

s32 SetGraphDebug(s32 mode) {
    s32 previous = hm_environment.debug;
    hm_environment.debug = mode;
    return previous;
}

static void hm_gpu_dimensions(s32 width, s32 height, const char *api) {
    if (width < 1 || width > 1024 || height < 1 || height > 512)
        hm_runtime_error(api, "invalid environment dimensions");
}

DRAWENV *SetDefDrawEnv(DRAWENV *environment, s32 x, s32 y, s32 width, s32 height) {
    if (!environment)
        hm_runtime_error("SetDefDrawEnv", "null environment");
    hm_gpu_dimensions(width, height, "SetDefDrawEnv");
    memset(environment, 0, sizeof(*environment));
    environment->clip = (RECT){(s16)x, (s16)y, (s16)width, (s16)height};
    environment->ofs[0] = (s16)x;
    environment->ofs[1] = (s16)y;
    environment->dtd = 1;
    return environment;
}

DISPENV *SetDefDispEnv(DISPENV *environment, s32 x, s32 y, s32 width, s32 height) {
    if (!environment)
        hm_runtime_error("SetDefDispEnv", "null environment");
    hm_gpu_dimensions(width, height, "SetDefDispEnv");
    memset(environment, 0, sizeof(*environment));
    environment->disp = (RECT){(s16)x, (s16)y, (s16)width, (s16)height};
    return environment;
}

DRAWENV *PutDrawEnv(DRAWENV *environment) {
    if (!environment)
        hm_runtime_error("PutDrawEnv", "null environment");
    hm_environment.draw = *environment;
    if (environment->isbg)
        ClearImage(&environment->clip, environment->r0, environment->g0, environment->b0);
    return environment;
}

DISPENV *PutDispEnv(DISPENV *environment) {
    if (!environment)
        hm_runtime_error("PutDispEnv", "null environment");
    if (environment->isrgb24 || environment->isinter)
        hm_runtime_error("PutDispEnv", "24-bit/interlaced scanout is not implemented");
    hm_environment.display = *environment;
    return environment;
}

s32 SetDispMask(s32 enabled) {
    hm_environment.display_enabled = enabled != 0;
    return 0;
}

void GsInitGraph(s32 width, s32 height, s32 mode, s32 interlace, s32 dither) {
    hm_gpu_dimensions(width, height, "GsInitGraph");
    if (mode || interlace || dither)
        hm_runtime_error("GsInitGraph", "unsupported GS initialization flags");
    hm_environment.width = width;
    hm_environment.height = height;
    hm_gs_x = hm_gs_y = 0;
}

void GsDefDispBuff(s32 x0, s32 y0, s32 x1, s32 y1) {
    hm_gs_buffers[0][0] = (s16)x0;
    hm_gs_buffers[0][1] = (s16)y0;
    hm_gs_buffers[1][0] = (s16)x1;
    hm_gs_buffers[1][1] = (s16)y1;
}

void GsSetOffset(s32 x, s32 y) {
    hm_gs_x = x;
    hm_gs_y = y;
}
void GsSetDrawBuffOffset(void) {
    hm_environment.draw.ofs[0] = hm_gte_half(hm_gs_buffers[0][0] + hm_gs_x);
    hm_environment.draw.ofs[1] = hm_gte_half(hm_gs_buffers[0][1] + hm_gs_y);
}
void GsInit3D(void) {
    hm_gte.zsf[0] = 341;
    hm_gte.zsf[1] = 256;
}
void GsSetWorkBase(void *base) {
    if (!base || ((uintptr_t)base & 3))
        hm_runtime_error("GsSetWorkBase", "null or unaligned work base");
    hm_gs_work = base;
}
void *GsGetWorkBase(void) {
    if (!hm_gs_work)
        hm_runtime_error("GsGetWorkBase", "work base is not bound");
    return hm_gs_work;
}
