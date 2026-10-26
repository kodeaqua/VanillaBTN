/* Test real overlay state restoration without starting the game. */
#include "../../src/overlay/27_GF_TITLE/title.c"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    const s16 initial_load = title_load_state;
    const s16 initial_menu = title_menu_page;
    TitleBlockEntry first_block = title_block_entries[0];
    FarmStoredItem first_tool = title_new_game_toolbox[0];
    hm_title_overlay_capture();
    for (int visit = 0; visit < 5; ++visit) {
        CHECK(hm_overlay_select(27, 0) == 0 && hm_overlay_active(0) == 27);
        CHECK(title_load_state == initial_load && title_menu_page == initial_menu);
        CHECK(title_step_fn == title_logo_begin && title_logo_slide == -160);
        CHECK(title_drop_count == 48 && title_logo_wipe == 320);
        CHECK(title_idle_frames == 0 && title_sprite_file[222] == 0);
        CHECK(title_birth_text[19] == 0 && title_image_ptrs[0] == (TitleImage *)title_image_0);
        CHECK(!memcmp(&title_block_entries[0], &first_block, sizeof(first_block)));
        CHECK(!memcmp(&title_new_game_toolbox[0], &first_tool, sizeof(first_tool)));
        title_load_state = 99;
        title_menu_page = 99;
        title_step_fn = NULL;
        title_logo_slide = 88;
        title_drop_count = 88;
        title_logo_wipe = 88;
        title_idle_frames = 88;
        title_sprite_file[222] = 88;
        title_birth_text[19] = 88;
        title_image_ptrs[0] = NULL;
        memset(&title_block_entries[0], 0, sizeof(first_block));
        memset(&title_new_game_toolbox[0], 0, sizeof(first_tool));
        hm_title_overlay_capture(); /* A repeated init must not capture dirty state. */
    }
    hm_overlay_reset();
    CHECK(hm_overlay_select(27, 0) == 0 && title_step_fn == title_logo_begin);
    return 0;
}
