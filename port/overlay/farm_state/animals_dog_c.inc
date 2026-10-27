/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_animals_dog_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(dog_enter_handlers),
        HM_OVERLAY_OBJECT(dog_cue_handlers),
        HM_OVERLAY_OBJECT(cue_anim_table),
        HM_OVERLAY_OBJECT(dog_drive_timer),
        HM_OVERLAY_OBJECT(dog_follow_timer),
        HM_OVERLAY_OBJECT(dog_approach_timer),
        HM_OVERLAY_OBJECT(dog_approach_stage),
        HM_OVERLAY_OBJECT(dog_target_animal),
        HM_OVERLAY_OBJECT(dog_target_unflagged),
        HM_OVERLAY_OBJECT(dog_target_action),
        HM_OVERLAY_OBJECT(dog_action_row),
        HM_OVERLAY_OBJECT(dog_drive_script),
        HM_OVERLAY_OBJECT(dog_follow_script),
        HM_OVERLAY_OBJECT(dog_approach_script),
        HM_OVERLAY_OBJECT(dog_action_table),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
