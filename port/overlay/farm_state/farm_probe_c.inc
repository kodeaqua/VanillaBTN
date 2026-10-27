/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_farm_probe_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(farm_cached_status),
        HM_OVERLAY_OBJECT(farm_cached_flags),
        HM_OVERLAY_OBJECT(farm_callbacks_queued),
        HM_OVERLAY_OBJECT(farm_model),
        HM_OVERLAY_OBJECT(farm_choices_default),
        HM_OVERLAY_OBJECT(farm_choices_barn0),
        HM_OVERLAY_OBJECT(farm_choices_barn1),
        HM_OVERLAY_OBJECT(farm_choices_coop0),
        HM_OVERLAY_OBJECT(farm_choices_coop1),
        HM_OVERLAY_OBJECT(farm_choices_flagged),
        HM_OVERLAY_OBJECT(farm_zone_rows),
        HM_OVERLAY_OBJECT(record_season_masks),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
