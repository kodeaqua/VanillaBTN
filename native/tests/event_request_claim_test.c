/* Native regression test for the event-request slot return contract. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "farm_state.h"
#include "event_request.h"

FarmState game_state;

static void reset_request_slots(void) {
    memset(&game_state.requests, 0, sizeof(game_state.requests));
    for (int i = 0; i < 5; ++i) {
        game_state.requests.items[i].value = -1;
    }
}

int main(void) {
    reset_request_slots();

    FarmRequestSlot *first = event_request_claim_any(0);
    if (first != &game_state.requests.items[0]) return 1;
    first->value = 0x74; /* New-game intro / Thomas event request. */
    first->args[0] = 1;

    FarmRequestSlot *second = event_request_claim_any(0);
    if (second != &game_state.requests.items[1]) return 2;

    reset_request_slots();
    first = event_request_claim(0x75);
    if (first != &game_state.requests.items[0]) return 3;
    first->value = 0x75;
    if (event_request_claim(0x75) != NULL) return 4;

    puts("event_request_claim_test: native request-slot returns passed");
    return 0;
}
