#include "farm_state.h"
#include "farm_tables.h"
#include "picture_cells.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static ItemInfo item;
static u32 codes[3][4];
int main(void) {
    game_state.itemInfo = &item;
    game_state.pictureCodes = codes;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 40; ++col)
            codes[row][col / 10] |= (u32)((row + col) % 7) << ((col % 10) * 3);
    for (int row = 0; row < 3; ++row) {
        item.picture = row;
        for (int col = 0; col < 40; ++col) {
            item.itemFlags = 0;
            int expected = (row + col) % 7;
            CHECK(item_villager_reaction(0, col) == expected);
            item.itemFlags = 0x180;
            int flagged = expected < 3 ? 5 : expected < 5 ? 6 : expected;
            CHECK(item_villager_reaction(0, col) == flagged);
        }
    }
    item.picture = -1;
    CHECK(item_villager_reaction(0, 0) == -1);
    item.picture = 0;
    item.itemFlags = ITEM_FLAG_TOOL;
    CHECK(item_villager_reaction(0, 0) == -2);
    farm_tables_register(0);
    FarmStoredItem used = {0};
    used.id = ITEM_COIN;
    game_state.player.gold = 1000;
    CHECK(item_run_use(&used) == 1 && game_state.player.gold == 1100);
    used.id = ITEM_POWER_BERRY;
    game_state.player.maxStamina = 100;
    game_state.player.stamina = 10;
    game_state.player.fatigue = 10;
    game_state.progress.estate.powerBerries = 0;
    CHECK(item_run_use(&used) == 1);
    CHECK(game_state.player.maxStamina == 110 && game_state.player.stamina == 110);
    CHECK(game_state.player.fatigue == 9 && game_state.progress.estate.powerBerries == 1);
    return 0;
}
