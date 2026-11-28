#include "types.h"

#ifdef __psyz
#include "overlay_regions.h"

/* Keep the load-window and work-region distances from the PS1 linker layout. */
enum { HIGH_REGION_OFFSET = 0x75FC8, WORK_REGION_OFFSET = 0x106390 };
static _Alignas(OT_TYPE) u8 native_scratch[WORK_REGION_OFFSET + FRAME_REGION_BYTES];
u8 *const native_work_region = native_scratch + WORK_REGION_OFFSET;

u8 *const overlay_region_table[] = {
    native_scratch,
    native_scratch + HIGH_REGION_OFFSET,
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
