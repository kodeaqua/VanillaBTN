/* Original pack/shift/checksum/unpack helpers against synthetic native state.
 * No disc, gameplay, window or real save directory is used. */
#include "compat/save_codec.h"
#include "../../src/overlay/15_saveload/saveload.c"
#include "psx/card.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed %d: %s\n", __LINE__, #x);                                      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static u8 original_image[HM_SAVE_IMAGE_BYTES], second_image[HM_SAVE_IMAGE_BYTES];
static FarmState snapshot, expected;
enum { CARD_BYTES = 512 + 15 * 8192 };
static u8 card_bytes[CARD_BYTES];
static int card_present;
static int card_read(void *context, int slot, void *bytes, size_t size) {
    (void)context;
    if (slot || size != CARD_BYTES)
        return -1;
    if (!card_present)
        return 0;
    memcpy(bytes, card_bytes, size);
    return 1;
}
static int card_write(void *context, int slot, const void *bytes, size_t size) {
    (void)context;
    if (slot || size != CARD_BYTES)
        return -1;
    memcpy(card_bytes, bytes, size);
    card_present = 1;
    return 1;
}
static int card_complete(s32 expected) {
    s32 command, result;
    return MemCardSync(1, &command, &result) == 1 && command == expected && result == McErrNone;
}
int main(void) {
    memset(&game_state, 0xa5, sizeof game_state);
    farm_tables_register(0);
    game_state.player.gold = 1234567;
    game_state.progress.clock.date.day = 17;
    game_state.animals.livestock[19].actor.affection = 231;
    game_state.animals.chickens[9].actor.affection = 193;
    game_state.helpers[6].harvestSkill = 217;
    game_state.ending.tracks.stages[9] = 8;
    game_state.runs.records[16][9].itemId = ITEM_STONE;
    game_state.runs.records[18][1].itemId = ITEM_BRANCH;
    expected = game_state;
    CHECK(hm_save_encode(original_image, sizeof original_image, &game_state));
    CHECK(!hm_save_encode(second_image, HM_SAVE_IMAGE_BYTES - 1, &game_state));
    CHECK(original_image[0x3060 + 92] == (1234567 & 255));
    for (size_t i = 0; i < 24; ++i)
        CHECK(original_image[0x2274 + i] == 0);
    for (size_t i = 0; i < 28; ++i)
        CHECK(original_image[120 + 20 + i] == 0);
    for (size_t i = 0; i < 8; ++i)
        CHECK(original_image[0x3060 + 76 + i] == 0);
    save_slot_record_fill(0);
    save_pack_farm_state();
    CHECK(!memcmp(save_image_data, original_image, sizeof original_image));
    save_image_shift_in();
    save_image_checksum_write();
    CHECK(save_image_checksum_verify() == 0);
    save_file_header[0x500] ^= 1;
    CHECK(save_image_checksum_verify() == -1);
    save_file_header[0x500] ^= 1;
    hm_card_storage storage = {NULL, card_read, card_write};
    hm_card_attach(storage);
    MemCardInit(0);
    MemCardStart();
    CHECK(MemCardFormat(0) == McErrNone);
    CHECK(MemCardCreateFile(0, "TEST-SAVE", 4) == McErrNone);
    CHECK(MemCardWriteFile(0, "TEST-SAVE", save_file_header, 0, 0x8000) == 1 && card_complete(4));
    hm_card_detach();
    memset(save_file_header, 0, 0x8000);
    hm_card_attach(storage);
    MemCardInit(0);
    MemCardStart();
    CHECK(MemCardReadFile(0, "TEST-SAVE", save_file_header, 0, 0x8000) == 1 && card_complete(3));
    CHECK(save_image_checksum_verify() == 0);
    hm_card_detach();
    save_image_shift_out();
    CHECK(!memcmp(save_image_data, original_image, sizeof original_image));
    memset(&game_state, 0, sizeof game_state);
    farm_tables_register(0);
    game_state.blockIndex = 987;
    memset(&game_state.system, 0x37, sizeof game_state.system);
    memset(game_state.runs.records[16], 0, 0xf0);
    memset(game_state.runs.records[18], 0, 0x30);
    game_state.player.target = &snapshot;
    snapshot = game_state;
    save_unpack_farm_state();
    CHECK(!memcmp(&game_state.progress, &expected.progress, sizeof game_state.progress));
    CHECK(!memcmp(game_state.fieldTiles, expected.fieldTiles, sizeof game_state.fieldTiles));
    CHECK(!memcmp(game_state.slots7x7, expected.slots7x7, sizeof game_state.slots7x7));
    CHECK(!memcmp(&game_state.requests, &expected.requests, sizeof game_state.requests));
    CHECK(!memcmp(&game_state.world, &expected.world, sizeof game_state.world));
    CHECK(!memcmp(&game_state.days, &expected.days, sizeof game_state.days));
    CHECK(!memcmp(&game_state.drift, &expected.drift, sizeof game_state.drift));
    CHECK(!memcmp(&game_state.family, &expected.family, sizeof game_state.family));
    CHECK(!memcmp(&game_state.ending, &expected.ending, sizeof game_state.ending));
    CHECK(!memcmp(save_image_data, original_image, sizeof original_image));
    CHECK(game_state.player.gold == 1234567 && game_state.progress.clock.date.day == 17);
    CHECK(game_state.animals.livestock[19].actor.affection == 231);
    CHECK(game_state.animals.chickens[9].actor.affection == 193 &&
          game_state.helpers[6].harvestSkill == 217);
    CHECK(game_state.ending.tracks.stages[9] == 8);
    CHECK(game_state.runs.records[16][9].itemId == ITEM_STONE &&
          game_state.runs.records[18][1].itemId == ITEM_BRANCH);
    CHECK(game_state.player.target == &snapshot && game_state.blockIndex == 987);
    CHECK(!memcmp(&game_state.system, &snapshot.system, sizeof game_state.system));
    CHECK(game_state.itemInfo == snapshot.itemInfo &&
          game_state.pictureCodes == snapshot.pictureCodes);
    CHECK(hm_save_encode(second_image, sizeof second_image, &game_state));
    CHECK(!memcmp(original_image, second_image, sizeof original_image));
    CHECK(!hm_save_decode(original_image, HM_SAVE_IMAGE_BYTES - 1, &game_state));
    game_state.runs.records[16] = NULL;
    snapshot = game_state;
    CHECK(!hm_save_decode(original_image, sizeof original_image, &game_state));
    CHECK(!memcmp(&snapshot, &game_state, sizeof snapshot));
    puts("Fixed save payload, original shifts/checksums, native bindings and persistent roundtrip "
         "passed");
    return 0;
}
