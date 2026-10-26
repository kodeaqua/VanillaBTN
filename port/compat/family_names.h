#ifndef HMBTN_FAMILY_NAMES_H
#define HMBTN_FAMILY_NAMES_H
#include "psx/disc.h"
/* Read immutable text from the validated owner's archive, outside CD simulation. */
hm_disc_result hm_family_names_load(hm_disc *disc);
u16 *hm_family_name(s32 index);
#endif
