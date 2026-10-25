#include "psx/gpu_links.h"
#include "psx/error.h"

#include <string.h>

enum { HM_GPU_LINK_BUCKETS = HM_GPU_LINK_CAPACITY * 2 };
static const void *hm_gpu_link_pointers[HM_GPU_LINK_CAPACITY];
static u32 hm_gpu_link_buckets[HM_GPU_LINK_BUCKETS];
static u32 hm_gpu_link_count;

static void hm_gpu_require_tag(const void *tag, const char *function) {
    if (tag == NULL) {
        hm_runtime_error(function, "null tag");
    }
    if (((uintptr_t)tag & 3) != 0) {
        hm_runtime_error(function, "unaligned tag");
    }
}

void hm_gpu_links_reset(void) {
    memset(hm_gpu_link_buckets, 0, sizeof(hm_gpu_link_buckets));
    hm_gpu_link_count = 0;
}

u32 hm_gpu_link_encode(const void *tag) {
    hm_gpu_require_tag(tag, "hm_gpu_link_encode");
    uintptr_t address = (uintptr_t)tag;
    u32 bucket = ((u32)(address >> 2) ^ (u32)(address >> 32)) & (HM_GPU_LINK_BUCKETS - 1);
    for (;;) {
        u32 handle = hm_gpu_link_buckets[bucket];
        if (handle == 0) {
            if (hm_gpu_link_count == HM_GPU_LINK_CAPACITY) {
                hm_runtime_error("hm_gpu_link_encode", "handle capacity exceeded");
            }
            handle = ++hm_gpu_link_count;
            hm_gpu_link_pointers[handle - 1] = tag;
            hm_gpu_link_buckets[bucket] = handle;
            return handle;
        }
        if (hm_gpu_link_pointers[handle - 1] == tag) {
            return handle;
        }
        bucket = (bucket + 1) & (HM_GPU_LINK_BUCKETS - 1);
    }
}

const u32 *hm_gpu_link_decode(u32 tag) {
    u32 handle = tag & HM_GPU_LINK_MASK;
    if (handle == HM_GPU_LINK_END) {
        return NULL;
    }
    if (handle == 0 || handle > hm_gpu_link_count) {
        hm_runtime_error("hm_gpu_link_decode", "invalid handle");
    }
    return hm_gpu_link_pointers[handle - 1];
}

void hm_gpu_tag_set_length(void *packet, u8 words) {
    hm_gpu_require_tag(packet, "hm_gpu_tag_set_length");
    /* Write only the length byte: the old address bytes may be uninitialized. */
    ((u8 *)packet)[3] = words;
}

static void hm_gpu_join(u32 *head, void *packet, s32 preserve_head_length) {
    hm_gpu_require_tag(head, "hm_gpu_join");
    u32 handle = hm_gpu_link_encode(packet);
    u32 old_head;
    memcpy(&old_head, head, sizeof(old_head));
    u32 packet_tag = ((u32)((u8 *)packet)[3] << 24) | (old_head & HM_GPU_LINK_MASK);
    u32 new_head = handle | (preserve_head_length ? old_head & ~HM_GPU_LINK_MASK : 0);
    memcpy(packet, &packet_tag, sizeof(packet_tag));
    memcpy(head, &new_head, sizeof(new_head));
}

void hm_gpu_add_prim(u32 *head, void *packet) {
    hm_gpu_join(head, packet, 1);
}

void hm_gpu_ot_insert(u32 *head, void *packet) {
    hm_gpu_join(head, packet, 0);
}

u32 hm_gpu_link_prim(u32 *head, void *packet, u8 words) {
    hm_gpu_tag_set_length(packet, words);
    hm_gpu_ot_insert(head, packet);
    return *head;
}
