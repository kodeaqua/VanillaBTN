/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_field_fishing_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(fishing_marker_phase),
        HM_OVERLAY_OBJECT(fishing_marker_vertex),
        HM_OVERLAY_OBJECT(fishing_marker_screen),
        HM_OVERLAY_OBJECT(fishing_cell_texture),
        HM_OVERLAY_OBJECT(fishing_state),
        HM_OVERLAY_OBJECT(overview_camera),
        HM_OVERLAY_OBJECT(fishing_unused_8012DB70),
        HM_OVERLAY_OBJECT(fishing_marker_origin),
        HM_OVERLAY_OBJECT(fishing_marker_active),
        HM_OVERLAY_OBJECT(fishing_marker_value),
        HM_OVERLAY_OBJECT(fishing_marker_frame),
        HM_OVERLAY_OBJECT(fishing_streak),
        HM_OVERLAY_OBJECT(fishing_bite_ranges),
        HM_OVERLAY_OBJECT(fishing_bite_amounts),
        HM_OVERLAY_OBJECT(fishing_spot_catches_1),
        HM_OVERLAY_OBJECT(fishing_spot_catches_3),
        HM_OVERLAY_OBJECT(fishing_spot_catches_2),
        HM_OVERLAY_OBJECT(fishing_spot_catches_4),
        HM_OVERLAY_OBJECT(fishing_season_catches),
        HM_OVERLAY_OBJECT(fishing_fallback_catch),
        HM_OVERLAY_OBJECT(cell_texture_entries),
        HM_OVERLAY_OBJECT(grid_column_forward_bits),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
