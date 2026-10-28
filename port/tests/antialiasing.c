#include "enhance/antialiasing/antialiasing.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d\n", __LINE__); return 1; } } while (0)
int main(void) {
    CHECK(hm_antialiasing_samples(HM_AA_OFF) == 1 && hm_antialiasing_samples(HM_AA_FXAA) == 1);
    CHECK(hm_antialiasing_samples(HM_AA_MSAA2) == 2 && hm_antialiasing_samples(HM_AA_MSAA4) == 4);
    CHECK(hm_antialiasing_cycle(0, -1, 15) == HM_AA_MSAA4);
    CHECK(hm_antialiasing_cycle(HM_AA_FXAA, 1, 3) == HM_AA_OFF);
    CHECK(hm_antialiasing_cycle(HM_AA_OFF, 1, 1) == HM_AA_OFF);
    CHECK(hm_antialiasing_cycle(HM_AA_FXAA, 1, 9) == HM_AA_MSAA4);
    const s32 a[] = {0, 0, 8 * 256, 0, 0, 8 * 256};
    const s32 b[] = {8 * 256, 8 * 256, 0, 8 * 256, 8 * 256, 0};
    for (unsigned samples = 1; samples <= 4; samples *= 2)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                unsigned left = hm_antialiasing_coverage(a, x, y, samples);
                unsigned right = hm_antialiasing_coverage(b, x, y, samples);
                CHECK(!(left & right));
                CHECK((left | right) == (1u << samples) - 1);
            }
    CHECK(hm_antialiasing_coverage(a, -1, -1, 4) == 0);
    CHECK(hm_antialiasing_coverage(a, 0, 0, 3) == 0);
    const s32 large[] = {-1048576, -1048576, 1048576, -1048576, 0, 1048576};
    CHECK(hm_antialiasing_coverage(large, 0, 0, 4) == 15);
    return 0;
}
