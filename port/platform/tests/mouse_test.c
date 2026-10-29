#include "platform/mouse_input.h"
#include <stdio.h>

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "Check failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    int vx = 0, vy = 0;

    /* 1. Test 4:3 window coordinates (640x480) */
    CHECK(hm_mouse_calc_virtual_pos(0.0f, 0.0f, 640, 480, &vx, &vy) == 1);
    CHECK(vx == 0 && vy == 0);

    CHECK(hm_mouse_calc_virtual_pos(320.0f, 240.0f, 640, 480, &vx, &vy) == 1);
    CHECK(vx == 160 && vy == 120);

    CHECK(hm_mouse_calc_virtual_pos(639.0f, 479.0f, 640, 480, &vx, &vy) == 1);
    CHECK(vx == 319 && vy == 239);

    /* 2. Test 16:9 widescreen window (1280x720): 960x720 centered, 160px bars left & right */
    CHECK(hm_mouse_calc_virtual_pos(160.0f, 0.0f, 1280, 720, &vx, &vy) == 1);
    CHECK(vx == 0 && vy == 0);

    CHECK(hm_mouse_calc_virtual_pos(640.0f, 360.0f, 1280, 720, &vx, &vy) == 1);
    CHECK(vx == 160 && vy == 120);

    /* Outside in left pillarbox */
    CHECK(hm_mouse_calc_virtual_pos(50.0f, 360.0f, 1280, 720, &vx, &vy) == 0);

    /* 3. Rect hit testing */
    CHECK(hm_mouse_in_rect(10, 10, 5, 5, 20, 20) == 1);
    CHECK(hm_mouse_in_rect(4, 10, 5, 5, 20, 20) == 0);
    CHECK(hm_mouse_in_rect(10, 25, 5, 5, 20, 20) == 0);

    /* 4. Mouse click & hold states */
    hm_mouse_reset();
    CHECK(hm_mouse_consume_left_click() == 0);
    CHECK(hm_mouse_is_left_held() == 0);

    hm_mouse_button_down(HM_MOUSE_BTN_LEFT);
    CHECK(hm_mouse_is_left_held() == 1);
    CHECK(hm_mouse_consume_left_click() == 1);
    CHECK(hm_mouse_consume_left_click() == 0);
    CHECK(hm_mouse_is_left_held() == 1);

    hm_mouse_button_up(HM_MOUSE_BTN_LEFT);
    CHECK(hm_mouse_is_left_held() == 0);

    /* 5. Mouse wheel tracking */
    hm_mouse_wheel(1);
    hm_mouse_wheel(1);
    CHECK(hm_mouse_consume_wheel() == 2);
    CHECK(hm_mouse_consume_wheel() == 0);

    hm_mouse_wheel(-1);
    CHECK(hm_mouse_consume_wheel() == -1);

    printf("Mouse input tests passed.\n");
    return 0;
}
