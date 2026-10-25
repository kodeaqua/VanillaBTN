#include <libgte.h>

#include "psx/unimplemented.h"

/* Bring-up stubs: no approximation of PS1 math is allowed to pass silently. */
s32 rsin(s32 angle) {
    (void)angle;
    hm_unimplemented("rsin");
}

s32 rcos(s32 angle) {
    (void)angle;
    hm_unimplemented("rcos");
}

s32 SquareRoot0(s32 value) {
    (void)value;
    hm_unimplemented("SquareRoot0");
}

s32 VectorNormal(VECTOR *input, VECTOR *output) {
    (void)input;
    (void)output;
    hm_unimplemented("VectorNormal");
}
