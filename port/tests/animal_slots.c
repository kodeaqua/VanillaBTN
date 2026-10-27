#include "animal_actions.h"
#include "chicken.h"
#include "farm_state.h"
#include "screen_fade.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    memset(&game_state.animals, 0, sizeof game_state.animals);
    rand_seed = 123;
    CHECK(farm_action_pick_slot(0) == -1 && rand_seed == 123);
    CHECK(farm_action_pick_slot(1) == -1 && rand_seed == 123);
    FarmActor *cow = &game_state.animals.livestock[19].actor;
    cow->kind = ITEM_COW;
    cow->place.location = PLACE_FARM;
    CHECK(farm_action_pick_slot(0) == 19);
    cow->flags = ACTOR_FLAG_HELD;
    CHECK(farm_action_pick_slot(0) == -1);
    cow->flags = ACTOR_FLAG_IN_EVENT;
    CHECK(farm_action_pick_slot(0) == -1);
    cow->flags = 0;
    cow->place.location = PLACE_28;
    CHECK(farm_action_pick_slot(0) == -1);
    FarmActor *chicken = &game_state.animals.chickens[9].actor;
    chicken->kind = ITEM_CHICKEN;
    chicken->place.location = PLACE_FARM;
    CHECK(farm_action_pick_slot(1) == 9 && farm_action_pick_slot(7) == 9);
    chicken->flags = ACTOR_FLAG_HELD;
    CHECK(farm_action_pick_slot(1) == -1);
    memset(&game_state.animals.actions[5], 0xA5, sizeof(FarmAction));
    farm_action_clear(&game_state.animals.actions[5]);
    const u8 *bytes = (const u8 *)&game_state.animals.actions[5];
    for (size_t i = 0; i < sizeof(FarmAction); ++i)
        CHECK(bytes[i] == 0);
    FarmChicken *last = &game_state.animals.chickens[9];
    memset(last, 0xa5, sizeof(*last));
    chicken_clear(last);
    bytes = (const u8 *)last;
    for (size_t i = 0; i < sizeof(*last); ++i)
        CHECK(bytes[i] == 0);
    puts("Native animal scans preserve empty/held/location rules, last slots and action clearing");
    return 0;
}
