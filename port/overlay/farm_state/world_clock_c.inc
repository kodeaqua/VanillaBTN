/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_world_clock_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(clock_queue),
        HM_OVERLAY_OBJECT(clock_pending),
        HM_OVERLAY_OBJECT(clock_stepping),
        HM_OVERLAY_OBJECT(clock_fast_forward_left),
        HM_OVERLAY_OBJECT(clock_end_flag),
        HM_OVERLAY_OBJECT(clock_events),
        HM_OVERLAY_OBJECT(clock_callbacks),
        HM_OVERLAY_OBJECT(clock_date_table),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
