#include "psx/timing.h"
#include <libapi.h>
#include <libetc.h>
#include <stdio.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static u64 waited;
static u32 callbacks;
static void hm_wait(void *context, u64 tick) {
    (void)context;
    waited = tick;
}
static s32 hm_scanlines(void *context) {
    (void)context;
    return 17;
}
static void hm_callback(void) { ++callbacks; }

int main(void) {
    hm_timing_reset((hm_timing_host){NULL, hm_wait, hm_scanlines});
    CHECK(ResetCallback() == 0 && SetVideoMode(0) == 0);
    SetMem(2);
    CHECK(VSyncCallback(hm_callback) == NULL);
    CHECK(VSync(1) == 17 && VSync(-1) == 0 && waited == 0 && callbacks == 0);
    CHECK(VSync(0) == 0 && waited == 1 && callbacks == 1 && VSync(-1) == 1);
    CHECK(ResetCallback() == 0);
    CHECK(VSync(2) == 0 && waited == 3 && callbacks == 3 && hm_timing_ticks() == 3);
    CHECK(VSync(3) == 0 && waited == 6 && callbacks == 6);
    CHECK(VSyncCallback(NULL) == hm_callback);
    CHECK(VSync(0) == 0 && waited == 7 && callbacks == 6);
    hm_timing_reset((hm_timing_host){0});
    CHECK(VSync(1) == 0 && VSync(-1) == 0);
    return 0;
}
