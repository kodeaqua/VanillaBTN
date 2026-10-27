/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_event_milestones_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(milestone_clut_states),
        HM_OVERLAY_OBJECT(milestone_clut_colors),
        HM_OVERLAY_OBJECT(milestone_timer),
        HM_OVERLAY_OBJECT(milestone_result_frame),
        HM_OVERLAY_OBJECT(milestone_season_hours),
        HM_OVERLAY_OBJECT(milestone_result_start),
        HM_OVERLAY_OBJECT(milestone_result_end),
        HM_OVERLAY_OBJECT(milestone_group_a_ids),
        HM_OVERLAY_OBJECT(milestone_group_b_ids),
        HM_OVERLAY_OBJECT(milestone_group_c_ids),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
