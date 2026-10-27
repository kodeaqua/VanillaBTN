/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_estate_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(estate_screen_xy),
        HM_OVERLAY_OBJECT(estate_point),
        HM_OVERLAY_OBJECT(estate_current_object),
        HM_OVERLAY_OBJECT(record18_bounce_step),
        HM_OVERLAY_OBJECT(estate_op_pending),
        HM_OVERLAY_OBJECT(estate_op_cue_pending),
        HM_OVERLAY_OBJECT(record18_bounce_offsets),
        HM_OVERLAY_OBJECT(estate_result_rows_house0),
        HM_OVERLAY_OBJECT(estate_result_rows_house1),
        HM_OVERLAY_OBJECT(estate_result_rows_house2),
        HM_OVERLAY_OBJECT(estate_kind_values),
        HM_OVERLAY_OBJECT(estate_season_codes),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
