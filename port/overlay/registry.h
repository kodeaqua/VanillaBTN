#ifndef HMBTN_OVERLAY_REGISTRY_H
#define HMBTN_OVERLAY_REGISTRY_H

#include "compat/types.h"
#include <stddef.h>

typedef struct {
    void *address;
    size_t bytes;
} hm_overlay_span;
/* Capture before game entry, after host relocations/C initializers have run.
 * Each span is a real C object, preserving global sanitizer instrumentation. */
void hm_overlay_capture(s32 member, const hm_overlay_span *spans, size_t count);

/* Reset registry metadata; keep pristine snapshots for subsequent activations. */
void hm_overlay_reset(void);
s32 hm_overlay_select(s32 member, s32 region);
s32 hm_overlay_active(s32 region);

#endif
