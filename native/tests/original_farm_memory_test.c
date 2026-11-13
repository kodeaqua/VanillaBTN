#include "event_script.h"
#include "estate.h"
#include "farm_state.h"
#include "sdk_gpu.h"
#include "sdk_memory.h"
#include "scene.h"
#include "chicken.h"
#include "farm_probe.h"
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static Ps1Video video;
static u32 tail[2] = {22, 33};

int main(void) {
    /* The PS1 queue byte offsets overlap native entryStep after pointers grow. */
    memset(&game_state, 0, sizeof(game_state));
    game_state.progress.entryStep = 1;
    game_state.progress.estate.noticesSeen = 123;
    for (unsigned q = 0; q < MESSAGE_QUEUES; ++q) {
        for (unsigned i = 0; i < MESSAGE_QUEUE_LEN; ++i) {
            game_state.progress.estate.queues[q][i] = (FarmMessage){100 + (s16)i, 4, 7};
        }
    }
    estate_queues_clear();
    CHECK(game_state.progress.entryStep == 1);
    CHECK(game_state.progress.estate.noticesSeen == 0);
    for (unsigned q = 0; q < MESSAGE_QUEUES; ++q) {
        for (unsigned i = 0; i < MESSAGE_QUEUE_LEN; ++i) {
            FarmMessage *message = &game_state.progress.estate.queues[q][i];
            CHECK(message->kind == -1 && message->message == 100 + i && message->timer == 7);
        }
    }
    /* Original startup reads physical address zero before binding mode.data.
     * Retail PS1 maps RAM there; it is not a host null dereference. */
    sdk_memory_clear();
    memset(&farm_area, 0, sizeof(farm_area));
    CHECK(scene_resolve_selection(0x20) == 0x20);
    CHECK(farm_area.mode.data == NULL);
    farm_area.mode.data = &farm_area.mode.script;
    for (int i = 0; i < SCENE_MODEL_COUNT; ++i)
        farm_area.mode.script.models[i].flags = SCENE_ENTRY_ABSENT;
    scene_entries_clear();
    scene_populate_objects();
    /* Missing marked entries write their PS1 flags at physical 0 + 0x28. */
    CHECK(*(u16 *)(sdk_main_ram + 0x28) == SCENE_ENTRY_HIDDEN);
    sdk_main_ram[0] = 0x80;
    CHECK(farm_status_upper_bits() == 1);
    sdk_main_ram[0] = 0;
    /* Off-screen helper ticks still need the explicit x64 receiver argument. */
    game_state.globalFlags = GLOBAL_FLAG_CLOCK_TICKED;
    game_state.progress.clock.time.hm.hour = 12;
    memset(game_state.helpers, 0, sizeof(game_state.helpers));
    for (int i = 0; i < HELPER_COUNT; ++i) game_state.helpers[i].flags = HELPER_BIT5;
    FarmHelper *helper = &game_state.helpers[0];
    helper->flags = 0;
    helper->actor.place.location = PLACE_FARM;
    harvest_helpers_update();
    CHECK(helper->actor.taskId == CUE_STEP_PLAY);
    CHECK(helper->actor.flags & ACTOR_FLAG_CUE_RUNNING);
    harvest_helpers_update();
    CHECK(helper->actor.taskTime == 1);
    /* No ground objects: isolate original airborne movement from collision.
     * Row 1 lives 48 bytes past move_steps_large in the PS1 image, after the
     * riding table; native global adjacency must not select the hop distance. */
    TmdHeader empty_ground = {0};
    ModelCenters empty_centers = {0};
    farm_model = (Model){&empty_ground, &empty_centers};
    const int dx[4] = {0, -1, 0, 1};
    const int dz[4] = {-1, 0, 1, 0};
    for (int crouch = 0; crouch < 2; ++crouch) {
        int step = crouch ? 73 : 51;
        for (int facing = 0; facing < 4; ++facing) {
            FarmPlayer *player = &game_state.player;
            memset(player, 0, sizeof(*player));
            player->stateFlags = PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_AIRBORNE;
            player->facing38 = (u16)facing;
            player->mode = crouch ? PLAYER_MODE_JUMP_CROUCH : PLAYER_MODE_JUMP_LAUNCH;
            player->position = (VECTOR){1000, -1000, 2000, 0};
            player_update_position();
            CHECK(player->position.vx == 1000 + dx[facing] * step);
            CHECK(player->position.vz == 2000 + dz[facing] * step);
        }
    }
    farm_model = (Model){0};
    u32 first = 11, buffer[3] = {0};
    sdk_gpu_bind(&video);
    CHECK(ps1_video_upload(&video, 0, 0, 2, 1, (u16 *)&first, 2));
    vram_stash_rect = (RECT){0, 0, 2, 1};
    vram_stash_tail = (u8 *)tail;
    /* One word in VRAM, two words in a 64-bit host buffer. */
    vram_chunk_offsets[0] = 0;
    vram_chunk_offsets[1] = 12;
    chunk_checksums[0] = 66;
    CHECK(event_chunk_read_back((u8 *)buffer, 0, 0, 0) == (u8 *)buffer);
    const u32 expected[3] = {11, 22, 33};
    CHECK(memcmp(buffer, expected, sizeof(expected)) == 0);
    /* A chunk wholly in the tail must preserve the complete borrowed pointer. */
    vram_chunk_offsets[0] = 4;
    chunk_checksums[0] = 55;
    CHECK(event_chunk_read_back((u8 *)buffer, 0, 0, 0) == (u8 *)tail);
    sdk_gpu_bind(NULL);

    /* Event entry copies the active villagers' ordinary sprites into event
     * sprites. The PS1 code assumes 48-byte AnimState and 0x1D4-byte records;
     * x64 must keep all native pointers and use each record's real stride. */
    FarmRequestSlot event_request = {0};
    AnimFile files[CHARACTER_COUNT] = {0};
    AnimRange ranges[CHARACTER_COUNT] = {0};
    AnimPartList lists[CHARACTER_COUNT] = {0};
    AnimFrame frames[CHARACTER_COUNT] = {0};
    game_state.request = &event_request;
    memset(game_state.villagers, 0, sizeof(game_state.villagers));
    for (int i = 0; i < CHARACTER_COUNT; ++i) {
        FarmVillager *villager = &game_state.villagers[i];
        memset(&villager->sprite80, (unsigned char)(0x20 + i), sizeof(AnimState));
        memset(&villager->sprite, 0xAA, sizeof(AnimState));
        villager->sprite80.source = &files[i];
        villager->sprite80.ranges = &ranges[i];
        villager->sprite80.partLists = &lists[i];
        villager->sprite80.frames = (u8 *)&frames[i];
        villager->sprite80.animation = (u8)(i % 4);
        villager->sprite80.frame = (u8)(i % 3);
        villager->sprite80.scale = 4096 + i;
        villager->id = (CharacterId)i;
        villager->friendship = (s16)(50 + i);
        memset(&villager->cue, 0x5A, sizeof(villager->cue));
    }
    villager_sprites_copy();
    for (int i = 0; i < CHARACTER_COUNT; ++i) {
        FarmVillager *villager = &game_state.villagers[i];
        CHECK(memcmp(&villager->sprite, &villager->sprite80, sizeof(AnimState)) == 0);
        CHECK(villager->sprite.ranges == &ranges[i]);
        CHECK(villager->sprite.partLists == &lists[i]);
        CHECK(villager->sprite.frames == (u8 *)&frames[i]);
        CHECK(villager->sprite.source == &files[i]);
        CHECK(villager->id == (CharacterId)i && villager->friendship == 50 + i);
        for (size_t j = 0; j < sizeof(villager->cue); ++j)
            CHECK(((const u8 *)&villager->cue)[j] == 0x5A);
    }
    game_state.request = NULL;

    /* Removing a chicken must clear the entire host-sized actor, not only
     * the original 0xB4-byte PS1 layout (which predates 64-bit ANI pointers). */
    struct { FarmChicken record; u32 after; } chicken_guard = {0};
    memset(&chicken_guard.record, 0x7B, sizeof(chicken_guard.record));
    chicken_guard.after = 0xDAC0FFEEu;
    chicken_clear(&chicken_guard.record);
    for (size_t i = 0; i < sizeof(chicken_guard.record); i++)
        CHECK(((const u8 *)&chicken_guard.record)[i] == 0);
    CHECK(chicken_guard.after == 0xDAC0FFEEu);
    return 0;
}
