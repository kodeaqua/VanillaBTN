#include "enhance/pause_menu/menu.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    hm_pause_menu menu;
    hm_pause_open(&menu);
    CHECK(menu.page == HM_PAUSE_ROOT && hm_pause_count(&menu) == 3);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Resume"));
    CHECK(hm_pause_label(&menu, -1) == NULL && hm_pause_label(&menu, 3) == NULL);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_RESUME);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_UP) == HM_PAUSE_NONE && menu.selected == 2);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_EXIT);
    hm_pause_handle(&menu, HM_PAUSE_UP);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_NONE);
    CHECK(menu.page == HM_PAUSE_SETTINGS && hm_pause_count(&menu) == 3);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Graphics"));
    CHECK(!strcmp(hm_pause_label(&menu, 1), "Gameplay"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "Controls"));
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 1);
    hm_pause_handle(&menu, HM_PAUSE_UP);
    CHECK(menu.selected == 0);
    hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
    CHECK(menu.page == HM_PAUSE_GRAPHICS && hm_pause_count(&menu) == 11);
    CHECK(!strcmp(hm_pause_label(&menu, 10), "Renderer"));
    CHECK(!strcmp(hm_pause_label(&menu, 4), "Widescreen Mode"));
    CHECK(!strcmp(hm_pause_label(&menu, 7), "VSync") &&
          !strcmp(hm_pause_label(&menu, 8), "FPS Limit"));
    for (int i = 0; i < HM_PAUSE_GRAPHICS_ITEMS; ++i) {
        CHECK(menu.selected == i);
        CHECK(menu.selected >= menu.first_visible &&
              menu.selected < menu.first_visible + HM_PAUSE_VISIBLE_ROWS);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_RIGHT) == HM_PAUSE_CHANGE_GRAPHICS);
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    CHECK(menu.selected == 0 && menu.first_visible == 0);
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_SETTINGS && menu.selected == 0);
    /* Test gameplay settings menu */
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 1);
    hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
    CHECK(menu.page == HM_PAUSE_GAMEPLAY && hm_pause_count(&menu) == HM_PAUSE_GAMEPLAY_ITEMS);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Walk Crops"));
    CHECK(!strcmp(hm_pause_label(&menu, 1), "Item Stacking"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "Durable Fences"));
    CHECK(!strcmp(hm_pause_label(&menu, 3), "Connect Fences"));
    CHECK(!strcmp(hm_pause_label(&menu, 4), "Wild Dog Attacks"));
    CHECK(!strcmp(hm_pause_label(&menu, 5), "Smart Bell"));
    CHECK(!strcmp(hm_pause_label(&menu, 6), "Stamina HUD"));
    CHECK(!strcmp(hm_pause_label(&menu, 7), "Calendar Events"));
    CHECK(!strcmp(hm_pause_label(&menu, 8), "Roaming Horse"));
    for (int i = 0; i < HM_PAUSE_GAMEPLAY_ITEMS; ++i) {
        CHECK(menu.selected == i);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_RIGHT) == HM_PAUSE_CHANGE_GAMEPLAY);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_LEFT) == HM_PAUSE_CHANGE_GAMEPLAY);
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_SETTINGS && menu.selected == 1);

    /* Test controls settings menu */
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 2);
    hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
    CHECK(menu.page == HM_PAUSE_CONTROLS && hm_pause_count(&menu) == HM_PAUSE_CONTROLS_ITEMS);
    CHECK(!strcmp(hm_pause_label(&menu, 0), "Gamepad"));
    CHECK(!strcmp(hm_pause_label(&menu, 1), "Keyboard"));
    CHECK(!strcmp(hm_pause_label(&menu, 2), "Mouse"));
    for (int i = 0; i < HM_PAUSE_CONTROLS_ITEMS; ++i) {
        CHECK(menu.selected == i);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_RIGHT) == HM_PAUSE_CHANGE_CONTROLS);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_LEFT) == HM_PAUSE_CHANGE_CONTROLS);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_CONFIRM) == HM_PAUSE_CHANGE_CONTROLS);
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_SETTINGS && menu.selected == 2);

    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_ROOT && menu.selected == 1);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_BACK) == HM_PAUSE_RESUME);
    hm_pause_open(&menu);
    CHECK(menu.selected == 0 && menu.page == HM_PAUSE_ROOT);
    return 0;
}
