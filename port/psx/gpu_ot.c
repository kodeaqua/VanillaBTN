#include <libgpu.h>

#include "psx/error.h"
#include "psx/gpu_links.h"

#include <stddef.h>

u32 *ClearOTagR(u32 *table, s32 entries) {
    if (entries <= 0) {
        hm_runtime_error("ClearOTagR", "non-positive length");
    }
    if (table == NULL) {
        hm_runtime_error("ClearOTagR", "null table");
    }
    u32 previous = HM_GPU_LINK_END;
    for (s32 index = 0; index < entries; ++index) {
        u32 handle = hm_gpu_link_encode(&table[index]);
        table[index] = previous;
        previous = handle;
    }
    return table;
}

void setlen(void *packet, u8 words) {
    hm_gpu_tag_set_length(packet, words);
}

void addPrim(u32 *head, void *packet) {
    hm_gpu_add_prim(head, packet);
}

void AddPrim(u32 *head, void *packet) {
    hm_gpu_add_prim(head, packet);
}
