#ifndef HMBTN_GAMEPLAY_H
#define HMBTN_GAMEPLAY_H

#include <stddef.h>
#include <sys/types.h>
#include "compat/types.h"

#define HM_STACK_MAX 99

/* Forward declaration of platform struct */
struct hm_gameplay_options;

int hm_gameplay_walk_through_crops(void);
int hm_gameplay_item_stacking(void);
int hm_gameplay_durable_fences(void);
int hm_gameplay_connect_fences(void);
int hm_gameplay_wild_dog_enabled(void);
int hm_gameplay_smart_bell(void);
int hm_gameplay_stamina_hud(void);
int hm_gameplay_fatigue_fill(int fatigue);
void hm_gameplay_draw_fatigue_bar(s16 x, s16 y, int fatigue);
int hm_gameplay_calendar_events(void);
int hm_gameplay_roaming_horse(void);
int hm_gameplay_fast_text(void);
int hm_gameplay_tool_charge_gauge(void);
int hm_gameplay_tile_targeting(void);
int hm_gameplay_npc_bonds(void);

void hm_gameplay_set_active(const void *options_ptr);

/* Visual & logic helpers */
void hm_gameplay_draw_fence_connections(s16 row, s16 col, s16 x, s16 y, s32 depth);
void hm_gameplay_draw_tool_charge_gauge(const void *player_ptr, s32 depth, s16 screen_x, s16 screen_y);
void hm_gameplay_draw_tile_targeting(const void *player_ptr, s32 depth);
void hm_gameplay_smart_bell_ring(void);
void hm_gameplay_smart_bell_tick(void);
int hm_gameplay_calendar_has_event(s16 season, s16 day);
const char *hm_gameplay_calendar_event_name(s16 season, s16 day);
void hm_gameplay_horse_safety_check(void);

#endif
