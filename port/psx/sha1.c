#include "psx/sha1.h"
#include <string.h>

/* Independent implementation of the SHA-1 operations specified in RFC 3174.
 * This identifies the original disc; it is not an authentication primitive. */
static u32 hm_rotate(u32 value, u32 shift) { return (value << shift) | (value >> (32 - shift)); }

static void hm_sha1_block(hm_sha1 *hash) {
    u32 words[80];
    for (u32 i = 0; i < 16; ++i) {
        const u8 *p = hash->block + i * 4;
        words[i] = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
    }
    for (u32 i = 16; i < 80; ++i)
        words[i] = hm_rotate(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
    u32 a = hash->state[0], b = hash->state[1], c = hash->state[2];
    u32 d = hash->state[3], e = hash->state[4];
    for (u32 i = 0; i < 80; ++i) {
        u32 f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        u32 next = hm_rotate(a, 5) + f + e + k + words[i];
        e = d;
        d = c;
        c = hm_rotate(b, 30);
        b = a;
        a = next;
    }
    hash->state[0] += a;
    hash->state[1] += b;
    hash->state[2] += c;
    hash->state[3] += d;
    hash->state[4] += e;
}

void hm_sha1_init(hm_sha1 *hash) {
    *hash = (hm_sha1){.state = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}};
}

void hm_sha1_update(hm_sha1 *hash, const void *data, size_t size) {
    const u8 *source = data;
    hash->bytes += size;
    while (size) {
        size_t take = 64 - hash->used;
        if (take > size)
            take = size;
        memcpy(hash->block + hash->used, source, take);
        source += take;
        size -= take;
        hash->used += take;
        if (hash->used == 64) {
            hm_sha1_block(hash);
            hash->used = 0;
        }
    }
}

void hm_sha1_finish(hm_sha1 *hash, u8 digest[20]) {
    u64 bits = hash->bytes * 8;
    const u8 marker = 0x80, zero = 0;
    hm_sha1_update(hash, &marker, 1);
    while (hash->used != 56)
        hm_sha1_update(hash, &zero, 1);
    u8 length[8];
    for (u32 i = 0; i < 8; ++i)
        length[7 - i] = (u8)(bits >> (i * 8));
    hm_sha1_update(hash, length, sizeof(length));
    for (u32 i = 0; i < 20; ++i)
        digest[i] = (u8)(hash->state[i / 4] >> (24 - (i % 4) * 8));
}
