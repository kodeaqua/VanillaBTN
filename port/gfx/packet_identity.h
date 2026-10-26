#ifndef HMBTN_PACKET_IDENTITY_H
#define HMBTN_PACKET_IDENTITY_H
#include "compat/types.h"
/* Construction-time identities, read later during the same native OT draw.
 * Entity keys and packet-part offsets survive changes in work-buffer ordering. */
void hm_packet_identity_reset(void);
int hm_packet_identity_span(const void *begin, const void *end, u64 entity);
int hm_packet_identity_get(const void *packet, u64 *entity, u32 *part);
#endif
