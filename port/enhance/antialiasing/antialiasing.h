#ifndef HMBTN_ANTIALIASING_H
#define HMBTN_ANTIALIASING_H
#include "compat/types.h"
enum { HM_AA_OFF, HM_AA_FXAA, HM_AA_MSAA2, HM_AA_MSAA4, HM_AA_COUNT };
unsigned hm_antialiasing_samples(int mode);
int hm_antialiasing_cycle(int mode, int direction, unsigned supported);
const char *hm_antialiasing_name(int mode);
/* Scalar coverage reference: Q8 vertices, integer output pixel coordinates. */
unsigned hm_antialiasing_coverage(const s32 vertices[6], int x, int y, unsigned samples);
#endif
