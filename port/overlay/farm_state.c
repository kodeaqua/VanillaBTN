#include "overlay/farm_state.h"
#define HM_FARM_UNIT(id) extern hm_overlay_inventory hm_farm_inventory_##id(void);
#include "farm_units.inc"
#undef HM_FARM_UNIT
typedef hm_overlay_inventory (*hm_farm_provider)(void);
static const hm_farm_provider providers[] = {
#define HM_FARM_UNIT(id) hm_farm_inventory_##id,
#include "farm_units.inc"
#undef HM_FARM_UNIT
};
const hm_overlay_inventory *hm_farm_overlay_parts(size_t *count) {
    static hm_overlay_inventory parts[sizeof providers / sizeof providers[0]];
    static int initialized;
    if (!initialized) {
        for (size_t i = 0; i < sizeof providers / sizeof providers[0]; ++i)
            parts[i] = providers[i]();
        initialized = 1;
    }
    *count = sizeof parts / sizeof parts[0];
    return parts;
}
void hm_farm_overlay_capture(void) {
    size_t count;
    const hm_overlay_inventory *parts = hm_farm_overlay_parts(&count);
    hm_overlay_capture_parts(11, parts, count);
}
