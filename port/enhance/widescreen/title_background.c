#include "anim.h"
#include <libgs.h>
#ifdef HMBTN_ENH_WIDESCREEN
#include "gfx/packet_identity.h"
#endif
/* CMake remaps only the title overlay's construction calls. */
void hm_title_draw_sprt_cursor(u_long *ot, AnimState *sprite, u8 animation, u8 frame, s16 x,
                               s16 y) {
    void *begin = GsGetWorkBase();
    anim_draw_sprt_cursor(ot, sprite, animation, frame, x, y);
#ifdef HMBTN_ENH_WIDESCREEN
    if (animation == 0x25)
        hm_packet_background_span(begin, GsGetWorkBase());
#else
    (void)begin;
#endif
}
