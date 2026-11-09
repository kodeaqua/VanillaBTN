#include "farm_state.h"
#include "family.h"
#include "house.h"
#include "options_menu.h"
#include "pad.h"
#include "status_pages.h"
#include "main.h"
#include "sdk_gpu.h"
#include "sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

extern AnimState status_cursor_sprite;
extern AnimState status_mark_sprite;
extern u8 cooking_menu_step;
extern void (*farm_menu_handler)(void);
extern void (*farm_menu_handler_last)(void);

static FrameState dummy_frame;
static InputRecord dummy_pad;

void boot_fault_install(void (*hook)(void));

int main(void) {
    boot_fault_install(NULL);
    memset(&game_state, 0, sizeof(game_state));

    /* 1. Family table lookup: on x64 host, must return valid pointers to SJIS strings */
    static const u16 wife_first_chars[5] = {0x000A, 0x0000, 0x0004, 0x000C, 0x000F};
    for (s16 i = 0; i < 5; i++) {
        game_state.family.wifeIndex = i;
        uintptr_t ptr = family_table_lookup();
        CHECK(ptr != 0);
        const u16 *name = (const u16 *)ptr;
        CHECK(name[0] == wife_first_chars[i]);
        int len = 0;
        while (name[len] != 0xFFFF && len < 32) len++;
        CHECK(name[len] == 0xFFFF && len > 0);
    }
    game_state.family.wifeIndex = -1;
    CHECK(family_table_lookup() == 0);
    game_state.family.wifeIndex = 5;
    CHECK(family_table_lookup() == 0);
    game_state.family.wifeIndex = 100;
    CHECK(family_table_lookup() == 0);

    return 0;
}
