#include "sdk_sync.h"
#include <libetc.h>
#include <SDL3/SDL.h>
#include <limits.h>

static uint64_t sdl_now(void *context) {
    (void)context;
    return SDL_GetTicksNS();
}

static void sdl_wait(uint64_t deadline, void *context) {
    uint64_t now = sdl_now(context);
    if (deadline > now) SDL_DelayNS(deadline - now);
}

static uint64_t (*clock_now)(void *) = sdl_now;
static void (*clock_wait)(uint64_t, void *) = sdl_wait;
static void *clock_context;
static uint64_t epoch, last_wait;
static bool initialized;

static uint64_t units(uint64_t ns, uint64_t rate) {
    return ns / 1000000000 * rate + ns % 1000000000 * rate / 1000000000;
}

static int result_units(uint64_t ns, uint64_t rate) {
    uint64_t value = units(ns, rate);
    return value > INT_MAX ? INT_MAX : (int)value;
}

bool sdk_sync_bind(uint64_t (*now)(void *), void (*wait)(uint64_t, void *), void *context) {
    if ((now == NULL) != (wait == NULL)) return false;
    clock_now = now ? now : sdl_now;
    clock_wait = wait ? wait : sdl_wait;
    clock_context = context;
    initialized = false;
    return true;
}

int ResetCallback(void) {
    /* Original archive reads repeatedly call ResetCallback. Initializing the
     * platform timing source must not rewind the clock on every file read.
     * Interrupt callback registration is intentionally not provided yet. */
    if (!initialized) {
        epoch = last_wait = clock_now(clock_context);
        initialized = true;
    }
    return 0;
}

int VSync(int mode) {
    ResetCallback();
    uint64_t now = clock_now(clock_context);
    if (mode < 0) return result_units(now - epoch, 60);
    if (mode == 1) return result_units(now - last_wait, 15750);
    uint64_t tick = units((mode == 0 ? now : last_wait) - epoch, 60);
    tick += mode == 0 ? 1 : (unsigned)mode;
    /* Round upward so an integral-nanosecond deadline never falls before the
     * nominal vblank boundary; preserve phase rather than accumulating drift. */
    uint64_t deadline = epoch + tick / 60 * 1000000000
        + (tick % 60 * 1000000000 + 59) / 60;
    while (now < deadline) {
        clock_wait(deadline, clock_context);
        now = clock_now(clock_context);
    }
    int elapsed = result_units(now - last_wait, 15750);
    last_wait = now;
    return elapsed;
}
