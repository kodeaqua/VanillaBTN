#include "actor.h"
#include "animals.h"
#include "chicken.h"
#include "cue.h"
#include "dog.h"
#include "farm_state.h"
#include "harvest_sprites.h"
#include "livestock.h"
#include "model.h"
#include "picture_cells.h"
#include "player.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x);                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
extern s32 hm_psx_original_farm_action_begin(struct FarmActionSlot *self);
extern u16 hm_actor_helper_number(const void *target);
_Static_assert(offsetof(FarmActor, sprite) + offsetof(AnimState, partLists) == 0xa8,
               "Regression must cover the original padded cue alias");
_Static_assert(offsetof(FarmActor, cue) != 0xa8, "Native cue follows expanded animation pointers");
extern Model farm_model;
static int check_pickup_and_drop(ItemInfo *info) {
    static TmdHeader ground;
    static ModelCenters centers;
    static AnimPartList part_list;
    farm_model.tmd = &ground;
    farm_model.centers = &centers;
    for (int kind = ITEM_ADULT_DOG; kind <= ITEM_YOUNG_DOG; ++kind) {
        info[kind].itemFlags = ITEM_FLAG_BIT8;
        for (int facing = 0; facing < 4; ++facing) {
            for (int praised = 0; praised < 2; ++praised) {
                FarmPet *dog = &game_state.animals.dog;
                FarmPlayer *player = &game_state.player;
                memset(dog, 0, sizeof(*dog));
                memset(player, 0, sizeof(*player));
                dog->actor.kind = kind;
                dog->actor.flags = praised ? ACTOR_FLAG_BIT2 : 0;
                dog->actor.sprite.partLists = &part_list;
                CHECK(farm_action_begin((struct FarmActionSlot *)dog) == 0);
                CHECK(dog->actor.flags & ACTOR_FLAG_HELD);
                CHECK(dog->actor.sprite.partLists == &part_list);
                player->facing38 = facing;
                player->position = (VECTOR){123, 100, 234, 0};
                player->place.location = 1;
                player->held[HELD_ITEM].id = kind;
                player_drop_held_item(player);
                CHECK(!(dog->actor.flags & ACTOR_FLAG_HELD));
                CHECK(dog->actor.flags & ACTOR_FLAG_BIT2);
                CHECK(dog->actor.taskNo == DOG_PHASE_PLACED);
                CHECK(dog->actor.place.location == 1 && player->held[HELD_ITEM].id == 0);
                CHECK(dog->actor.cue.id == (praised ? -1 : 37));
                CHECK(dog->actor.cue.phase == (praised ? 0 : 45));
                for (int tick = 0; tick < 60; ++tick) {
                    CHECK(dog_update_phase(dog) == 0);
                    cue_countdown(&dog->actor.cue);
                }
                CHECK(dog->actor.sprite.partLists == &part_list);
                CHECK(dog->actor.taskNo == DOG_PHASE_PLACED && dog->actor.cue.phase == 0);
            }
        }
    }
    return 0;
}
int main(void) {
    static ItemInfo info[ITEM_YOUNG_DOG + 1];
    game_state.itemInfo = info;
    info[ITEM_YOUNG_DOG].itemFlags = ITEM_FLAG_BIT8;
    FarmPet dog = {0};
    FarmChicken chicken = {0};
    FarmLivestock livestock = {0};
    FarmAction action = {0};
    FarmActor *actors[] = {&dog.actor, &chicken.actor, &livestock.actor, &action.actor};
    for (size_t i = 0; i < sizeof(actors) / sizeof(actors[0]); ++i) {
        FarmActor *actor = actors[i];
        memset(actor, 0xa5, sizeof(*actor));
        actor->kind = ITEM_YOUNG_DOG; /* Same pickup contract for every native actor prefix. */
        actor->flags = 0x2000;
        FarmActor before = *actor;
        CHECK(farm_action_begin((struct FarmActionSlot *)actor) == 0);
        CHECK(!memcmp(&actor->sprite, &before.sprite, sizeof(actor->sprite)));
        CHECK(actor->flags == 0x20000 && actor->taskNo == 0 && actor->taskId == 0 &&
              actor->taskTime == 0 && actor->interruptTime == 0);
        CHECK(actor->speed.vx == 0 && actor->speed.vy == 0 && actor->speed.vz == 0);
        CHECK(actor->cue.id == -1 && actor->cue.phase == 0);
        CHECK(!memcmp(&actor->position, &before.position, sizeof(actor->position)));
        CHECK(!memcmp(&actor->destination, &before.destination, sizeof(actor->destination)));
        CHECK(!memcmp(actor->name, before.name, sizeof(actor->name)));
        info[ITEM_YOUNG_DOG].itemFlags = 0;
        before = *actor;
        CHECK(farm_action_begin((struct FarmActionSlot *)actor) == 1);
        CHECK(!memcmp(actor, &before, sizeof(before)));
        info[ITEM_YOUNG_DOG].itemFlags = ITEM_FLAG_BIT8;
    }
    /* Reproduce the exact former corruption without loading assets or gameplay. */
    dog.actor.kind = ITEM_YOUNG_DOG;
    dog.actor.flags = 0;
    AnimState before = dog.actor.sprite;
    CHECK(hm_psx_original_farm_action_begin((struct FarmActionSlot *)&dog.actor) == 0);
    CHECK(memcmp(&dog.actor.sprite, &before, sizeof(before)));
    u32 overwritten;
    memcpy(&overwritten, &dog.actor.sprite.partLists, sizeof(overwritten));
    CHECK(overwritten == 0x0000ffff);
    FarmHelper helper;
    memset(&helper, 0xa5, sizeof(helper));
    for (u16 number = 0; number < 7; ++number) {
        helper.number = number;
        CHECK(hm_actor_helper_number(&helper) == number);
        u16 old_number;
        memcpy(&old_number, (const u8 *)&helper + 0xac, sizeof(old_number));
        CHECK(old_number == 0xa5a5 && old_number != number);
    }
    CHECK(check_pickup_and_drop(info) == 0);
    return 0;
}
