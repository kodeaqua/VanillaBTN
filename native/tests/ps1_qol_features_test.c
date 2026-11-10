#include "ps1_card.h"
#include "sdk_sync.h"
#include "ps1_video.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "Assertion failed at line %d: %s\n", __LINE__, #cond); \
    exit(1); \
} } while (0)

static void test_backup_rotation(void) {
    const char *test_path = "test_save_card.mcr";
    Ps1Card card;
    ps1_card_init(&card);
    ps1_card_format(&card);
    card.present = true;

    /* Clean up any preexisting test artifacts */
    SDL_RemovePath(test_path);
    char buf[560];
    snprintf(buf, sizeof(buf), "%s.bak", test_path); SDL_RemovePath(buf);
    for (int i = 1; i <= 5; ++i) {
        snprintf(buf, sizeof(buf), "%s.bak.%d", test_path, i);
        SDL_RemovePath(buf);
    }

    /* Save 1: Primary created */
    CHECK(ps1_card_save_to_file(&card, test_path));
    SDL_PathInfo info;
    CHECK(SDL_GetPathInfo(test_path, &info));

    /* Save 2: Creates .bak */
    card.dirty = true;
    CHECK(ps1_card_save_to_file(&card, test_path));
    snprintf(buf, sizeof(buf), "%s.bak", test_path);
    CHECK(SDL_GetPathInfo(buf, &info));

    /* Save 3: Rotates .bak -> .bak.1, creates new .bak */
    card.dirty = true;
    CHECK(ps1_card_save_to_file(&card, test_path));
    snprintf(buf, sizeof(buf), "%s.bak.1", test_path);
    CHECK(SDL_GetPathInfo(buf, &info));

    /* Save 4: Rotates .bak.1 -> .bak.2, .bak -> .bak.1 */
    card.dirty = true;
    CHECK(ps1_card_save_to_file(&card, test_path));
    snprintf(buf, sizeof(buf), "%s.bak.2", test_path);
    CHECK(SDL_GetPathInfo(buf, &info));

    /* Verify rotation function edge cases */
    CHECK(!ps1_card_rotate_backups(NULL, 5));
    CHECK(!ps1_card_rotate_backups(test_path, 0));

    /* Clean up */
    SDL_RemovePath(test_path);
    snprintf(buf, sizeof(buf), "%s.bak", test_path); SDL_RemovePath(buf);
    for (int i = 1; i <= 5; ++i) {
        snprintf(buf, sizeof(buf), "%s.bak.%d", test_path, i);
        SDL_RemovePath(buf);
    }
}

static void test_turbo_speed(void) {
    /* Default turbo multiplier is 1 (normal) */
    sdk_sync_set_turbo(1);
    CHECK(sdk_sync_get_turbo() == 1);

    /* Set valid speeds */
    sdk_sync_set_turbo(2);
    CHECK(sdk_sync_get_turbo() == 2);

    sdk_sync_set_turbo(4);
    CHECK(sdk_sync_get_turbo() == 4);

    sdk_sync_set_turbo(8);
    CHECK(sdk_sync_get_turbo() == 8);

    /* Boundary safety: 0 or >8 fall back to 1 */
    sdk_sync_set_turbo(0);
    CHECK(sdk_sync_get_turbo() == 1);

    sdk_sync_set_turbo(100);
    CHECK(sdk_sync_get_turbo() == 1);
}

static void test_extended_draw_distance(void) {
    ps1_video_set_extended_draw_distance(false);
    CHECK(!ps1_video_get_extended_draw_distance());

    float near_z = 0.0f, far_z = 0.0f;
    uint32_t fog_color = 0;
    ps1_video_get_fog_params(&near_z, &far_z, &fog_color);
    CHECK(near_z == 500.0f && far_z == 3000.0f);

    ps1_video_set_extended_draw_distance(true);
    CHECK(ps1_video_get_extended_draw_distance());
    ps1_video_get_fog_params(&near_z, &far_z, &fog_color);
    CHECK(near_z == 1500.0f && far_z == 8000.0f);

    ps1_video_set_extended_draw_distance(false);
    CHECK(!ps1_video_get_extended_draw_distance());
}

static void test_hd_font(void) {
    ps1_video_set_hd_font(false);
    CHECK(!ps1_video_get_hd_font());

    ps1_video_set_hd_font(true);
    CHECK(ps1_video_get_hd_font());

    ps1_video_set_hd_font(false);
    CHECK(!ps1_video_get_hd_font());
}

int main(void) {
    test_backup_rotation();
    test_turbo_speed();
    test_extended_draw_distance();
    test_hd_font();

    printf("ps1_qol_features_test: All HD font, extended draw distance, turbo speed, and auto-backup tests passed.\n");
    return 0;
}
