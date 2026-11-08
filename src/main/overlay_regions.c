#include "types.h"
#ifdef VANILLABTN_NATIVE
#include "sdk_memory.h"

/* Keep original data-region offsets within one host allocation. Native scene
 * functions are linked separately; these ranges are never host code pointers. */
u8 *const overlay_region_table[] = {
    sdk_main_ram + 0xB7E20,
    sdk_main_ram + 0x12DDE8,
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
