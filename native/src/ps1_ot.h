#ifndef VANILLABTN_PS1_OT_H
#define VANILLABTN_PS1_OT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Ps1OtEntry {
    size_t next;
    uint32_t packet_id;
} Ps1OtEntry;

typedef struct Ps1Ot {
    size_t depth, capacity, count;
    size_t *heads;
    Ps1OtEntry *entries;
} Ps1Ot;

/* Host-side links replace the PS1 OT's 24-bit MIPS addresses. */
bool ps1_ot_init(Ps1Ot *ot, size_t depth, size_t capacity);
void ps1_ot_destroy(Ps1Ot *ot);
void ps1_ot_clear(Ps1Ot *ot);
bool ps1_ot_insert(Ps1Ot *ot, int bucket, uint32_t packet_id);

/* Walk high buckets first, with newest packets first inside each bucket. */
bool ps1_ot_walk(const Ps1Ot *ot, bool (*visit)(uint32_t, void *), void *context);

#endif
