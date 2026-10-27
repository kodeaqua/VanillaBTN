/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_people_harvest_sprites_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(harvest_frame_rows),
        HM_OVERLAY_OBJECT(harvest_helper_offsets),
        HM_OVERLAY_OBJECT(harvest_init_pairs),
        HM_OVERLAY_OBJECT(harvest_helper_name_offsets),
        HM_OVERLAY_OBJECT(harvest_helper_start_handlers),
        HM_OVERLAY_OBJECT(harvest_helper_tick_handlers),
        HM_OVERLAY_OBJECT(harvest_seasonal_results),
        HM_OVERLAY_OBJECT(harvest_helper_messages),
        HM_OVERLAY_OBJECT(harvest_helper_greet_menus),
        HM_OVERLAY_OBJECT(harvest_helper_cue_ids),
        HM_OVERLAY_OBJECT(harvest_helper_special_cue_ids),
        HM_OVERLAY_OBJECT(harvest_helper_mood_deltas),
        HM_OVERLAY_OBJECT(harvest_helper_lines),
        HM_OVERLAY_OBJECT(harvest_barn_placements),
        HM_OVERLAY_OBJECT(harvest_coop_placements),
        HM_OVERLAY_OBJECT(harvest_placements_kind2),
        HM_OVERLAY_OBJECT(harvest_placements_kind3),
        HM_OVERLAY_OBJECT(harvest_placements_kind4),
        HM_OVERLAY_OBJECT(harvest_placements_kind5),
        HM_OVERLAY_OBJECT(harvest_slot_rows),
        HM_OVERLAY_OBJECT(harvest_spot_bases),
        HM_OVERLAY_OBJECT(harvest_helper_spots),
        HM_OVERLAY_OBJECT(harvest_row_bonuses),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
