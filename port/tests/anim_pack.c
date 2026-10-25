#include "farm_data.h"
#include <stdio.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
_Static_assert(sizeof(AnimPackToc) == 4, "animation TOC prefix size");
_Static_assert(__builtin_offsetof(AnimPackToc, start) == 4, "animation TOC tail offset");
static AnimPackToc synthetic = {2, {12, 24, 48}};
int main(void) {
    CHECK(ANIM_PACK_START(&synthetic, 0) == 12 && ANIM_PACK_START(&synthetic, 2) == 48);
    CHECK(farm_slot_tocs[6]->count == 5);
    CHECK(ANIM_PACK_START(farm_slot_tocs[6], 0) == 0);
    CHECK(ANIM_PACK_START(farm_slot_tocs[6], 1) == 1008);
    CHECK(ANIM_PACK_START(farm_slot_tocs[6], 5) == 5376);
    CHECK(farm_item_tocs[2]->count == 43 && ANIM_PACK_START(farm_item_tocs[2], 43) == 6752);
    CHECK(farm_pack_tocs[17]->count == 18 && ANIM_PACK_START(farm_pack_tocs[17], 18) == 18456);
    return 0;
}
