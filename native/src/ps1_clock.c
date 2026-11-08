#include "ps1_clock.h"

#include <stddef.h>

void ps1_clock_init(Ps1Clock *clock, uint64_t tick_duration_ns, uint32_t max_ticks) {
    if (clock == NULL) return;
    clock->tick_duration_ns = tick_duration_ns > 0 ? tick_duration_ns : PS1_TICK_DURATION_NS;
    clock->max_ticks_per_step = max_ticks > 0 ? max_ticks : PS1_MAX_TICKS_PER_STEP;
    clock->accumulator_ns = 0;
    clock->total_ticks = 0;
}

uint32_t ps1_clock_advance(Ps1Clock *clock, uint64_t delta_ns) {
    if (clock == NULL || clock->tick_duration_ns == 0) return 0;

    clock->accumulator_ns += delta_ns;

    /* Clamp accumulator to avoid spiral of death */
    uint64_t max_accum = (uint64_t)clock->max_ticks_per_step * clock->tick_duration_ns;
    if (clock->accumulator_ns > max_accum) {
        clock->accumulator_ns = max_accum;
    }

    uint32_t ticks = (uint32_t)(clock->accumulator_ns / clock->tick_duration_ns);
    clock->accumulator_ns %= clock->tick_duration_ns;
    clock->total_ticks += ticks;
    return ticks;
}
