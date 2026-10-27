#include "compat/villager_sprites.h"
#include "villager.h"
void hm_villager_sprites_copy(FarmState *state) {
    for (s32 index = 0; index < CHARACTER_COUNT; ++index) {
        FarmVillager *villager = &state->villagers[index];
        villager->sprite = villager->sprite80;
        if (state->request->args[0] == 0x82)
            villager_load_request((s16)index, &villager->sprite);
    }
}
