#include "psx/timing.h"
#include "psx/cd_runtime.h"
#include "psx/error.h"
#include "psx/sound_voice.h"
#include <libapi.h>
#include <libetc.h>
#include <stdio.h>

static hm_timing_host hm_host;
static u64 hm_ticks;
static void (*hm_vblank)(void);
static int hm_in_tick;

void hm_timing_reset(hm_timing_host host) {
    hm_host = host;
    hm_ticks = 0;
    hm_vblank = NULL;
    hm_in_tick = 0;
}

u64 hm_timing_ticks(void) { return hm_ticks; }

s32 ResetCallback(void) {
    /* Native event dispatch has no BIOS IRQ installation step. Repeated calls
     * from A_FILE helpers must retain an already registered VSync callback. */
    return 0;
}

void (*VSyncCallback(void (*callback)(void)))(void) {
    void (*previous)(void) = hm_vblank;
    hm_vblank = callback;
    return previous;
}

s32 VSync(s32 mode) {
    if (mode == -1)
        return (s32)(u32)hm_ticks;
    if (mode == 1)
        return hm_host.scanlines ? hm_host.scanlines(hm_host.context) : 0;
    if (mode < 0 || hm_in_tick)
        hm_runtime_error("VSync", "invalid wait or recursive VBlank wait");
    s32 count = mode == 0 ? 1 : mode;
    for (s32 i = 0; i < count; ++i) {
        hm_in_tick = 1;
        if (hm_host.wait_tick)
            hm_host.wait_tick(hm_host.context, hm_ticks + 1);
        ++hm_ticks;
        hm_cd_tick();
        if (hm_vblank)
            hm_vblank();
        hm_sound_tick();
        hm_in_tick = 0;
    }
    return 0;
}

s32 SetVideoMode(s32 mode) {
    if (mode != 0)
        hm_runtime_error("SetVideoMode", "only USA NTSC timing is supported");
    return 0;
}

void SetMem(s32 megabytes) {
    if (megabytes != 2)
        hm_runtime_error("SetMem", "unsupported PS1 memory-size hint");
    /* The game uses ordinary native globals/heap and a bounded asset workspace.
     * This BIOS configuration hint does not allocate an emulated RAM image. */
}

void SystemError(s32 code, s32 detail) {
    char message[80];
    snprintf(message, sizeof(message), "game error 0x%08x, detail 0x%08x", (u32)code, (u32)detail);
    hm_runtime_error("SystemError", message);
}
