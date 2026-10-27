/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_world_scene_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(scene_project_vertex),
        HM_OVERLAY_OBJECT(scene_projected_xy),
        HM_OVERLAY_OBJECT(scene_entries),
        HM_OVERLAY_OBJECT(scene_morphs),
        HM_OVERLAY_OBJECT(scene_unused_80129174),
        HM_OVERLAY_OBJECT(scene_slot_entries),
        HM_OVERLAY_OBJECT(scene_flag_stage),
        HM_OVERLAY_OBJECT(scene_link_entries),
        HM_OVERLAY_OBJECT(scene_handback_state),
        HM_OVERLAY_OBJECT(scene_pan_object),
        HM_OVERLAY_OBJECT(scene_pan_state),
        HM_OVERLAY_OBJECT(scene_tilt_object),
        HM_OVERLAY_OBJECT(scene_tilt_state),
        HM_OVERLAY_OBJECT(scene_marked_entries),
        HM_OVERLAY_OBJECT(house_level_offsets),
        HM_OVERLAY_OBJECT(barn_layout_offsets),
        HM_OVERLAY_OBJECT(coop_layout_offsets),
        HM_OVERLAY_OBJECT(greenhouse_level_offsets),
        HM_OVERLAY_OBJECT(scene_interp_reversed),
        HM_OVERLAY_OBJECT(scene_interp_pos),
        HM_OVERLAY_OBJECT(interp_color_start),
        HM_OVERLAY_OBJECT(interp_color_end),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
