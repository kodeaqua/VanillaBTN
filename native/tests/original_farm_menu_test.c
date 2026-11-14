#include "farm_state.h"
#include "family.h"
#include "farm_init.h"
#include "farm_main.h"
#include "house.h"
#include "options_menu.h"
#include "pad.h"
#include "screen_fade.h"
#include "sdk_gpu.h"
#include "sound.h"
#include "sound_request.h"
#include "status_pages.h"
#include "swind.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

extern AnimState status_cursor_sprite;
extern AnimState status_mark_sprite;
extern u8 cooking_menu_step;
extern void (*farm_menu_handler)(void);
extern void (*farm_menu_handler_last)(void);
extern s32 main_scene;
extern s32 farm_startup_phase;
extern FrameState *current_frame;

static FrameState dummy_frame;
static InputRecord dummy_pad;
static Ps1Video video;
static u8 staging_storage[0x10000];

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

    /* 2. Farm event mode transitions to minigames, staff roll, save, load, and debug */
    farm_state_init();
    sound_fader_init();

    farm_area.mode.eventMode = FARM_MODE_MINIGAME_1;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_MG1);

    farm_area.mode.eventMode = FARM_MODE_MINIGAME_2;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_MG2);

    farm_area.mode.eventMode = FARM_MODE_MINIGAME_3;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_MG3);

    farm_area.mode.eventMode = FARM_MODE_MINIGAME_4;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_MG4);

    farm_area.mode.eventMode = FARM_MODE_MINIGAME_5;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_MG5);

    farm_area.mode.eventMode = FARM_MODE_STAFF_ROLL;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_STAFF_ROLL);

    farm_area.mode.eventMode = FARM_MODE_SAVE_SCREEN;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_SAVE);

    farm_area.mode.eventMode = FARM_MODE_LOAD_SCREEN;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_LOAD);

    fade_start_in(0, 0, 0);
    farm_area.mode.eventMode = FARM_MODE_MG_DEBUG_SCREEN;
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_MG_DEBUG);
    CHECK(farm_area.mode.eventMode == FARM_MODE_IDLE);

    farm_area.mode.eventMode = FARM_MODE_DEMO_END;
    farm_run_on_scratchpad();
    CHECK(farm_area.mode.eventMode == FARM_MODE_TO_TITLE);
    fade_start_in(0, 0, 0);
    farm_run_on_scratchpad();
    CHECK(main_scene == SCENE_TITLE);

    /* 3. Minigame return flow: farm_apply_mode_flags and farm_enter_dispatch */
    main_request_scene(SCENE_MG1);
    main_request_scene(SCENE_FARM_MINIGAME_EXIT);
    game_state.progress.entryStep = 4;
    farm_enter_dispatch();
    CHECK((game_state.statusFlags & STATUS_FLAG_FROM_MG1) != 0);
    CHECK((game_state.statusFlags & (STATUS_FLAG_FROM_MG2 | STATUS_FLAG_FROM_MG3 |
                                     STATUS_FLAG_FROM_MG4 | STATUS_FLAG_FROM_MG5)) == 0);
    CHECK(game_state.progress.entryStep == 1);
    CHECK(farm_startup_phase == FARM_STARTUP_LAYOUT);

    main_request_scene(SCENE_MG2);
    main_request_scene(SCENE_FARM_MINIGAME_EXIT);
    game_state.progress.entryStep = 4;
    farm_enter_dispatch();
    CHECK((game_state.statusFlags & STATUS_FLAG_FROM_MG2) != 0);
    CHECK((game_state.statusFlags & (STATUS_FLAG_FROM_MG1 | STATUS_FLAG_FROM_MG3 |
                                     STATUS_FLAG_FROM_MG4 | STATUS_FLAG_FROM_MG5)) == 0);
    CHECK(game_state.progress.entryStep == 1);
    CHECK(farm_startup_phase == FARM_STARTUP_LAYOUT);

    main_request_scene(SCENE_MG3);
    main_request_scene(SCENE_FARM_MINIGAME_EXIT);
    game_state.progress.entryStep = 4;
    farm_enter_dispatch();
    CHECK((game_state.statusFlags & STATUS_FLAG_FROM_MG3) != 0);
    CHECK(game_state.progress.entryStep == 1);
    CHECK(farm_startup_phase == FARM_STARTUP_LAYOUT);

    main_request_scene(SCENE_MG4);
    main_request_scene(SCENE_FARM_MINIGAME_EXIT);
    game_state.progress.entryStep = 4;
    farm_enter_dispatch();
    CHECK((game_state.statusFlags & STATUS_FLAG_FROM_MG4) != 0);
    CHECK(game_state.progress.entryStep == 1);
    CHECK(farm_startup_phase == FARM_STARTUP_LAYOUT);

    main_request_scene(SCENE_MG5);
    main_request_scene(SCENE_FARM_MINIGAME_EXIT);
    game_state.progress.entryStep = 4;
    farm_enter_dispatch();
    CHECK((game_state.statusFlags & STATUS_FLAG_FROM_MG5) != 0);
    CHECK(game_state.progress.entryStep == 1);
    CHECK(farm_startup_phase == FARM_STARTUP_LAYOUT);

    /* 4. Staff roll return scene dispatch */
    main_request_scene(SCENE_STAFF_ROLL);
    main_request_scene(SCENE_FARM_STAFF_EXIT);
    CHECK(main_scene == SCENE_FARM_STAFF_EXIT);
    CHECK(main_get_previous_scene() == SCENE_STAFF_ROLL);

    /* 5. In-game debug minigame settings window (swind.c) */
    sdk_gpu_bind(&video);
    region_base = staging_storage;
    input_pad1 = &dummy_pad;
    current_frame = &dummy_frame;

    /* Frame 1: SWIND_STATE_OPEN -> SWIND_STATE_FONT */
    swind_window_tick();

    /* Frame 2: SWIND_STATE_FONT -> SWIND_STATE_RUN */
    swind_window_tick();

    /* Frame 3: In SWIND_STATE_RUN, press START to request close */
    fade_start_in(0, 0, 0);
    dummy_pad.pressed = PAD_START;
    swind_window_tick();

    /* Frame 4: Wait for fade to settle -> SWIND_STATE_CLOSE */
    dummy_pad.pressed = 0;
    fade_start_in(0, 0, 0);
    swind_window_tick();

    /* Frame 5: SWIND_STATE_CLOSE -> restores screen and requests SCENE_FARM */
    swind_window_tick();
    CHECK(main_scene == SCENE_FARM);

    sdk_gpu_bind(NULL);

    /* 6. Animal pickup / action begin 64-bit FarmActor pointer integrity check */
    farm_tables_register(0);
    FarmPet test_dog;
    memset(&test_dog, 0, sizeof(test_dog));
    test_dog.actor.kind = ITEM_ADULT_DOG;
    test_dog.actor.sprite.partLists = (AnimPartList *)0x1234567812345678ULL;
    test_dog.actor.cue.id = 42;
    farm_action_begin((struct FarmActionSlot *)&test_dog.actor);
    CHECK((uintptr_t)test_dog.actor.sprite.partLists == 0x1234567812345678ULL);
    CHECK(test_dog.actor.cue.id == -1);

    return 0;
}
