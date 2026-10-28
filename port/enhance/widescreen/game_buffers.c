#include "main.h"
#include <libgs.h>
#include <stdio.h>
#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#include "gfx/game_frame.h"
enum { HM_WIDE_ARENA = 262144, HM_WIDE_PRIM = 1048576 };
static _Alignas(16) u8 wide_work[2][HM_WIDE_ARENA];
static u8 *original_work[2];
static u32 original_work_size[2];
/* Wide world scenes submit more model/field packets than the original 96000-byte primitive
 * arena holds, which dropped late objects such as the shipping bin at 21:9 and wider. Only
 * primStart moves; primBuffer/primSize keep describing the original region, which the
 * overlay backup copies use. */
static _Alignas(16) u8 wide_prim[2][HM_WIDE_PRIM];
static u8 *original_prim[2];
static int prim_exhausted_reported;
#endif
void hm_game_set_work_base(void *base) {
#ifdef HMBTN_ENH_WIDESCREEN
    if (!current_frame || base != current_frame->workBase) {
        GsSetWorkBase(base);
        return;
    }
    hm_widescreen_scene(current_frame->drawEnv.clip.w == 320 &&
                        current_frame->drawEnv.clip.h == 240);
    if (current_frame == &frame_records[0] || current_frame == &frame_records[1]) {
        int i = current_frame == &frame_records[1];
        if (current_frame->workBase != wide_work[i]) {
            original_work[i] = current_frame->workBase;
            original_work_size[i] = current_frame->workSize;
        }
        if (hm_widescreen_margin()) {
            current_frame->workBase = wide_work[i];
            current_frame->workSize = HM_WIDE_ARENA;
        } else {
            current_frame->workBase = original_work[i];
            current_frame->workSize = original_work_size[i];
        }
        if (current_frame->primStart != wide_prim[i])
            original_prim[i] = current_frame->primStart;
        current_frame->primStart = hm_widescreen_margin() ? wide_prim[i] : original_prim[i];
        base = current_frame->workBase;
    }
#endif
    GsSetWorkBase(base);
}
/* Bytes available from the current frame's primStart. */
size_t hm_game_prim_capacity(void) {
    if (!current_frame)
        return 0;
#ifdef HMBTN_ENH_WIDESCREEN
    if (current_frame->primStart == wide_prim[0] || current_frame->primStart == wide_prim[1])
        return HM_WIDE_PRIM;
#endif
    return current_frame->primSize > 0 ? (size_t)current_frame->primSize : 0;
}
#ifdef HMBTN_ENH_WIDESCREEN
/* True when model submission must stop to keep an 8 KiB reserve for UI packets. */
int hm_widescreen_prim_full(const void *cursor) {
    const u8 *limit = current_frame->primStart + hm_game_prim_capacity() - 52 - 8192;
    int full = (const u8 *)cursor > limit;
    if (full && !prim_exhausted_reported) {
        prim_exhausted_reported = 1;
        fprintf(stderr, "Widescreen: primitive arena exhausted; later model parts are omitted\n");
    }
    return full;
}
#endif
