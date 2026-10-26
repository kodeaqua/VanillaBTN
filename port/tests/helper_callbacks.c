#include "cue.h"
#include "farm_state.h"
#include "harvest_sprites.h"
#include "scene.h"
#include <stdio.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    game_state.globalFlags = GLOBAL_FLAG_CLOCK_TICKED;
    game_state.progress.clock.time.hm.hour = 6;
    farm_area.mode.place.location = PLACE_FARM;
    for (u32 i = 0; i < HELPER_COUNT; ++i) {
        FarmHelper *helper = &game_state.helpers[i];
        helper->number = (u16)i;
        helper->actor.place.location = PLACE_28;
        helper->actor.speed.vx = 100 + (s32)i;
        helper->actor.affection = (s16)(150 + i);
        helper->harvestSkill = (u16)(10 + i);
    }
    harvest_helpers_update();
    harvest_helpers_update();
    CHECK(game_state.globalFlags == GLOBAL_FLAG_CLOCK_TICKED);
    for (u32 i = 0; i < HELPER_COUNT; ++i) {
        const FarmHelper *helper = &game_state.helpers[i];
        CHECK(helper->number == i && helper->harvestSkill == 10 + i);
        CHECK(helper->actor.affection == 150 + (s32)i);
        CHECK(helper->actor.taskId == CUE_STEP_PLAY && helper->actor.taskTime == 1);
        CHECK(helper->actor.action == 0 && (helper->actor.flags & ACTOR_FLAG_CUE_RUNNING));
        CHECK(helper->actor.position.vx == 100 + (s32)i && !helper->actor.speed.vx);
    }
    return 0;
}
