#include "sdk_sync.h"
#include <libetc.h>
#include <assert.h>

typedef struct Clock { uint64_t now; unsigned waits; } Clock;
static uint64_t now(void *context) { return ((Clock *)context)->now; }
static void wait_until(uint64_t deadline, void *context) {
    Clock *clock = context;
    assert(deadline > clock->now);
    clock->now = deadline;
    ++clock->waits;
}

static unsigned callbacks, pumps;
static bool registered = true;
static Clock *pump_clock;
static uint64_t pause_ns;
static void on_vblank(void) { ++callbacks; }
static void on_pump(void *context) {
    assert(context == &pumps);
    assert(callbacks == pumps + (registered ? 1u : 0u));
    ++pumps;
    if (pause_ns) {
        /* Model a host holding the pump (for example while unfocused). */
        sdk_sync_suspend();
        pump_clock->now += pause_ns;
        pause_ns = 0;
        sdk_sync_resume();
    }
}

int main(void) {
    Clock clock = {0};
    assert(sdk_sync_bind(now, wait_until, &clock));
    assert(ResetCallback() == 0);
    assert(VSyncCallback(on_vblank) == 0);
    sdk_sync_pump(on_pump, &pumps);
    assert(VSync(-1) == 0 && VSync(1) == 0);
    VSync(2);
    assert(clock.now == 33333334 && clock.waits == 2);
    assert(VSync(-1) == 2);
    clock.now += 1000000;
    assert(VSync(1) == 15 && clock.waits == 2);
    ResetCallback();
    VSync(2);
    assert(clock.now == 66666667 && clock.waits == 4);
    assert(callbacks == 4 && pumps == 4);
    /* When work is late, VSync(n) does not add another n vblanks of delay. */
    clock.now = 200000000;
    VSync(2);
    assert(clock.now == 200000000 && clock.waits == 4);
    VSync(0);
    assert(clock.now == 216666667 && clock.waits == 5);
    assert(VSync(-1) == 13);
    assert(callbacks == 6 && pumps == 6);
    /* A suspended pump must not make the loop catch up: five held seconds add no
     * frames, and the next VSync waits exactly one nominal interval. */
    sdk_sync_resume();
    pump_clock = &clock;
    pause_ns = 5000000000ull;
    VSync(0);
    assert(clock.now == 233333334ull + 5000000000ull);
    assert(VSync(-1) == 14);
    {
        uint64_t held_at = clock.now;
        unsigned waits = clock.waits;
        VSync(0);
        assert(clock.waits == waits + 1 && clock.now - held_at == 16666666);
    }
    assert(callbacks == 8 && pumps == 8);
    registered = false;
    VSyncCallback(NULL);
    VSync(0);
    assert(callbacks == 8 && pumps == 9);
    sdk_sync_pump(NULL, NULL);
    assert(!sdk_sync_bind(now, NULL, &clock));
    assert(sdk_sync_bind(NULL, NULL, NULL));
    assert(SetVideoMode(0) == 0);
    assert(SetVideoMode(1) == 0);
    return 0;
}
