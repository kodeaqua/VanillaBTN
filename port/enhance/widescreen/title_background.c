#include "anim.h"
#include <libgs.h>
#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#include "gfx/packet_identity.h"
#endif
/* CMake remaps only the title overlay's construction calls. */
void hm_title_draw_sprt_cursor(u_long *ot, AnimState *sprite, u8 animation, u8 frame, s16 x,
                               s16 y) {
    void *begin = GsGetWorkBase();
    anim_draw_sprt_cursor(ot, sprite, animation, frame, x, y);
#ifdef HMBTN_ENH_WIDESCREEN
    int margin = hm_widescreen_margin();
    if (animation == 0x1d && margin) {
        for (int offset = 320; offset < 320 + margin; offset += 320) {
            anim_draw_sprt_cursor(ot, sprite, animation, frame, x - offset, y);
            anim_draw_sprt_cursor(ot, sprite, animation, frame, x + offset, y);
        }
    }
    if (animation == 0x25 || animation == 0x1d)
        hm_packet_background_span(begin, GsGetWorkBase());
#else
    (void)begin;
#endif
}
