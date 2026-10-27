/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_menu_photo_album_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(photo_sprite_header),
        HM_OVERLAY_OBJECT(photo_sprites),
        HM_OVERLAY_OBJECT(ending_track_latch),
        HM_OVERLAY_OBJECT(ending_track_arg),
        HM_OVERLAY_OBJECT(photo_fade_state),
        HM_OVERLAY_OBJECT(photo_map_spot),
        HM_OVERLAY_OBJECT(photo_id_pairs),
        HM_OVERLAY_OBJECT(photo_map_spots),
        HM_OVERLAY_OBJECT(photo_screen_rect),
        HM_OVERLAY_OBJECT(track_message_offsets),
        HM_OVERLAY_OBJECT(photo_sequence_step),
        HM_OVERLAY_OBJECT(top_score_texts),
        HM_OVERLAY_OBJECT(top_score_indices),
        HM_OVERLAY_OBJECT(item_choice_message),
        HM_OVERLAY_OBJECT(photo_countdown),
        HM_OVERLAY_OBJECT(saved_status_latch),
        HM_OVERLAY_OBJECT(message_choice_rows),
        HM_OVERLAY_OBJECT(message_extra_rows),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
