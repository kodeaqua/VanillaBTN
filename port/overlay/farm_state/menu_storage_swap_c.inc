/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_menu_storage_swap_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(storage_swap_state),
        HM_OVERLAY_OBJECT(storage_swap_cursor),
        HM_OVERLAY_OBJECT(storage_swap_pick),
        HM_OVERLAY_OBJECT(storage_swap_marked),
        HM_OVERLAY_OBJECT(storage_spots),
        HM_OVERLAY_OBJECT(storage_swap_frames),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
