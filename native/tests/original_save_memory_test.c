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
    return 0;
}
