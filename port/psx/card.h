#ifndef HMBTN_PSX_CARD_H
#define HMBTN_PSX_CARD_H

#include <stddef.h>

/* Host callbacks return 1 on success, 0 on missing input, -1 on I/O failure.
 * Writes must replace the entire container, preserving the previous file on failure. */
typedef struct {
    void *context;
    int (*read)(void *context, int slot, void *data, size_t bytes);
    int (*write)(void *context, int slot, const void *data, size_t bytes);
} hm_card_storage;

void hm_card_attach(hm_card_storage storage);
void hm_card_detach(void);

#endif
