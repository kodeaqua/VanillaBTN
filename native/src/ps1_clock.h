#ifndef VANILLABTN_PS1_CLOCK_H
#define VANILLABTN_PS1_CLOCK_H

/*
 * ps1_clock: Fixed-cadence simulation clock for the native Windows port.
 *
 * Harvest Moon: Back to Nature relies on fixed VSync-derived simulation steps
 * (60 Hz NTSC cadence).  Host monitors on Windows may run at variable refresh
 * rates (60Hz, 120Hz, 144Hz, 240Hz, or variable VRR).
 *
 * This clock accumulates host elapsed time in nanoseconds and produces an exact
 * integer count of 60 Hz simulation ticks, capping catch-up bursts to prevent
 * simulation stalls from cascading.
 */

#include <stdint.h>

#define PS1_TICK_RATE_HZ 60
#define PS1_TICK_DURATION_NS (1000000000ull / PS1_TICK_RATE_HZ) /* 16,666,666 ns */
#define PS1_MAX_TICKS_PER_STEP 4

typedef struct Ps1Clock {
    uint64_t tick_duration_ns;
    uint64_t accumulator_ns;
    uint32_t max_ticks_per_step;
    uint64_t total_ticks;
} Ps1Clock;

void ps1_clock_init(Ps1Clock *clock, uint64_t tick_duration_ns, uint32_t max_ticks);

/* Advance time by delta_ns; returns number of fixed simulation ticks to execute.
 * Excess time beyond max_ticks_per_step * tick_duration_ns is dropped to prevent
 * simulation stalls from cascading. */
uint32_t ps1_clock_advance(Ps1Clock *clock, uint64_t delta_ns);

#endif /* VANILLABTN_PS1_CLOCK_H */
