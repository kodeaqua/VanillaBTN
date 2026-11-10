/* ps1_card_test.c — synthetic tests for PS1 memory card system and crash recovery */

#include "ps1_card.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define rmdir _rmdir
#else
#include <unistd.h>
#endif

/* Fail headless tests directly instead of opening the Windows CRT dialog. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static void test_card_format_and_validate(void) {
    Ps1Card card;
    ps1_card_init(&card);
    assert(!card.present);

    ps1_card_format(&card);
    assert(card.present);
    assert(card.dirty);
    assert(ps1_card_validate(&card) == PS1_CARD_OK);
    assert(ps1_card_get_free_blocks(&card) == 15);

    /* Corrupt magic */
    card.data[0] = 'X';
    assert(ps1_card_validate(&card) == PS1_CARD_ERR_UNFORMATTED);

    /* Restore magic, corrupt checksum */
    card.data[0] = 'M';
    card.data[127] ^= 0xFF;
    assert(ps1_card_validate(&card) == PS1_CARD_ERR_DAMAGED);

    /* Restore frame 0 checksum, corrupt directory frame 1 checksum */
    card.data[127] = ps1_card_checksum(card.data);
    card.data[128 + 127] ^= 0xFF;
    assert(ps1_card_validate(&card) == PS1_CARD_ERR_DAMAGED);
}

static void test_card_file_roundtrip_and_delete(void) {
    Ps1Card card;
    ps1_card_init(&card);
    ps1_card_format(&card);

    /* Create 32 KB synthetic payload */
    uint8_t payload[PS1_CARD_SAVE_SIZE];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i & 0xFF);
    }

    const char *fn0 = "BASLUS-01115-FARM0";
    Ps1CardStatus status = ps1_card_write_file(&card, fn0, payload, sizeof(payload));
    (void)status;
    assert(status == PS1_CARD_OK);
    assert(ps1_card_get_free_blocks(&card) == 11);

    /* Find file */
    uint32_t found_sz = 0;
    int start_blk = ps1_card_find_file(&card, fn0, &found_sz);
    (void)found_sz;
    (void)start_blk;
    assert(start_blk >= 1 && start_blk <= 15);
    assert(found_sz == sizeof(payload));

    /* Read back and verify exact byte match */
    uint8_t readback[PS1_CARD_SAVE_SIZE];
    size_t actual_sz = 0;
    status = ps1_card_read_file(&card, fn0, readback, sizeof(readback), &actual_sz);
    (void)actual_sz;
    assert(status == PS1_CARD_OK);
    assert(actual_sz == sizeof(payload));
    assert(memcmp(payload, readback, sizeof(payload)) == 0);

    /* Write second file (FARM1) */
    const char *fn1 = "BASLUS-01115-FARM1";
    status = ps1_card_write_file(&card, fn1, payload, sizeof(payload));
    assert(status == PS1_CARD_OK);
    assert(ps1_card_get_free_blocks(&card) == 7);

    /* Delete first file */
    status = ps1_card_delete_file(&card, fn0);
    assert(status == PS1_CARD_OK);
    assert(ps1_card_get_free_blocks(&card) == 11);
    assert(ps1_card_find_file(&card, fn0, NULL) == 0);

    /* Second file should still exist and match */
    status = ps1_card_read_file(&card, fn1, readback, sizeof(readback), &actual_sz);
    assert(status == PS1_CARD_OK);
    assert(memcmp(payload, readback, sizeof(payload)) == 0);
}

static void test_card_farm_slot_header_and_checksum(void) {
    Ps1Card card;
    ps1_card_init(&card);
    ps1_card_format(&card);

    uint8_t save_data[PS1_CARD_SAVE_SIZE];
    memset(save_data, 0, sizeof(save_data));

    /* Populate slot header at offset 0x200 */
    uint8_t *hdr = save_data + PS1_CARD_HEADER_OFFSET;
    hdr[0] = 1; /* Year */
    hdr[1] = 0; /* Spring */
    hdr[2] = 15; /* Day */
    memcpy(hdr + 8, "Sweet Farm", 11);

    /* Calculate XOR checksum of bytes 0..126 into byte 127 */
    uint8_t c = 0;
    for (size_t i = 0; i < 127; i++) {
        c ^= hdr[i];
    }
    hdr[127] = c;

    /* Write to slot 0 */
    Ps1CardStatus st = ps1_card_write_farm_slot(&card, 0, save_data, sizeof(save_data));
    (void)st;
    assert(st == PS1_CARD_OK);

    /* Read header */
    uint8_t out_hdr[128];
    st = ps1_card_read_slot_header(&card, 0, out_hdr, sizeof(out_hdr));
    assert(st == PS1_CARD_OK);
    assert(out_hdr[0] == 1 && out_hdr[2] == 15);

    /* Corrupt byte inside header */
    uint32_t file_sz;
    int blk = ps1_card_find_file(&card, "BASLUS-01115-FARM0", &file_sz);
    (void)file_sz;
    assert(blk > 0);
    card.data[(size_t)blk * PS1_CARD_BLOCK_SIZE + PS1_CARD_HEADER_OFFSET + 5] ^= 0x55;

    /* Header read must now fail with bad checksum */
    st = ps1_card_read_slot_header(&card, 0, out_hdr, sizeof(out_hdr));
    assert(st == PS1_CARD_ERR_BAD_CHECKSUM);
}

static void test_atomic_save_and_crash_recovery(void) {
    const char *test_path = "test_virtual_card.mcr";
    const char *test_bak  = "test_virtual_card.mcr.bak";
    const char *test_tmp  = "test_virtual_card.mcr.tmp";

    remove(test_path);
    remove(test_bak);
    remove(test_tmp);

    Ps1Card card;
    ps1_card_init(&card);
    ps1_card_format(&card);

    /* Save to file */
    bool ok = ps1_card_save_to_file(&card, test_path);
    (void)ok;
    assert(ok);

    /* 1. Simulate interrupted write during temp creation:
     *    Create corrupted test_path, but valid test_bak */
    rename(test_path, test_bak);
    FILE *corrupt = fopen(test_path, "wb");
    assert(corrupt != NULL);
    if (corrupt) {
        fwrite("GARBAGE DATA", 1, 12, corrupt);
        fclose(corrupt);
    }

    /* Recovery should restore from .bak */
    ok = ps1_card_recover_file(test_path);
    assert(ok);

    Ps1Card loaded;
    ok = ps1_card_load_from_file(&loaded, test_path);
    assert(ok);
    assert(ps1_card_validate(&loaded) == PS1_CARD_OK);

    /* 2. Simulate interrupted rename where .tmp exists and primary is missing */
    remove(test_path);
    remove(test_bak);
    ok = ps1_card_save_to_file(&card, test_tmp); /* write valid card to tmp */
    assert(ok);
    rename(test_tmp, test_path); /* prepare valid card at path */
    rename(test_path, test_tmp); /* move it to .tmp simulating crash before rename */

    ok = ps1_card_recover_file(test_path);
    assert(ok);
    ok = ps1_card_load_from_file(&loaded, test_path);
    assert(ok);
    assert(ps1_card_validate(&loaded) == PS1_CARD_OK);

    remove(test_path);
    remove(test_bak);
    remove(test_tmp);
}

static void test_card_manager(void) {
    const char *custom_dir = "test_mc_dir";
#ifdef _WIN32
    _mkdir(custom_dir);
#else
    mkdir(custom_dir, 0777);
#endif

    Ps1CardManager mgr;
    bool ok = ps1_card_manager_init(&mgr, custom_dir);
    (void)ok;
    assert(ok);
    assert(mgr.cards[0].present);
    assert(mgr.cards[1].present);
    assert(ps1_card_validate(&mgr.cards[0]) == PS1_CARD_OK);
    assert(ps1_card_validate(&mgr.cards[1]) == PS1_CARD_OK);

    ps1_card_manager_shutdown(&mgr);

    char p0[512], p1[512];
    char b0[512], b1[512];
    snprintf(p0, sizeof(p0), "%s/card1.mcr", custom_dir);
    snprintf(p1, sizeof(p1), "%s/card2.mcr", custom_dir);
    snprintf(b0, sizeof(b0), "%s/card1.mcr.bak", custom_dir);
    snprintf(b1, sizeof(b1), "%s/card2.mcr.bak", custom_dir);
    remove(p0);
    remove(p1);
    remove(b0);
    remove(b1);
    rmdir(custom_dir);
}

static void test_failed_replacement_preserves_card(void) {
    Ps1Card *card = malloc(sizeof(*card));
    Ps1Card *before = malloc(sizeof(*before));
    uint8_t *data = calloc(PS1_CARD_DATA_BLOCKS, PS1_CARD_BLOCK_SIZE);
    assert(card && before && data);
    ps1_card_format(card);
    assert(ps1_card_write_file(card, "save", data, PS1_CARD_BLOCK_SIZE) == PS1_CARD_OK);
    assert(ps1_card_write_file(card, "filler", data,
        14u * PS1_CARD_BLOCK_SIZE) == PS1_CARD_OK);
    memcpy(before, card, sizeof(*before));
    assert(ps1_card_write_file(card, "save", data,
        2u * PS1_CARD_BLOCK_SIZE) == PS1_CARD_ERR_FULL);
    assert(memcmp(before, card, sizeof(*card)) == 0);
    assert(ps1_card_write_file(card, "save", data, SIZE_MAX) == PS1_CARD_ERR_FULL);
    assert(memcmp(before, card, sizeof(*card)) == 0);
    free(data);
    free(before);
    free(card);
}

/* Fixtures are raw bytes, independent of the persistence writer under test. */
static void write_image_fixture(const char *path, const void *bytes, size_t size) {
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(bytes, 1, size, file) == size);
    assert(fclose(file) == 0);
}

static void check_image_fixture(const char *path, const uint8_t *bytes, size_t size) {
    FILE *file = fopen(path, "rb");
    assert(file);
    for (size_t i = 0; i < size; ++i) assert(fgetc(file) == bytes[i]);
    assert(fgetc(file) == EOF && !ferror(file));
    assert(fclose(file) == 0);
}

static void test_invalid_load_preserves_image(void) {
    const char *path = "test_invalid_card.mcr";
    Ps1Card *source = malloc(sizeof(*source));
    Ps1CardManager *manager = calloc(1, sizeof(*manager));
    uint8_t *bytes = malloc(PS1_CARD_SIZE + 1);
    assert(source && manager && bytes);
    remove(path);
    remove("test_invalid_card.mcr.tmp");
    remove("test_invalid_card.mcr.bak");
    for (int kind = 0; kind < 4; ++kind) {
        ps1_card_format(source);
        memcpy(bytes, source->data, PS1_CARD_SIZE);
        size_t size = PS1_CARD_SIZE;
        if (kind == 0) bytes[0] = 'X';
        if (kind == 1) bytes[PS1_CARD_FRAME_SIZE + 127] ^= 1;
        if (kind == 2) size = PS1_CARD_FRAME_SIZE;
        if (kind == 3) bytes[size++] = 0x5A;
        write_image_fixture(path, bytes, size);
        Ps1Card *loaded = &manager->cards[0];
        assert(!ps1_card_load_from_file(loaded, path));
        assert(!loaded->dirty);
        if (size == PS1_CARD_SIZE) {
            assert(loaded->present);
            assert(memcmp(loaded->data, bytes, size) == 0);
        } else {
            assert(!loaded->present);
        }
        /* Shutdown must never turn a failed load into an empty replacement. */
        ps1_card_manager_shutdown(manager);
        check_image_fixture(path, bytes, size);
        remove(path);
    }
    /* Missing parent makes creation fail; never report a persisted card. */
    assert(!ps1_card_load_from_file(&manager->cards[0],
        "test_card_absent_parent/card.mcr"));
    assert(!manager->cards[0].present && !manager->cards[0].dirty);
    ps1_card_format(&manager->cards[0]);
    snprintf(manager->cards[0].filepath, sizeof(manager->cards[0].filepath),
        "%s", "test_card_absent_parent/card.mcr");
    ps1_card_manager_shutdown(manager);
    assert(manager->cards[0].dirty);
    assert(ps1_card_validate(&manager->cards[0]) == PS1_CARD_OK);
    free(bytes);
    free(manager);
    free(source);
}

static void test_recovery_validates_directory(void) {
    const char *path = "test_directory_recovery.mcr";
    const char *temp = "test_directory_recovery.mcr.tmp";
    Ps1Card *good = malloc(sizeof(*good));
    Ps1Card *bad = malloc(sizeof(*bad));
    assert(good && bad);
    ps1_card_format(good);
    memcpy(bad, good, sizeof(*bad));
    bad->data[PS1_CARD_FRAME_SIZE + 127] ^= 1;
    remove(path);
    remove(temp);
    remove("test_directory_recovery.mcr.bak");
    write_image_fixture(path, bad->data, PS1_CARD_SIZE);
    write_image_fixture(temp, good->data, PS1_CARD_SIZE);
#ifdef _WIN32
    /* Allow validation reads, but deny renaming the candidate. */
    HANDLE lock = CreateFileA(temp, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(lock != INVALID_HANDLE_VALUE);
    assert(!ps1_card_recover_file(path));
    assert(CloseHandle(lock));
    check_image_fixture(path, bad->data, PS1_CARD_SIZE);
    check_image_fixture(temp, good->data, PS1_CARD_SIZE);
#endif
    assert(ps1_card_recover_file(path));
    check_image_fixture(path, good->data, PS1_CARD_SIZE);
    remove(path);
    free(bad);
    free(good);
}

static void fixture_directory(Ps1Card *card, unsigned block, uint8_t tag,
    uint32_t size, uint16_t next, const char *name) {
    uint8_t *dir = card->data + block * PS1_CARD_FRAME_SIZE;
    memset(dir, 0, PS1_CARD_FRAME_SIZE);
    dir[0] = tag;
    for (unsigned i = 0; i < 4; ++i) dir[4 + i] = (uint8_t)(size >> (8 * i));
    dir[8] = (uint8_t)next;
    dir[9] = (uint8_t)(next >> 8);
    if (name) memcpy(dir + 10, name, strlen(name));
    dir[127] = ps1_card_checksum(dir);
}

static void test_external_block_chains(void) {
    Ps1Card *card = malloc(sizeof(*card)), *valid = malloc(sizeof(*valid));
    Ps1Card *damaged = malloc(sizeof(*damaged));
    uint8_t *out = malloc(2 * PS1_CARD_BLOCK_SIZE);
    assert(card && valid && damaged && out);
    ps1_card_format(card);
    /* Literal PS1 directory: physical block 5 -> block 1 uses wire link zero.
     * This fixture is independent of our file allocator/chain writer. */
    fixture_directory(card, 5, 0x51, 16384, 0, "EXTERNAL");
    fixture_directory(card, 1, 0x53, 0, 0xFFFF, NULL);
    memset(card->data + 5 * PS1_CARD_BLOCK_SIZE, 0x12, PS1_CARD_BLOCK_SIZE);
    memset(card->data + PS1_CARD_BLOCK_SIZE, 0x34, PS1_CARD_BLOCK_SIZE);
    card->dirty = false;
    memcpy(valid, card, sizeof(*valid));
    memset(out, 0xA5, 2 * PS1_CARD_BLOCK_SIZE);
    size_t actual = 0;
    assert(ps1_card_read_file(card, "EXTERNAL", out, 16384, &actual) == PS1_CARD_OK);
    assert(actual == 16384 && out[8191] == 0x12 && out[8192] == 0x34);
    for (int kind = 0; kind < 9; ++kind) {
        memcpy(card, valid, sizeof(*card));
        if (kind < 3) fixture_directory(card, 5, 0x51, 16384,
            kind == 0 ? 0xFFFF : kind == 1 ? 4 : 15, "EXTERNAL");
        if (kind == 3) fixture_directory(card, 1, 0x52, 0, 0xFFFF, NULL);
        if (kind == 4) fixture_directory(card, 6, 0x51, 16384, 0, "CROSSLINK");
        if (kind == 5) fixture_directory(card, 2, 0x52, 0, 0xFFFF, NULL);
        if (kind >= 6) fixture_directory(card, 5, 0x51,
            kind == 6 ? 0 : kind == 7 ? 16383 : PS1_CARD_SIZE, 0, "EXTERNAL");
        memcpy(damaged, card, sizeof(*damaged));
        assert(ps1_card_validate(card) == PS1_CARD_ERR_DAMAGED);
        memset(out, 0xA5, 16384);
        actual = 123;
        assert(ps1_card_read_file(card, "EXTERNAL", out, 128, &actual) == PS1_CARD_ERR_DAMAGED);
        for (size_t i = 0; i < 16384; ++i) assert(out[i] == 0xA5);
        assert(actual == 123);
        assert(ps1_card_delete_file(card, "EXTERNAL") == PS1_CARD_ERR_DAMAGED);
        assert(ps1_card_write_file(card, "EXTERNAL", out, 8192) == PS1_CARD_ERR_DAMAGED);
        assert(memcmp(card, damaged, sizeof(*card)) == 0);
    }
    memcpy(card, valid, sizeof(*card));
    assert(ps1_card_delete_file(card, "EXTERNAL") == PS1_CARD_OK);
    assert(ps1_card_get_free_blocks(card) == 15);
    assert(ps1_card_write_file(card, "NEW", out, 16384) == PS1_CARD_OK);
    assert(card->data[PS1_CARD_FRAME_SIZE + 8] == 1); /* block 2 minus one */
    assert(card->data[2 * PS1_CARD_FRAME_SIZE] == 0x53);
    ps1_card_format(card);
    fixture_directory(card, 5, 0x51, 16384, 14, "LAST_BLOCK");
    fixture_directory(card, 15, 0x53, 0, 0xFFFF, NULL);
    memset(card->data + 15 * PS1_CARD_BLOCK_SIZE, 0x56, PS1_CARD_BLOCK_SIZE);
    assert(ps1_card_read_file(card, "LAST_BLOCK", out, 16384, &actual) == PS1_CARD_OK);
    assert(out[8192] == 0x56 && out[16383] == 0x56);
    ps1_card_format(card);
    assert(ps1_card_write_file(card, "PADDED", out, 128) == PS1_CARD_OK);
    uint32_t allocated_size = 0;
    assert(ps1_card_find_file(card, "PADDED", &allocated_size) == 1);
    assert(allocated_size == PS1_CARD_BLOCK_SIZE);
    assert(card->data[PS1_CARD_BLOCK_SIZE + 128] == 0);
    ps1_card_format(card);
    for (unsigned b = 1; b <= 15; ++b)
        fixture_directory(card, b, 0xA1, 0, 0xFFFF, NULL);
    assert(ps1_card_get_free_blocks(card) == 15);
    assert(ps1_card_write_file(card, "REUSED", out, 8192) == PS1_CARD_OK);
    ps1_card_format(card);
    for (unsigned f = 16; f < 36; ++f) {
        const uint8_t *entry = card->data + f * PS1_CARD_FRAME_SIZE;
        assert(entry[0] == 255 && entry[1] == 255 && entry[2] == 255 && entry[3] == 255);
        assert(entry[127] == ps1_card_checksum(entry));
    }
    assert(memcmp(card->data, card->data + 63 * PS1_CARD_FRAME_SIZE, 128) == 0);
    free(out);
    free(damaged);
    free(valid);
    free(card);
}

int main(void) {
    test_external_block_chains();
    test_invalid_load_preserves_image();
    test_recovery_validates_directory();
    test_failed_replacement_preserves_card();
    test_card_format_and_validate();
    test_card_file_roundtrip_and_delete();
    test_card_farm_slot_header_and_checksum();
    test_atomic_save_and_crash_recovery();
    test_card_manager();
    printf("ps1_card tests passed.\n");
    return 0;
}
