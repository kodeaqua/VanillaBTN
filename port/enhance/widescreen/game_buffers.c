#include "main.h"
#include <libgs.h>
#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#include "gfx/game_frame.h"
enum { HM_WIDE_ARENA = 262144 };
static _Alignas(16) u8 wide_work[2][HM_WIDE_ARENA];
static u8 *original_work[2];
static u32 original_work_size[2];
#endif
void hm_game_set_work_base(void *base) {
#ifdef HMBTN_ENH_WIDESCREEN
    if (!current_frame || base != current_frame->workBase) {
        GsSetWorkBase(base);
        return;
    }
    hm_widescreen_scene(hm_game_render_frame().world_view && current_frame->drawEnv.clip.w == 320 &&
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
        base = current_frame->workBase;
    }
#endif
    GsSetWorkBase(base);
}
