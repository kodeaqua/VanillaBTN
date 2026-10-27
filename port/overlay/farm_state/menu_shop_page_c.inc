/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_menu_shop_page_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(shop_slot_values),
        HM_OVERLAY_OBJECT(shop_slot_current),
        HM_OVERLAY_OBJECT(shop_gold),
        HM_OVERLAY_OBJECT(shop_gold_pending),
        HM_OVERLAY_OBJECT(shop_number_text),
        HM_OVERLAY_OBJECT(shop_menu_state),
        HM_OVERLAY_OBJECT(shop_cursor),
        HM_OVERLAY_OBJECT(shop_item_name_offsets),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
