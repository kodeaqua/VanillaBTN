#include "enhance/dithering/dithering.h"

#if defined(HMBTN_ENH_DITHERING) && HMBTN_ENH_DITHERING

static int s_dither_enabled = 1;

void hm_dither_set_enabled(int enabled) {
    s_dither_enabled = !!enabled;
}

int hm_dither_enabled(void) {
    return s_dither_enabled;
}

#endif
