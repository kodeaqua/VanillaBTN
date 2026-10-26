#ifndef HMBTN_COMPAT_LIBSPU_H
#define HMBTN_COMPAT_LIBSPU_H

#include "compat/types.h"

enum {
    SPU_00CH = 1u << 0,
    SPU_01CH = 1u << 1,
    SPU_02CH = 1u << 2,
    SPU_03CH = 1u << 3,
    SPU_04CH = 1u << 4,
    SPU_05CH = 1u << 5,
    SPU_06CH = 1u << 6,
    SPU_07CH = 1u << 7,
    SPU_08CH = 1u << 8,
    SPU_09CH = 1u << 9,
    SPU_10CH = 1u << 10,
    SPU_11CH = 1u << 11,
    SPU_12CH = 1u << 12,
    SPU_13CH = 1u << 13,
    SPU_14CH = 1u << 14,
    SPU_15CH = 1u << 15,
    SPU_16CH = 1u << 16,
    SPU_17CH = 1u << 17,
    SPU_18CH = 1u << 18,
    SPU_19CH = 1u << 19,
    SPU_20CH = 1u << 20,
    SPU_21CH = 1u << 21,
    SPU_22CH = 1u << 22,
    SPU_23CH = 1u << 23
};

/* Native source-facing configuration, never an on-disc or proprietary SDK ABI.
 * Include only attributes actually accessed by sound_init. */
typedef struct {
    s16 left, right;
} hm_spu_volume;
typedef struct {
    u32 mask;
    hm_spu_volume mvol;
    struct {
        hm_spu_volume volume;
        s32 mix;
    } cd;
} SpuCommonAttr;

void SpuSetCommonAttr(const SpuCommonAttr *attributes);
void SpuGetAllKeysStatus(u8 status[24]);
s32 SpuGetKeyStatus(u32 voices);

#endif
