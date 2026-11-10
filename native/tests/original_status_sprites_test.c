/* Private status sprites layout tested without exporting internal symbols. */
#include "../../src/overlay/11_GF_FARM/menu/status_pages.c"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

int main(void) {
    /* The code-verified PS1 layout had status_cursor_sprite and status_mark_sprite
     * adjacent to status_page_sprites[2] in BSS. On x64 host, status_page_sprite_ptr
     * explicitly maps indices 3 and 4 to these globals. */
    CHECK(status_page_sprite_ptr(0) == &status_page_sprites[0]);
    CHECK(status_page_sprite_ptr(1) == &status_page_sprites[1]);
    CHECK(status_page_sprite_ptr(2) == &status_page_sprites[2]);
    CHECK(status_page_sprite_ptr(3) == &status_cursor_sprite);
    CHECK(status_page_sprite_ptr(4) == &status_mark_sprite);
    return 0;
}
