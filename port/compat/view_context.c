#include "main.h"
#include <libgte.h>
extern s32 main_scene;
/* main.c alone remaps this SDK call. Its zero-offset frame reset is also used
 * on return from save/load menus, where farm re-entry skips camera startup. */
void hm_game_set_geom_offset(s32 x, s32 y) {
    if (!x && !y && (main_scene == SCENE_FARM_SAVED || main_scene == SCENE_FARM_MENU_RETURN)) {
        x = 160;
        y = 120;
    }
    SetGeomOffset(x, y);
}
