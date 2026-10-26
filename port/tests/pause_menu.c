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
    CHECK(menu.page == HM_PAUSE_SETTINGS && hm_pause_count(&menu) == 1);
    hm_pause_handle(&menu, HM_PAUSE_DOWN);
    CHECK(menu.selected == 0);
    hm_pause_handle(&menu, HM_PAUSE_CONFIRM);
    CHECK(menu.page == HM_PAUSE_GRAPHICS && hm_pause_count(&menu) == 7);
    CHECK(!strcmp(hm_pause_label(&menu, 4), "Widescreen Mode"));
    for (int i = 0; i < 7; ++i) {
        CHECK(menu.selected == i);
        CHECK(hm_pause_handle(&menu, HM_PAUSE_RIGHT) == HM_PAUSE_CHANGE_GRAPHICS);
        hm_pause_handle(&menu, HM_PAUSE_DOWN);
    }
    CHECK(menu.selected == 0);
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_SETTINGS);
    hm_pause_handle(&menu, HM_PAUSE_BACK);
    CHECK(menu.page == HM_PAUSE_ROOT && menu.selected == 1);
    CHECK(hm_pause_handle(&menu, HM_PAUSE_BACK) == HM_PAUSE_RESUME);
    hm_pause_open(&menu);
    CHECK(menu.selected == 0 && menu.page == HM_PAUSE_ROOT);
    return 0;
}
