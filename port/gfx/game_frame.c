#ifdef HMBTN_ENH_WIDESCREEN
#include "enhance/widescreen/widescreen.h"
#endif
#include "farm_main.h"
#include "farm_state.h"
#include "gfx/game_frame.h"
#include "enhance/pgxp/pgxp.h"
#include "gfx/packet_identity.h"
#include "loading_screen.h"
#include "main.h"
#include "psx/timing.h"
#include <libgpu.h>

extern s32 main_scene, main_put_env_enabled;
extern void (*scene_frame_handler)(void);
hm_render_frame hm_game_render_frame(void) {
    hm_render_frame frame = {0};
    if (!current_frame)
        return frame;
    frame.tick = hm_timing_ticks();
    frame.bypass = loading_screen_is_active();
    frame.retain = !main_put_env_enabled;
    frame.key = ((u64)(u16)main_scene << 48) | ((u64)(u16)farm_area.mode.area << 32) |
                ((u64)(u16)farm_area.mode.place.value << 16) | (u16)farm_area.mode.place.location;
    frame.world_view = main_put_env_enabled && scene_frame_handler == farm_enter_dispatch &&
                       !loading_screen_is_active() && farm_area.mode.data &&
                       farm_area.mode.view == FARM_VIEW_NORMAL;
    frame.eligible = main_put_env_enabled && scene_frame_handler == farm_enter_dispatch &&
                     !loading_screen_is_active() && farm_area.mode.data &&
                     farm_area.mode.view == FARM_VIEW_NORMAL &&
                     !(game_state.globalFlags &
                       (GLOBAL_FLAG_TRANSITION | GLOBAL_FLAG_TRANSITION_BEGIN |
                        GLOBAL_FLAG_TRANSITION_END | GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN));
    frame.arena[0] = current_frame->primStart;
    frame.arena_bytes[0] = hm_game_prim_capacity();
    frame.arena[1] = current_frame->workBase;
    frame.arena_bytes[1] = current_frame->workSize;
    return frame;
}
/* Port CMake remaps only main.c's SDK call sites to these adapters. */
DRAWENV *hm_game_put_draw_env(DRAWENV *environment) {
    DRAWENV *result = PutDrawEnv(environment);
    hm_render_frame frame = hm_game_render_frame();
#ifdef HMBTN_ENH_WIDESCREEN
    hm_widescreen_scene(environment->clip.w == 320 && environment->clip.h == 240);
#endif
    hm_render_begin(&frame);
    return result;
}
void hm_game_draw_ot(u32 *table) {
    /* Frame blends intentionally skip PutDrawEnv. Observe their actual OT too;
     * otherwise presentation falls back to the 4:3 native scanout mid-fade. */
    if (!main_put_env_enabled && active_ot && table == active_ot + 1) {
        hm_render_frame frame = hm_game_render_frame();
        hm_render_begin(&frame);
    }
    hm_render_group(current_frame && table == current_frame->ot + FRAME_OT_DEPTH - 1);
    DrawOTag(table);
    if (active_ot && table == active_ot + 1)
        hm_render_end();
}
u32 *hm_game_clear_ot(u32 *table, s32 count) {
    if (current_frame && table == current_frame->ot) {
        hm_packet_identity_reset();
        hm_pgxp_frame_begin();
    }
    return ClearOTagR(table, count);
}
