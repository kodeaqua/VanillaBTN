#include <stdlib.h>
#include "sdk_sync.h"
#include <libetc.h>
#include <SDL3/SDL.h>
#include <limits.h>

static uint64_t sdl_now(void *context) {
    (void)context;
    return SDL_GetTicksNS();
}

static uint64_t (*host_subtick_hook)(uint64_t deadline, void *);
static void *host_subtick_context;
#ifdef _DEBUG
static void (*host_stall_hook)(bool waiting, uint64_t duration_ns, void *);
static void *host_stall_context;
static uint64_t last_vsync_exit_ns;

void sdk_sync_stall_hook(void (*hook)(bool waiting, uint64_t duration_ns, void *),
                         void *context) {
    host_stall_hook = hook;
    host_stall_context = context;
    last_vsync_exit_ns = 0;
}
#endif


void sdk_sync_subtick_hook(uint64_t (*subtick)(uint64_t deadline, void *), void *context) {
    host_subtick_hook = subtick;
    host_subtick_context = context;
}

static void sdl_wait(uint64_t deadline, void *context) {
    uint64_t now = sdl_now(context);
    while (now < deadline) {
        uint64_t wake = deadline;
        if (host_subtick_hook) {
            wake = host_subtick_hook(deadline, host_subtick_context);
            /* A malformed hook must not spin the VSync thread. */
            if (wake <= now || wake > deadline) wake = deadline;
        }
        SDL_DelayNS(wake - now);
        now = sdl_now(context);
    }
}

static uint64_t (*clock_now)(void *) = sdl_now;
static void (*clock_wait)(uint64_t, void *) = sdl_wait;
static void *clock_context;
static uint64_t pace_anchor_ns, pace_anchor_tick, last_wait;
static bool initialized;
static void (*vblank_callback)(void);
static void (*host_pump)(void *);
static void *host_context;
static uint64_t dispatched_tick;
static bool dispatching;
static bool suspended;
static bool shifted;
static uint64_t suspended_at;

void sdk_sync_pump(void (*pump)(void *), void *context) {
    host_pump = pump;
    host_context = context;
}

int VSyncCallback(void (*callback)(void)) {
    vblank_callback = callback;
    return 0;
}

static uint32_t s_turbo_multiplier = 1;
static uint64_t units(uint64_t ns, uint64_t rate);

void sdk_sync_set_turbo(uint32_t multiplier) {
    uint32_t next = multiplier >= 1 && multiplier <= 8 ? multiplier : 1;
    if (next == s_turbo_multiplier) return;
    if (initialized) {
        uint64_t now = clock_now(clock_context);
        uint64_t elapsed = now >= pace_anchor_ns ? now - pace_anchor_ns : 0;
        uint64_t rate = 60ULL * (uint64_t)s_turbo_multiplier;
        pace_anchor_tick += units(elapsed, rate);
        pace_anchor_ns = now;
    }
    s_turbo_multiplier = next;
}

uint32_t sdk_sync_get_turbo(void) {
    return s_turbo_multiplier;
}

static uint64_t tick_deadline(uint64_t tick) {
    uint64_t rate = 60ULL * (uint64_t)s_turbo_multiplier;
    uint64_t delta = tick > pace_anchor_tick ? tick - pace_anchor_tick : 0;
    return pace_anchor_ns + delta / rate * 1000000000
        + (delta % rate * 1000000000 + rate - 1) / rate;
}

static uint64_t current_tick(uint64_t now) {
    uint64_t elapsed = now >= pace_anchor_ns ? now - pace_anchor_ns : 0;
    return pace_anchor_tick + units(elapsed, 60ULL * (uint64_t)s_turbo_multiplier);
}

static void dispatch_vblank(uint64_t tick) {
    if (dispatching || tick <= dispatched_tick) return;
    dispatched_tick = tick;
    dispatching = true;
    if (vblank_callback) vblank_callback();
    if (host_pump) host_pump(host_context);
    dispatching = false;
}

void sdk_sync_suspend(void) {
    if (!initialized || suspended) return;
    suspended_at = clock_now(clock_context);
    suspended = true;
}

void sdk_sync_resume(void) {
    if (!suspended) return;
    uint64_t now = clock_now(clock_context);
#ifdef _DEBUG
    if (host_stall_hook) last_vsync_exit_ns = now;
#endif
    uint64_t held = now > suspended_at ? now - suspended_at : 0;
    /* Shifting both anchors preserves tick numbering and VSync(1) deltas. */
    pace_anchor_ns += held;
    last_wait += held;
    suspended = false;
    shifted = true;
}

/* A pump may have suspended for a long time. The caller's cached timestamp then
 * predates the shifted epoch, so re-read the clock once after dispatch. */
static void refresh_after_dispatch(uint64_t *now) {
    if (!shifted) return;
    shifted = false;
    *now = clock_now(clock_context);
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
#ifdef _DEBUG
    last_vsync_exit_ns = 0;
#endif
    suspended = shifted = false;
    return true;
}

int ResetCallback(void) {
    /* Original archive reads repeatedly call ResetCallback. Initializing the
     * platform timing source must not rewind the clock on every file read.
     * Registration survives these repeated archive-read resets. */
    if (!initialized) {
        pace_anchor_ns = last_wait = clock_now(clock_context);
        pace_anchor_tick = 0;
        initialized = true;
    }
    return 0;
}

int VSync(int mode) {
    ResetCallback();
    uint64_t now = clock_now(clock_context);
    if (mode < 0) {
        uint64_t tick = current_tick(now);
        return tick > INT_MAX ? INT_MAX : (int)tick;
    }
    if (mode == 1) return result_units(now >= last_wait ? now - last_wait : 0, 15750);
#ifdef _DEBUG
    if (host_stall_hook && last_vsync_exit_ns && now >= last_vsync_exit_ns)
        host_stall_hook(false, now - last_vsync_exit_ns, host_stall_context);
#endif
    uint64_t base = (mode == 0 ? now : last_wait);
    uint64_t tick = current_tick(base);
    tick += mode == 0 ? 1 : (unsigned)mode;
    /* Round upward so an integral-nanosecond deadline never falls before the
     * nominal vblank boundary; preserve phase rather than accumulating drift. */
    uint64_t deadline = tick_deadline(tick);
    bool waited = false;
    while (now < deadline) {
        waited = true;
        /* Service each intervening vblank, so VSync(2/3) does not batch the
         * original loading-screen and draw callbacks at the final deadline. */
        uint64_t next = current_tick(now) + 1;
#ifdef _DEBUG
        uint64_t before_wait = now;
#endif
        clock_wait(tick_deadline(next), clock_context);
        now = clock_now(clock_context);
#ifdef _DEBUG
        if (host_stall_hook && now >= before_wait)
            host_stall_hook(true, now - before_wait, host_stall_context);
#endif
        dispatch_vblank(current_tick(now));
        refresh_after_dispatch(&now);
    }
    /* Late work delivers the current boundary once, without replaying stale
     * rendering callbacks or adding another frame delay. Queries stay pure. */
    if (!waited) {
        dispatch_vblank(current_tick(now));
        refresh_after_dispatch(&now);
    }
    int elapsed = result_units(now >= last_wait ? now - last_wait : 0, 15750);
    last_wait = now;
#ifdef _DEBUG
    if (host_stall_hook) last_vsync_exit_ns = clock_now(clock_context);
#endif
    return elapsed;
}

long SetVideoMode(long mode) {
    /* The current port targets the USA NTSC release exclusively. */
    (void)mode;
    return 0;
}

static bool game_running;
void sdk_sync_start(void) { game_running = true; }
void sdk_sync_stop(void) { game_running = false; }
bool sdk_sync_running(void) { return game_running; }
