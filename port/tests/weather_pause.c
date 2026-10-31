/* Verify paused weather animation without advancing the shared gameplay RNG. */
#include "../../src/overlay/11_GF_FARM/world/weather.c"
#include <stdio.h>

FarmState game_state;
FarmLoadArea farm_area;
FrameState weather_test_frame;
FrameState *current_frame = &weather_test_frame;
AnimState ui_icon_anim;
u32 rand_seed;
s32 drift_by_hour[4][3];
s32 drift_by_weather[8][3];

static u32 dummy_ot[FRAME_OT_DEPTH];
static u32 weather_draws;

enum ClockPeriod clock_time_period(s16 hour) {
    (void)hour;
    return CLOCK_PERIOD_DAY;
}

s32 rand_range(s32 range) {
    unsigned int value = rand_seed * 0x41C64E6Du + 12345u;
    rand_seed = value;
    return (s32)(((value >> 16) * (u32)(range & 0xffff)) >> 16);
}

s32 estate_point_on_screen(s32 x, s32 y) {
    (void)x;
    (void)y;
    return 1;
}

void anim_draw_sprt_cursor(u_long *ot, AnimState *sprite, u8 animation, u8 frame, s16 x, s16 y) {
    (void)ot;
    (void)sprite;
    (void)animation;
    (void)frame;
    (void)x;
    (void)y;
    ++weather_draws;
}

void screen_fill_quad(u32 r, u32 g, u32 b) {
    (void)r;
    (void)g;
    (void)b;
}

#ifdef HMBTN_ENH_WIDESCREEN
s32 hm_widescreen_margin(void) { return 0; }
#endif

/* Traps for unrelated weather.c entry points retained by the COFF linker. */
s32 field_gate_open(s16 layer, s16 tile_x, s16 tile_y, s32 unused) {
    (void)layer;
    (void)tile_x;
    (void)tile_y;
    (void)unused;
    return 0;
}
s32 clock_date_index(s16 season, s16 day) {
    (void)season;
    (void)day;
    return 0;
}
s32 event_flag_get(u16 flag) {
    (void)flag;
    return 0;
}
void event_flag_set(u16 flag, u16 on) {
    (void)flag;
    (void)on;
}
void anim_bind_file(AnimState *state, AnimFile *file) {
    (void)state;
    (void)file;
}
void greenhouse_clear_cells(void) {}
void farm_put_back_blocked_animals(void) {}
void estate_trigger_stamp_first(void) {}
void estate_trigger_stamp_second(void) {}

int main(void) {
    static FarmModeScript area;
    current_frame->ot = dummy_ot;
    farm_area.mode.data = &area;
    area.head.place.location = PLACE_FARM;
    game_state.globalFlags = GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_MENU_OPEN;
    game_state.world.weather = WEATHER_RAIN;
    weather_fx_init();

    FarmParticle *rain = &weather_particles[0];
    *rain = (FarmParticle){.x = 80,
                           .y = 100,
                           .sprite = 0x19,
                           .frame = 0,
                           .delay = -1,
                           .age = 0,
                           .flags = PARTICLE_FLAG_MOVING};

    rand_seed = 0x12345678u;
    u32 gameplay_seed = rand_seed;
    weather_fx_frame();
    if (rain->age != 1 || rain->y != 101 || weather_draws != 1) {
        fprintf(stderr, "Paused rain did not advance and draw its moving particle\n");
        return 1;
    }
    if (rand_seed != gameplay_seed) {
        fprintf(stderr, "Paused weather changed the shared gameplay RNG\n");
        return 1;
    }

    /* Reaching the life limit respawns through its private RNG and keeps the
     * gameplay seed intact. The next paused frames resume the new particle. */
    rain->delay = -1;
    rain->age = 110;
    weather_fx_frame();
    if (rand_seed != gameplay_seed || rain->delay < 0 || rain->age != 0) {
        fprintf(stderr, "Paused rain respawn consumed gameplay RNG or failed\n");
        return 1;
    }
    u32 draws_after_respawn = weather_draws;
    for (u32 i = 0; i < 240; ++i)
        weather_fx_frame();
    if (weather_draws <= draws_after_respawn || rand_seed != gameplay_seed) {
        fprintf(stderr, "Paused rain did not keep animating with isolated randomness\n");
        return 1;
    }
    return 0;
}
