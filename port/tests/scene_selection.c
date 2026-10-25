#include "event_flags.h"
#include "farm_state.h"
#include "scene.h"
#include <stdio.h>
_Static_assert(sizeof(FarmModeScript) == 0xBDC, "scene file layout");
_Static_assert(offsetof(FarmModeScript, models) == 0x948, "scene model file offset");
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    CHECK(!farm_area.mode.data && scene_resolve_selection(100) == 100);
    game_state.progress.estate.houseLevel = 1;
    game_state.progress.clock.date.season = 2;
    CHECK(scene_resolve_selection(HOUSE_SCENE_BASE) == HOUSE_SCENE_BASE + 6);
    farm_area.mode.data = &farm_area.mode.script;
    farm_area.mode.script.head.place.value = PLACE_MINE_43;
    farm_area.mode.script.head.place.location = PLACE_44;
    CHECK(scene_resolve_selection(100) == MINE_43_SCENE_BASE + 2);
    event_flag_set(EVENT_FLAG_MINE_NEXT_LAYER, 1);
    CHECK(scene_resolve_selection(100) == 100);
    event_flag_set(EVENT_FLAG_MINE_RETURN_43, 1);
    CHECK(scene_resolve_selection(100) == MINE_43_SCENE_BASE + 2);
    for (u32 i = 0; i < SCENE_MODEL_COUNT; ++i)
        farm_area.mode.script.models[i].flags = SCENE_ENTRY_ABSENT;
    scene_entries_clear();
    scene_morphs_clear();
    scene_populate_objects();
    return 0;
}
