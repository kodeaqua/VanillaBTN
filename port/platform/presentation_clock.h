#ifndef HMBTN_PLATFORM_PRESENTATION_CLOCK_H
#define HMBTN_PLATFORM_PRESENTATION_CLOCK_H
#include "compat/types.h"

typedef struct {
    u32 numerator, denominator;
} hm_present_rate;
typedef struct {
    u64 next_ns, whole_ns;
    u32 remainder, phase, numerator;
} hm_present_clock;
/* Absolute monotonic presentation deadlines. Never advances simulation time. */
int hm_present_clock_init(hm_present_clock *clock, u64 now_ns, hm_present_rate rate);
int hm_present_clock_due(hm_present_clock *clock, u64 now_ns);
int hm_present_rate_equal(hm_present_rate a, hm_present_rate b);
#endif
