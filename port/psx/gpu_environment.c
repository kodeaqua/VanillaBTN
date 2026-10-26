#include "psx/gpu_environment.h"
#include "psx/error.h"
#include "psx/vram.h"
#include <inline_c.h>
#include <libgs.h>
#include <string.h>

static hm_gpu_environment hm_default_environment;
static hm_gpu_environment *hm_environment_current = &hm_default_environment;
#define hm_environment (*hm_environment_current)
static void *hm_gs_work;
static s32 hm_gs_x, hm_gs_y;
static s16 hm_gs_buffers[2][2];

const hm_gpu_environment *hm_gpu_environment_state(void) { return &hm_environment; }
hm_gpu_environment *hm_gpu_environment_bind(hm_gpu_environment *environment) {
    hm_gpu_environment *previous = hm_environment_current;
    hm_environment_current = environment ? environment : &hm_default_environment;
    return previous;
}

RECT hm_gpu_environment_set_clip(RECT clip) {
    RECT previous = hm_environment.draw.clip;
    hm_environment.draw.clip = clip;
    return previous;
}
static s16 hm_gpu_signed11(u32 value) {
    value &= 2047;
    return (s16)(value < 1024 ? (s32)value : (s32)value - 2048);
}

void hm_gpu_environment_command(u32 word) {
    DRAWENV *draw = &hm_environment.draw;
    switch (word >> 24) {
    case 0xE1:
        draw->tpage = (u16)(word & 0x1FF);
        draw->dtd = (u8)((word >> 9) & 1);
        draw->dfe = (u8)((word >> 10) & 1);
        break;
    case 0xE2:
        draw->tw.x = (s16)(((word >> 10) & 31) * 8);
        draw->tw.y = (s16)(((word >> 15) & 31) * 8);
        draw->tw.w = (s16)(256 - (word & 31) * 8);
        draw->tw.h = (s16)(256 - ((word >> 5) & 31) * 8);
        break;
    case 0xE3: {
        s32 right = draw->clip.x + draw->clip.w - 1, bottom = draw->clip.y + draw->clip.h - 1;
        draw->clip.x = (s16)(word & 1023);
        draw->clip.y = (s16)((word >> 10) & 511);
        draw->clip.w = (s16)(right - draw->clip.x + 1);
        draw->clip.h = (s16)(bottom - draw->clip.y + 1);
        break;
    }
    case 0xE4:
        draw->clip.w = (s16)((s32)(word & 1023) - draw->clip.x + 1);
        draw->clip.h = (s16)((s32)((word >> 10) & 511) - draw->clip.y + 1);
        break;
    case 0xE5:
        draw->ofs[0] = hm_gpu_signed11(word);
        draw->ofs[1] = hm_gpu_signed11(word >> 11);
        break;
    case 0xE6:
        hm_vram_mask(word & 1, word & 2);
        break;
    default:
        hm_runtime_error("hm_gpu_environment_command", "unknown environment opcode");
    }
}

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
void GsSetProjection(s32 distance) {
    if (distance <= 0 || distance > 65535)
        hm_runtime_error("GsSetProjection", "invalid projection distance");
    SetGeomScreen(distance);
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
