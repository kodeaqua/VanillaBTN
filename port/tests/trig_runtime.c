#include "compat/fixed_math.h"
#include <libgte.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    CHECK(ratan2(0, 0) == 0 && ratan2(0, 1) == 0 && ratan2(0, -1) == 2048);
    CHECK(ratan2(1, 0) == 1024 && ratan2(-1, 0) == -1024);
    CHECK(ratan2(1, 1) == 512 && ratan2(-1, 1) == -512);
    CHECK(ratan2(1, -1) == 1536 && ratan2(-1, -1) == -1536);
    CHECK(ratan2(INT32_MIN, INT32_MIN) == -1536);
    CHECK(ratan2(INT32_MAX, INT32_MIN) == 1536);
    for (int y = -127; y <= 127; ++y)
        for (int x = -127; x <= 127; ++x) {
            int expected = (int)lround(atan2((double)y, (double)x) * 4096 / 6.283185307179586);
            CHECK(abs(ratan2(y, x) - expected) <= 1);
        }
    CHECK(hm_psx_csqrt(0) == 0 && hm_psx_csqrt(4096) == 4096 && hm_psx_csqrt(16384) == 8192);
    for (uint32_t v = 1; v<INT32_MAX; v = v> INT32_MAX / 2 ? INT32_MAX : v * 2 + 1) {
        uint64_t n = (uint64_t)v << 12, r = (uint32_t)hm_psx_csqrt((s32)v);
        CHECK(r * r <= n && (r + 1) * (r + 1) > n);
        if (v == INT32_MAX)
            break;
    }
    puts("Integer atan2 quadrants/extremes and Q12 square roots passed");
    return 0;
}
