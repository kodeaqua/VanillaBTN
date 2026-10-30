#ifndef HMBTN_PAUSE_MENU_H
#define HMBTN_PAUSE_MENU_H

typedef enum {
    HM_PAUSE_ROOT,
    HM_PAUSE_SETTINGS,
    HM_PAUSE_GRAPHICS,
    HM_PAUSE_GAMEPLAY,
    HM_PAUSE_CONTROLS,
    HM_PAUSE_AUDIO
} hm_pause_page;

enum {
#ifdef __ANDROID__
    HM_PAUSE_GRAPHICS_ITEMS = 1,
#else
    HM_PAUSE_GRAPHICS_ITEMS = 10,
#endif
    HM_PAUSE_GAMEPLAY_ITEMS = 12,
    HM_PAUSE_CONTROLS_ITEMS = 3,
    HM_PAUSE_AUDIO_ITEMS = 3,
    HM_PAUSE_VISIBLE_ROWS = 7
};

typedef enum {
    HM_PAUSE_UP,
    HM_PAUSE_DOWN,
    HM_PAUSE_BACK,
    HM_PAUSE_CONFIRM,
    HM_PAUSE_LEFT,
    HM_PAUSE_RIGHT
} hm_pause_input;

typedef enum {
    HM_PAUSE_NONE,
    HM_PAUSE_RESUME,
    HM_PAUSE_EXIT,
    HM_PAUSE_CHANGE_GRAPHICS,
    HM_PAUSE_CHANGE_GAMEPLAY,
    HM_PAUSE_CHANGE_CONTROLS,
    HM_PAUSE_CHANGE_AUDIO
} hm_pause_action;

typedef struct {
    hm_pause_page page;
    int selected;
    int first_visible;
} hm_pause_menu;

void hm_pause_open(hm_pause_menu *menu);
int hm_pause_count(const hm_pause_menu *menu);
const char *hm_pause_label(const hm_pause_menu *menu, int index);
hm_pause_action hm_pause_handle(hm_pause_menu *menu, hm_pause_input input);

#endif
