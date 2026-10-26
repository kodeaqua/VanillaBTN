/* Original transfer staging/restoration, with synthetic states and no card files. */
#include "../../src/overlay/15_saveload/saveload.c"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static FarmState foreign;
static hm_save_partner decoded;
static u8 original[HM_SAVE_IMAGE_BYTES];
static u8 foreign_tail[HM_SAVE_IMAGE_BYTES - 0x1f34];
int main(void) {
    farm_tables_register(0);
    foreign = game_state;
    for (int chickens = 0; chickens <= 1; ++chickens) {
        memset(&game_state.animals, 0, sizeof game_state.animals);
        memset(&foreign.animals, 0, sizeof foreign.animals);
        foreign.progress.estate.farmName[0] = 100;
        foreign.progress.estate.farmName[1] = 0xffff;
        foreign.progress.estate.coopLevel = 1;
        foreign.progress.estate.barnLevel = 1;
        for (int i = 0; i < 5; ++i) {
            if (chickens) {
                game_state.animals.chickens[i].actor.kind = ITEM_CHICKEN;
                game_state.animals.chickens[i].grazingTime = 100 + i;
                foreign.animals.chickens[i + 5].actor.kind = ITEM_CHICKEN;
                foreign.animals.chickens[i + 5].grazingTime = 200 + i;
            } else {
                game_state.animals.livestock[i].actor.kind = ITEM_COW;
                game_state.animals.livestock[i].grazeCount = 100 + i;
                foreign.animals.livestock[i + 15].actor.kind = ITEM_COW;
                foreign.animals.livestock[i + 15].grazeCount = 200 + i;
            }
        }
        CHECK(hm_save_encode(save_image_data, sizeof save_image_data, &foreign));
        save_image = save_image_data;
        memcpy(original, save_image, HM_SAVE_IMAGE_BYTES);
        memcpy(foreign_tail, (u8 *)save_image + 0x1f34, sizeof foreign_tail);
        CHECK(hm_save_partner_decode(&hm_save_partner_state, save_image, HM_SAVE_IMAGE_BYTES));
        CHECK(hm_save_partner_state.estate.farmName[0] == 100);
        CHECK(hm_save_partner_state.estate.barnLevel == 1);
        CHECK(!memcmp(&hm_save_partner_state.estate, &foreign.progress.estate,
                      sizeof hm_save_partner_state.estate));
        CHECK(!memcmp(hm_save_partner_state.identity, &foreign, 8));
        memset(save_out_buffer, 0xa5, sizeof save_out_buffer);
        save_reset_work_tables();
        for (size_t i = 0; i < sizeof save_out_buffer; ++i)
            CHECK(save_out_buffer[i] == 0);
        for (int i = 0; i < 5; ++i) {
            save_entries[0][i].kind = chickens ? SAVE_PANEL_CHICKENS : SAVE_PANEL_CATTLE;
            save_entries[0][i].index = i;
            save_entries[1][i].kind = chickens ? SAVE_PANEL_CHICKENS : SAVE_PANEL_CATTLE;
            save_entries[1][i].index = i + (chickens ? 5 : 15);
        }
        CHECK(save_count_records(save_image,
                                 chickens ? SAVE_TABLE_CHICKENS : SAVE_TABLE_LIVESTOCK) == 5);
        save_transfer_stash();
        CHECK(save_count_records(&game_state.animals,
                                 chickens ? SAVE_TABLE_CHICKENS : SAVE_TABLE_LIVESTOCK) == 0);
        CHECK(hm_save_partner_encode_animals(save_image, HM_SAVE_IMAGE_BYTES,
                                             &hm_save_partner_state));
        CHECK(!memcmp((u8 *)save_image + 0x1f34, foreign_tail, sizeof foreign_tail));
        CHECK(hm_save_partner_decode(&decoded, save_image, HM_SAVE_IMAGE_BYTES));
        for (int i = 0; i < 5; ++i) {
            if (chickens)
                CHECK(decoded.animals.chickens[i].grazingTime == 100 + i);
            else
                CHECK(decoded.animals.livestock[i].grazeCount == 100 + i);
        }
        save_image_shift_in();
        save_image_checksum_write();
        CHECK(save_image_checksum_verify() != SAVE_CHECKSUM_BAD);
        save_transfer_restore();
        for (int i = 0; i < 5; ++i) {
            if (chickens) {
                CHECK(game_state.animals.chickens[i].actor.kind == ITEM_CHICKEN);
                CHECK(game_state.animals.chickens[i].grazingTime == 200 + i);
                CHECK(!game_state.animals.chickens[i].actor.sprite.source);
            } else {
                CHECK(game_state.animals.livestock[i].actor.kind == ITEM_COW);
                CHECK(game_state.animals.livestock[i].grazeCount == 200 + i);
                CHECK(!game_state.animals.livestock[i].actor.sprite.source);
            }
        }
        save_image_shift_out();
        CHECK(!memcmp((u8 *)save_image + 0x1f34, foreign_tail, sizeof foreign_tail));
        memcpy(original, save_image, HM_SAVE_IMAGE_BYTES);
        CHECK(!hm_save_partner_encode_animals(save_image, HM_SAVE_IMAGE_BYTES - 1, &decoded));
        CHECK(!memcmp(original, save_image, HM_SAVE_IMAGE_BYTES));
        CHECK(!hm_save_partner_decode(&decoded, save_image, HM_SAVE_IMAGE_BYTES - 1));
    }
    return 0;
}
