#include "sdk_card.h"
#include <libmcrd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static unsigned char data[16384], snapshot[PS1_CARD_SIZE];
static unsigned int buffer[32];

static void completion(long expected, long expected_command) {
    long command = -1, result = -1;
    CHECK(MemCardSync(1, &command, &result) == 1);
    CHECK(result == expected && command == expected_command);
    CHECK(MemCardSync(1, &command, &result) == -1);
}

int main(void) {
    Ps1Card *card = sdk_card_get(0);
    MemCardInit(0);
    CHECK(MemCardSync(1, NULL, NULL) == -1);
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = (unsigned char)i;
    CHECK(ps1_card_write_file(card, "TEST", data, sizeof(data)) == PS1_CARD_OK);
    card->dirty = false;
    memcpy(snapshot, card->data, sizeof(snapshot));
    CHECK(MemCardCreateFile(0, "TEST", 1) == McErrAlreadyExist);
    CHECK(!card->dirty && memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    CHECK(MemCardReadFile(0, "TEST", buffer, 8128, sizeof(buffer)) == 1);
    completion(McErrNone, 3);
    CHECK(memcmp(buffer, data + 8128, sizeof(buffer)) == 0);
    CHECK(ps1_card_write_file(card, "SECOND", data, 128) == PS1_CARD_OK);
    CHECK(ps1_card_write_file(card, "THIRD", data, 128) == PS1_CARD_OK);
    struct DIRENTRY entries[2], guard;
    memset(entries, 0xA5, sizeof(entries));
    guard = entries[1];
    long files = -1;
    CHECK(MemCardGetDirentry(0, "*", entries, &files, 0, 1) == 0);
    CHECK(files == 1 && memcmp(&guard, &entries[1], sizeof(guard)) == 0);
    CHECK(MemCardGetDirentry(0, "*", NULL, &files, 0, 0) == 0 && files == 0);
    CHECK(MemCardGetDirentry(0, "*", entries, &files, -1, 1) == -1);
    CHECK(MemCardCreateFile(0, "HUGE", 16) == -1);
    CHECK(MemCardOpen(0, "TEST", 0) == -1);
    CHECK(MemCardReadData(buffer, 0, 128) == 0);
    CHECK(MemCardWriteData(buffer, 0, 128) == 0);
    card->dirty = false;
    memcpy(snapshot, card->data, sizeof(snapshot));
    CHECK(MemCardWriteFile(0, "TEST", buffer, -1, 128) == 0);
    CHECK(MemCardWriteFile(0, "TEST", buffer, sizeof(data) - 2, 128) == 1);
    completion(McErrCardInvalid, 4);
    CHECK(MemCardWriteFile(42, "TEST", buffer, 0, 128) == 1);
    completion(McErrCardInvalid, 4);
    CHECK(!card->dirty && memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    CHECK(MemCardReadFile(0, "TEST", buffer, 0, -1) == 0);
    CHECK(MemCardReadFile(0, "TEST", buffer, 0, 4) == 0);
    CHECK(MemCardAccept(0) == 1);
    CHECK(MemCardExist(0) == 0); /* One unconsumed completion at a time. */
    completion(McErrNone, 2);
    CHECK(MemCardExist(0) == 1);
    completion(McErrNone, 1);
    CHECK(MemCardExist(0) == 1);
    long command = -1, result = -1;
    CHECK(MemCardSync(2, &command, &result) == -1);
    CHECK(MemCardSync(0, &command, &result) == 1);
    CHECK(command == 1 && result == 0);
    card->present = false;
    /* Formatting an absent card must not manufacture media or erase bytes. */
    CHECK(MemCardFormat(0) == McErrCardNotExist);
    CHECK(!card->present && !card->dirty);
    CHECK(memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    CHECK(MemCardCreateFile(0, "MISSING", 1) == McErrCardNotExist);
    CHECK(!card->dirty && memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    CHECK(MemCardReadFile(0, "TEST", buffer, 0, 128) == 1);
    completion(McErrCardNotExist, 3);
    card->present = true;
    /* A checksum-valid truncated chain must fail before even its first byte. */
    int first = ps1_card_find_file(card, "TEST", NULL);
    unsigned char *entry = card->data + first * PS1_CARD_FRAME_SIZE;
    entry[8] = entry[9] = 0xFF;
    entry[127] = 0;
    for (int i = 0; i < 127; ++i) entry[127] ^= entry[i];
    memcpy(snapshot, card->data, sizeof(snapshot));
    memset(buffer, 0xA5, sizeof(buffer));
    CHECK(MemCardReadFile(0, "TEST", buffer, 0, 128) == 1);
    completion(McErrCardInvalid, 3);
    CHECK(buffer[0] == 0xA5A5A5A5u);
    CHECK(MemCardWriteFile(0, "TEST", buffer, 0, 128) == 1);
    completion(McErrCardInvalid, 4);
    CHECK(!card->dirty && memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    /* Imported physical block 5 -> block 1, encoded as wire next=0. */
    ps1_card_format(card);
    unsigned char *head = card->data + 5 * PS1_CARD_FRAME_SIZE;
    unsigned char *last = card->data + PS1_CARD_FRAME_SIZE;
    memset(head, 0, PS1_CARD_FRAME_SIZE);
    head[0] = 0x51; head[5] = 0x40;
    memcpy(head + 10, "EXTERNAL", 8);
    head[127] = ps1_card_checksum(head);
    memset(last, 0, PS1_CARD_FRAME_SIZE);
    last[0] = 0x53; last[8] = last[9] = 0xFF;
    last[127] = ps1_card_checksum(last);
    memset(card->data + 5 * PS1_CARD_BLOCK_SIZE, 0x12, PS1_CARD_BLOCK_SIZE);
    memset(card->data + PS1_CARD_BLOCK_SIZE, 0x34, PS1_CARD_BLOCK_SIZE);
    CHECK(MemCardReadFile(0, "EXTERNAL", buffer, 8128, 128) == 1);
    completion(McErrNone, McFuncReadFile);
    const unsigned char *bytes = (const unsigned char *)buffer;
    for (unsigned i = 0; i < 128; ++i) CHECK(bytes[i] == (i < 64 ? 0x12 : 0x34));
    memset(buffer, 0xAB, sizeof(buffer));
    CHECK(MemCardWriteFile(0, "EXTERNAL", buffer, 8128, 128) == 1);
    completion(McErrNone, McFuncWriteFile);
    CHECK(card->dirty);
    for (unsigned i = 0; i < 64; ++i) {
        CHECK(card->data[5 * PS1_CARD_BLOCK_SIZE + 8128 + i] == 0xAB);
        CHECK(card->data[PS1_CARD_BLOCK_SIZE + i] == 0xAB);
    }
    CHECK(card->data[5 * PS1_CARD_BLOCK_SIZE] == 0x12);
    CHECK(card->data[PS1_CARD_BLOCK_SIZE + 64] == 0x34);
    CHECK(MemCardCreateFile(0, "FILLER", 13) == McErrNone);
    CHECK(ps1_card_get_free_blocks(card) == 0);
    card->dirty = false;
    memcpy(snapshot, card->data, sizeof(snapshot));
    CHECK(MemCardCreateFile(0, "FULL", 1) == McErrBlockFull);
    CHECK(!card->dirty && memcmp(snapshot, card->data, sizeof(snapshot)) == 0);
    card->data[0] = 'X';
    CHECK(MemCardCreateFile(0, "UNFORMATTED", 1) == McErrNotFormat);
    card->data[0] = 'M';
    card->data[127] ^= 1;
    CHECK(MemCardCreateFile(0, "DAMAGED", 1) == McErrCardInvalid);
    CHECK(MemCardFormat(0) == McErrNone);
    CHECK(card->present && card->dirty && ps1_card_validate(card) == PS1_CARD_OK);
    return 0;
}
