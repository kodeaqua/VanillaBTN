/* ps1_card_test.c — synthetic tests for PS1 memory card system and crash recovery */

#include "ps1_card.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define rmdir _rmdir
#else
#include <unistd.h>
#endif

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

int main(void) {
    test_failed_replacement_preserves_card();
    test_card_format_and_validate();
    test_card_file_roundtrip_and_delete();
    test_card_farm_slot_header_and_checksum();
    test_atomic_save_and_crash_recovery();
    test_card_manager();
    printf("ps1_card tests passed.\n");
    return 0;
}
