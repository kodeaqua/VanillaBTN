#include "compat/work_memory.h"

/* Normal C storage for the source's contiguous asset/frame workspace, not a
 * PS1 RAM image. Code, globals, overlays and the heap do not live in this array.
 * Preserve the vanilla capacity between the sector-aligned load window and
 * WORK_REGION; callers must still validate asset lengths before reading them. */
_Alignas(16) static u8 hm_work_storage[
    HM_FARM_IMAGE_BYTES + HM_FARM_SECTION_CAPACITY + HM_FRAME_WORK_BYTES];

u8 *hm_farm_load_window(void) {
    return hm_work_storage;
}

u8 *hm_work_region(void) {
    return hm_work_storage + HM_FARM_IMAGE_BYTES + HM_FARM_SECTION_CAPACITY;
}
