#include "overlay/game_lifecycle.h"

void hm_title_overlay_capture(void);
void hm_saveload_overlay_capture(void);

void hm_game_overlays_init(void) {
    hm_title_overlay_capture();
    hm_saveload_overlay_capture();
}
