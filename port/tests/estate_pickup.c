#include "estate.h"
#include "farm_state.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    for (int season = 0; season < 4; ++season) {
        game_state.progress.clock.date.season = season;
        FarmEstateRecord record = {0};
        record.itemId = ITEM_DOG_BALL;
        record.recordFlags = 0x81 | (2u << season);
        CHECK(estate_field_pickup(&record, NULL) == 1);
        CHECK(!(record.recordFlags & ESTATE_RECORD_ENABLED));
        CHECK(estate_field_pickup(&record, NULL) == 0);
        const s16 items[] = {ITEM_WEEDS, ITEM_STONE, ITEM_BRANCH};
        for (int i = 0; i < 3; ++i) {
            record.itemId = items[i];
            record.recordFlags = 0x81 | (2u << season);
            FarmStoredItem item = {0};
            CHECK(estate_field_pickup(&record, &item) == 1 && item.count == 1);
            s16 expected = items[i];
            if (season == SEASON_FALL && i == 0)
                expected = ITEM_DEAD_WEED;
            if (season == SEASON_WINTER && i == 1)
                expected = ITEM_WINTER_STONE;
            if (season == SEASON_WINTER && i == 2)
                expected = ITEM_WINTER_BRANCH;
            CHECK(item.id == expected);
        }
        record.itemId = ITEM_AJA_GRAPES;
        record.recordFlags = 0x81 | (2u << season);
        CHECK(estate_field_pickup(&record, NULL) == 1);
        CHECK(record.recordFlags & ESTATE_RECORD_CLAIMED);
        CHECK(estate_field_pickup(&record, NULL) == 0);
    }
    return 0;
}
