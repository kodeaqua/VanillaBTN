#include "compat/async_wait.h"
#include "psx/timing.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned polls, ticks;
s32 sound_fader_ready(void) { return hm_timing_ticks() >= 3; }
void sound_load_tick(void) {
    if (++polls > 6) {
        fputs("Loader wait did not yield\n", stderr);
        exit(1);
    }
}
static void tick(void *context, u64 frame) {
    (void)context;
    (void)frame;
    ++ticks;
}
int main(void) {
    hm_timing_reset((hm_timing_host){NULL, tick, NULL});
    hm_wait_sound_fader();
    if (polls != 3 || ticks != 3 || hm_timing_ticks() != 3)
        return 1;
    hm_wait_sound_fader();
    if (polls != 3 || ticks != 3)
        return 1;
    puts("Asynchronous sound wait advances VBlank callbacks and exits when ready");
    return 0;
}
