#ifndef VANILLABTN_COMPAT_LIBSPU_H
#define VANILLABTN_COMPAT_LIBSPU_H

#include "sdk_types.h"

#define SPU_OFF 0
#define SPU_ON 1
#define SPU_OFF_ENV_ON 2
#define SPU_ON_ENV_OFF 3

#define SPU_COMMON_MVOLL (1 << 0)
#define SPU_COMMON_MVOLR (1 << 1)
#define SPU_COMMON_CDVOLL (1 << 6)
#define SPU_COMMON_CDVOLR (1 << 7)
#define SPU_COMMON_CDMIX (1 << 9)

typedef struct SpuVolume {
    short left;
    short right;
} SpuVolume;

typedef struct SpuExtAttr {
    SpuVolume volume;
    long reverb;
    long mix;
} SpuExtAttr;

typedef struct SpuCommonAttr {
    unsigned long mask;
    SpuVolume mvol;
    SpuVolume mvolmode;
    SpuVolume mvolx;
    SpuExtAttr cd;
    SpuExtAttr ext;
} SpuCommonAttr;

void SpuSetCommonAttr(SpuCommonAttr *attr);
void SpuGetAllKeysStatus(void *status);
long SpuGetKeyStatus(unsigned long voice_bit);

#endif
