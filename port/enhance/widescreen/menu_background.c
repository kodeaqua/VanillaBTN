#include "anim.h"
#include <libgs.h>
#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#include "gfx/packet_identity.h"
#endif

/* Only bg_tile.c calls these adapters. Repeat four-column (96-pixel) periods
 * outside its original fifteen-column grid, preserving frame order and scroll.
 * Extra packets are tagged as backgrounds; all menu text remains native-sized. */
#ifdef HMBTN_ENH_WIDESCREEN
static int hm_menu_edge(s16 x) {
    int scroll = ((-12 - x) % 24 + 24) % 24;
    int column = (x + 12 + scroll) / 24;
    return column < 4 ? -1 : column >= 11 ? 1 : 0;
}
#endif
void hm_menu_draw_sprt_cursor(u_long *ot, AnimState *sprite, u8 animation, u8 frame, s16 x, s16 y) {
    void *begin = GsGetWorkBase();
    anim_draw_sprt_cursor(ot, sprite, animation, frame, x, y);
#ifdef HMBTN_ENH_WIDESCREEN
    int direction = hm_menu_edge(x), margin = hm_widescreen_margin();
    if (direction && margin)
        for (int offset = 96; offset <= margin + 96; offset += 96)
            anim_draw_sprt_cursor(ot, sprite, animation, frame, x + direction * offset, y);
    hm_packet_background_span(begin, GsGetWorkBase());
#else
    (void)begin;
#endif
}
void hm_menu_draw_sprt(u_long *ot, SPRT **sprites, DR_MODE **modes, AnimState *sprite, u8 animation,
                       u8 frame, s16 x, s16 y) {
    void *begin = *sprites;
    void *mode_begin = *modes;
    anim_draw_sprt(ot, sprites, modes, sprite, animation, frame, x, y);
#ifdef HMBTN_ENH_WIDESCREEN
    int direction = hm_menu_edge(x), margin = hm_widescreen_margin();
    if (direction && margin)
        for (int offset = 96; offset <= margin + 96; offset += 96)
            anim_draw_sprt(ot, sprites, modes, sprite, animation, frame, x + direction * offset, y);
    hm_packet_background_span(begin, *sprites);
    hm_packet_background_span(mode_begin, *modes);
#else
    (void)begin;
    (void)mode_begin;
#endif
}
