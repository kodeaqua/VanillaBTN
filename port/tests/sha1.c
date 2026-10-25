#include "psx/sha1.h"
#include <stdio.h>
#include <string.h>

static int hm_check(const void *data, size_t size, const char *expected, size_t chunk) {
    hm_sha1 hash;
    hm_sha1_init(&hash);
    const u8 *p = data;
    for (size_t at = 0; at < size;) {
        size_t take = size - at < chunk ? size - at : chunk;
        hm_sha1_update(&hash, p + at, take);
        at += take;
    }
    u8 digest[20];
    char hex[41];
    hm_sha1_finish(&hash, digest);
    for (size_t i = 0; i < 20; ++i)
        snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    if (strcmp(hex, expected)) {
        fprintf(stderr, "SHA-1 mismatch: %s\n", hex);
        return 0;
    }
    return 1;
}

int main(void) {
    static u8 million[1000000];
    memset(million, 'a', sizeof(million));
    const char *long_message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    return !(
        hm_check("", 0, "da39a3ee5e6b4b0d3255bfef95601890afd80709", 1) &&
        hm_check("abc", 3, "a9993e364706816aba3e25717850c26c9cd0d89d", 1) &&
        hm_check(long_message, strlen(long_message), "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
                 7) &&
        hm_check(million, sizeof(million), "34aa973cd4c4daa4f61eeb2bdbad27316534016f", 65536) &&
        hm_check(million, sizeof(million), "34aa973cd4c4daa4f61eeb2bdbad27316534016f", 63));
}
