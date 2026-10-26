/* Run the original asset-loading helpers with a bounded asynchronous fixture.
 * No gameplay, disc, window or real save directory is used. */
#define afile_read_bytes hm_test_afile_read_bytes
#define anim_bind_file hm_test_anim_bind_file
#define texture_pack_load hm_test_texture_pack_load
#include "../../src/overlay/15_saveload/saveload.c"
#undef afile_read_bytes
#undef anim_bind_file
#undef texture_pack_load
#include "psx/card.h"
#include "psx/timing.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int hm_polls, hm_reads, hm_bindings, hm_textures, hm_bad;
static u64 hm_read_start;
static s32 hm_expected_file, hm_expected_size;
static u8 *hm_expected_destination;
static unsigned hm_host_ticks;
static u8 hm_icons[0x900];
static FrameState hm_frame;
static u_long hm_ot[FRAME_OT_DEPTH];
/* An empty synthetic ordinary card; this fixture never reads/writes real files. */
static int hm_empty_card_read(void *context, int slot, void *data, size_t bytes) {
    (void)context;
    (void)slot;
    (void)data;
    (void)bytes;
    return 0;
}
static int hm_empty_card_write(void *context, int slot, const void *data, size_t bytes) {
    (void)context;
    (void)slot;
    (void)data;
    (void)bytes;
    return 1;
}

u8 *hm_test_afile_read_bytes(s32 file, s32 offset, s32 size, u8 *destination, s32 align,
                             s32 restart) {
    if (file != hm_expected_file || offset || size != hm_expected_size ||
        destination != hm_expected_destination || align != 1)
        hm_bad = 1;
    if (restart) {
        ++hm_reads;
        hm_polls = 0;
        hm_read_start = hm_timing_ticks();
        return NULL;
    }
    /* An unfixed busy loop fails promptly, instead of hanging the test runner. */
    if (++hm_polls > 8) {
        fprintf(stderr, "Save/load asset poll did not yield a VBlank\n");
        exit(1);
    }
    if (hm_timing_ticks() - hm_read_start < 2)
        return NULL;
    memset(destination, 0, (size_t)size);
    return destination;
}
void hm_test_anim_bind_file(AnimState *state, AnimFile *file) {
    int index = hm_bindings++;
    if (index >= SAVE_SPRITE_COUNT || state != &save_sprites[index] ||
        file != (AnimFile *)(save_sheet + save_sprite_offsets[index]))
        hm_bad = 1;
}
void hm_test_texture_pack_load(u8 *pack) {
    if (pack != save_file_header + sizeof(SaveSheet))
        hm_bad = 1;
    ++hm_textures;
}
static void hm_wait_tick(void *context, u64 tick) {
    (void)context;
    (void)tick;
    ++hm_host_ticks; /* The real window host pumps its event queue here. */
}
int main(void) {
    MemCardInit(0);
    MemCardStart();
    current_frame = &hm_frame;
    hm_frame.ot = hm_ot;
    memset(&hm_save_workspace, 0xA5, sizeof(hm_save_workspace));
    save_reset_state();
    CHECK(save_icon_base == farm_area.sectorBuffer && save_image == save_file_header + 0x280);
    save_icon_base = hm_icons;
    for (size_t i = 0; i < sizeof(save_file_header); ++i)
        CHECK(save_file_header[i] == 0);
    save_icon_frame_0.bytes[3] = 11;
    save_icon_frame_1.bytes[3] = 22;
    save_icon_frame_2.bytes[3] = 33;
    save_slot_record[7] = 44;
    save_image_data[9] = 55;
    CHECK(save_file_header[0x83] == 11 && save_file_header[0x103] == 22);
    CHECK(save_file_header[0x183] == 33 && save_file_header[0x207] == 44);
    CHECK(save_file_header[0x289] == 55);
    hm_timing_reset((hm_timing_host){NULL, hm_wait_tick, NULL});
    hm_expected_file = 0x10;
    hm_expected_size = sizeof(hm_icons);
    hm_expected_destination = hm_icons;
    save_load_payload();
    CHECK(!hm_bad && hm_reads == 1 && hm_host_ticks == 2 && hm_timing_ticks() == 2);
    hm_expected_file = SAVE_SPRITE_FILE;
    hm_expected_size = SAVE_SPRITE_SIZE;
    hm_expected_destination = save_file_header;
    save_load_image_file();
    CHECK(!hm_bad && hm_reads == 2 && hm_host_ticks == 4 && hm_timing_ticks() == 4);
    CHECK(hm_bindings == SAVE_SPRITE_COUNT && hm_textures == 1);
    hm_card_attach((hm_card_storage){NULL, hm_empty_card_read, hm_empty_card_write});
    McxStartCom();
    CHECK(save_card_probe(SAVE_PORT_1) == SAVE_CARD_OK);
    CHECK(save_card_poll(SAVE_PORT_1) == SAVE_CARD_BUSY);
    CHECK(save_card_poll(SAVE_PORT_1) == SAVE_CARD_OK);
    CHECK(save_card_probe(SAVE_PORT_2) == SAVE_CARD_OK);
    CHECK(save_card_poll(SAVE_PORT_2) == SAVE_CARD_BUSY);
    CHECK(save_card_poll(SAVE_PORT_2) == SAVE_CARD_OK);
    hm_card_detach();
    return 0;
}
