/*
 * ps1_clock_test: synthetic checks for fixed simulation timing cadence.
 */
#include "ps1_clock.h"

int main(void) {
    Ps1Clock clock;
    /* main_run_frame waits two NTSC vblanks: game updates are nominally 30 Hz. */
    ps1_clock_init(&clock, 0, 0);
    for (unsigned i = 0; i < 1000; ++i) ps1_clock_advance(&clock, 1000000ull);
    if (clock.total_ticks != 30) return 15;
    ps1_clock_init(&clock, 16666666ull, 4);

    if (clock.tick_duration_ns != 16666666ull) return 1;
    if (clock.max_ticks_per_step != 4) return 2;
    if (clock.accumulator_ns != 0 || clock.total_ticks != 0) return 3;

    /* 1. Advance less than 1 frame (10ms = 10,000,000ns) -> 0 ticks */
    uint32_t ticks = ps1_clock_advance(&clock, 10000000ull);
    if (ticks != 0) return 4;
    if (clock.accumulator_ns != 10000000ull) return 5;
    if (clock.total_ticks != 0) return 6;

    /* 2. Advance another 10ms (total 20ms) -> exactly 1 tick, remainder 3,333,334ns */
    ticks = ps1_clock_advance(&clock, 10000000ull);
    if (ticks != 1) return 7;
    if (clock.accumulator_ns != 3333334ull) return 8;
    if (clock.total_ticks != 1) return 9;

    /* 3. Advance exactly two ticks worth of time */
    ticks = ps1_clock_advance(&clock, 33333332ull);
    if (ticks != 2) return 10;
    if (clock.total_ticks != 3) return 11;

    /* 4. Large spike (e.g. 1 second = 1,000,000,000ns) must clamp to max_ticks (4) */
    ticks = ps1_clock_advance(&clock, 1000000000ull);
    if (ticks != 4) return 12;
    if (clock.total_ticks != 7) return 13;

    /* 5. NULL pointer safety */
    if (ps1_clock_advance(NULL, 10000000ull) != 0) return 14;

    return 0;
}
