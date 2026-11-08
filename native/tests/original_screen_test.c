#include "sdk_gpu.h"
#include "debug_stubs.h"
#include "farm_data_size.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "main.h"
#include "screen_fade.h"
#include "shop_restock.h"
#include "text_bank.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

/* Main-loop state and controls are fixture boundaries. Fade state, counters,
 * packet storage and builders all come from the actual screen_fade.c. */
FrameState frame_records[2];
FrameState *current_frame = frame_records;
FarmState game_state;
extern Recipe recipes[RECIPE_COUNT];
extern u32 picture_codes[174][4];
extern u16 horse_names[253];
extern TextBank text_bank;
s32 frame_index;
static u_long ot[2];
u_long *active_ot = ot;
InputRecord *input_pad1;
u8 main_get_speed_mode(void) { return MAIN_SPEED_FAST_2; }
void main_set_clear_screen(s32 value) { (void)value; }
void main_set_put_env(s32 value) { (void)value; }
static Ps1Video video;

static void draw_frame(void) {
    sdk_gpu_reset_packets();
    ClearOTagR(ot, 2); /* Native SDK maps a module-owned OT. */
    screen_clear_rect(0, 0, 320, 240, 255, 0, 0);
    screen_fade_frame();
    DrawOTag(ot + 1);
    CHECK(sdk_gpu_ok());
}

int main(void) {
    sdk_gpu_bind(&video);
    DRAWENV draw;
    CHECK(SetDefDrawEnv(&draw, 0, 0, 320, 240));
    CHECK(PutDrawEnv(&draw));
    fade_reset();
    fade_start_out(3, 0, 2);
    CHECK(fade_get_mode() == SCREEN_FADE_OUT && !fade_is_settled());
    draw_frame();
    CHECK(video.vram[10][10] == 24);
    draw_frame();
    CHECK(video.vram[10][10] == 16);
    draw_frame();
    CHECK(video.vram[10][10] == 8);
    draw_frame();
    CHECK(video.vram[10][10] == 0);
    draw_frame();
    CHECK(fade_is_settled());
    fade_start_in(4, 0, 2);
    draw_frame();
    CHECK(video.vram[10][10] == 16);
    draw_frame();
    CHECK(video.vram[10][10] == 24);
    draw_frame();
    CHECK(video.vram[10][10] == 31 && fade_is_settled());

    /* Verify original game tables and shop_restock race-runner generation. */
    CHECK(farm_section_size == 0 && farm_swind_size == 0);
    debug_print_buf();
    CHECK(recipes[0].id == ITEM_MAYONNAISE_S);
    CHECK(recipes[RECIPE_COUNT - 1].id == ITEM_FOOD_FIASCO &&
          recipes[RECIPE_COUNT - 1].quality == 246);
    CHECK(picture_codes[0][0] == 02122412222u);
    CHECK(HORSE_NAME(0)[0] == 0x0007 && HORSE_NAME(0)[6] == 0xFFFF);
    CHECK(text_bank.messages.prompt_yes_no[0] == 0x0FFD &&
          text_bank.messages.prompt_yes_no[8] == 0xFFFF);

    memset(&game_state, 0, sizeof(game_state));
    rand_seed = 0x12345678u;
    game_state.raceBets.race = 0;
    shop_restock();
    u8 seen_horses[40] = {0};
    for (unsigned group = 0; group < BET_RACES; ++group) {
        for (unsigned row = 0; row < BET_RUNNERS; ++row) {
            s16 runner = game_state.raceBets.rows[group][row].runner;
            CHECK(runner >= 0 && runner < 40);
            CHECK(!seen_horses[runner]);
            seen_horses[runner] = 1;
            CHECK(game_state.raceBets.rows[group][row].odds >= 1);
        }
    }

    memset(&game_state, 0, sizeof(game_state));
    game_state.raceBets.race = BET_RACE_HORSE;
    game_state.animals.horse.training = 210;
    game_state.animals.horse.actor.affection = 180;
    shop_restock();
    CHECK(game_state.raceBets.rows[2][2].runner == -1);
    CHECK(game_state.raceBets.rows[2][2].chance == 0 &&
          game_state.raceBets.rows[2][2].rivalChance == 0);
    CHECK(game_state.raceBets.rows[2][2].training == 210 &&
          game_state.raceBets.rows[2][2].affection == 180);

    sdk_gpu_bind(NULL);
    return 0;
}
