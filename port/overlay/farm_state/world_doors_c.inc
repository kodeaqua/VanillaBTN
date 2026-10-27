/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_world_doors_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(door_open_angles),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
