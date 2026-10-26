#ifndef HMBTN_COMPAT_WORK_MEMORY_H
#define HMBTN_COMPAT_WORK_MEMORY_H

#include "compat/types.h"

enum {
    HM_FARM_IMAGE_BYTES = 0x49000,
    HM_FARM_SECTION_CAPACITY = 0x47390,
    HM_FRAME_WORK_BYTES = 0x3EE50
};

extern u8 hm_work_storage[HM_FARM_IMAGE_BYTES + HM_FARM_SECTION_CAPACITY + HM_FRAME_WORK_BYTES];

u8 *hm_farm_load_window(void);
u8 *hm_work_region(void);

#endif
