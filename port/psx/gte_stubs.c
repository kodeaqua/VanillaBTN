#include <libgte.h>

#include "psx/unimplemented.h"

/* Remaining high-level math must fail explicitly until implemented. */
s32 SquareRoot0(s32 value) {
    (void)value;
    hm_unimplemented("SquareRoot0");
}

s32 VectorNormal(VECTOR *input, VECTOR *output) {
    (void)input;
    (void)output;
    hm_unimplemented("VectorNormal");
}
