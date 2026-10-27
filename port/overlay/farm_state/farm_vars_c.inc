/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_farm_vars_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(farm_section_id),
        HM_OVERLAY_OBJECT(zone_event_pending),
        HM_OVERLAY_OBJECT(current_zone),
        HM_OVERLAY_OBJECT(farm_startup_phase),
        HM_OVERLAY_OBJECT(farm_store_source),
        HM_OVERLAY_OBJECT(farm_loaded_slot),
        HM_OVERLAY_OBJECT(farm_sector_row),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
