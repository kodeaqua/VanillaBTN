/* Real objects only; const objects are filtered by their C type. */
#include "overlay/state_inventory.h"
hm_overlay_inventory hm_farm_inventory_menu_calendar_c(void) {
    static const hm_overlay_span spans[] = {
        HM_OVERLAY_OBJECT(calendar_scratch),
        HM_OVERLAY_OBJECT(calendar_state),
        HM_OVERLAY_OBJECT(calendar_choice),
        HM_OVERLAY_OBJECT(calendar_cursor_day),
        HM_OVERLAY_OBJECT(calendar_close_hook_active),
        HM_OVERLAY_OBJECT(calendar_events),
        HM_OVERLAY_OBJECT(calendar_display_entries),
    };
    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};
}
