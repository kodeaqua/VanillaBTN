/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_field_greenhouse_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(greenhouse_column),
        HM_OVERLAY_OBJECT(greenhouse_row),
        HM_OVERLAY_OBJECT(greenhouse_probe_point),
        HM_OVERLAY_OBJECT(greenhouse_level_values),
        HM_OVERLAY_OBJECT(greenhouse_level_alt_values),
        HM_OVERLAY_OBJECT(greenhouse_quad_uvs),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
