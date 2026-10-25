#include <libgpu.h>

#include "main.h"
#include "frame_buffers.h"
#include "prim.h"
#include "psx/error.h"
#include "psx/gpu_links.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                            \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/* Test-only GS pool for the original prim_add_* wrappers. The real game work
 * allocator has separate coverage in game.work_memory. */
_Alignas(16) static u8 hm_test_pool[512];
static size_t hm_test_pool_used;
FrameState frame_records[2];
FrameState *current_frame;
u8 *region_base;

void *gs_work_alloc(s32 size) {
    if (size < 0 || (size_t)size > sizeof(hm_test_pool) - hm_test_pool_used) {
        hm_runtime_error("test gs_work_alloc", "unexpected allocation");
    }
    void *result = hm_test_pool + hm_test_pool_used;
    hm_test_pool_used += (size_t)size;
    return result;
}

void gs_work_release(void *end) {
    uintptr_t used = (uintptr_t)end - (uintptr_t)hm_test_pool;
    if (used > sizeof(hm_test_pool)) {
        hm_runtime_error("test gs_work_release", "unexpected cursor");
    }
    hm_test_pool_used = used;
}

static int hm_test_addresses(void) {
    const size_t stride = 0x1000000;
    u8 *storage = malloc(stride + sizeof(u32));
    CHECK(storage != NULL);
    u32 *first = (u32 *)storage;
    u32 *second = (u32 *)(storage + stride);
    CHECK(((uintptr_t)first & HM_GPU_LINK_MASK) == ((uintptr_t)second & HM_GPU_LINK_MASK));
    hm_gpu_links_reset();
    u32 a = hm_gpu_link_encode(first);
    u32 b = hm_gpu_link_encode(second);
    CHECK(a != b);
    CHECK(hm_gpu_link_encode(first) == a);
    CHECK(hm_gpu_link_decode(a) == first && hm_gpu_link_decode(b) == second);
    hm_gpu_links_reset(); /* Discard handles before releasing their objects. */
    free(storage);

    u32 left[2] = {0}, right[2] = {0};
    a = hm_gpu_link_encode(&left[1]);
    b = hm_gpu_link_encode(&left[0]);
    hm_gpu_links_reset();
    CHECK(hm_gpu_link_encode(&right[1]) == a);
    CHECK(hm_gpu_link_encode(&right[0]) == b);
    hm_gpu_links_reset();
    return 0;
}

static int hm_test_chain(void) {
    u32 table[4];
    ClearOTagR(table, 4);
    CHECK(table[0] == HM_GPU_LINK_END);
    for (s32 index = 1; index < 4; ++index) {
        CHECK(hm_gpu_link_decode(table[index]) == &table[index - 1]);
        CHECK(table[index] >> 24 == 0);
    }
    u32 head = 0x07FFFFFF;
    u32 first[3] = {0x02DEADBE, 0x11111111, 0x22222222};
    u32 second[2] = {0x01000000, 0x33333333};
    addPrim(&head, first);
    CHECK(head >> 24 == 7 && first[0] >> 24 == 2);
    CHECK(hm_gpu_link_decode(head) == first && hm_gpu_link_decode(first[0]) == NULL);
    AddPrim(&head, second);
    CHECK(head >> 24 == 7 && hm_gpu_link_decode(second[0]) == first);
    POLY_F3 triangle = {0};
    u32 triangle_handle = LINK_PRIM(&head, &triangle, POLY_F3);
    CHECK(triangle_handle == head);
    CHECK(head >> 24 == 0 && triangle.tag >> 24 == 4);
    CHECK(hm_gpu_link_decode(head) == (const u32 *)&triangle);
    CHECK(hm_gpu_link_decode(triangle.tag) == second);
    CHECK(first[1] == 0x11111111 && second[1] == 0x33333333);
    u32 tag = 0x00FEDCBA;
    setlen(&tag, 9);
    CHECK(tag == 0x09FEDCBA);
    hm_gpu_links_reset();
    return 0;
}

static int hm_test_builders(void) {
    u32 table[2];
    TILE tile = {0};
    _Alignas(4) u8 packets[32] = {0};
    ClearOTagR(table, 2);
    CHECK(prim_make_tile(&tile, &table[1], -5, 7, 3, 4, 0xA1B2C3, 1) == &tile + 1);
    CHECK(tile.tag >> 24 == 3 && tile.code == 0x62);
    CHECK(tile.x0 == -5 && tile.y0 == 7 && tile.w == 3 && tile.h == 4);
    CHECK(tile.r0 == 0xA1 && tile.g0 == 0xB2 && tile.b0 == 0xC3);
    CHECK(hm_gpu_link_decode(tile.tag) == &table[0]);
    CHECK(prim_make_rect_mode(packets, &table[1], 1, 2, 10, 20, 0x010203, 1, 3) == packets + 32);
    const u32 *mode = hm_gpu_link_decode(table[1]);
    CHECK((const void *)mode == packets + 24 && *mode >> 24 == 1);
    CHECK(mode[1] == 0xE1000260);
    const u32 *rectangle = hm_gpu_link_decode(*mode);
    CHECK((const void *)rectangle == packets && *rectangle >> 24 == 5);
    CHECK(packets[7] == 0x2A && packets[4] == 1 && packets[5] == 2 && packets[6] == 3);
    CHECK(hm_gpu_link_decode(*rectangle) == (const u32 *)&tile);
    hm_test_pool_used = 0;
    prim_add_rect_mode(&table[1], 0, 0, 8, 8, 0, 0, 0);
    CHECK(hm_test_pool_used == 32);
    CHECK((const void *)hm_gpu_link_decode(table[1]) == hm_test_pool + 24);
    hm_gpu_links_reset();
    return 0;
}

static int hm_test_rejection(const char *name) {
    _Alignas(4) u8 storage[8] = {0};
    hm_gpu_links_reset();
    if (strcmp(name, "null") == 0) {
        hm_gpu_link_encode(NULL);
    } else if (strcmp(name, "unaligned") == 0) {
        hm_gpu_link_encode(storage + 1);
    } else if (strcmp(name, "unknown") == 0) {
        hm_gpu_link_decode(1);
    } else if (strcmp(name, "zero") == 0) {
        hm_gpu_link_decode(0);
    } else if (strcmp(name, "empty") == 0) {
        ClearOTagR((u32 *)storage, 0);
    } else if (strcmp(name, "capacity") == 0) {
        u32 *tags = malloc((HM_GPU_LINK_CAPACITY + 1) * sizeof(u32));
        CHECK(tags != NULL);
        for (u32 index = 0; index < HM_GPU_LINK_CAPACITY; ++index) {
            CHECK(hm_gpu_link_encode(&tags[index]) == index + 1);
        }
        CHECK(hm_gpu_link_encode(tags) == 1); /* Existing IDs still work at capacity. */
        CHECK(hm_gpu_link_decode(HM_GPU_LINK_CAPACITY) == &tags[HM_GPU_LINK_CAPACITY - 1]);
        hm_gpu_link_encode(&tags[HM_GPU_LINK_CAPACITY]);
    }
    return 2; /* Every recognized rejection must abort, not return success/error. */
}

static int hm_test_original_ot_insert(void) {
    hm_gpu_links_reset();
    region_base = hm_work_region();
    frame_buffers_layout();
    current_frame = &frame_records[0];
    u32 *first = current_frame->primCursor.tag;
    setlen(first, 3);
    CHECK(ot_insert_prim(FRAME_OT_DEPTH + 100, 16) == first);
    CHECK(hm_gpu_link_decode(current_frame->ot[FRAME_OT_DEPTH - 1]) == first);
    CHECK(first[0] >> 24 == 3);
    CHECK(hm_gpu_link_decode(first[0]) == &current_frame->ot[FRAME_OT_DEPTH - 2]);
    CHECK(current_frame->primCursor.bytes == (u8 *)first + 16);
    u32 *second = current_frame->primCursor.tag;
    setlen(second, 1);
    CHECK(ot_insert_prim(1, 8) == second);
    CHECK(hm_gpu_link_decode(current_frame->ot[1]) == second);
    CHECK(current_frame->primCursor.bytes == (u8 *)first + 24);

    const u32 *node = &current_frame->ot[FRAME_OT_DEPTH - 1];
    u32 visited = 0;
    while (node != NULL) {
        CHECK(++visited <= FRAME_OT_DEPTH + 2);
        node = hm_gpu_link_decode(*node);
    }
    CHECK(visited == FRAME_OT_DEPTH + 2);
    u32 handle = hm_gpu_link_encode(first);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    CHECK(hm_gpu_link_encode(first) == handle); /* Re-clearing does not reset cached links. */
    CHECK(hm_gpu_link_decode(current_frame->ot[FRAME_OT_DEPTH - 1])
          == &current_frame->ot[FRAME_OT_DEPTH - 2]);
    hm_gpu_links_reset();
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        return hm_test_rejection(argv[1]);
    }
    CHECK(argc == 1);
    CHECK(hm_test_addresses() == 0);
    CHECK(hm_test_chain() == 0);
    CHECK(hm_test_builders() == 0);
    CHECK(hm_test_original_ot_insert() == 0);
    return 0;
}
