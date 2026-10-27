/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_field_mine_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(mine_column),
        HM_OVERLAY_OBJECT(mine_row),
        HM_OVERLAY_OBJECT(mine_probe_point),
        HM_OVERLAY_OBJECT(mine_clear_state),
        HM_OVERLAY_OBJECT(mine_layer_weights),
        HM_OVERLAY_OBJECT(mine_layer_weights_mine),
        HM_OVERLAY_OBJECT(mine_layer_events),
        HM_OVERLAY_OBJECT(mine_layer_events_mine),
        HM_OVERLAY_OBJECT(mine_messages),
        HM_OVERLAY_OBJECT(mine_cell_textures),
        HM_OVERLAY_OBJECT(facing_steps),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
