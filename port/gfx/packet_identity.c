#include "gfx/packet_identity.h"
#include <stdint.h>
enum { HM_IDENTITY_SPANS = 4096 };
typedef struct {
    uintptr_t begin, end;
    u64 entity;
} hm_identity_span;
static hm_identity_span hm_spans[HM_IDENTITY_SPANS];
static u32 hm_span_count;
static int hm_span_invalid;
void hm_packet_identity_reset(void) {
    hm_span_count = 0;
    hm_span_invalid = 0;
}
int hm_packet_identity_span(const void *begin, const void *end, u64 entity) {
    uintptr_t first = (uintptr_t)begin, last = (uintptr_t)end;
    if (hm_span_invalid || !first || last <= first || ((last - first) & 3))
        return 0;
    if (hm_span_count == HM_IDENTITY_SPANS ||
        (hm_span_count && first < hm_spans[hm_span_count - 1].end)) {
        hm_span_invalid = 1;
        hm_span_count = 0;
        return 0;
    }
    hm_spans[hm_span_count++] = (hm_identity_span){first, last, entity};
    return 1;
}
int hm_packet_identity_get(const void *packet, u64 *entity, u32 *part) {
    if (hm_span_invalid || !hm_span_count)
        return 0;
    uintptr_t at = (uintptr_t)packet;
    u32 lo = 0, hi = hm_span_count;
    while (lo < hi) {
        u32 mid = lo + (hi - lo) / 2;
        if (hm_spans[mid].begin <= at)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (!lo)
        return 0;
    const hm_identity_span *span = &hm_spans[lo - 1];
    if (at >= span->end || ((at - span->begin) & 3) || (at - span->begin) / 4 > UINT32_MAX)
        return 0;
    *entity = span->entity;
    *part = (u32)((at - span->begin) / 4);
    return 1;
}
