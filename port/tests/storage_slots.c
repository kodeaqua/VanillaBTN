/* Exercise original private slot routing with real native player/store fields. */
#include "../../src/overlay/11_GF_FARM/menu/item_storage.c"
#include <stdio.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
_Static_assert(sizeof(FarmMarker) == 16, "storage marker byte layout");
_Static_assert(offsetof(FarmMarker, next_cell) == 4, "storage navigation byte offset");
int main(void) {
    for (s32 state = 0; state < 3; ++state) {
        item_result_state = state;
        CHECK(item_row_at(0, 0) == &game_state.player.held[state ? 1 : 0]);
        for (s32 col = 1; col < 9; ++col) {
            FarmStoredItem *slot =
                state ? &game_state.player.items[col - 1] : &game_state.player.tools[col - 1];
            CHECK(item_row_at(col, 0) == slot && item_row_at(col, 7) == slot);
        }
        FarmStoredItem *store =
            state == ITEM_RESULT_LIST_TOOLBOX  ? game_state.progress.estate.toolboxItems
            : state == ITEM_RESULT_LIST_ICEBOX ? game_state.progress.estate.iceboxItems
                                               : game_state.progress.estate.cabinetItems;
        for (s32 row = 0; row < ITEM_STORE_SLOTS / 8; ++row)
            for (s32 col = 9; col < 17; ++col)
                CHECK(item_row_at(col, row) == &store[row * 8 + col - 9]);
    }
    return 0;
}
