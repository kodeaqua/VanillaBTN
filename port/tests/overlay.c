#include "overlay/registry.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    hm_overlay_reset();
    if (argc == 2) {
        if (!strcmp(argv[1], "unknown"))
            hm_overlay_select(0, 0);
        if (!strcmp(argv[1], "region"))
            hm_overlay_select(27, 1);
        if (!strcmp(argv[1], "reload")) {
            hm_overlay_select(27, 0);
            hm_overlay_select(27, 0);
        }
        return 2;
    }
    CHECK(hm_overlay_active(0) == -1 && hm_overlay_active(1) == -1);
    const s32 members[] = {8, 10, 11, 15, 17, 19, 21, 23, 25, 27, 28, 30, 31};
    for (u32 i = 0; i < 13; ++i) {
        s32 region = members[i] == 15 || members[i] == 30;
        CHECK(hm_overlay_select(members[i], region) == 0);
        CHECK(hm_overlay_active(region) == members[i]);
    }
    CHECK(hm_overlay_active(0) == 31 && hm_overlay_active(1) == 30);
    return 0;
}
