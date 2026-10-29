#ifndef HMBTN_PLATFORM_MOUSE_INPUT_H
#define HMBTN_PLATFORM_MOUSE_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HM_MOUSE_BTN_LEFT 1
#define HM_MOUSE_BTN_RIGHT 2
#define HM_MOUSE_BTN_MIDDLE 3

/* Update current window mouse state from platform layer */
void hm_mouse_update_pos(float mx, float my, int win_w, int win_h);
void hm_mouse_button_down(int button);
void hm_mouse_button_up(int button);
void hm_mouse_wheel(int delta);

/* Calculate virtual 320x240 coordinates from screen coordinates and window size.
 * Returns 1 if within 320x240 canvas, 0 if in border/letterbox. */
int hm_mouse_calc_virtual_pos(float mx, float my, int win_w, int win_h, int *vx, int *vy);

/* Query current virtual coordinates. Returns 1 if within 320x240 canvas, 0 if outside. */
int hm_mouse_get_pos(int *vx, int *vy);

/* Check if (vx, vy) is within a rectangle [rx, ry, rw, rh] */
int hm_mouse_in_rect(int vx, int vy, int rx, int ry, int rw, int rh);

/* Query and consume click actions (returns 1 and resets if clicked) */
int hm_mouse_consume_left_click(void);
int hm_mouse_consume_right_click(void);

/* Query and consume wheel delta (returns > 0 for up, < 0 for down, 0 for none, resets to 0) */
int hm_mouse_consume_wheel(void);

/* Continuous button state */
int hm_mouse_is_left_held(void);
int hm_mouse_is_right_held(void);

/* Reset all mouse click/held states */
void hm_mouse_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HMBTN_PLATFORM_MOUSE_INPUT_H */
