/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_menu_house_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(options_four_cursor),
        HM_OVERLAY_OBJECT(options_pick_choice),
        HM_OVERLAY_OBJECT(options_pick_result),
        HM_OVERLAY_OBJECT(cooking_record_values),
        HM_OVERLAY_OBJECT(farm_menu_handler),
        HM_OVERLAY_OBJECT(farm_menu_handler_last),
        HM_OVERLAY_OBJECT(cooking_menu_step),
        HM_OVERLAY_OBJECT(cooking_menu_substep),
        HM_OVERLAY_OBJECT(cooking_menu_selection),
        HM_OVERLAY_OBJECT(cooking_menu_cursor),
        HM_OVERLAY_OBJECT(cooking_menu_group),
        HM_OVERLAY_OBJECT(cooking_toggle_row),
        HM_OVERLAY_OBJECT(cooking_menu_page),
        HM_OVERLAY_OBJECT(cooking_unused_801273EB),
        HM_OVERLAY_OBJECT(cooking_record_index),
        HM_OVERLAY_OBJECT(cooking_option_flags),
        HM_OVERLAY_OBJECT(cooking_item_chosen),
        HM_OVERLAY_OBJECT(cooking_utensil_items),
        HM_OVERLAY_OBJECT(cooking_value_codes),
        HM_OVERLAY_OBJECT(cooking_unused_80127424),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
