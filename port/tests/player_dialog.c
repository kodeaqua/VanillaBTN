/* Original confirm handler with synthetic NPC/player state; no gameplay. */
#include "../../src/overlay/11_GF_FARM/people/player.c"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    memset(&game_state, 0, sizeof game_state);
    FarmObjectStatus npc = {.talkVillager = 9, .mode = 0x1000};
    farmTarget = &npc;
    farmTargetFlags = 0x80;
    FarmPlayer *p = &game_state.player;
    p->stateFlags = PLAYER_FLAG_ACTION;
    p->talkVillager = -1;
    CHECK(player_confirm_action(p, 0) == 0 && p->talkVillager == -1);
    CHECK(player_confirm_action(p, PAD_CROSS) == 1 && p->talkVillager == 9);
    CHECK(p->mode == PLAYER_MODE_IDLE && (p->stateFlags & PLAYER_FLAG_IDLE_POSE));
    npc.mode = 1;
    p->talkVillager = -1;
    CHECK(player_advance_state(p) == 0 && p->talkVillager == -1);
    puts("Native confirm forwards the player to ordinary NPC interaction");
    return 0;
}
