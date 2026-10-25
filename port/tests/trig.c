#include "compat/fixed_math.h"
#include <libgte.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    for (s32 angle = 0; angle < 4096; ++angle) {
        double value = sin((double)angle * 6.283185307179586476925286766559 / 4096) * 4096;
        s32 expected = (s32)(value < 0 ? ceil(value - 0.5) : floor(value + 0.5));
        if (rsin(angle) != expected || rcos(angle) != rsin(angle + 1024) ||
            rsin(angle - 4096) != expected || hm_psx_csin(angle) != expected ||
            hm_psx_ccos(angle) != rcos(angle)) {
            fprintf(stderr, "Q12 sine mismatch at phase %d\n", angle);
            return 1;
        }
    }
    if (rsin(0) != 0 || rsin(512) != 2896 || rsin(1024) != 4096 || rsin(-1024) != -4096 ||
        rcos(INT32_MIN) != 4096 || rcos(INT32_MAX) != 4096)
        return 1;
    return 0;
}
