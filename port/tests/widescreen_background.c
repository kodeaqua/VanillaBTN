#include "anim.h"
#include "enhance/widescreen/widescreen.h"
#include "gfx/packet_identity.h"
#include <libgs.h>
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

void hm_menu_draw_sprt_cursor(u_long *, AnimState *, u8, u8, s16, s16);
void hm_menu_draw_sprt(u_long *, SPRT **, DR_MODE **, AnimState *, u8, u8, s16, s16);
void hm_title_draw_sprt_cursor(u_long *, AnimState *, u8, u8, s16, s16);
static u32 work[65536], *cursor;
static SPRT sprites[4096];
static DR_MODE modes[4096];
static int calls, left, right;
static void record(s16 x) {
    ++calls;
    if (x < left)
        left = x;
    if (x > right)
        right = x;
}
void *GsGetWorkBase(void) { return cursor; }
void anim_draw_sprt_cursor(u_long *ot, AnimState *sprite, u8 animation, u8 frame, s16 x, s16 y) {
    (void)ot;
    (void)sprite;
    (void)animation;
    (void)frame;
    (void)y;
    record(x);
    cursor += 8;
}
void anim_draw_sprt(u_long *ot, SPRT **s, DR_MODE **m, AnimState *sprite, u8 animation, u8 frame,
                    s16 x, s16 y) {
    (void)ot;
    (void)sprite;
    (void)animation;
    (void)frame;
    (void)y;
    record(x);
    ++*s;
    ++*m;
}
static void reset(void) {
    cursor = work;
    calls = 0;
    left = 32767;
    right = -32768;
    hm_packet_identity_reset();
}
int main(void) {
    AnimState sprite = {0};
    hm_widescreen_scene(1);
    for (int enabled = 0; enabled <= 2; ++enabled) {
        hm_widescreen_configure(enabled != 0, enabled == 2 ? 1024 : 1920,
                                enabled == 2 ? 240 : 1080);
        int margin = hm_widescreen_margin();
        for (int scroll = 0; scroll < 24; ++scroll) {
            for (int explicit_cursor = 0; explicit_cursor <= 1; ++explicit_cursor) {
                reset();
                SPRT *s = sprites;
                DR_MODE *m = modes;
                for (int y = 0; y < 264; y += 24)
                    for (int x = -12; x < 344; x += 24)
                        if (explicit_cursor)
                            hm_menu_draw_sprt(NULL, &s, &m, &sprite, 0x1d, 0, x - scroll,
                                              y - scroll);
                        else
                            hm_menu_draw_sprt_cursor(NULL, &sprite, 0x1d, 0, x - scroll,
                                                     y - scroll);
                CHECK(enabled ? calls > 165 : calls == 165);
                if (enabled) {
                    CHECK(left <= -margin && right + 24 >= 320 + margin);
                    if (explicit_cursor) {
                        CHECK(hm_packet_background_get(sprites) && hm_packet_background_get(s - 1));
                        CHECK(hm_packet_background_get(modes) && hm_packet_background_get(m - 1));
                    } else {
                        CHECK(hm_packet_background_get(work) &&
                              hm_packet_background_get(cursor - 1));
                    }
                }
            }
        }
        reset();
        hm_title_draw_sprt_cursor(NULL, &sprite, 0x1d, 0, 0, 0);
        int repeats = (margin + 319) / 320;
        CHECK(calls == 1 + 2 * repeats);
        CHECK(left == -320 * repeats && right == 320 * repeats);
        reset();
        hm_title_draw_sprt_cursor(NULL, &sprite, 1, 0, 0, 0);
        CHECK(calls == 1 && !hm_packet_background_get(work));
    }
    return 0;
}
