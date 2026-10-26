#include "platform/presentation_clock.h"

int hm_present_rate_equal(hm_present_rate a, hm_present_rate b) {
    return a.numerator && b.numerator && a.denominator && b.denominator &&
           (u64)a.numerator * b.denominator == (u64)b.numerator * a.denominator;
}
int hm_present_clock_init(hm_present_clock *c, u64 now, hm_present_rate rate) {
    if (!c || !rate.numerator || !rate.denominator || rate.denominator > 1000000 ||
        rate.numerator < rate.denominator || (u64)rate.numerator > (u64)rate.denominator * 1000)
        return 0;
    u64 period = 1000000000ull * rate.denominator;
    *c = (hm_present_clock){now, period / rate.numerator, (u32)(period % rate.numerator), 0,
                            rate.numerator};
    return 1;
}
static void hm_present_step(hm_present_clock *c) {
    c->next_ns += c->whole_ns;
    u64 phase = (u64)c->phase + c->remainder;
    c->next_ns += phase / c->numerator;
    c->phase = (u32)(phase % c->numerator);
}
int hm_present_clock_due(hm_present_clock *c, u64 now) {
    if (!c || !c->numerator || now < c->next_ns)
        return 0;
    hm_present_step(c);
    if (c->next_ns <= now) {
        /* Discard missed presentations instead of submitting a catch-up burst.
         * Only this presentation schedule is rebased; game ticks are untouched. */
        c->next_ns = now;
        c->phase = 0;
        hm_present_step(c);
    }
    return 1;
}
