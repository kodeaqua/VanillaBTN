/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_event_messages_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(message_index),
        HM_OVERLAY_OBJECT(message_phase),
        HM_OVERLAY_OBJECT(message_pairs),
        HM_OVERLAY_OBJECT(message_window),
        HM_OVERLAY_OBJECT(message_scroll_counter),
        HM_OVERLAY_OBJECT(message_scroll_group),
        HM_OVERLAY_OBJECT(message_kind_ids),
        HM_OVERLAY_OBJECT(message_calendar_rows),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
