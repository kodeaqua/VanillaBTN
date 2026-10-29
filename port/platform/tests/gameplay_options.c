#include "platform/gameplay_options.h"
#include "enhance/gameplay/gameplay.h"
#include "platform/user_data.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(int argc, char **argv) {
    CHECK(argc == 2 && SDL_Init(0));
    char *root = NULL;
    CHECK(SDL_asprintf(&root, "%s-%llu", argv[1], (unsigned long long)SDL_GetPerformanceCounter()) >= 0);
    CHECK(hm_user_data_start_at(root));

    hm_gameplay_options opt;
    hm_gameplay_defaults(&opt);

    CHECK(opt.walk_through_crops == 0);
    CHECK(opt.item_stacking == 0);
    CHECK(opt.durable_fences == 0);
    CHECK(opt.connect_fences == 0);
    CHECK(opt.wild_dog == 1);
    CHECK(opt.smart_bell == 0);
    CHECK(opt.stamina_hud == 0);
    CHECK(opt.calendar_events == 0);
    CHECK(opt.roaming_horse == 0);
    CHECK(opt.fast_text == 0);
    CHECK(opt.tool_charge_gauge == 0);
    CHECK(opt.tile_targeting == 0);

    CHECK(hm_gameplay_walk_through_crops() == 0);
    CHECK(hm_gameplay_item_stacking() == 0);
    CHECK(hm_gameplay_durable_fences() == 0);
    CHECK(hm_gameplay_connect_fences() == 0);
    CHECK(hm_gameplay_wild_dog_enabled() == 1);
    CHECK(hm_gameplay_smart_bell() == 0);
    CHECK(hm_gameplay_stamina_hud() == 0);
    CHECK(hm_gameplay_calendar_events() == 0);
    CHECK(hm_gameplay_roaming_horse() == 0);
    CHECK(hm_gameplay_fast_text() == 0);
    CHECK(hm_gameplay_tool_charge_gauge() == 0);
    CHECK(hm_gameplay_tile_targeting() == 0);

    char val[64];
    hm_gameplay_row_value(&opt, 0, val, sizeof(val));
    CHECK(!strcmp(val, "Off"));
    hm_gameplay_row_value(&opt, 4, val, sizeof(val));
    CHECK(!strcmp(val, "Enabled"));

    CHECK(hm_gameplay_change(&opt, 0, 1) == 1);
    CHECK(opt.walk_through_crops == 1);
    CHECK(hm_gameplay_walk_through_crops() == 1);

    CHECK(hm_gameplay_change(&opt, 4, -1) == 1);
    CHECK(opt.wild_dog == 0);
    CHECK(hm_gameplay_wild_dog_enabled() == 0);

    CHECK(hm_gameplay_change(&opt, 5, 1) == 1);
    CHECK(opt.smart_bell == 1);
    CHECK(hm_gameplay_smart_bell() == 1);

    CHECK(hm_gameplay_change(&opt, 6, 1) == 1);
    CHECK(opt.stamina_hud == 1);
    CHECK(hm_gameplay_stamina_hud() == 1);

    CHECK(hm_gameplay_change(&opt, 7, 1) == 1);
    CHECK(opt.calendar_events == 1);
    CHECK(hm_gameplay_calendar_events() == 1);

    CHECK(hm_gameplay_change(&opt, 8, 1) == 1);
    CHECK(opt.roaming_horse == 1);
    CHECK(hm_gameplay_roaming_horse() == 1);
    CHECK(hm_gameplay_change(&opt, 9, 1) == 1);
    CHECK(opt.fast_text == 1);
    CHECK(hm_gameplay_fast_text() == 1);

    CHECK(hm_gameplay_change(&opt, 10, 1) == 1);
    CHECK(opt.tool_charge_gauge == 1);
    CHECK(hm_gameplay_tool_charge_gauge() == 1);

    CHECK(hm_gameplay_change(&opt, 11, 1) == 1);
    CHECK(opt.tile_targeting == 1);
    CHECK(hm_gameplay_tile_targeting() == 1);

    /* Test save & load persistence */
    CHECK(hm_gameplay_save(&opt) == 1);

    hm_gameplay_options loaded;
    hm_gameplay_load(&loaded);
    CHECK(loaded.walk_through_crops == 1);
    CHECK(loaded.wild_dog == 0);
    CHECK(loaded.smart_bell == 1);
    CHECK(loaded.stamina_hud == 1);
    CHECK(loaded.calendar_events == 1);
    CHECK(loaded.roaming_horse == 1);
    CHECK(loaded.fast_text == 1);
    CHECK(loaded.tool_charge_gauge == 1);
    CHECK(loaded.tile_targeting == 1);

    hm_gameplay_defaults(&opt);
    hm_gameplay_set_active(&opt);
    CHECK(hm_gameplay_walk_through_crops() == 0);
    CHECK(hm_gameplay_wild_dog_enabled() == 1);
    CHECK(hm_gameplay_smart_bell() == 0);
    CHECK(hm_gameplay_stamina_hud() == 0);
    CHECK(hm_gameplay_calendar_events() == 0);
    CHECK(hm_gameplay_roaming_horse() == 0);
    CHECK(hm_gameplay_fast_text() == 0);
    CHECK(hm_gameplay_tool_charge_gauge() == 0);
    CHECK(hm_gameplay_tile_targeting() == 0);

    SDL_free(root);
    SDL_Quit();
    return 0;
}
