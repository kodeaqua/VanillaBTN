#include "ps1_ot.h"

#include <stdint.h>

typedef struct Visits {
    uint32_t values[5];
    size_t count;
} Visits;

static bool collect(uint32_t packet_id, void *context) {
    Visits *visits = context;
    if (visits->count == 5) return false;
    visits->values[visits->count++] = packet_id;
    return true;
}

int main(void) {
    Ps1Ot ot = {0};
    Visits visits = {{0}, 0};
    if (!ps1_ot_init(&ot, 4, 4)) return 1;
    if (!ps1_ot_insert(&ot, 1, 10) || !ps1_ot_insert(&ot, 3, 30)
        || !ps1_ot_insert(&ot, 1, 11) || !ps1_ot_insert(&ot, 0, 0)
        || ps1_ot_insert(&ot, 0, 99) || ps1_ot_insert(&ot, -1, 99)
        || !ps1_ot_walk(&ot, collect, &visits)) {
        ps1_ot_destroy(&ot);
        return 2;
    }
    if (visits.count != 4 || visits.values[0] != 30 || visits.values[1] != 11
        || visits.values[2] != 10 || visits.values[3] != 0) {
        ps1_ot_destroy(&ot);
        return 3;
    }
    ps1_ot_clear(&ot);
    visits.count = 0;
    if (!ps1_ot_insert(&ot, 99, 42) || !ps1_ot_walk(&ot, collect, &visits)
        || visits.count != 1 || visits.values[0] != 42) {
        ps1_ot_destroy(&ot);
        return 4;
    }
    /* NULL guards and corrupted node index bounds check */
    if (ps1_ot_walk(NULL, collect, &visits)
        || ps1_ot_walk(&ot, NULL, &visits)) {
        ps1_ot_destroy(&ot);
        return 5;
    }
    ot.heads[ot.depth - 1] = ot.count + 10;
    if (ps1_ot_walk(&ot, collect, &visits)) {
        ps1_ot_destroy(&ot);
        return 6;
    }
    ps1_ot_destroy(&ot);
    return 0;
}
