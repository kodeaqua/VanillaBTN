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

int main(void) {
    Clock clock = {0};
    assert(sdk_sync_bind(now, wait_until, &clock));
    assert(ResetCallback() == 0);
    assert(VSync(-1) == 0 && VSync(1) == 0);
    VSync(2);
    assert(clock.now == 33333334 && clock.waits == 1);
    assert(VSync(-1) == 2);
    clock.now += 1000000;
    assert(VSync(1) == 15 && clock.waits == 1);
    ResetCallback();
    VSync(2);
    assert(clock.now == 66666667 && clock.waits == 2);
    /* When work is late, VSync(n) does not add another n vblanks of delay. */
    clock.now = 200000000;
    VSync(2);
    assert(clock.now == 200000000 && clock.waits == 2);
    VSync(0);
    assert(clock.now == 216666667 && clock.waits == 3);
    assert(VSync(-1) == 13);
    assert(!sdk_sync_bind(now, NULL, &clock));
    assert(sdk_sync_bind(NULL, NULL, NULL));
    return 0;
}
