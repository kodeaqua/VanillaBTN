#ifndef HMBTN_OVERLAY_STATE_INVENTORY_H
#define HMBTN_OVERLAY_STATE_INVENTORY_H
#include "overlay/registry.h"
typedef struct {
    const hm_overlay_span *spans;
    size_t count;
} hm_overlay_inventory;
/* Ignore only top-level const objects, including arrays and const pointers.
 * Pointed-to const data does not make a pointer variable immutable. */
#define HM_OVERLAY_OBJECT(object)                                                                  \
    {(void *)&(object),                                                                            \
     __builtin_types_compatible_p(__typeof__(&(object)), const __typeof__(object) *)               \
         ? 0                                                                                       \
         : sizeof(object)}
void hm_overlay_capture_parts(s32 member, const hm_overlay_inventory *parts, size_t count);
#endif
