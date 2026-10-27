/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_event_event_script_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(event_script_base),
        HM_OVERLAY_OBJECT(event_script_cursor),
        HM_OVERLAY_OBJECT(event_block_base),
        HM_OVERLAY_OBJECT(event_block_offsets),
        HM_OVERLAY_OBJECT(event_step_index),
        HM_OVERLAY_OBJECT(event_script_unused_80129228),
        HM_OVERLAY_OBJECT(event_staging_sprites),
        HM_OVERLAY_OBJECT(event_sprite_offset_x),
        HM_OVERLAY_OBJECT(event_sprite_offset_y),
        HM_OVERLAY_OBJECT(event_block_size),
        HM_OVERLAY_OBJECT(event_request_status),
        HM_OVERLAY_OBJECT(chunk_checksums),
        HM_OVERLAY_OBJECT(vram_stash_tail),
        HM_OVERLAY_OBJECT(event_script_offsets),
        HM_OVERLAY_OBJECT(vram_chunk_offsets),
        HM_OVERLAY_OBJECT(vram_stash_rect),
        HM_OVERLAY_OBJECT(event_script_handlers),
        HM_OVERLAY_OBJECT(event_state_handlers),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
