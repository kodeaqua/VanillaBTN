/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_animals_chicken_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(farm_actor_enter_handlers),
        HM_OVERLAY_OBJECT(farm_actor_cue_handlers),
        HM_OVERLAY_OBJECT(farm_anim_table),
        HM_OVERLAY_OBJECT(chicken_cue_steps_adult),
        HM_OVERLAY_OBJECT(chicken_cue_steps_chick),
        HM_OVERLAY_OBJECT(chicken_cue_steps_adult_2),
        HM_OVERLAY_OBJECT(chicken_cue_steps_chick_2),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
