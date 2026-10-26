/* Exercise actual private setup state without opening a window or reading assets. */
#include "../../src/overlay/11_GF_FARM/menu/shop_dialogues.c"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv) {
    if (argc == 2) {
        shop_request_dispatch(!strcmp(argv[1], "negative") ? -1 : 29);
        return 1;
    }
    shop_screen_id = 0;
    shop_dialogue_tick_index = 7;
    CHECK(shop_request_dispatch(28) == 0);
    CHECK(shop_screen_id == 28 && shop_dialogue_tick_index == 0);
    shop_screen_id = 0;
    shop_dialogue_tick_index = 7;
    CHECK(shop_request_dispatch(21) == 0); /* Original null initializer. */
    CHECK(shop_screen_id == 21 && shop_dialogue_tick_index == 7);
    shop_screen_id = 0;
    shop_step = -123;
    shop_dialogue_tick_index = 7;
    CHECK(shop_request_dispatch(4) == 0);
    CHECK(shop_step == -123 && shop_dialogue_tick_index == 0);
    shop_screen_id = 0;
    CHECK(shop_request_dispatch(0) == 0);
    farm_tables_register(0);
    game_state.progress.estate.statusFlags |= ESTATE_STATUS_NO_BASKET;
    game_state.progress.estate.estateFlags &= ~ESTATE_FLAG_BASKET;
    shop_step = -123;
    CHECK(shop_basket_step() == 0);
    shop_step = SHOP_GRANT_LEAVE;
    CHECK(shop_basket_step() == 1);
    CHECK(!(game_state.player.stateFlags & PLAYER_FLAG_SCRIPTED));
    return 0;
}
