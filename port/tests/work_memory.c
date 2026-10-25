/* Include, rather than copy, the original TU so its static setup functions can
 * be exercised without exporting test hooks from production game code. */
#define main static hm_test_unexecuted_game_entry
#include "../../src/main/main.c"
#undef main

#include <stdio.h>
#include <stdlib.h>
#include "psx/unimplemented.h"

_Static_assert(REGION_BYTES == HM_FRAME_WORK_BYTES, "Native workspace follows the source capacity");
_Static_assert(FARM_IMAGE_SIZE == HM_FARM_IMAGE_BYTES, "Native image window follows the source");

#define CHECK(condition)                                                                            \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/* Test-only GPU/GS observations. They record setup requests, not OT encoding or
 * rendering. No production target links these definitions. */
static struct {
    u32 *table;
    s32 entries;
} hm_test_clears[8];
static u32 hm_test_clear_count;
static void *hm_test_gs_work;

u32 *ClearOTagR(u32 *table, s32 entries) {
    if (hm_test_clear_count >= 8 || table == NULL || entries <= 0) {
        abort();
    }
    hm_test_clears[hm_test_clear_count].table = table;
    hm_test_clears[hm_test_clear_count].entries = entries;
    ++hm_test_clear_count;
    return table;
}

void GsSetWorkBase(void *base) {
    hm_test_gs_work = base;
}

void *GsGetWorkBase(void) {
    return hm_test_gs_work;
}

s32 host_poll(void) {
    hm_unimplemented("unexpected work-memory test host_poll");
}

void SystemError(s32 code, s32 detail) {
    (void)code;
    (void)detail;
    hm_unimplemented("unexpected work-memory test SystemError");
}

static int hm_test_frame_span(const void *pointer, size_t bytes) {
    uintptr_t offset = (uintptr_t)pointer - (uintptr_t)hm_work_region();
    return offset <= HM_FRAME_WORK_BYTES && bytes <= HM_FRAME_WORK_BYTES - offset;
}

static int hm_test_frame_setup(s32 wide) {
    u8 *base = hm_work_region();
    base[-1] = 0x6B;
    base[HM_FRAME_WORK_BYTES - 1] = 0xFF;
    hm_test_clear_count = 0;
    if (wide) {
        main_init_frame_records_wide();
    } else {
        main_init_frame_records();
    }
    CHECK(region_base == base && main_get_region_base() == base);
    CHECK(base[-1] == 0x6B && base[HM_FRAME_WORK_BYTES - 1] == 0);
    CHECK(hm_test_clear_count == 2);
    CHECK(frame_ot_base_init == base + (wide ? REGION_BYTES - 8 : REGION_FRAME_OT_BASE));
    u32 *initial_tables[2] = {hm_test_clears[0].table, hm_test_clears[1].table};
    s32 initial_lengths[2] = {hm_test_clears[0].entries, hm_test_clears[1].entries};
    for (s32 index = 0; index < 2; ++index) {
        FrameState *frame = &frame_records[index];
        CHECK(initial_tables[index] == frame->ot);
        CHECK(initial_lengths[index] == FRAME_OT_DEPTH);
        CHECK(frame->otDepth == FRAME_OT_DEPTH && frame->otDepthBits == 11);
        CHECK(frame->primSize == FRAME_PRIM_SIZE);
        CHECK(frame->primCursor.bytes == frame->primStart);
        CHECK(frame->primBuffer == frame->primStart);
        CHECK(hm_test_frame_span(frame->ot, FRAME_OT_DEPTH * sizeof(u32)));
        CHECK(hm_test_frame_span(frame->primStart, frame->primSize));
        CHECK(hm_test_frame_span(frame->workBase, frame->workSize));
        CHECK(frame->workSize == (u32)(wide ? 0x5EDC : 0x2EE0));
        for (s32 layer = 0; layer < 3; ++layer) {
            CHECK(hm_test_frame_span(frame->otTags[layer], frame->otTagCounts[layer] * sizeof(u32)));
            CHECK(frame->ots[layer].org == frame->otTags[layer]);
            CHECK(frame->ots[layer].tag == frame->otTags[layer] + frame->otTagCounts[layer] - 1);
        }
        CHECK(frame->otTags[3] == NULL && frame->otTagCounts[3] == 0);
        current_frame = frame;
        frame_index = index;
        active_ot = frame_ots;
        hm_test_clear_count = 0;
        hm_test_gs_work = NULL;
        main_frame_begin();
        CHECK(hm_test_gs_work == frame->workBase);
        CHECK(frame_ot_base == base + REGION_FRAME_OT_BASE + index * 0x3000);
        CHECK(hm_test_frame_span(frame_ot_base, 0x3000));
        CHECK(hm_test_clear_count == 5);
        CHECK(hm_test_clears[0].table == active_ot && hm_test_clears[0].entries == 2);
        CHECK(hm_test_clears[4].table == frame->ot);
        CHECK(gs_work_alloc(frame->workSize) == frame->workBase);
        CHECK(hm_test_gs_work == frame->workBase + frame->workSize);
        gs_work_release(frame->workBase);
        CHECK(hm_test_gs_work == frame->workBase);
    }
    return 0;
}

int main(void) {
    CHECK(hm_test_frame_setup(0) == 0);
    farm_state_init();
    CHECK(farm_area.loadWindow == hm_farm_load_window());
    CHECK(farm_area.sectionBuffer == farm_area.loadWindow + FARM_IMAGE_SIZE);
    CHECK((u8 *)game_state.demo + sizeof(InputDemo) == region_base);
    CHECK((u8 *)game_state.demo >= farm_area.sectionBuffer);
    CHECK((uintptr_t)game_state.demo % _Alignof(InputDemo) == 0);
    game_state.demo->frame = 123;
    game_state.demo->recording.inputs[DEMO_INPUTS - 1] = 0xBEEF;
    CHECK(hm_test_frame_setup(1) == 0);
    CHECK(game_state.demo->frame == 123);
    CHECK(game_state.demo->recording.inputs[DEMO_INPUTS - 1] == 0xBEEF);
    farm_section_size = 0;
    CHECK(main_region_free_bytes() == HM_FARM_SECTION_CAPACITY);
    CHECK(main_region_free_start() == farm_area.sectionBuffer);
    farm_section_size = 17;
    CHECK(main_region_free_bytes() == HM_FARM_SECTION_CAPACITY - 17);
    CHECK(main_region_free_start() == farm_area.sectionBuffer + 17);
    farm_section_size = HM_FARM_SECTION_CAPACITY;
    CHECK(main_region_free_bytes() == 0 && main_region_free_start() == region_base);
    farm_section_size += 1;
    CHECK(main_region_free_bytes() == -1); /* Preserve the signed budget; do not clamp. */
    return 0;
}
