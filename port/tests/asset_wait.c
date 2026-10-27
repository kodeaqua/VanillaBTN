#include "afile.h"
#include "psx/timing.h"
#include <stdio.h>
#include <stdlib.h>
u8 *hm_blocking_asset_read(s32, s32, s32, u8 *, s32, s32);
static int polls, bad;
static u64 start;
static u8 data[32];
u8 *afile_read_bytes(s32 file, s32 offset, s32 bytes, u8 *destination, s32 align, s32 restart) {
    if (file != 7 || offset != 12 || bytes != 32 || destination != data || align != 1)
        bad = 1;
    if (restart) {
        polls = 0;
        start = hm_timing_ticks();
        return NULL;
    }
    if (++polls > 6) {
        fprintf(stderr, "Blocking asset poll made no hardware progress\n");
        exit(1);
    }
    return hm_timing_ticks() - start >= 2 ? data : NULL;
}
int main(void) {
    hm_timing_reset((hm_timing_host){0});
    if (hm_blocking_asset_read(7, 12, 32, data, 1, 1) || hm_timing_ticks() != 0)
        return 1;
    u8 *ready;
    do {
        ready = hm_blocking_asset_read(7, 12, 32, data, 1, 0);
    } while (!ready);
    if (bad || ready != data || polls != 3 || hm_timing_ticks() != 2)
        return 1;
    if (hm_blocking_asset_read(7, 12, 32, data, 1, 0) != data || hm_timing_ticks() != 2)
        return 1;
    return 0;
}
