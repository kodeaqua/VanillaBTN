#include "farm_state.h"
#include "overlay/game_lifecycle.h"
#include "overlay/registry.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    hm_game_overlays_init();
    /* Resident game state must survive restoring either overlay. */
    game_state.player.stamina = 73;
    for (int visit = 0; visit < 6; ++visit) {
        CHECK(hm_overlay_select(27, 0) == 0);
        CHECK(hm_overlay_active(0) == 27);
        CHECK(hm_overlay_select(15, 1) == 0);
        CHECK(hm_overlay_active(0) == 27 && hm_overlay_active(1) == 15);
        CHECK(game_state.player.stamina == 73);
    }
    return 0;
}
