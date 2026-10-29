#include "platform/gameplay_options.h"
#include "enhance/gameplay/gameplay.h"
#include "platform/settings_json.h"
#include "platform/user_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hm_gameplay_defaults(hm_gameplay_options *options) {
    if (!options)
        return;
    options->walk_through_crops = 0;
    options->item_stacking = 0;
    options->durable_fences = 0;
    options->connect_fences = 0;
    options->wild_dog = 1;
    options->smart_bell = 0;
    options->stamina_hud = 0;
    options->calendar_events = 0;
    options->roaming_horse = 0;
    options->auto_run = 0;
}

void hm_gameplay_load(hm_gameplay_options *options) {
    hm_gameplay_defaults(options);
    char *text = malloc(65537);
    int bytes = text ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object;
    size_t length;
    if (bytes >= 0 && hm_json_property(text, (size_t)bytes, "enhance", &object, &length) == 1) {
        const char *val;
        size_t val_len;
        if (hm_json_property(object, length, "walk_through_crops", &val, &val_len) == 1)
            options->walk_through_crops = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "item_stacking", &val, &val_len) == 1)
            options->item_stacking = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "durable_fences", &val, &val_len) == 1)
            options->durable_fences = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "connect_fences", &val, &val_len) == 1)
            options->connect_fences = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "wild_dog", &val, &val_len) == 1) {
            if (val_len == 5 && !memcmp(val, "false", 5))
                options->wild_dog = 0;
            else if (val_len == 4 && !memcmp(val, "true", 4))
                options->wild_dog = 1;
        }
        if (hm_json_property(object, length, "smart_bell", &val, &val_len) == 1)
            options->smart_bell = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "stamina_hud", &val, &val_len) == 1)
            options->stamina_hud = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "calendar_events", &val, &val_len) == 1)
            options->calendar_events = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "roaming_horse", &val, &val_len) == 1)
            options->roaming_horse = (val_len == 4 && !memcmp(val, "true", 4));
        if (hm_json_property(object, length, "auto_run", &val, &val_len) == 1)
            options->auto_run = (val_len == 4 && !memcmp(val, "true", 4));
    }
    free(text);
    hm_gameplay_set_active(options);
}

int hm_gameplay_save(const hm_gameplay_options *options) {
    if (!options)
        return 0;
    hm_gameplay_set_active(options);
    int ok = 1;
    ok &= hm_user_data_set_nested_setting("enhance", "walk_through_crops",
                                          options->walk_through_crops ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "item_stacking",
                                          options->item_stacking ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "durable_fences",
                                          options->durable_fences ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "connect_fences",
                                          options->connect_fences ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "wild_dog",
                                          options->wild_dog ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "smart_bell",
                                          options->smart_bell ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "stamina_hud",
                                          options->stamina_hud ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "calendar_events",
                                          options->calendar_events ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "roaming_horse",
                                          options->roaming_horse ? "true" : "false");
    ok &= hm_user_data_set_nested_setting("enhance", "auto_run",
                                          options->auto_run ? "true" : "false");
    return ok;
}

int hm_gameplay_change(hm_gameplay_options *options, int item, int direction) {
    (void)direction;
    if (!options)
        return 0;
    switch (item) {
    case 0:
        options->walk_through_crops = !options->walk_through_crops;
        break;
    case 1:
        options->item_stacking = !options->item_stacking;
        break;
    case 2:
        options->durable_fences = !options->durable_fences;
        break;
    case 3:
        options->connect_fences = !options->connect_fences;
        break;
    case 4:
        options->wild_dog = !options->wild_dog;
        break;
    case 5:
        options->smart_bell = !options->smart_bell;
        break;
    case 6:
        options->stamina_hud = !options->stamina_hud;
        break;
    case 7:
        options->calendar_events = !options->calendar_events;
        break;
    case 8:
        options->roaming_horse = !options->roaming_horse;
        break;
    case 9:
        options->auto_run = !options->auto_run;
        break;
    default:
        return 0;
    }
    hm_gameplay_set_active(options);
    return 1;
}

void hm_gameplay_value(const hm_gameplay_options *options, int item, char *text, size_t capacity) {
    if (!options || !text || !capacity)
        return;
    switch (item) {
    case 0:
        snprintf(text, capacity, "Walk freely through crops and wild weeds");
        break;
    case 1:
        snprintf(text, capacity, "Stack identical items in rucksack up to 99");
        break;
    case 2:
        snprintf(text, capacity, "Prevent farm lumber fences from rotting");
        break;
    case 3:
        snprintf(text, capacity, "Draw connecting rails between adjacent fences");
        break;
    case 4:
        snprintf(text, capacity, "Allow wild stray dog farm attacks at night");
        break;
    case 5:
        snprintf(text, capacity, "Ringing bell guides animals in/out of barn and coop");
        break;
    case 6:
        snprintf(text, capacity, "Show stamina bar on HUD with Pete head and hearts");
        break;
    case 7:
        snprintf(text, capacity, "Show birthdays and livestock milestones on calendar");
        break;
    case 8:
        snprintf(text, capacity, "Allow riding horse outside the farm estate");
        break;
    case 9:
        snprintf(text, capacity, "Run automatically while moving; hold run button to walk");
        break;
    default:
        text[0] = 0;
        break;
    }
}

void hm_gameplay_row_value(const hm_gameplay_options *options, int item, char *text, size_t capacity) {
    if (!options || !text || !capacity)
        return;
    switch (item) {
    case 0:
        snprintf(text, capacity, "%s", options->walk_through_crops ? "On" : "Off");
        break;
    case 1:
        snprintf(text, capacity, "%s", options->item_stacking ? "On" : "Off");
        break;
    case 2:
        snprintf(text, capacity, "%s", options->durable_fences ? "On" : "Off");
        break;
    case 3:
        snprintf(text, capacity, "%s", options->connect_fences ? "On" : "Off");
        break;
    case 4:
        snprintf(text, capacity, "%s", options->wild_dog ? "Enabled" : "Disabled");
        break;
    case 5:
        snprintf(text, capacity, "%s", options->smart_bell ? "On" : "Off");
        break;
    case 6:
        snprintf(text, capacity, "%s", options->stamina_hud ? "On" : "Off");
        break;
    case 7:
        snprintf(text, capacity, "%s", options->calendar_events ? "On" : "Off");
        break;
    case 8:
        snprintf(text, capacity, "%s", options->roaming_horse ? "On" : "Off");
        break;
    case 9:
        snprintf(text, capacity, "%s", options->auto_run ? "On" : "Off");
        break;
    default:
        text[0] = 0;
        break;
    }
}
