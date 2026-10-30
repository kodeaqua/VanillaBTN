#include "platform/mouse_input.h"
#include <stdint.h>
#include <math.h>

static float s_mx = 0.0f;
static float s_my = 0.0f;
static int s_win_w = 0;
static int s_win_h = 0;
static int s_left_clicked = 0;
static int s_right_clicked = 0;
static int s_wheel_delta = 0;
static int s_left_held = 0;
static int s_right_held = 0;

void hm_mouse_update_pos(float mx, float my, int win_w, int win_h) {
    s_mx = mx;
    s_my = my;
    s_win_w = win_w;
    s_win_h = win_h;
}

void hm_mouse_button_down(int button) {
    if (button == HM_MOUSE_BTN_LEFT) {
        s_left_clicked = 2;
        s_left_held = 1;
    } else if (button == HM_MOUSE_BTN_RIGHT) {
        s_right_clicked = 2;
        s_right_held = 1;
    }
}

void hm_mouse_button_up(int button) {
    if (button == HM_MOUSE_BTN_LEFT) {
        s_left_held = 0;
    } else if (button == HM_MOUSE_BTN_RIGHT) {
        s_right_held = 0;
    }
}

void hm_mouse_wheel(int delta) {
    s_wheel_delta += delta;
}

int hm_mouse_calc_virtual_pos(float mx, float my, int win_w, int win_h, int *vx, int *vy) {
    if (win_w <= 0 || win_h <= 0) {
        if (vx) *vx = 0;
        if (vy) *vy = 0;
        return 0;
    }
    float scale;
    float offset_x = 0.0f;
    float offset_y = 0.0f;

    /* Check aspect ratio against 4:3 (320x240) */
    if ((int64_t)win_w * 3 > (int64_t)win_h * 4) {
        /* Wider than 4:3 -> pillarbox bars left and right */
        scale = (float)win_h / 240.0f;
        offset_x = ((float)win_w - 320.0f * scale) * 0.5f;
    } else {
        /* Taller than 4:3 -> letterbox bars top and bottom */
        scale = (float)win_w / 320.0f;
        offset_y = ((float)win_h - 240.0f * scale) * 0.5f;
    }

    if (scale <= 0.0f) {
        if (vx) *vx = 0;
        if (vy) *vy = 0;
        return 0;
    }

    float virt_x = (mx - offset_x) / scale;
    float virt_y = (my - offset_y) / scale;

    int cx = (int)floorf(virt_x);
    int cy = (int)floorf(virt_y);
    if (vx) *vx = cx;
    if (vy) *vy = cy;

    return (cx >= 0 && cx < 320 && cy >= 0 && cy < 240);
}

int hm_mouse_get_pos(int *vx, int *vy) {
    return hm_mouse_calc_virtual_pos(s_mx, s_my, s_win_w, s_win_h, vx, vy);
}

int hm_mouse_in_rect(int vx, int vy, int rx, int ry, int rw, int rh) {
    return (vx >= rx && vx < rx + rw && vy >= ry && vy < ry + rh);
}

int hm_mouse_consume_left_click(void) {
    if (s_left_clicked > 0) {
        s_left_clicked = 0;
        return 1;
    }
    return 0;
}

int hm_mouse_consume_right_click(void) {
    if (s_right_clicked > 0) {
        s_right_clicked = 0;
        return 1;
    }
    return 0;
}

int hm_mouse_consume_wheel(void) {
    int w = s_wheel_delta;
    s_wheel_delta = 0;
    return w;
}

int hm_mouse_is_left_held(void) {
    return s_left_held;
}

int hm_mouse_is_right_held(void) {
    return s_right_held;
}

void hm_mouse_tick(void) {
    if (s_left_clicked > 0)
        s_left_clicked--;
    if (s_right_clicked > 0)
        s_right_clicked--;
}

void hm_mouse_reset(void) {
    s_left_clicked = 0;
    s_right_clicked = 0;
    s_wheel_delta = 0;
    s_left_held = 0;
    s_right_held = 0;
}
