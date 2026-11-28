#include "event_decode.h"
#undef NDEBUG
#include <assert.h>

int main(void) {
    const unsigned char chunk[] = {0xfe, 1, 0xfe, 2, 3, 4};
    const unsigned char expected[] = {1, 2, 2, 2, 4, 0};
    unsigned char out[6] = {0};
    assert(event_decode(out, 5, chunk, sizeof(chunk)) == 5);
    assert(memcmp(out, expected, sizeof(out)) == 0);
    memset(out, 0, sizeof(out));
    assert(event_decode(out, 1, chunk, sizeof(chunk)) == 0 && out[1] == 0);
    assert(event_decode(out, sizeof(out), chunk, 4) == 0);
    return 0;
}
