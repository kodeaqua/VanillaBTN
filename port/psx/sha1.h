#ifndef HMBTN_PSX_SHA1_H
#define HMBTN_PSX_SHA1_H

#include "compat/types.h"
#include <stddef.h>

typedef struct {
    u32 state[5];
    u64 bytes;
    u8 block[64];
    size_t used;
} hm_sha1;

void hm_sha1_init(hm_sha1 *hash);
void hm_sha1_update(hm_sha1 *hash, const void *data, size_t size);
void hm_sha1_finish(hm_sha1 *hash, u8 digest[20]);

#endif
