#ifndef HMBTN_PSX_GPU_LINKS_H
#define HMBTN_PSX_GPU_LINKS_H

#include "compat/types.h"

#define HM_GPU_LINK_MASK UINT32_C(0x00FFFFFF)
#define HM_GPU_LINK_END UINT32_C(0x00FFFFFF)
enum { HM_GPU_LINK_CAPACITY = 262144 };

/* Tags remain PS1-sized. Only live, word-aligned tag addresses may be encoded.
 * Handles depend on first-use order, never on host address bits. Reset only
 * after discarding every chain; it is not a per-frame reset or a save format. */
void hm_gpu_links_reset(void);
u32 hm_gpu_link_encode(const void *tag);
const u32 *hm_gpu_link_decode(u32 tag);
void hm_gpu_tag_set_length(void *packet, u8 words);
void hm_gpu_add_prim(u32 *head, void *packet);
void hm_gpu_ot_insert(u32 *head, void *packet);
u32 hm_gpu_link_prim(u32 *head, void *packet, u8 words);

#endif
