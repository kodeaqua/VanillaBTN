/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_animals_animals_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(farm_draw_point),
        HM_OVERLAY_OBJECT(farm_draw_screen_xy),
        HM_OVERLAY_OBJECT(farm_request_screen_xy),
        HM_OVERLAY_OBJECT(farm_request_draw_point),
        HM_OVERLAY_OBJECT(farm_walker_phase),
        HM_OVERLAY_OBJECT(farm_mode_check_timers),
        HM_OVERLAY_OBJECT(farm_mode_cue_latch),
        HM_OVERLAY_OBJECT(farm_mode_check_handle),
        HM_OVERLAY_OBJECT(farm_walker_not_stray),
        HM_OVERLAY_OBJECT(farm_message_days),
        HM_OVERLAY_OBJECT(farm_turn_weights),
        HM_OVERLAY_OBJECT(farm_proximity_threshold),
        HM_OVERLAY_OBJECT(farm_mode_cues),
        HM_OVERLAY_OBJECT(farm_visitor_positions),
        HM_OVERLAY_OBJECT(farm_mode_spawn_positions),
        HM_OVERLAY_OBJECT(farm_spawn_table),
        HM_OVERLAY_OBJECT(chicken_spawn_sources),
        HM_OVERLAY_OBJECT(farm_move_compare_table),
        HM_OVERLAY_OBJECT(farm_walker_timer),
        HM_OVERLAY_OBJECT(farm_heading_table),
        HM_OVERLAY_OBJECT(farm_row_spots),
        HM_OVERLAY_OBJECT(farm_grid_spots),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
