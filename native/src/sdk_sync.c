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
static void (*vblank_callback)(void);
static void (*host_pump)(void *);
static void *host_context;
static uint64_t dispatched_tick;
static bool dispatching;

void sdk_sync_pump(void (*pump)(void *), void *context) {
    host_pump = pump;
    host_context = context;
}

int VSyncCallback(void (*callback)(void)) {
    vblank_callback = callback;
    return 0;
}

static uint64_t tick_deadline(uint64_t tick) {
    return epoch + tick / 60 * 1000000000
        + (tick % 60 * 1000000000 + 59) / 60;
}

static void dispatch_vblank(uint64_t tick) {
    if (dispatching || tick <= dispatched_tick) return;
    dispatched_tick = tick;
    dispatching = true;
    if (vblank_callback) vblank_callback();
    if (host_pump) host_pump(host_context);
    dispatching = false;
}

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
    dispatched_tick = 0;
    return true;
}

int ResetCallback(void) {
    /* Original archive reads repeatedly call ResetCallback. Initializing the
     * platform timing source must not rewind the clock on every file read.
     * Registration survives these repeated archive-read resets. */
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
    uint64_t deadline = tick_deadline(tick);
    while (now < deadline) {
        /* Service each intervening vblank, so VSync(2/3) does not batch the
         * original loading-screen and draw callbacks at the final deadline. */
        uint64_t next = units(now - epoch, 60) + 1;
        clock_wait(tick_deadline(next), clock_context);
        now = clock_now(clock_context);
        dispatch_vblank(units(now - epoch, 60));
    }
    /* Late work delivers the current boundary once, without replaying stale
     * rendering callbacks or adding another frame delay. Queries stay pure. */
    dispatch_vblank(units(now - epoch, 60));
    int elapsed = result_units(now - last_wait, 15750);
    last_wait = now;
    return elapsed;
}
