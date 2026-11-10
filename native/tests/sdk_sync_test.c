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
static void on_vblank(void) { ++callbacks; }
static void on_pump(void *context) {
    assert(context == &pumps);
    assert(callbacks == pumps + (registered ? 1u : 0u));
    ++pumps;
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
    registered = false;
    VSyncCallback(NULL);
    VSync(0);
    assert(callbacks == 6 && pumps == 7);
    sdk_sync_pump(NULL, NULL);
    assert(!sdk_sync_bind(now, NULL, &clock));
    assert(sdk_sync_bind(NULL, NULL, NULL));
    assert(SetVideoMode(0) == 0);
    assert(SetVideoMode(1) == 0);
    return 0;
}
