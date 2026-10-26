/* Synthetic OT construction only; not a frame/rendering or game benchmark. */
#include <libgpu.h>

#include "psx/gpu_links.h"

#include <stdio.h>
#include <time.h>

enum { HM_BENCH_OT_WORDS = 2048, HM_BENCH_PACKETS = 1000, HM_BENCH_FRAMES = 100000 };
static u32 hm_bench_ot[HM_BENCH_OT_WORDS];
static u32 hm_bench_packets[HM_BENCH_PACKETS][4];

static void hm_bench_construct(void) {
    ClearOTagR(hm_bench_ot, HM_BENCH_OT_WORDS);
    for (u32 index = 0; index < HM_BENCH_PACKETS; ++index) {
        setlen(hm_bench_packets[index], 3);
        addPrim(&hm_bench_ot[index], hm_bench_packets[index]);
    }
}

int main(void) {
    hm_gpu_links_reset();
    hm_bench_construct(); /* Register the persistent addresses before timing. */
    clock_t start = clock();
    for (u32 frame = 0; frame < HM_BENCH_FRAMES; ++frame) {
        hm_bench_construct();
    }
    clock_t end = clock();
    if (start == (clock_t)-1 || end == (clock_t)-1 || end <= start) {
        fputs("No usable clock measurement\n", stderr);
        return 1;
    }
    double milliseconds = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;
    printf("frames=%d ot_words=%d packets=%d total_ms=%.3f ms_per_construct=%.6f\n",
           HM_BENCH_FRAMES, HM_BENCH_OT_WORDS, HM_BENCH_PACKETS,
           milliseconds, milliseconds / HM_BENCH_FRAMES);
    return 0;
}
