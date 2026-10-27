/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_menu_options_menu_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(options_settings),
        HM_OVERLAY_OBJECT(options_menu_state),
        HM_OVERLAY_OBJECT(options_main_index),
        HM_OVERLAY_OBJECT(options_column),
        HM_OVERLAY_OBJECT(options_row),
        HM_OVERLAY_OBJECT(options_settings_column),
        HM_OVERLAY_OBJECT(options_settings_row),
        HM_OVERLAY_OBJECT(options_state_handlers),
        HM_OVERLAY_OBJECT(options_row2_handlers),
        HM_OVERLAY_OBJECT(options_row3_handlers),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
