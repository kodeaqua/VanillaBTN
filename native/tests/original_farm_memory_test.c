#include "event_script.h"
#include "estate.h"
#include "farm_state.h"
#include "sdk_gpu.h"
#include "sdk_memory.h"
#include "scene.h"
#include "farm_probe.h"
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
    return 0;
}
