#ifndef HMBTN_CONTROLS_OPTIONS_H
#define HMBTN_CONTROLS_OPTIONS_H

#include <stddef.h>

enum {
    HM_KEYBOARD_LAYOUT_ARROWS = 0,
    HM_KEYBOARD_LAYOUT_WASD   = 1,
    HM_KEYBOARD_LAYOUT_OFF    = 2
};

enum {
    HM_MOUSE_MODE_OFF    = 0,
    HM_MOUSE_MODE_ACTION = 1,
    HM_MOUSE_MODE_MOVE   = 2
};

typedef struct {
    int controller_enabled; /* 0 = Off, 1 = Enabled (default 1) */
    int keyboard_layout;    /* 0 = Arrows+ZX, 1 = WASD+JK, 2 = Off (default 0) */
    int mouse_mode;         /* 0 = Off, 1 = Action, 2 = Move (default 0) */
} hm_controls_options;

void hm_controls_defaults(hm_controls_options *opt);
int hm_controls_load(hm_controls_options *opt);
int hm_controls_save(const hm_controls_options *opt);
int hm_controls_change(hm_controls_options *opt, int item, int step);
void hm_controls_row_value(const hm_controls_options *opt, int item, char *out, size_t size);
void hm_controls_value(const hm_controls_options *opt, int item, char *out, size_t size);

#endif
