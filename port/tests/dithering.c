#include "enhance/dithering/dithering.h"
#include <stdio.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    /* 1. Default state: dithering must be enabled for vanilla parity */
    CHECK(hm_dither_enabled() == 1);

    /* 2. Disable dithering */
    hm_dither_set_enabled(0);
#ifndef HMBTN_ENH_DITHERING
    CHECK(hm_dither_enabled() == 1);
    return 0;
#endif
    CHECK(hm_dither_enabled() == 0);

    /* 3. Re-enable dithering */
    hm_dither_set_enabled(1);
    CHECK(hm_dither_enabled() == 1);

    return 0;
}
