#include "enhance/pause_menu/menu.h"
#include <stddef.h>

static const char *const hm_root_items[] = {"Resume", "Settings", "Exit Game"};
static const char *const hm_settings_items[] = {"Graphics", "Gameplay", "Controls", "Audio"};
#ifdef __ANDROID__
static const char *const hm_graphics_items[] = {
    "Enhance Scaling", "Widescreen Mode", "Anti-Aliasing",
    "Filtering",       "VSync",           "FPS Limit",     "Interpolation"};
#else
static const char *const hm_graphics_items[] = {
    "Monitor",   "Window Mode", "Resolution", "Enhance Scaling", "Widescreen Mode", "Anti-Aliasing",
    "Filtering", "VSync",       "FPS Limit",  "Interpolation"};
#endif
static const char *const hm_gameplay_items[] = {
    "Walk Crops", "Item Stacking", "Durable Fences", "Connect Fences", "Wild Dog Attacks",
    "Smart Bell", "Stamina HUD",   "Calendar Events","Roaming Horse",  "Fast Text",
    "Tool Charge Bar", "Tile Targeting"};
static const char *const hm_controls_items[] = {"Gamepad", "Keyboard", "Mouse"};
static const char *const hm_audio_items[] = {"BGM Volume", "SFX Volume", "Resampling"};

void hm_pause_open(hm_pause_menu *menu) {
    menu->page = HM_PAUSE_ROOT;
    menu->selected = 0;
    menu->first_visible = 0;
}

int hm_pause_count(const hm_pause_menu *menu) {
    return menu->page == HM_PAUSE_ROOT       ? 3
           : menu->page == HM_PAUSE_SETTINGS ? 4
           : menu->page == HM_PAUSE_GAMEPLAY ? HM_PAUSE_GAMEPLAY_ITEMS
           : menu->page == HM_PAUSE_CONTROLS ? HM_PAUSE_CONTROLS_ITEMS
           : menu->page == HM_PAUSE_AUDIO    ? HM_PAUSE_AUDIO_ITEMS
                                             : HM_PAUSE_GRAPHICS_ITEMS;
}

const char *hm_pause_label(const hm_pause_menu *menu, int index) {
    if (index < 0 || index >= hm_pause_count(menu))
        return NULL;
    return menu->page == HM_PAUSE_ROOT       ? hm_root_items[index]
           : menu->page == HM_PAUSE_SETTINGS ? hm_settings_items[index]
           : menu->page == HM_PAUSE_GAMEPLAY ? hm_gameplay_items[index]
           : menu->page == HM_PAUSE_CONTROLS ? hm_controls_items[index]
           : menu->page == HM_PAUSE_AUDIO    ? hm_audio_items[index]
                                             : hm_graphics_items[index];
}

hm_pause_action hm_pause_handle(hm_pause_menu *menu, hm_pause_input input) {
    if (input == HM_PAUSE_UP || input == HM_PAUSE_DOWN) {
        int count = hm_pause_count(menu);
        menu->selected = (menu->selected + (input == HM_PAUSE_UP ? count - 1 : 1)) % count;
        if (menu->selected < menu->first_visible)
            menu->first_visible = menu->selected;
        if (menu->selected >= menu->first_visible + HM_PAUSE_VISIBLE_ROWS)
            menu->first_visible = menu->selected - HM_PAUSE_VISIBLE_ROWS + 1;
    } else if (input == HM_PAUSE_BACK) {
        if (menu->page == HM_PAUSE_ROOT)
            return HM_PAUSE_RESUME;
        if (menu->page == HM_PAUSE_GRAPHICS || menu->page == HM_PAUSE_GAMEPLAY ||
            menu->page == HM_PAUSE_CONTROLS || menu->page == HM_PAUSE_AUDIO) {
            int prev = (menu->page == HM_PAUSE_AUDIO) ? 3 :
                       ((menu->page == HM_PAUSE_CONTROLS) ? 2 :
                       ((menu->page == HM_PAUSE_GAMEPLAY) ? 1 : 0));
            menu->page = HM_PAUSE_SETTINGS;
            menu->selected = prev;
        } else {
            menu->page = HM_PAUSE_ROOT;
            menu->selected = 1;
        }
        menu->first_visible = 0;
    } else if (input == HM_PAUSE_CONFIRM) {
        if (menu->page == HM_PAUSE_ROOT) {
            if (menu->selected == 0)
                return HM_PAUSE_RESUME;
            if (menu->selected == 2)
                return HM_PAUSE_EXIT;
            menu->page = HM_PAUSE_SETTINGS;
        } else if (menu->page == HM_PAUSE_SETTINGS) {
            menu->page = (menu->selected == 3) ? HM_PAUSE_AUDIO :
                         ((menu->selected == 2) ? HM_PAUSE_CONTROLS :
                         ((menu->selected == 1) ? HM_PAUSE_GAMEPLAY : HM_PAUSE_GRAPHICS));
        } else if (menu->page == HM_PAUSE_GAMEPLAY) {
            return HM_PAUSE_CHANGE_GAMEPLAY;
        } else if (menu->page == HM_PAUSE_CONTROLS) {
            return HM_PAUSE_CHANGE_CONTROLS;
        } else if (menu->page == HM_PAUSE_AUDIO) {
            return HM_PAUSE_CHANGE_AUDIO;
        } else {
            return HM_PAUSE_CHANGE_GRAPHICS;
        }
        menu->selected = 0;
        menu->first_visible = 0;
    } else if (menu->page == HM_PAUSE_GRAPHICS &&
               (input == HM_PAUSE_LEFT || input == HM_PAUSE_RIGHT)) {
        return HM_PAUSE_CHANGE_GRAPHICS;
    } else if (menu->page == HM_PAUSE_GAMEPLAY &&
               (input == HM_PAUSE_LEFT || input == HM_PAUSE_RIGHT)) {
        return HM_PAUSE_CHANGE_GAMEPLAY;
    } else if (menu->page == HM_PAUSE_CONTROLS &&
               (input == HM_PAUSE_LEFT || input == HM_PAUSE_RIGHT)) {
        return HM_PAUSE_CHANGE_CONTROLS;
    } else if (menu->page == HM_PAUSE_AUDIO &&
               (input == HM_PAUSE_LEFT || input == HM_PAUSE_RIGHT)) {
        return HM_PAUSE_CHANGE_AUDIO;
    }
    return HM_PAUSE_NONE;
}
