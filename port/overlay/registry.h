#ifndef HMBTN_OVERLAY_REGISTRY_H
#define HMBTN_OVERLAY_REGISTRY_H

#include "compat/types.h"

/* Registry metadata only, not a reset of compiled overlay globals. */
void hm_overlay_reset(void);
s32 hm_overlay_select(s32 member, s32 region);
s32 hm_overlay_active(s32 region);

#endif
