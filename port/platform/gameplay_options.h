#ifndef HMBTN_PLATFORM_GAMEPLAY_OPTIONS_H
#define HMBTN_PLATFORM_GAMEPLAY_OPTIONS_H

#include <stddef.h>

typedef struct {
    int walk_through_crops; /* 0 = Off, 1 = On */
    int item_stacking;      /* 0 = Off, 1 = On */
    int durable_fences;     /* 0 = Off, 1 = On */
    int connect_fences;     /* 0 = Off, 1 = On */
    int wild_dog;           /* 0 = Disabled, 1 = Enabled (default 1) */
    int smart_bell;         /* 0 = Off, 1 = On */
    int stamina_hud;        /* 0 = Off, 1 = On */
    int calendar_events;    /* 0 = Off, 1 = On */
    int roaming_horse;      /* 0 = Off, 1 = On */
    int auto_run;           /* 0 = Off, 1 = On */
} hm_gameplay_options;

void hm_gameplay_defaults(hm_gameplay_options *options);
void hm_gameplay_load(hm_gameplay_options *options);
int hm_gameplay_save(const hm_gameplay_options *options);
int hm_gameplay_change(hm_gameplay_options *options, int item, int direction);
void hm_gameplay_value(const hm_gameplay_options *options, int item, char *text, size_t capacity);
void hm_gameplay_row_value(const hm_gameplay_options *options, int item, char *text, size_t capacity);

#endif
