/* Test native backing sizes without exporting private game globals. */
#include "../../src/overlay/27_GF_TITLE/title.c"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

int main(void) {
    CHECK(sizeof(title_image_0) == sizeof(TitleImage));
    CHECK(sizeof(title_image_1) == sizeof(TitleImage));
    CHECK(sizeof(title_image_2) == sizeof(TitleImage));
    static const unsigned lengths[3] = {0x1358,0x1068,0x1628};
    static const unsigned hashes[3] = {3141558908u,1780012554u,1565562257u};
    struct { unsigned before; TitleImage image; unsigned after; } target;
    for (unsigned i = 0; i < 3; i++) {
        target.before = 0x12345678; target.after = 0x87654321;
        target.image = *title_image_ptrs[i];
        CHECK(target.before == 0x12345678 && target.after == 0x87654321);
        unsigned hash = 2166136261u;
        for (unsigned j = 0; j < lengths[i]; j++) hash = (hash ^ target.image.bytes[j]) * 16777619u;
        CHECK(hash == hashes[i]);
        for (unsigned j = lengths[i]; j < sizeof(TitleImage); j++) CHECK(target.image.bytes[j] == 0);
    }
    return 0;
}
