/* Exercise the original summary loader with bounded asynchronous completion. */
#define afile_read_bytes hm_test_asset_poll
#include "../compat/asset_wait.c"
#undef afile_read_bytes
#define afile_read_bytes hm_blocking_asset_read
#define texture_pack_load hm_test_texture_load
#define anim_bind_file hm_test_anim_bind
#define sound_play hm_test_sound_play
#include "../../src/overlay/11_GF_FARM/menu/status_pages.c"
#undef afile_read_bytes
#undef texture_pack_load
#undef anim_bind_file
#undef sound_play
#include "psx/timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int polls, reads, textures, bindings, bad, ticks;
static u64 began;
static FrameState test_frame;
static u_long test_ot[FRAME_OT_DEPTH];
s32 hm_test_sound_play(s16 id) {
    if (id != SOUND_MENU_OPEN_CLOSE)
        bad = 1;
    return 0;
}
u8 *hm_test_asset_poll(s32 file, s32 offset, s32 bytes, u8 *destination, s32 align, s32 restart) {
    if (file != STATUS_PACK_FILE || offset || bytes != STATUS_PACK_BYTES ||
        destination != STAGING_BUFFER || align != 1)
        bad = 1;
    if (restart) {
        ++reads;
        polls = 0;
        began = hm_timing_ticks();
        return NULL;
    }
    if (++polls > 8) {
        fprintf(stderr, "Status loader did not yield hardware progress\n");
        exit(1);
    }
    if (hm_timing_ticks() - began < 2)
        return NULL;
    memset(destination, 0x5a, (size_t)bytes);
    return destination;
}
void hm_test_texture_load(u8 *pack) {
    if (pack != STAGING_BUFFER + sizeof(FarmPack))
        bad = 1;
    ++textures;
}
void hm_test_anim_bind(AnimState *state, AnimFile *file) {
    int index = bindings++;
    if (index >= 5 || state != hm_status_sprite(index) ||
        file != (AnimFile *)(status_pack.bytes + status_sprite_offsets[index]))
        bad = 1;
}
static void host_tick(void *context, u64 tick) {
    (void)context;
    (void)tick;
    ++ticks;
}
int main(void) {
    hm_timing_reset((hm_timing_host){.wait_tick = host_tick});
    current_frame = &test_frame;
    test_frame.ot = test_ot;
    region_base = hm_work_region();
    game_state.family.wifeIndex = -1;
    game_state.progress.estate.farmName[0] = TEXT_END;
    text_bank.messages.farm[0] = TEXT_END;
    if (hm_status_sprite(3) != &status_cursor_sprite || hm_status_sprite(4) != &status_mark_sprite)
        return 1;
    status_load_sprites();
    if (bad || reads != 1 || polls != 3 || textures != 1 || bindings != 5 || ticks != 2 ||
        hm_timing_ticks() != 2)
        return 1;
    for (size_t i = 0; i < sizeof(status_pack); ++i)
        if (status_pack.bytes[i] != 0x5a)
            return 1;
    return 0;
}
