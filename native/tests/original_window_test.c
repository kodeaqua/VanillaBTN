#include "farm_state.h"
#include "farm_tables.h"
#include "heart.h"
#include "main.h"
#include "menu_window.h"
#include "name_entry.h"
#include "pad.h"
#include "sdk_gpu.h"
#include "sdk_work.h"
#include "text.h"
#include "window_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

FarmState game_state;
FarmLoadArea farm_area;
s32 frame_index;
FrameState frame_records[2];
FrameState *current_frame = &frame_records[0];
u_long *active_ot;
InputRecord *input_pad1;
VabStream vab_stream_copy;

u8 main_get_speed_mode(void) { return MAIN_SPEED_NORMAL; }
void main_set_put_env(s32 value) { (void)value; }
void main_set_clear_screen(s32 value) { (void)value; }

void *farm_work_sector_get(void) {
    if (game_state.system.workSectorBusy == 0) {
        game_state.system.workSectorBusy = 1;
        return game_state.work.sector;
    }
    return 0;
}

void farm_work_sector_release(void) {
    game_state.system.workSectorBusy = 0;
}

static Ps1Video video;
static u_long work[8192], ot[32];
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
    DrawOTag(ot + 31);
    CHECK(sdk_gpu_ok());

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

    sdk_gpu_bind(NULL);
    CHECK(sdk_work_bind(NULL, 0));
    return 0;
}
