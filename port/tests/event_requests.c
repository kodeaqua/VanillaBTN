#include "event_request.h"
#include "farm_state.h"
#include <stdio.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    scene_item_queue_reset();
    FarmRequestSlot *first = event_request_claim(0);
    CHECK(first == &game_state.requests.items[0] && first->value == 0);
    for (u32 j = 0; j < 5; ++j)
        CHECK(first->args[j] == 0);
    CHECK(event_request_claim(0) == NULL);
    for (u32 i = 1; i < 5; ++i) {
        FarmRequestSlot *slot = event_request_claim_any(0);
        CHECK(slot == &game_state.requests.items[i] && slot->value == 0);
        for (u32 j = 0; j < 5; ++j)
            CHECK(slot->args[j] == 0);
        slot->args[0] = (s16)i;
    }
    CHECK(event_request_claim_any(0) == NULL);
    scene_item_queue_reset();
    CHECK(event_request_claim_any(0) == first);
    return 0;
}
