/* Exercise private original save-buffer operations without adding game APIs. */
#include "../../src/overlay/15_saveload/saveload.c"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static unsigned int readback[0x8000 / sizeof(unsigned int)];
static u8 payload[0x6378];

int main(void) {
    /* reset and card I/O address a complete image, not a 128-byte header. */
    CHECK(sizeof(save_file_header) >= SAVE_FILE_BYTES);
    CHECK((uintptr_t)&save_icon_frame_0 == (uintptr_t)save_file_header + 0x80);
    CHECK((uintptr_t)&save_icon_frame_1 == (uintptr_t)save_file_header + 0x100);
    CHECK((uintptr_t)&save_icon_frame_2 == (uintptr_t)save_file_header + 0x180);
    CHECK((uintptr_t)save_slot_record == (uintptr_t)save_file_header + 0x200);
    CHECK((uintptr_t)save_image_data == (uintptr_t)save_file_header + 0x280);
    memset(save_file_name_buf, 0x5A, sizeof(save_file_name_buf));
    save_reset_state();
    for (size_t i = 0; i < SAVE_FILE_BYTES; ++i) CHECK(save_file_header[i] == 0);
    for (size_t i = 0; i < sizeof(save_file_name_buf); ++i) CHECK(save_file_name_buf[i] == 0x5A);
    CHECK(save_image == save_image_data);
    memset(save_file_header, 0x11, 0x80);
    memset(save_icon_frame_0.bytes, 0x22, 0x80);
    memset(save_icon_frame_1.bytes, 0x33, 0x80);
    memset(save_icon_frame_2.bytes, 0x44, 0x80);
    memset(save_slot_record, 0x55, 0x80);
    memset(save_image_data, 0x66, SAVE_FILE_BYTES - 0x280);
    for (size_t i = 0; i < SAVE_FILE_BYTES; ++i) {
        unsigned expected = i < 0x280 ? (unsigned)(i / 0x80 + 1) * 0x11 : 0x66;
        CHECK(save_file_header[i] == expected);
    }
    for (size_t i = 0; i < sizeof(payload); ++i)
        save_image_data[i] = payload[i] = (u8)(i * 37u + 11u);
    save_image_shift_in();
    save_image_checksum_write();
    CHECK(save_image_checksum_verify() == 0);
    save_file_header[0x281] ^= 1;
    CHECK(save_image_checksum_verify() == SAVE_CHECKSUM_BAD);
    save_file_header[0x281] ^= 1;
    CHECK(save_image_checksum_verify() == 0);

    /* The SDK must see all views in the 4-block file, not unrelated globals. */
    MemCardInit(0);
    CHECK(MemCardCreateFile(0, "BUFFER", 4) == 0);
    CHECK(MemCardWriteFile(0, "BUFFER", (unsigned int *)save_file_header, 0, 0x8000) == 1);
    long command = 0, result = -1;
    CHECK(MemCardSync(1, &command, &result) == 1 && result == McErrNone);
    CHECK(MemCardReadFile(0, "BUFFER", readback, 0, sizeof(readback)) == 1);
    CHECK(MemCardSync(1, &command, &result) == 1 && result == McErrNone);
    CHECK(memcmp(readback, save_file_header, sizeof(readback)) == 0);
    save_image_shift_out();
    CHECK(memcmp(save_image_data, payload, sizeof(payload)) == 0);

    /* Verify PS1 wire translation and round-trip packing/unpacking of farm state. */
    CHECK(sizeof(Ps1AnimWire) == 0x30);
    CHECK(sizeof(Ps1ActorWire) == 0xAC);
    CHECK(sizeof(Ps1ChickenWire) == 0xB4);
    CHECK(sizeof(Ps1LivestockWire) == 0xB8);
    CHECK(sizeof(Ps1PetWire) == 0xF0);
    CHECK(sizeof(Ps1ActionWire) == 0xB0);
    CHECK(sizeof(Ps1AnimalsWire) == 0x1F34);
    CHECK(sizeof(Ps1PlayerToyWire) == 52);
    CHECK(sizeof(Ps1PlayerWire) == 0x3FC);
    CHECK(sizeof(Ps1HelperWire) == 248);
    CHECK(sizeof(SaveFile) == 0x6378);
    CHECK(sizeof(SaveImage) == 0x6378);

    /* Set up test game_state */
    memset(&game_state, 0, sizeof(game_state));
    game_state.player.stamina = 120;
    game_state.player.maxStamina = 150;
    game_state.player.gold = 54321;
    game_state.player.medals = 77;
    game_state.player.position.vx = 1000;
    game_state.player.position.vy = 2000;
    game_state.player.position.vz = 3000;
    game_state.animals.chickens[0].actor.affection = 250;
    game_state.animals.livestock[0].actor.affection = 180;
    game_state.helpers[0].actor.affection = 255;
    game_state.helpers[0].harvestSkill = 15;
    game_state.request = &game_state.requests.items[3];

    save_pack_farm_state();

    /* Verify wire representations in save_image_data */
    SaveFile *wire_file = (SaveFile *)save_image_data;
    CHECK(wire_file->player.stamina == 120);
    CHECK(wire_file->player.maxStamina == 150);
    CHECK(wire_file->player.gold == 54321);
    CHECK(wire_file->player.medals == 77);
    CHECK(wire_file->player.position.vx == 1000);
    CHECK(wire_file->player.position.vy == 2000);
    CHECK(wire_file->player.position.vz == 3000);
    CHECK(wire_file->slots.chickens[0].actor.affection == 250);
    CHECK(wire_file->slots.livestock[0].actor.affection == 180);
    CHECK(wire_file->helpers[0].actor.affection == 255);
    CHECK(wire_file->helpers[0].harvestSkill == 15);
    CHECK(wire_file->request == 0x800742B0 + 3 * sizeof(FarmRequestSlot));

    /* Reset game_state and unpack */
    memset(&game_state, 0, sizeof(game_state));
    save_unpack_farm_state();

    CHECK(game_state.player.stamina == 120);
    CHECK(game_state.player.maxStamina == 150);
    CHECK(game_state.player.gold == 54321);
    CHECK(game_state.player.medals == 77);
    CHECK(game_state.player.position.vx == 1000);
    CHECK(game_state.player.position.vy == 2000);
    CHECK(game_state.player.position.vz == 3000);
    CHECK(game_state.animals.chickens[0].actor.affection == 250);
    CHECK(game_state.animals.livestock[0].actor.affection == 180);
    CHECK(game_state.helpers[0].actor.affection == 255);
    CHECK(game_state.helpers[0].harvestSkill == 15);
    CHECK(game_state.request == &game_state.requests.items[3]);

    return 0;
}
