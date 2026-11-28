#ifndef VANILLABTN_EVENT_DECODE_H
#define VANILLABTN_EVENT_DECODE_H

#include <stddef.h>
#include <string.h>

/* Return the expanded byte count, or zero to retry the event from disc. */
static inline size_t event_decode(unsigned char *out, size_t capacity,
                                  const unsigned char *chunk, size_t size) {
    size_t written = 0;
    for (size_t i = 1; i < size;) {
        unsigned char value = chunk[i++];
        size_t count = 1;
        if (value == chunk[0]) {
            if (size - i < 2) {
                return 0;
            }
            value = chunk[i++];
            count = chunk[i++];
        }
        if (count > capacity - written) {
            return 0;
        }
        memset(out + written, value, count);
        written += count;
    }
    return written;
}

#endif
