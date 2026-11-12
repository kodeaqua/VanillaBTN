#include "debug.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "heart.h"
#include "item_storage.h"
#include "main.h"
#include "mcard.h"
#include "menu_window.h"
#include "mg1.h"
#include "mg2.h"
#include "mg4.h"
#include "name_entry.h"
#include "pad.h"
#include "pdasamp.h"
#include "picture_cells.h"
#include "screen_fade.h"
#include "sdk_card.h"
#include "sdk_gpu.h"
#include "sdk_pad.h"
#include "sdk_sync.h"
#include "sdk_work.h"
#include "sound_request.h"
#include "staff.h"
#include "text.h"
#include "title.h"
#include "window_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

static uint64_t sync_clock_time;
static uint64_t sync_now(void *context) { (void)context; return sync_clock_time; }
static void sync_wait_until(uint64_t deadline, void *context) { (void)context; sync_clock_time = deadline; }

static u8 demo_and_region_storage[sizeof(InputDemo) + 0x40000];
u8 *region_base = demo_and_region_storage + sizeof(InputDemo);
s32 frame_index;
FrameState frame_records[2];
FrameState *current_frame = &frame_records[0];
u_long *active_ot;
InputRecord input_records[2];
InputRecord *input_pad1 = input_records;
InputRecord *input_pad2 = input_records + 1;
VabStream vab_stream_copy;
static s32 requested_scene = -1;

u8 *main_get_region_base(void) { return region_base; }
u8 main_get_speed_mode(void) { return MAIN_SPEED_NORMAL; }
void main_set_put_env(s32 value) { (void)value; }
void main_set_clear_screen(s32 value) { (void)value; }
void main_request_scene(s32 scene) { requested_scene = scene; }
s32 estate_gold_change(s32 amount, s32 reset) {
    (void)reset;
    game_state.player.gold += amount;
    return 0;
}
int scene_pan_is_held(void) { return 0; }
int scene_tilt_is_held(void) { return 0; }
void farm_unpause(void) {}

static Ps1Video video;
static u_long work[8192], ot[64], main_ot[FRAME_OT_DEPTH], active_ot_storage[4];
static FrameState frame;
static InputRecord fake_pad;

typedef struct IconFixture {
    AnimFile file;
    AnimImage images[2];
    AnimPartList lists[1];
    AnimPart parts[1];
    AnimRange ranges[16];
    AnimFrame frames[2];
} IconFixture;

static IconFixture icon_fixture;

static void init_icon_fixture(void) {
    memset(&icon_fixture, 0, sizeof(icon_fixture));
    icon_fixture.file.magic = 0xE;
    icon_fixture.file.section[0] = (s32)offsetof(IconFixture, images);
    icon_fixture.file.section[1] = (s32)offsetof(IconFixture, lists);
    icon_fixture.file.section[2] = (s32)offsetof(IconFixture, parts);
    icon_fixture.file.section[3] = (s32)offsetof(IconFixture, ranges);
    icon_fixture.file.section[4] = (s32)offsetof(IconFixture, frames);
    icon_fixture.images[0] = (AnimImage){64, 0, 2, 8};
    icon_fixture.images[1] = (AnimImage){0, 240, 16, 1};
    icon_fixture.lists[0].count = 1;
    icon_fixture.parts[0].image = 0;
    icon_fixture.parts[0].palette = 1;
    for (int i = 0; i < 16; ++i) {
        icon_fixture.ranges[i].frames = 2;
        icon_fixture.ranges[i].offset = 0;
    }
    icon_fixture.frames[0].duration = 4;
    icon_fixture.frames[1].duration = 4;
    memcpy(farm_area.stageData, &icon_fixture, sizeof(icon_fixture));
    anim_bind_file(&menu_anim, (AnimFile *)farm_area.stageData);
}

int main(void) {
    CHECK(sdk_sync_bind(sync_now, sync_wait_until, NULL));
    sdk_gpu_bind(&video);
    CHECK(sdk_gpu_map(work, sizeof(work)));
    CHECK(sdk_work_bind(work, sizeof(work)));
    ClearOTagR(ot, 32);
    window_frame_draw(ot, 20, 20, 40, 30);
    DrawOTag(ot + 31);
    CHECK(sdk_gpu_ok());
    CHECK(video.vram[20][22] == 0x6BBF); /* Original light upper edge. */
    CHECK(video.vram[22][59] == 0x2237); /* Original dark right edge. */
    CHECK(video.vram[22][22] == 0x5F7E); /* Original gradient's first row. */
    CHECK(video.vram[45][22] != 0 && video.vram[45][22] != video.vram[22][22]);
    CHECK(video.vram[19][22] == 0 && video.vram[50][22] == 0);

    /* Exercise text formatting, seasons, weekdays, list append, and matching. */
    u16 num_buf[8];
    u16 *end = text_format_number(42, num_buf, 4);
    *end = TEXT_END;
    CHECK(end - num_buf == 4);
    CHECK(num_buf[0] == TEXT_CODE_SPACE && num_buf[1] == TEXT_CODE_SPACE);
    CHECK(num_buf[2] == 0x44 && num_buf[3] == 0x42);

    u16 season_buf[8];
    end = text_write_season(0, season_buf);
    *end = TEXT_END;
    CHECK(end - season_buf == 6);

    u16 weekday_buf[8];
    end = text_write_weekday(1, weekday_buf);
    *end = TEXT_END;
    CHECK(end - weekday_buf == 3);

    u16 list_a[16] = {0x0000, 0x0001, TEXT_END};
    u16 list_b[8] = {0x0002, TEXT_END};
    u16 *list_end = text_list_append(list_a, list_b);
    CHECK(list_end - list_a == 3 && list_a[2] == 0x0002 && list_a[3] == TEXT_END);
    CHECK(text_match_either(list_a, list_a, list_b) == 0);

    /* Exercise DR_AREA clipping and text_window_draw typing/close lifecycle. */
    init_icon_fixture();
    memset(&frame, 0, sizeof(frame));
    SetDefDrawEnv(&frame.drawEnv, 0, 0, 320, 240);
    frame.otTags[0] = ot;
    current_frame = &frame;
    input_pad1 = &fake_pad;

    u16 msg[] = {0x0000, 0x0001, TEXT_END};
    text_reset();
    text_set_speed(0);
    ClearOTagR(ot, 32);
    CHECK(text_window_draw(&ot[2], msg, 30, 168, 0, 48) == TEXT_WINDOW_PRINTING);
    CHECK(text_window_draw(&ot[2], msg, 30, 168, 0, 48) == TEXT_WINDOW_PRINTING);
    CHECK(text_window_draw(&ot[2], msg, 30, 168, 0, 48) == TEXT_WINDOW_PRINTING);
    CHECK(text_window_draw(&ot[2], msg, 30, 168, 0, 48) == TEXT_WINDOW_END_WAIT);
    fake_pad.pressed = PAD_CROSS;
    CHECK(text_window_draw(&ot[2], msg, 30, 168, 0, 48) == TEXT_WINDOW_CLOSED);
    fake_pad.pressed = 0;

    /* Dialogue, menus and event text share insert expansion. An absent optional
     * animal or unknown archive insert must not index past the source table. */
    game_state.animals.slots[0].animal = -1;
    u16 missing_insert[] = {0x4FFF, 0x4016, 0x0001, TEXT_END};
    text_reset();
    ClearOTagR(ot, 32);
    text_draw_plain(&ot[2], missing_insert, 30, 168);
    CHECK(text_window_draw(&ot[2], missing_insert, 30, 168, 0, 48) == TEXT_WINDOW_PRINTING);
    CHECK(text_window_draw(&ot[2], missing_insert, 30, 168, 0, 48) == TEXT_WINDOW_PRINTING);

    DrawOTag(ot + 31);
    CHECK(sdk_gpu_ok());
    input_pad1 = input_records;

    /* Exercise menu_box_animate, heart_icon_draw, name_entry_start, and name_entry_step. */
    CHECK(sdk_work_bind(work, sizeof(work)));
    s32 box_frame = 0;
    ClearOTagR(ot, 32);
    CHECK(menu_box_animate(&ot[4], 0, 26, 164, 267, 62, &box_frame, 4, 0) == 0);
    CHECK(box_frame == 1);
    heart_icon_draw();

    u16 entered_name[9];
    name_entry_start(entered_name);
    CHECK(name_entry_step() == NAME_STEP_EDITING);
    DrawOTag(ot + 31);
    CHECK(sdk_gpu_ok());

    /* Exercise picture_cells.c and item_storage.c helpers with registered farm tables. */
    farm_state_init();
    CHECK(item_buy_price(ITEM_TURNIP_SEEDS) == 120);
    CHECK(item_sell_price(ITEM_TURNIP) == 60);
    CHECK(item_is_seed(ITEM_TURNIP_SEEDS) == 1 && item_is_seed(ITEM_TURNIP) == 0);
    CHECK(crop_grown_stage(ITEM_TURNIP_SEEDS) > 0 && crop_season(ITEM_TURNIP_SEEDS) == 0);
    CHECK(crop_harvest_item(ITEM_TURNIP_SEEDS) == ITEM_TURNIP);
    CHECK(item_villager_reaction(ITEM_SICKLE, 0) == -2);
    CHECK(item_villager_reaction(ITEM_TURNIP, 0) >= 0);

    game_state.player.maxStamina = 100;
    game_state.player.stamina = 50;
    game_state.player.fatigue = 10;
    game_state.player.happiness = 20;
    CHECK(farm_clamp_status(80, 15, 30, 0) == 1);
    CHECK(game_state.player.stamina == 100 && game_state.player.fatigue == 25 &&
          game_state.player.happiness == 50);
    CHECK(farm_adjust_stamina_fatigue(15, -5) == 1);
    CHECK(game_state.player.maxStamina == 115 && game_state.player.stamina == 115);
    CHECK(game_state.player.fatigue == 20 && game_state.progress.estate.powerBerries == 1);
    s32 coin_out = -1;
    CHECK(item_use_coin(50, 0, 0, &coin_out) == 1 && coin_out == 0 && game_state.player.gold == 50);

    picture_grid_clear();
    CHECK(picture_slot_claim(ITEM_TURNIP, PICTURE_ROW_PICKUP) == 0);
    CHECK(farm_area.mode.pictureIds[PICTURE_ROW_PICKUP][0] == ITEM_TURNIP);
    picture_release_item(ITEM_TURNIP);
    CHECK(farm_area.mode.pictureIds[PICTURE_ROW_PICKUP][0] == 0);
    item_picture_slots_clear();
    item_picture_slots_unmark();

    /* Exercise 27_GF_TITLE/title.c and 28_GF_STAFF/staff.c entry/exit dispatch. */
    CHECK(sdk_gpu_map(main_ot, sizeof(main_ot)));
    CHECK(sdk_gpu_map(active_ot_storage, sizeof(active_ot_storage)));
    ClearOTagR(active_ot_storage, 2);
    active_ot = active_ot_storage;
    frame.ot = main_ot;
    frame.otDepth = FRAME_OT_DEPTH;

    game_state.globalFlags = 0;
    game_state.progress.entryStep = 0;
    title_step_dispatch();
    CHECK(game_state.progress.entryStep == 1);
    CHECK(game_state.player.gold == 500 && game_state.player.stamina == 100);
    CHECK(game_state.world.options.stereo == 1 && game_state.world.options.vibration == 1);
    CHECK(game_state.progress.estate.toolboxItems[0].id == ITEM_SICKLE);

    requested_scene = -1;
    game_state.progress.entryStep = 6;
    title_step_dispatch();
    CHECK(game_state.progress.entryStep == 0);
    CHECK(requested_scene == SCENE_LOAD);
    CHECK((game_state.globalFlags & GLOBAL_FLAG_CONTINUE) != 0);

    /* The retail demo grid spans three adjacent PS1 data declarations.
     * Hash is from the code-matching reference overlay's 1125 cell words. */
    game_state.globalFlags = GLOBAL_FLAG_DEMO_START;
    game_state.progress.entryStep = 6;
    title_step_dispatch();
    CHECK(requested_scene == SCENE_FARM);
    uint32_t layout_hash = 2166136261u;
    for (int row = 0; row < FIELD_ROWS; ++row) {
        for (int col = 0; col < FIELD_COLS; ++col) {
            uint16_t value = game_state.fieldTiles[row].rows[col].value;
            layout_hash = (layout_hash ^ (value & 255u)) * 16777619u;
            layout_hash = (layout_hash ^ (value >> 8)) * 16777619u;
        }
    }
    CHECK(layout_hash == 0xf618c92bu);
    game_state.globalFlags = GLOBAL_FLAG_CONTINUE;

    game_state.progress.entryStep = 0;
    staff_tick();
    CHECK(game_state.progress.entryStep == 1);

    requested_scene = -1;
    game_state.ending.route = 0;
    game_state.progress.entryStep = 6;
    staff_tick();
    CHECK(requested_scene == SCENE_FARM_STAFF_EXIT);

    requested_scene = -1;
    game_state.ending.route = 10;
    game_state.progress.entryStep = 6;
    staff_tick();
    CHECK(requested_scene == SCENE_TITLE);

    /* Exercise 08_DEBUG/debug.c jump menu and setup. */
    sound_fader_init();
    frame_records[0].drawEnv.r0 = 0;
    debug_tick();
    CHECK(frame_records[0].drawEnv.r0 == 0x40);
    debug_tick();
    input_pad1->pressed = PAD_CIRCLE;
    requested_scene = -1;
    fade_start_in(0, 0, 0);
    debug_tick();
    CHECK(requested_scene == SCENE_DEBUG);
    input_pad1->pressed = 0;

    /* Exercise minigame step and player actions (17_GF_MG1, 19_GF_MG2, 23_GF_MG4). */
    game_state.progress.entrySubStep = 3;
    mg1_run_state();
    CHECK(game_state.progress.entrySubStep == 4);

    {
        Mg2Player player;
        memset(&player, 0, sizeof(player));
        mg2_player_set_action(&player, 3, 3, 15);
        CHECK(player.state == 3);
        CHECK(player.dir == 3);
        CHECK(player.timer == 15);
        mg2_move_step(&player);
    }

    game_state.progress.entrySubStep = 3;
    mg4_run_step();
    CHECK(game_state.progress.entrySubStep == 4);

    fade_start_in(0, 0, 0);
    sound_fader_init();
    requested_scene = -1;
    game_state.globalFlags = 0;
    mg4_run_step();
    CHECK(game_state.progress.entrySubStep == 0xFF);
    CHECK(requested_scene == SCENE_FARM_MINIGAME_EXIT);
    CHECK((game_state.globalFlags & GLOBAL_FLAG_MINIGAME_EXIT) != 0);

    /* Exercise 31_gf_mcard/mcard.c memory-card utility test screen. */
    Ps1Card *test_card = sdk_card_get(0);
    CHECK(test_card != NULL);
    ps1_card_format(test_card);
    {
        uint8_t dummy_save[8192] = {0};
        CHECK(ps1_card_write_file(test_card, "BISLPS-00000-FARM0", dummy_save, sizeof(dummy_save)) == PS1_CARD_OK);
    }

    /* Initial menu draw */
    input_pad1->pressed = 0;
    mcard_screen_tick();

    /* Probe card type: PAD_CIRCLE */
    input_pad1->pressed = PAD_CIRCLE;
    mcard_screen_tick();
    /* Status should display "Card" */
    input_pad1->pressed = 0;
    mcard_screen_tick();
    /* Clear status line by pressing a button */
    input_pad1->pressed = PAD_TRIANGLE;
    mcard_screen_tick();

    /* Rename save files: PAD_SELECT */
    input_pad1->pressed = PAD_SELECT;
    mcard_screen_tick();
    {
        uint32_t renamed_size = 0;
        CHECK(ps1_card_find_file(test_card, "BISLPS-02489-FARM0", &renamed_size) > 0);
        CHECK(ps1_card_find_file(test_card, "BISLPS-00000-FARM0", &renamed_size) == 0);
    }

    /* Clear status */
    input_pad1->pressed = PAD_TRIANGLE;
    mcard_screen_tick();

    /* List directory: PAD_CROSS */
    input_pad1->pressed = PAD_CROSS;
    mcard_screen_tick();
    input_pad1->pressed = 0;
    mcard_screen_tick();

    /* Clear status */
    input_pad1->pressed = PAD_TRIANGLE;
    mcard_screen_tick();

    /* Unformat card: PAD_TRIANGLE */
    input_pad1->pressed = PAD_TRIANGLE;
    mcard_screen_tick();
    CHECK(test_card->data[0] != 'M' || test_card->data[1] != 'C');

    /* Clear status */
    input_pad1->pressed = PAD_CROSS;
    mcard_screen_tick();

    /* Format card: PAD_SQUARE */
    input_pad1->pressed = PAD_SQUARE;
    mcard_screen_tick();
    CHECK(test_card->data[0] == 'M' && test_card->data[1] == 'C');

    /* Clear status */
    input_pad1->pressed = PAD_CROSS;
    mcard_screen_tick();

    /* Reboot / scene debug request: PAD_START */
    requested_scene = -1;
    input_pad1->pressed = PAD_START;
    mcard_screen_tick();
    CHECK(requested_scene == SCENE_DEBUG);

    /* Exercise 10_PdaSamp/pdasamp.c PDA sample utility screen. */
    input_pad1 = input_records;
    input_pad2 = input_records + 1;
    game_state.globalFlags = 0;
    sdk_pad_sample(0, true, false, PAD_SELECT, 128, 128);
    sdk_pad_sample(1, true, false, 0, 128, 128);
    pad_init();
    pad_reset_records();
    requested_scene = -1;
    CHECK(pda_sample_run() == 0);
    CHECK(requested_scene == SCENE_DEBUG);

    sdk_gpu_bind(NULL);
    CHECK(sdk_work_bind(NULL, 0));
    return 0;
}
