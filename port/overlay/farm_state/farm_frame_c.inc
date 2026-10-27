/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_farm_frame_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(zone_action_result),
        HM_OVERLAY_OBJECT(player_unused_80128B64),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
