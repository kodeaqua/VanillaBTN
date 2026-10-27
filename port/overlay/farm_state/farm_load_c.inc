/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_farm_load_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(section_table),
        HM_OVERLAY_OBJECT(farm_season_file_offsets),
        HM_OVERLAY_OBJECT(farm_slot_file_offsets),
        HM_OVERLAY_OBJECT(farm_item_file_offsets),
        HM_OVERLAY_OBJECT(farm_span_offsets),
        HM_OVERLAY_OBJECT(farm_anim8_offsets),
        HM_OVERLAY_OBJECT(farm_pack_file_offsets),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
