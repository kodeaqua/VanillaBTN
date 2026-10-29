#include "enhance/gameplay/gameplay.h"
#include <libgpu.h>
#include <libgte.h>
#include "farm_state.h"
#include "farm_tables.h"
#include "field.h"
#include "main.h"
#include "prim.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FIELD_CELL_SIZE 400
#define LIVESTOCK_ADULT_DAYS 28

typedef struct {
    int walk_through_crops;
    int item_stacking;
    int durable_fences;
    int connect_fences;
    int wild_dog;
    int smart_bell;
    int stamina_hud;
    int calendar_events;
    int roaming_horse;
    int fast_text;
} hm_gameplay_runtime_options;

static hm_gameplay_runtime_options active_options = {
    .walk_through_crops = 0,
    .item_stacking = 0,
    .durable_fences = 0,
    .connect_fences = 0,
    .wild_dog = 1,
    .smart_bell = 0,
    .stamina_hud = 0,
    .calendar_events = 0,
    .roaming_horse = 0,
    .fast_text = 0,
};

int hm_gameplay_walk_through_crops(void) {
    return active_options.walk_through_crops;
}

int hm_gameplay_item_stacking(void) {
    return active_options.item_stacking;
}

int hm_gameplay_durable_fences(void) {
    return active_options.durable_fences;
}

int hm_gameplay_connect_fences(void) {
    return active_options.connect_fences;
}

int hm_gameplay_wild_dog_enabled(void) {
    return active_options.wild_dog;
}

int hm_gameplay_smart_bell(void) {
    return active_options.smart_bell;
}

int hm_gameplay_stamina_hud(void) {
    return active_options.stamina_hud;
}

int hm_gameplay_calendar_events(void) {
    return active_options.calendar_events;
}

int hm_gameplay_roaming_horse(void) {
    return active_options.roaming_horse;
}
int hm_gameplay_fast_text(void) {
    return active_options.fast_text;
}


void hm_gameplay_set_active(const void *options_ptr) {
    if (options_ptr) {
        const hm_gameplay_runtime_options *opt = (const hm_gameplay_runtime_options *)options_ptr;
        active_options = *opt;
        if (!active_options.roaming_horse) {
            hm_gameplay_horse_safety_check();
        }
    }
}

extern s32 estate_point_project(SVECTOR *input, FarmScreenPoint *result);

void hm_gameplay_draw_fence_connections(s16 row, s16 col, s16 x, s16 y, s32 depth) {
    if (depth < 0 || depth >= (FRAME_OT_DEPTH << 2))
        return;
    s32 base_ot = (depth >> 2) + 1;
    if (base_ot >= FRAME_OT_DEPTH)
        base_ot = depth >> 2;
    if (base_ot < 0 || base_ot >= FRAME_OT_DEPTH)
        return;

    /* Check right neighbor (col + 1) */
    if (col + 1 < FIELD_COLS && game_state.fieldTiles[row].rows[col + 1].value == ITEM_LUMBER) {
        SVECTOR right_pt = {
            .vx = (col + 1) * FIELD_CELL_SIZE - 4050,
            .vy = -100,
            .vz = 1250 - row * FIELD_CELL_SIZE
        };
        FarmScreenPoint right_scr;
        if (estate_point_project(&right_pt, &right_scr) != 0) {
            /* Upper rail: wood highlight and shadow */
            POLY_F4 *p_top = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_top);
            setRGB0(p_top, 175, 125, 78); /* Warm lumber highlight */
            p_top->x0 = x;           p_top->y0 = y - 9;
            p_top->x1 = right_scr.x; p_top->y1 = right_scr.y - 9;
            p_top->x2 = x;           p_top->y2 = y - 7;
            p_top->x3 = right_scr.x; p_top->y3 = right_scr.y - 7;
            AddPrim(&current_frame->ot[base_ot], p_top);

            POLY_F4 *p_bot = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_bot);
            setRGB0(p_bot, 105, 68, 40); /* Lumber shadow */
            p_bot->x0 = x;           p_bot->y0 = y - 7;
            p_bot->x1 = right_scr.x; p_bot->y1 = right_scr.y - 7;
            p_bot->x2 = x;           p_bot->y2 = y - 5;
            p_bot->x3 = right_scr.x; p_bot->y3 = right_scr.y - 5;
            AddPrim(&current_frame->ot[base_ot], p_bot);

            /* Lower rail: wood highlight and shadow */
            POLY_F4 *p_top2 = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_top2);
            setRGB0(p_top2, 160, 115, 70);
            p_top2->x0 = x;           p_top2->y0 = y - 3;
            p_top2->x1 = right_scr.x; p_top2->y1 = right_scr.y - 3;
            p_top2->x2 = x;           p_top2->y2 = y - 1;
            p_top2->x3 = right_scr.x; p_top2->y3 = right_scr.y - 1;
            AddPrim(&current_frame->ot[base_ot], p_top2);

            POLY_F4 *p_bot2 = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_bot2);
            setRGB0(p_bot2, 95, 60, 35);
            p_bot2->x0 = x;           p_bot2->y0 = y - 1;
            p_bot2->x1 = right_scr.x; p_bot2->y1 = right_scr.y - 1;
            p_bot2->x2 = x;           p_bot2->y2 = y + 1;
            p_bot2->x3 = right_scr.x; p_bot2->y3 = right_scr.y + 1;
            AddPrim(&current_frame->ot[base_ot], p_bot2);
        }
    }

    /* Check down neighbor (row + 1) */
    if (row + 1 < FIELD_ROWS && game_state.fieldTiles[row + 1].rows[col].value == ITEM_LUMBER) {
        SVECTOR down_pt = {
            .vx = col * FIELD_CELL_SIZE - 4050,
            .vy = -100,
            .vz = 1250 - (row + 1) * FIELD_CELL_SIZE
        };
        FarmScreenPoint down_scr;
        if (estate_point_project(&down_pt, &down_scr) != 0) {
            /* Upper rail */
            POLY_F4 *p_top = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_top);
            setRGB0(p_top, 165, 120, 75);
            p_top->x0 = x - 2;          p_top->y0 = y - 9;
            p_top->x1 = down_scr.x - 2; p_top->y1 = down_scr.y - 9;
            p_top->x2 = x + 1;          p_top->y2 = y - 7;
            p_top->x3 = down_scr.x + 1; p_top->y3 = down_scr.y - 7;
            AddPrim(&current_frame->ot[base_ot], p_top);

            POLY_F4 *p_bot = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_bot);
            setRGB0(p_bot, 95, 60, 35);
            p_bot->x0 = x - 2;          p_bot->y0 = y - 7;
            p_bot->x1 = down_scr.x - 2; p_bot->y1 = down_scr.y - 7;
            p_bot->x2 = x + 1;          p_bot->y2 = y - 5;
            p_bot->x3 = down_scr.x + 1; p_bot->y3 = down_scr.y - 5;
            AddPrim(&current_frame->ot[base_ot], p_bot);

            /* Lower rail */
            POLY_F4 *p_top2 = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_top2);
            setRGB0(p_top2, 155, 110, 68);
            p_top2->x0 = x - 2;          p_top2->y0 = y - 3;
            p_top2->x1 = down_scr.x - 2; p_top2->y1 = down_scr.y - 3;
            p_top2->x2 = x + 1;          p_top2->y2 = y - 1;
            p_top2->x3 = down_scr.x + 1; p_top2->y3 = down_scr.y - 1;
            AddPrim(&current_frame->ot[base_ot], p_top2);

            POLY_F4 *p_bot2 = (POLY_F4 *)current_frame->primCursor.bytes;
            current_frame->primCursor.bytes += sizeof(POLY_F4);
            SetPolyF4(p_bot2);
            setRGB0(p_bot2, 85, 52, 30);
            p_bot2->x0 = x - 2;          p_bot2->y0 = y - 1;
            p_bot2->x1 = down_scr.x - 2; p_bot2->y1 = down_scr.y - 1;
            p_bot2->x2 = x + 1;          p_bot2->y2 = y + 1;
            p_bot2->x3 = down_scr.x + 1; p_bot2->y3 = down_scr.y + 1;
            AddPrim(&current_frame->ot[base_ot], p_bot2);
        }
    }
}

extern void window_frame_draw_at(u_long *ot, s32 style, s16 x, s16 y, s16 w, s16 h);

extern s32 livestock_action_dispatch(FarmLivestock *action, s32 state);
extern s32 farm_actor_dispatch_state(FarmActor *object, s32 state);

void hm_gameplay_smart_bell_ring(void) {
    if (!active_options.smart_bell)
        return;
    s16 loc = farm_area.mode.place.location;
    if (loc == PLACE_FARM) {
        /* On farm: herd cows & sheep to barn entrance (1200, 0, 3200) */
        for (int i = 0; i < MAX_LIVESTOCK; ++i) {
            FarmLivestock *item = &game_state.animals.livestock[i];
            if (item->actor.kind != ITEM_NOTHING && item->actor.place.location == PLACE_FARM) {
                item->actor.destination.vx = 1200;
                item->actor.destination.vy = 0;
                item->actor.destination.vz = 3200;
                item->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
                livestock_action_dispatch(item, 18); /* LIVESTOCK_STATE_WALK */
            }
        }
        /* Herd chickens to coop entrance (-5800, 0, -6950) */
        for (int i = 0; i < MAX_CHICKENS; ++i) {
            FarmChicken *ch = &game_state.animals.chickens[i];
            if (ch->actor.kind != ITEM_NOTHING && ch->actor.place.location == PLACE_FARM) {
                ch->actor.destination.vx = -5800;
                ch->actor.destination.vy = 0;
                ch->actor.destination.vz = -6950;
                ch->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
                farm_actor_dispatch_state(&ch->actor, 3);
            }
        }
    } else if (loc == PLACE_5) {
        /* Inside barn: herd cows & sheep to exit door (1500, 0, -2900) */
        for (int i = 0; i < MAX_LIVESTOCK; ++i) {
            FarmLivestock *item = &game_state.animals.livestock[i];
            if (item->actor.kind != ITEM_NOTHING && item->actor.place.location == PLACE_5) {
                item->actor.destination.vx = 1500;
                item->actor.destination.vy = 0;
                item->actor.destination.vz = -2900;
                item->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
                livestock_action_dispatch(item, 18);
            }
        }
    } else if (loc == PLACE_4) {
        /* Inside coop: herd chickens to exit door (1850, 0, -1200) */
        for (int i = 0; i < MAX_CHICKENS; ++i) {
            FarmChicken *ch = &game_state.animals.chickens[i];
            if (ch->actor.kind != ITEM_NOTHING && ch->actor.place.location == PLACE_4) {
                ch->actor.destination.vx = 1850;
                ch->actor.destination.vy = 0;
                ch->actor.destination.vz = -1200;
                ch->actor.flags |= ACTOR_FLAG_APPROACHING_PLAYER;
                farm_actor_dispatch_state(&ch->actor, 3);
            }
        }
    }
}

void hm_gameplay_smart_bell_tick(void) {
    if (!active_options.smart_bell)
        return;
    s16 loc = farm_area.mode.place.location;
    if (loc == PLACE_FARM) {
        /* Check if any livestock reached barn door */
        for (int i = 0; i < MAX_LIVESTOCK; ++i) {
            FarmLivestock *item = &game_state.animals.livestock[i];
            if (item->actor.kind != ITEM_NOTHING && item->actor.place.location == PLACE_FARM &&
                (item->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
                s32 dx = abs(item->actor.position.vx - 1200);
                s32 dz = abs(item->actor.position.vz - 3200);
                if (dx < 350 && dz < 350) {
                    item->actor.place.location = PLACE_5;
                    item->actor.position.vx = 1534;
                    item->actor.position.vz = -2000;
                    item->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
                }
            }
        }
        /* Check if any chicken reached coop door */
        for (int i = 0; i < MAX_CHICKENS; ++i) {
            FarmChicken *ch = &game_state.animals.chickens[i];
            if (ch->actor.kind != ITEM_NOTHING && ch->actor.place.location == PLACE_FARM &&
                (ch->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
                s32 dx = abs(ch->actor.position.vx - (-5800));
                s32 dz = abs(ch->actor.position.vz - (-6950));
                if (dx < 350 && dz < 350) {
                    ch->actor.place.location = PLACE_4;
                    ch->actor.position.vx = 1300;
                    ch->actor.position.vz = -1255;
                    ch->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
                }
            }
        }
    } else if (loc == PLACE_5) {
        /* Check if any livestock reached barn exit */
        for (int i = 0; i < MAX_LIVESTOCK; ++i) {
            FarmLivestock *item = &game_state.animals.livestock[i];
            if (item->actor.kind != ITEM_NOTHING && item->actor.place.location == PLACE_5 &&
                (item->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
                s32 dx = abs(item->actor.position.vx - 1500);
                s32 dz = abs(item->actor.position.vz - (-2900));
                if (dx < 350 && dz < 350) {
                    item->actor.place.location = PLACE_FARM;
                    item->actor.position.vx = 1240;
                    item->actor.position.vz = 2800;
                    item->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
                }
            }
        }
    } else if (loc == PLACE_4) {
        /* Check if any chicken reached coop exit */
        for (int i = 0; i < MAX_CHICKENS; ++i) {
            FarmChicken *ch = &game_state.animals.chickens[i];
            if (ch->actor.kind != ITEM_NOTHING && ch->actor.place.location == PLACE_4 &&
                (ch->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
                s32 dx = abs(ch->actor.position.vx - 1850);
                s32 dz = abs(ch->actor.position.vz - (-1200));
                if (dx < 350 && dz < 350) {
                    ch->actor.place.location = PLACE_FARM;
                    ch->actor.position.vx = -5400;
                    ch->actor.position.vz = -7100;
                    ch->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
                }
            }
        }
    }
}

typedef struct {
    s16 season;
    s16 day;
    const char *name;
} hm_calendar_extra_entry;

static const hm_calendar_extra_entry calendar_birthdays[] = {
    /* Spring */
    {0, 10, "Saibara's Birthday"},
    {0, 15, "Elli's Birthday"},
    {0, 16, "Barley's Birthday"},
    {0, 18, "Lillia's Birthday"},
    {0, 28, "Greg's Birthday"},
    {0, 29, "Sasha's Birthday"},
    /* Summer */
    {1, 2, "Popuri's Birthday"},
    {1, 10, "Basil's Birthday"},
    {1, 15, "Tim's Birthday"},
    {1, 16, "Ann's Birthday"},
    {1, 21, "Kai's Birthday"},
    {1, 24, "Mayor's Birthday"},
    {1, 28, "Zack's Birthday"},
    /* Fall */
    {2, 1, "Gotz's Birthday"},
    {2, 4, "Stu's Birthday"},
    {2, 9, "Manna's Birthday"},
    {2, 14, "Karen's Birthday"},
    {2, 16, "Doctor's Birthday"},
    {2, 19, "Carter's Birthday"},
    {2, 26, "Rick's Birthday"},
    /* Winter */
    {3, 1, "Kano's Birthday"},
    {3, 5, "Gray's Birthday"},
    {3, 10, "Doug's Birthday"},
    {3, 12, "Ellen's Birthday"},
    {3, 19, "Mary's Birthday"},
    {3, 21, "Cliff's Birthday"},
};

int hm_gameplay_calendar_has_event(s16 season, s16 day) {
    if (!active_options.calendar_events)
        return 0;
    /* Check villager birthdays */
    for (size_t i = 0; i < sizeof(calendar_birthdays) / sizeof(calendar_birthdays[0]); ++i) {
        if (calendar_birthdays[i].season == season && calendar_birthdays[i].day == day)
            return 1;
    }
    /* Check player birthday */
    if (game_state.player.birthday.season == season && game_state.player.birthday.day == day)
        return 1;
    /* Check livestock maturation */
    for (int i = 0; i < MAX_LIVESTOCK; ++i) {
        FarmLivestock *item = &game_state.animals.livestock[i];
        if (item->actor.kind != ITEM_NOTHING && item->growthDays < LIVESTOCK_ADULT_DAYS) {
            s32 mat_day = game_state.progress.clock.date.day + (LIVESTOCK_ADULT_DAYS - item->growthDays);
            if (mat_day == day && game_state.progress.clock.date.season == season)
                return 1;
        }
    }
    return 0;
}

const char *hm_gameplay_calendar_event_name(s16 season, s16 day) {
    if (!active_options.calendar_events)
        return NULL;
    for (size_t i = 0; i < sizeof(calendar_birthdays) / sizeof(calendar_birthdays[0]); ++i) {
        if (calendar_birthdays[i].season == season && calendar_birthdays[i].day == day)
            return calendar_birthdays[i].name;
    }
    if (game_state.player.birthday.season == season && game_state.player.birthday.day == day)
        return "Player's Birthday";
    for (int i = 0; i < MAX_LIVESTOCK; ++i) {
        FarmLivestock *item = &game_state.animals.livestock[i];
        if (item->actor.kind != ITEM_NOTHING && item->growthDays < LIVESTOCK_ADULT_DAYS) {
            s32 mat_day = game_state.progress.clock.date.day + (LIVESTOCK_ADULT_DAYS - item->growthDays);
            if (mat_day == day && game_state.progress.clock.date.season == season)
                return "Livestock Matures";
        }
    }
    return NULL;
}

void hm_gameplay_horse_safety_check(void) {
    /* If roaming horse is OFF, horse must only be on farm (PLACE_FARM) or stable (PLACE_3) */
    if (!active_options.roaming_horse) {
        FarmPet *horse = &game_state.animals.horse;
        if (horse->actor.kind != ITEM_NOTHING &&
            horse->actor.place.location != PLACE_FARM &&
            horse->actor.place.location != PLACE_3) {
            horse->actor.place.location = PLACE_3;
            horse->actor.position.vx = -173;
            horse->actor.position.vy = 0;
            horse->actor.position.vz = -400;
            if (game_state.player.stateFlags & PLAYER_FLAG_RIDING) {
                game_state.player.stateFlags &= ~(PLAYER_FLAG_RIDING_COW | PLAYER_FLAG_RIDING);
            }
        }
    }
}
