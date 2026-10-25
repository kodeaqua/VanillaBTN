#include "picture_cells.h"
#include "psx/unimplemented.h"

/* Test-only link dependencies for farm_tables.c's item table. Registration may
 * copy their addresses, but no item action is part of this test's coverage.
 * These traps must never be linked into the native game target. */
s32 farm_clamp_status(s8 stamina, s8 fatigue, s16 happiness, s32 unused) {
    (void)stamina;
    (void)fatigue;
    (void)happiness;
    (void)unused;
    hm_unimplemented("test-only farm_clamp_status");
}

s32 item_use_coin(s8 amount, s8 reset, s32 unused, s32 *out) {
    (void)amount;
    (void)reset;
    (void)unused;
    (void)out;
    hm_unimplemented("test-only item_use_coin");
}

s32 farm_adjust_stamina_fatigue(s8 stamina, s8 fatigue) {
    (void)stamina;
    (void)fatigue;
    hm_unimplemented("test-only farm_adjust_stamina_fatigue");
}
