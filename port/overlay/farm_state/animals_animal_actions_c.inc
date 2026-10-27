/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_animals_animal_actions_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(farm_action_countdown_step),
        HM_OVERLAY_OBJECT(farm_action_start_value),
        HM_OVERLAY_OBJECT(farm_action_start_handlers),
        HM_OVERLAY_OBJECT(farm_action_tick_handlers),
        HM_OVERLAY_OBJECT(farm_table_pairs),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
