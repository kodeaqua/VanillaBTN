#include "anim.h"
#include "estate.h"
#include "gfx/packet_identity.h"
#include <libgs.h>
static u64 hm_field_entity;
static int hm_field_projected;
/* Native-only CMake adapters: no source/game-state changes. Field sprites share
 * animation records, so their world cell identifies them instead of allocation
 * order or the shared AnimState address. */
s32 hm_field_point_project(SVECTOR *input, FarmScreenPoint *result) {
    hm_field_entity = ((u64)(u16)input->vx << 32) | ((u64)(u16)input->vy << 16) | (u16)input->vz;
    s32 visible = estate_point_project(input, result);
    hm_field_projected = visible != 0;
    return visible;
}
void hm_field_draw_sprt_cursor(u_long *ot, AnimState *sprite, u8 anim, u8 frame, s16 x, s16 y) {
    void *begin = GsGetWorkBase();
    anim_draw_sprt_cursor(ot, sprite, anim, frame, x, y);
    if (hm_field_projected)
        hm_packet_identity_span(begin, GsGetWorkBase(), hm_field_entity);
    hm_field_projected = 0;
}
