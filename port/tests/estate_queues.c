#include "estate.h"
#include "farm_state.h"
#include <stdio.h>
#include <string.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    memset(&game_state, 0x25, sizeof(game_state));
    game_state.progress.entryStep = 1;
    for (u32 queue = 0; queue < MESSAGE_QUEUES; ++queue)
        for (u32 slot = 0; slot < MESSAGE_QUEUE_LEN; ++slot) {
            FarmMessage *message = &game_state.progress.estate.queues[queue][slot];
            message->message = (s16)(queue * 100 + slot);
            message->timer = (s8)slot;
            message->kind = 4;
        }
    estate_queues_clear();
    CHECK(game_state.progress.entryStep == 1 && !game_state.progress.estate.noticesSeen);
    for (u32 queue = 0; queue < MESSAGE_QUEUES; ++queue)
        for (u32 slot = 0; slot < MESSAGE_QUEUE_LEN; ++slot) {
            FarmMessage *message = &game_state.progress.estate.queues[queue][slot];
            CHECK(message->kind == -1 && message->message == (s16)(queue * 100 + slot));
            CHECK(message->timer == (s8)slot);
        }
    return 0;
}
