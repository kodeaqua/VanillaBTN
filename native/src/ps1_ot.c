#include "ps1_ot.h"

#include <stdint.h>
#include <stdlib.h>

bool ps1_ot_init(Ps1Ot *ot, size_t depth, size_t capacity) {
    if (ot == NULL || depth == 0 || capacity == 0
        || depth > SIZE_MAX / sizeof(size_t)
        || capacity > SIZE_MAX / sizeof(Ps1OtEntry)) {
        return false;
    }
    size_t *heads = malloc(depth * sizeof(size_t));
    Ps1OtEntry *entries = malloc(capacity * sizeof(Ps1OtEntry));
    if (heads == NULL || entries == NULL) {
        free(heads);
        free(entries);
        return false;
    }
    *ot = (Ps1Ot){depth, capacity, 0, heads, entries};
    ps1_ot_clear(ot);
    return true;
}

void ps1_ot_destroy(Ps1Ot *ot) {
    if (ot == NULL) return;
    free(ot->heads);
    free(ot->entries);
    *ot = (Ps1Ot){0};
}

void ps1_ot_clear(Ps1Ot *ot) {
    if (ot == NULL || ot->heads == NULL) return;
    for (size_t bucket = 0; bucket < ot->depth; ++bucket) {
        ot->heads[bucket] = SIZE_MAX;
    }
    ot->count = 0;
}

bool ps1_ot_insert(Ps1Ot *ot, int bucket, uint32_t packet_id) {
    if (ot == NULL || ot->heads == NULL || ot->entries == NULL
        || bucket < 0 || ot->count == ot->capacity) {
        return false;
    }
    /* The PS1 game's ot_insert_prim clamps large depth values to the last entry. */
    size_t index = (size_t)bucket < ot->depth ? (size_t)bucket : ot->depth - 1;
    size_t node = ot->count++;
    ot->entries[node] = (Ps1OtEntry){ot->heads[index], packet_id};
    ot->heads[index] = node;
    return true;
}

bool ps1_ot_walk(const Ps1Ot *ot, bool (*visit)(uint32_t, void *), void *context) {
    if (ot == NULL || ot->heads == NULL || visit == NULL) return false;
    for (size_t bucket = ot->depth; bucket > 0; --bucket) {
        for (size_t node = ot->heads[bucket - 1]; node != SIZE_MAX;
             node = ot->entries[node].next) {
            if (!visit(ot->entries[node].packet_id, context)) return false;
        }
    }
    return true;
}
