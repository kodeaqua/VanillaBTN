#include "compat/player_steps.h"
#include <stdio.h>
extern s16 move_steps_small[16], move_steps_large[16], facing_reach[8][2];
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    for (u32 facing = 0; facing < 4; ++facing) {
        CHECK(hm_player_probe_step(0, facing, move_steps_small, move_steps_large, facing_reach) ==
              move_steps_small + facing * 2);
        CHECK(hm_player_probe_step(0x18, facing, move_steps_small, move_steps_large,
                                   facing_reach) == move_steps_large + 8 + facing * 2);
        CHECK(hm_player_probe_step(0x38, facing, move_steps_small, move_steps_large,
                                   facing_reach) == facing_reach[4 + facing]);
    }
    return 0;
}
