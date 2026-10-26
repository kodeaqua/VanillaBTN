#include "compat/villager_sprites.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static FarmState state;
static FarmVillager expected[CHARACTER_COUNT];
static FarmRequestSlot request;
static AnimRange ranges[CHARACTER_COUNT];
static AnimImage images[CHARACTER_COUNT];
static AnimFrame frames[CHARACTER_COUNT];
static int reloads, wrong;
s32 villager_load_request(s32 id, AnimState *sprite) {
    if (id != reloads || sprite != &state.villagers[id].sprite)
        wrong = 1;
    ++reloads;
    sprite->frame = 7;
    return 1;
}
int main(void) {
    for (int special = 0; special <= 1; ++special) {
        memset(state.villagers, 0xa5, sizeof state.villagers);
        state.request = &request;
        request.args[0] = special ? 0x82 : 0;
        reloads = wrong = 0;
        for (int i = 0; i < CHARACTER_COUNT; ++i) {
            AnimState *sprite = &state.villagers[i].sprite80;
            memset(sprite, 0, sizeof *sprite);
            sprite->ranges = &ranges[i];
            sprite->images = &images[i];
            sprite->frames = (u8 *)&frames[i];
            sprite->source = (AnimFile *)&images[i];
            sprite->animation = (u8)i;
            sprite->frame = 2;
            sprite->scale = 4096 + i;
            expected[i] = state.villagers[i];
            expected[i].sprite = *sprite;
            if (special)
                expected[i].sprite.frame = 7;
        }
        hm_villager_sprites_copy(&state);
        CHECK(!wrong && reloads == (special ? CHARACTER_COUNT : 0));
        CHECK(!memcmp(expected, state.villagers, sizeof expected));
        CHECK(state.request == &request && request.args[0] == (special ? 0x82 : 0));
    }
    return 0;
}
