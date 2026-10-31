#define field_world_to_cell hm_test_field_cell
#define greenhouse_world_to_cell hm_test_greenhouse_cell
#include "../enhance/gameplay/gameplay.h"
#include "../platform/gameplay_options.h"
#include "../../src/overlay/11_GF_FARM/farm_probe.c"
#undef field_world_to_cell
#undef greenhouse_world_to_cell
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static ItemInfo info[ITEM_DEAD_WEED + 1];
static FarmCrop crops[CROP_COUNT];
static FarmTileRule rule;

s32 hm_test_field_cell(VECTOR *point, s16 *column, s16 *row) {
    (void)point;
    *column = 0;
    *row = 0;
    return 0;
}
s32 hm_test_greenhouse_cell(VECTOR *point, s16 *column, s16 *row) {
    return hm_test_field_cell(point, column, row);
}
int main(void) {
    game_state.itemInfo = info;
    game_state.cropTable = crops;
    game_state.tileRules = &rule;
    info[ITEM_WEEDS].tileRule[0] = 0;
    info[ITEM_WEEDS].itemFlags = ITEM_FLAG_PICKUP;
    info[ITEM_DEAD_WEED].tileRule[0] = 0;
    info[ITEM_DEAD_WEED].itemFlags = ITEM_FLAG_PICKUP;
    info[ITEM_GRASS_SEEDS].tileRule[0] = 0;
    info[ITEM_GRASS_SEEDS].itemFlags = ITEM_FLAG_TOOL | ITEM_FLAG_SEED;
    info[ITEM_GRASS_SEEDS].crop = CROP_GRASS;
    crops[CROP_GRASS].grownStage = 1;
    VECTOR pos = {0};
    for (int greenhouse = 0; greenhouse <= 1; ++greenhouse) {
        LimitPair place = {0};
        place.location = greenhouse ? 6 : 1;
        FarmFieldTile *tile =
            greenhouse ? &game_state.slots7x7[0].rows[0] : &game_state.fieldTiles[0].rows[0];
        tile->value = ITEM_WEEDS;
        tile->kind = 0;
        for (int blocked = 0; blocked <= 1; ++blocked) {
            rule.status = blocked ? TILE_RULE_BLOCKED : TILE_RULE_OPEN;
            s32 flags = 0;
            void *found = NULL;
            FarmTileQuery query = {.pos = &pos,
                                   .place = &place,
                                   .tile = &found,
                                   .flags = &flags,
                                   .hitFlags = FARM_HIT_TILE};
            s32 expected = blocked ? TILE_CLASS_BLOCKED : TILE_CLASS_PASSABLE;
            CHECK(farm_tile_classify(&query, 0) == expected);
            CHECK(found == tile && flags == (FARM_HIT_TILE | 0x4000 | 0x2000));
            CHECK(farm_probe_tile(pos, place, FACING_DOWN, 1) == expected);
            CHECK(farm_tile_occupant(&pos, place, NULL, NULL, NULL, OCCUPANT_TILE) == 1);
            farm_callback_reset();
            for (int i = 0; i < FARM_CALLBACK_CAP + 2; ++i)
                CHECK(farm_callback_capped(&pos, place, 0, 0, NULL, NULL, NULL, OCCUPANT_TILE) ==
                      1);
            CHECK(farm_callbacks_queued == FARM_CALLBACK_CAP);
            flags = 123;
            CHECK(farm_callback_capped(&pos, place, 0, 0, NULL, &flags, NULL, OCCUPANT_TILE) == 1 &&
                  flags == 0);
        }
        place.location = 2;
        CHECK(farm_probe_tile(pos, place, FACING_DOWN, 1) == 0);
    }

    /* Walk Crops keeps interaction flags intact for both wild weeds and harvestable grass. */
    {
        hm_gameplay_options gameplay_options = {0};
        const ItemId pickup_tiles[] = {ITEM_WEEDS, ITEM_DEAD_WEED, ITEM_GRASS_SEEDS};
        LimitPair place = {0};
        FarmFieldTile *tile = &game_state.fieldTiles[0].rows[0];
        gameplay_options.walk_through_crops = 1;
        gameplay_options.roaming_horse = 1;
        hm_gameplay_set_active(&gameplay_options);
        place.location = 1;
        rule.status = TILE_RULE_BLOCKED;
        for (size_t i = 0; i < sizeof(pickup_tiles) / sizeof(pickup_tiles[0]); ++i) {
            s32 flags = 0;
            void *found = NULL;
            tile->value = pickup_tiles[i];
            tile->kind = pickup_tiles[i] == ITEM_GRASS_SEEDS ? 1 : 0;
            FarmTileQuery query = {.pos = &pos,
                                   .place = &place,
                                   .tile = &found,
                                   .flags = &flags,
                                   .hitFlags = FARM_HIT_TILE};
            CHECK(farm_tile_classify(&query, 0) == TILE_CLASS_PASSABLE);
            CHECK(found == tile && flags == (FARM_HIT_TILE | 0x4000 | 0x2000));
            CHECK(farm_probe_tile(pos, place, FACING_DOWN, 1) == TILE_CLASS_PASSABLE);
            flags = 0;
            found = NULL;
            CHECK(farm_tile_occupant(&pos, place, &found, &flags, NULL, OCCUPANT_TILE) == 1);
            CHECK(found == tile && flags == (FARM_HIT_TILE | 0x4000 | 0x2000));
        }
    }

    /* The real estate/basket caller intentionally omits both query outputs. */
    FarmEstateRecord stamps[2] = {0};
    stamps[0].location = 1;
    stamps[1].location = 6;
    game_state.runs.records[18] = stamps;
    farm_callback_reset();
    estate_basket_check();
    CHECK(farm_callbacks_queued == 2);
    CHECK(stamps[0].position.vx == -5000 && stamps[0].location == 1);
    CHECK(stamps[1].position.vx == -1200 && stamps[1].location == 1);
    return 0;
}
