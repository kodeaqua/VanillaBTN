/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_people_villager_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(villager_route_mode),
        HM_OVERLAY_OBJECT(villager_greet_dist_sq),
        HM_OVERLAY_OBJECT(villager_greet_min_dist),
        HM_OVERLAY_OBJECT(schedule_row),
        HM_OVERLAY_OBJECT(schedule_entry),
        HM_OVERLAY_OBJECT(one_minute),
        HM_OVERLAY_OBJECT(villager_speed_outdoors),
        HM_OVERLAY_OBJECT(villager_speed_indoors),
        HM_OVERLAY_OBJECT(position_correction),
        HM_OVERLAY_OBJECT(villager_parity),
        HM_OVERLAY_OBJECT(facing_angles),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
