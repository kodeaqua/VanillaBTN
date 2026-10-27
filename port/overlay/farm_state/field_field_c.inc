/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_field_field_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(cell_looks),
        HM_OVERLAY_OBJECT(column_mirror_table),
        HM_OVERLAY_OBJECT(field_column),
        HM_OVERLAY_OBJECT(field_row),
        HM_OVERLAY_OBJECT(field_probe_point),
        HM_OVERLAY_OBJECT(field_cell_look),
        HM_OVERLAY_OBJECT(field_neighbour_x),
        HM_OVERLAY_OBJECT(field_neighbour_y),
        HM_OVERLAY_OBJECT(field_tile_shape),
        HM_OVERLAY_OBJECT(field_retexture_handlers),
        HM_OVERLAY_OBJECT(field_limits),
        HM_OVERLAY_OBJECT(field_grid_sizes),
        HM_OVERLAY_OBJECT(field_neighbour_offsets),
        HM_OVERLAY_OBJECT(event_request_checks),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
