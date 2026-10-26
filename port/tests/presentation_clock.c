#include "platform/presentation_clock.h"
#include "psx/timing.h"
#include <libetc.h>
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static hm_present_clock hm_clock;
static u64 hm_presentations, hm_callbacks;
static void hm_callback(void) { ++hm_callbacks; }
static void hm_wait(void *context, u64 tick) {
    (void)context;
    u64 deadline = tick * 1000000000ull / 60;
    while (hm_clock.next_ns <= deadline) {
        if (hm_present_clock_due(&hm_clock, hm_clock.next_ns))
            ++hm_presentations;
    }
}
int main(void) {
    const hm_present_rate rates[] = {{30, 1},  {59, 1},  {60, 1},      {75, 1},
                                     {144, 1}, {240, 1}, {60000, 1001}};
    for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        hm_present_clock c;
        CHECK(hm_present_clock_init(&c, 0, rates[i]));
        for (u64 frame = 0; frame < 200000; ++frame) {
            u64 reference = frame * 1000000000ull * rates[i].denominator / rates[i].numerator;
            CHECK(c.next_ns == reference);
            if (reference) CHECK(!hm_present_clock_due(&c, reference - 1));
            CHECK(hm_present_clock_due(&c, reference));
        }
        hm_presentations = hm_callbacks = 0;
        CHECK(hm_present_clock_init(&hm_clock, 0, rates[i]));
        hm_timing_reset((hm_timing_host){NULL, hm_wait, NULL});
        VSyncCallback(hm_callback);
        for (unsigned frame = 0; frame < 300; ++frame)
            VSync(2);
        CHECK(hm_callbacks == 600 && hm_timing_ticks() == 600);
        CHECK(hm_presentations == 10ull * rates[i].numerator / rates[i].denominator + 1);
    }
    hm_present_clock c;
    CHECK(!hm_present_clock_init(&c, 0, (hm_present_rate){0, 1}));
    CHECK(!hm_present_clock_init(&c, 0, (hm_present_rate){60, 0}));
    CHECK(hm_present_rate_equal((hm_present_rate){60000, 1000}, (hm_present_rate){60, 1}));
    CHECK(!hm_present_rate_equal((hm_present_rate){60000, 1001}, (hm_present_rate){60, 1}));
    CHECK(hm_present_clock_init(&c, 0, (hm_present_rate){75, 1}));
    CHECK(hm_present_clock_due(&c, 1000000000ull));
    CHECK(c.next_ns > 1000000000ull && !hm_present_clock_due(&c, 1000000000ull));
    return 0;
}
