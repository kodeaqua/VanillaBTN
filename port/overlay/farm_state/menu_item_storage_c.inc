/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_menu_item_storage_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(item_picture_rect),
        HM_OVERLAY_OBJECT(item_picture_slots),
        HM_OVERLAY_OBJECT(item_number_scratch),
        HM_OVERLAY_OBJECT(item_picture_offset),
        HM_OVERLAY_OBJECT(item_picture_ids),
        HM_OVERLAY_OBJECT(item_result_state),
        HM_OVERLAY_OBJECT(item_cursor_x),
        HM_OVERLAY_OBJECT(item_drop_x),
        HM_OVERLAY_OBJECT(item_held_x),
        HM_OVERLAY_OBJECT(item_cursor_y),
        HM_OVERLAY_OBJECT(item_held_y),
        HM_OVERLAY_OBJECT(item_held_list),
        HM_OVERLAY_OBJECT(item_anim_frame),
        HM_OVERLAY_OBJECT(item_anim_tick),
        HM_OVERLAY_OBJECT(item_markers),
        HM_OVERLAY_OBJECT(item_list_sizes),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
