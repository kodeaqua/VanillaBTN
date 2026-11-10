/* Private original tables are tested without exporting host-only game APIs. */
#include "../../src/overlay/11_GF_FARM/people/player.c"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
int main(void) {
    /* The code-verified PS1 farm image has exactly these two probe rows at
     * halfword offsets 0x18 and 0x38 from move_steps_small. */
    static const s16 reach[4][2] = {{0,-128},{-128,0},{0,128},{128,0}};
    for (u16 facing = 0; facing < 4; facing++) {
        s16 *offset = player_probe_offset(0, facing);
        CHECK(offset[0] == 0 && offset[1] == 0);
        offset = player_probe_offset(0x18, facing);
        CHECK(offset == move_steps_large + 8 + facing * 2);
        CHECK(offset[0] == reach[facing][0] && offset[1] == reach[facing][1]);
        offset = player_probe_offset(0x38, facing);
        CHECK(offset == facing_reach[4 + facing]);
        CHECK(offset[0] == 0 && offset[1] == 0);
    }
    return 0;
}
