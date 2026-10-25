#include "types.h"

#ifdef HMBTN_PORT
/* PORT: asset workspace only; code stays statically linked -- see docs/port/DECISIONS.md#adr-017 */
#include "compat/work_memory.h"
u8 *const overlay_region_table[] = {
    hm_work_storage,
    hm_work_storage + HM_FARM_IMAGE_BYTES,
};
#else
/* The two overlay load regions. Overlays are read in at the low one or the high one. */
extern u8 overlay_low[];
extern u8 overlay_high[];

/* OVERLAY_LOW is where the EXE's .bss ends: most overlays load there (farm, minigames, title).
 * OVERLAY_HIGH is where the farm's image ends: saveload and swind load there, and some farm loads too. */
u8 *const overlay_region_table[] = {
    overlay_low,
    overlay_high,
};
#endif
