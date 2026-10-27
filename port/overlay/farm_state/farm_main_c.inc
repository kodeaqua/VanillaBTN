/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_farm_main_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(farm_enter_handlers),
        HM_OVERLAY_OBJECT(section_load_pending),
        HM_OVERLAY_OBJECT(farm_music_enabled),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
