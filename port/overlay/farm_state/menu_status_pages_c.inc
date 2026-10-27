/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_menu_status_pages_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(status_day_text),
        HM_OVERLAY_OBJECT(status_number_scratch),
        HM_OVERLAY_OBJECT(status_pack),
        HM_OVERLAY_OBJECT(status_text_line),
        HM_OVERLAY_OBJECT(status_farm_percentage),
        HM_OVERLAY_OBJECT(status_building_score),
        HM_OVERLAY_OBJECT(status_recipe_count),
        HM_OVERLAY_OBJECT(status_page_sprites),
        HM_OVERLAY_OBJECT(status_cursor_sprite),
        HM_OVERLAY_OBJECT(status_mark_sprite),
        HM_OVERLAY_OBJECT(status_latch),
        HM_OVERLAY_OBJECT(status_sequence_step),
        HM_OVERLAY_OBJECT(status_page),
        HM_OVERLAY_OBJECT(status_list_top),
        HM_OVERLAY_OBJECT(status_cycle_frames),
        HM_OVERLAY_OBJECT(status_band_thresholds),
        HM_OVERLAY_OBJECT(status_list_rows),
        HM_OVERLAY_OBJECT(status_summary_entries),
        HM_OVERLAY_OBJECT(status_low_feed),
        HM_OVERLAY_OBJECT(status_shipment_rows),
        HM_OVERLAY_OBJECT(status_sprite_offsets),
        HM_OVERLAY_OBJECT(status_page_handlers),
        HM_OVERLAY_OBJECT(status_mark_table),
        HM_OVERLAY_OBJECT(status_heading_xs),
        HM_OVERLAY_OBJECT(status_ledger_offsets),
        HM_OVERLAY_OBJECT(status_level_indices),
        HM_OVERLAY_OBJECT(status_mark_frames),
        HM_OVERLAY_OBJECT(status_gauge_colors_a),
        HM_OVERLAY_OBJECT(status_gauge_colors_b),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
